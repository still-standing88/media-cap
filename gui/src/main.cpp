#include "capture.hpp"
#include "third_party/keyboard/keyboard.hpp"
#include "audiopix.h"

#include <algorithm>
#include <chrono>
#include <codecvt>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <locale>
#include <stdexcept>

namespace audiopix {
namespace {
using namespace std::chrono_literals;
using Row = slint::language::StandardListViewItem;

std::string text(const slint::SharedString& value) { return std::string(std::string_view(value)); }

template<class Type>
auto model(std::vector<Type> rows) {
    return std::make_shared<slint::VectorModel<Type>>(std::move(rows));
}

bool valid(int index, size_t count) { return index >= 0 && size_t(index) < count; }

std::string display_name(const std::string& name) {
    return name.find_first_not_of(" \t\r\n") == std::string::npos ? "(Untitled)" : name;
}

slint::Image app_icon() {
    slint::SharedPixelBuffer<slint::Rgba8Pixel> pixels(32, 32);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const int dx = std::max({6 - x, x - 25, 0});
            const int dy = std::max({6 - y, y - 25, 0});
            slint::Rgba8Pixel pixel{0, 0, 0, 0};
            if (dx * dx + dy * dy <= 36) {
                pixel = {15, 51, 63, 255};
                constexpr int heights[]{3, 7, 11, 7, 3};
                if (x >= 5 && x < 28 && (x - 5) % 5 < 3 &&
                    std::abs(y - 15) <= heights[(x - 5) / 5])
                    pixel = {123, 244, 215, 255};
            }
            pixels.begin()[y * 32 + x] = pixel;
        }
    }
    return slint::Image(pixels);
}

std::wstring wide(const std::string& utf8) {
#ifdef _WIN32
    if (utf8.empty()) return {};
    const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), int(utf8.size()), nullptr, 0);
    if (!length) throw std::invalid_argument("Shortcut is not valid UTF-8.");
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), int(utf8.size()), result.data(), length);
    return result;
#else
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(utf8);
#endif
}

class App {
public:
    App() {
        try {
            settings = load_settings(path);
        } catch (const std::exception& error) {
            settings = load_settings({});
            status(error.what());
        }
        sync_preferences();
        sync_shortcut_fields();
        sync_sessions();

        const auto icon = app_icon();
        ui->set_app_icon(icon);
        tray->set_app_icon(icon);
        wire_actions();
        try { register_shortcuts(settings.shortcuts); }
        catch (const std::exception& error) { status(std::string("Global shortcuts: ") + error.what()); }
    }

    ~App() {
        shortcuts.stop();
        timer.stop();
        if (job.valid()) job.wait();
    }

    void run(bool smoke_test) {
        ui->show();
        timer.start(slint::TimerMode::Repeated, 48ms, [this] { tick(); });
        if (smoke_test) slint::Timer::single_shot(300ms, [] { slint::quit_event_loop(); });
        slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
    }

private:
    enum class Mode { Idle, Recording, Preview };
    using Completion = std::function<void()>;

    const std::filesystem::path path = config_path();
    Settings settings;
    Capture capture;
    std::future<Completion> job;
    slint::ComponentHandle<AudioPixWindow> ui = AudioPixWindow::create();
    slint::ComponentHandle<AudioPixTray> tray = AudioPixTray::create();
    slint::Timer timer;
    std::atomic<unsigned> shortcut_actions{0};
    Keyboard::Listener shortcuts;
    Mode mode = Mode::Idle;
    int session_index = 0;
    int source_index = -1;
    Source draft;
    std::vector<Candidate> candidates;
    bool quitting = false;

    SavedSession& session() { return settings.sessions[size_t(session_index)]; }
    bool idle() const { return mode == Mode::Idle && !job.valid() && !quitting; }
    bool selected() { return valid(source_index, session().sources.size()); }

    void status(const std::string& value) { ui->set_status(slint::SharedString(value)); }

    void sync_sessions() {
        std::vector<Row> names;
        for (const auto& item : settings.sessions) names.push_back({slint::SharedString(item.name)});
        ui->set_sessions(model(std::move(names)));
        sync_session_selection();
    }

    void sync_session_selection() {
        ui->set_session_index(session_index);
        ui->set_saved_session_name(slint::SharedString(session().name));
        sync_sources();
    }

    void sync_sources() {
        std::vector<Row> names;
        for (const auto& item : session().sources) {
            const auto label = std::string(kind_names[size_t(item.kind)]) + "  ·  " +
                display_name(item.name);
            names.push_back({slint::SharedString(label)});
        }
        ui->set_sources(model(std::move(names)));
        ui->set_source_index(source_index);
    }

    void sync_preferences() {
        AudioSettings audio{};
        audio.rate = settings.rate;
        audio.channels = settings.channels;
        audio.kbps = settings.audio_kbps;
        audio.codec = settings.audio_codec;
        ui->set_audio(audio);
        VideoSettings video{};
        video.width = settings.width;
        video.height = settings.height;
        video.fps = settings.fps;
        video.kbps = settings.video_kbps;
        video.codec = settings.video_codec;
        ui->set_video(video);
        ui->set_output(slint::SharedString(settings.output));
        ui->set_close_to_tray(settings.close_to_tray);
    }

    void sync_shortcut_fields() {
        ui->set_start_key(slint::SharedString(settings.shortcuts[0]));
        ui->set_pause_key(slint::SharedString(settings.shortcuts[1]));
        ui->set_stop_key(slint::SharedString(settings.shortcuts[2]));
    }

    void read_preferences() {
        const auto audio = ui->get_audio();
        settings.rate = audio.rate;
        settings.channels = audio.channels;
        settings.audio_kbps = audio.kbps;
        settings.audio_codec = audio.codec;
        const auto video = ui->get_video();
        settings.width = video.width;
        settings.height = video.height;
        settings.fps = video.fps;
        settings.video_kbps = video.kbps;
        settings.video_codec = video.codec;
        settings.output = text(ui->get_output());
        settings.close_to_tray = ui->get_close_to_tray();
    }

    void persist(const std::string& result) {
        try {
            save_settings(path, settings);
            status(result);
        } catch (const std::exception& error) {
            status(std::string("Could not save settings: ") + error.what());
        }
    }

    void register_shortcuts(const std::array<std::string, 3>& values) {
        std::array<Keyboard::Hotkey, 3> keys;
        for (size_t i = 0; i < values.size(); ++i) {
            if (values[i].find_first_not_of(" \t") == std::string::npos) continue;
            keys[i] = Keyboard::parse_hotkey(wide(values[i]));
            for (auto& chord : keys[i]) {
                for (auto& codes : chord) std::ranges::sort(codes);
                std::ranges::sort(chord);
            }
            if (std::find(keys.begin(), keys.begin() + i, keys[i]) != keys.begin() + i)
                throw std::invalid_argument("Assign a different shortcut to each action.");
        }
        shortcuts.stop();
        shortcuts.clear_handlers();
        shortcut_actions = 0;
        try {
            for (size_t i = 0; i < keys.size(); ++i) {
                if (!keys[i].empty())
                    shortcuts.add_hotkey(std::move(keys[i]), [this, i] { shortcut_actions.fetch_or(1u << i); });
            }
        } catch (...) {
            shortcuts.stop();
            shortcuts.clear_handlers();
            shortcut_actions = 0;
            throw;
        }
    }

    void save_shortcuts() {
        const std::array values{text(ui->get_start_key()), text(ui->get_pause_key()), text(ui->get_stop_key())};
        try {
            register_shortcuts(values);
            settings.shortcuts = values;
            persist("Global shortcuts saved.");
        } catch (const std::exception& error) {
            if (!shortcuts.running()) {
                try { register_shortcuts(settings.shortcuts); }
                catch (...) {}
            }
            status(std::string("Global shortcuts: ") + error.what());
        }
    }

    template<class Work>
    void background(const std::string& label, Work work) {
        if (job.valid()) return;
        try {
            job = std::async(std::launch::async, std::move(work));
            ui->set_busy(true);
            status(label);
        } catch (const std::exception& error) {
            status(error.what());
        }
    }

    void begin_editor(bool adding) {
        if (!idle() || (!adding && !selected())) return;
        draft = adding ? Source{} : session().sources[size_t(source_index)];
        ui->set_source_kind(int(draft.kind));
        ui->set_capture_fps(draft.fps);
        ui->set_capture_width(draft.width);
        ui->set_capture_height(draft.height);
        ui->set_gain(draft.gain);
        ui->set_editing_name(slint::SharedString(display_name(draft.name)));
        ui->set_editing_gain(draft.kind == Kind::Process || draft.id.starts_with("flex:"));
        ui->set_editor(adding ? 1 : 2);
        if (adding) refresh_devices(int(draft.kind));
    }

    void refresh_devices(int kind) {
        if (!idle() || ui->get_editor() != 1 || !valid(kind, kind_names.size())) return;
        const auto selected_kind = Kind(kind);
        candidates.clear();
        ui->set_devices(model(std::vector<slint::SharedString>{}));
        ui->set_device_gains(model(std::vector<bool>{}));
        ui->set_device_index(-1);
        background("Looking for capture sources…", [this, selected_kind]() -> Completion {
            auto found = discover(selected_kind);
            return [this, selected_kind, found = std::move(found)]() mutable {
                if (!ui->get_editor()) return;
                if (ui->get_source_kind() != int(selected_kind)) {
                    refresh_devices(ui->get_source_kind());
                    return;
                }
                candidates = std::move(found);
                std::vector<slint::SharedString> names;
                std::vector<bool> gains;
                for (const auto& option : candidates) {
                    gains.push_back(supports_gain(option));
                    const auto name = display_name(option.name);
                    names.emplace_back(name + "  ·  " +
                        (option.id.starts_with("legacy:") || option.id.starts_with("flex:")
                            ? std::to_string(names.size() + 1) : option.id));
                }
                ui->set_devices(model(std::move(names)));
                ui->set_device_gains(model(std::move(gains)));
                ui->set_device_index(candidates.empty() ? -1 : 0);
                status(candidates.empty() ? "No matching capture sources found." : "Choose a capture source.");
            };
        });
    }

    void save_source() {
        const auto index = ui->get_device_index();
        const bool editing = ui->get_editor() == 2;
        if (!idle() || !ui->get_editor() || (editing ? !selected() : !valid(index, candidates.size()))) return;
        auto& sources = session().sources;
        Source next = draft;
        if (!editing) {
            const auto& option = candidates[size_t(index)];
            next.kind = Kind(ui->get_source_kind());
            next.id = option.id;
            next.name = option.name;
        }
        next.fps = ui->get_capture_fps();
        next.width = ui->get_capture_width();
        next.height = ui->get_capture_height();
        next.gain = next.kind == Kind::Process || next.id.starts_with("flex:") ? ui->get_gain() : 100;
        for (size_t i = 0; i < sources.size(); ++i) {
            if (editing && i == size_t(source_index)) continue;
            const auto& source = sources[i];
            if (video_source(source.kind) && video_source(next.kind)) {
                status("A MediaCap recording supports one video source.");
                return;
            }
            if (source.kind == next.kind && source.id == next.id) {
                status("This source is already in the session.");
                return;
            }
        }
        if (editing) sources[size_t(source_index)] = std::move(next);
        else {
            sources.push_back(std::move(next));
            source_index = int(sources.size()) - 1;
        }
        ui->set_editor(0);
        sync_sources();
        persist(editing ? "Source updated." : "Source added.");
    }

    void set_mode(Mode value, bool paused = false) {
        mode = value;
        const bool recording = value == Mode::Recording;
        ui->set_recording(recording);
        ui->set_paused(paused);
        ui->set_previewing(value == Mode::Preview);
        tray->set_recording(recording);
        tray->set_paused(paused);
        if (value != Mode::Preview) {
            ui->set_preview_image(slint::Image{});
            ui->set_preview_level(0);
        }
    }

    void start_recording() {
        if (!idle() || ui->get_editor() || ui->get_renaming()) return;
        read_preferences();
        const auto sources = session().sources;
        auto recording = settings;
        recording.sessions.clear();
        background("Preparing recording…", [this, sources, recording = std::move(recording)]() -> Completion {
            capture.start_recording(sources, recording);
            return [this] { set_mode(Mode::Recording); status("Recording"); };
        });
    }

    void finish(bool cancel) {
        if (mode != Mode::Recording) return;
        background(cancel ? "Canceling recording…" : "Finalizing recording…", [this, cancel]() -> Completion {
            auto result = capture.finish_recording(cancel);
            return [this, result = std::move(result)] { set_mode(Mode::Idle); status(result); };
        });
    }

    void pause_resume() {
        if (mode != Mode::Recording || quitting) return;
        background(ui->get_paused() ? "Resuming recording…" : "Pausing recording…", [this]() -> Completion {
            const bool paused = capture.toggle_pause();
            return [this, paused] { set_mode(Mode::Recording, paused); status(paused ? "Paused" : "Recording"); };
        });
    }

    void toggle_preview() {
        if (job.valid() || quitting) return;
        if (mode == Mode::Preview) { stop_preview(); return; }
        if (!idle() || !selected() || ui->get_editor()) return;
        const auto source = session().sources[size_t(source_index)];
        background("Preparing preview…", [this, source]() -> Completion {
            capture.start_preview(source);
            return [this, source] {
                ui->set_audio_preview(!video_source(source.kind));
                set_mode(Mode::Preview);
                status("Previewing " + display_name(source.name));
            };
        });
    }

    void stop_preview(std::string result = "Preview stopped.") {
        background("Stopping preview…", [this, result = std::move(result)]() -> Completion {
            capture.stop_preview();
            return [this, result] { set_mode(Mode::Idle); status(result); };
        });
    }

    void tick() {
        if (job.valid() && job.wait_for(0ms) == std::future_status::ready) {
            ui->set_busy(false);
            try { job.get()(); }
            catch (const std::exception& error) {
                if (!capture.running()) set_mode(Mode::Idle);
                status(error.what());
            }
        }
        if (job.valid()) return;

        if (quitting) {
            if (mode == Mode::Recording) finish(false);
            else if (mode == Mode::Preview) stop_preview();
            else slint::quit_event_loop();
            return;
        }
        if (mode != Mode::Idle && !capture.running()) {
            if (mode == Mode::Recording) finish(false);
            else {
                auto error = capture.failure();
                stop_preview(error.empty() ? "Preview ended." : error);
            }
            return;
        }
        if (mode == Mode::Preview) {
            const auto frame = capture.take_image();
            if (!frame.rgba.empty()) {
                slint::SharedPixelBuffer<slint::Rgba8Pixel> pixels(frame.width, frame.height);
                std::memcpy(pixels.begin(), frame.rgba.data(), frame.rgba.size());
                ui->set_preview_image(slint::Image(pixels));
            }
            ui->set_preview_level(capture.take_level());
        }
        const auto actions = shortcut_actions.exchange(0);
        if (actions & 4) finish(false);
        else if (actions & 2) pause_resume();
        else if (actions & 1) start_recording();
    }

    void wire_actions() {
        ui->on_select_session([this](int index) {
            if (!idle() || ui->get_renaming() || index == session_index || !valid(index, settings.sessions.size())) return;
            session_index = index;
            source_index = -1;
            sync_session_selection();
        });
        ui->on_select_source([this](int index) {
            if (!idle() || !valid(index, session().sources.size())) return;
            source_index = index;
            ui->set_source_index(index);
        });
        ui->on_add_session([this] {
            if (!idle() || ui->get_editor()) return;
            settings.sessions.emplace_back();
            session_index = int(settings.sessions.size()) - 1;
            source_index = -1;
            sync_sessions();
            persist("Session added.");
        });
        ui->on_rename_session([this] {
            if (!idle() || ui->get_editor()) return false;
            auto name = text(ui->get_session_name());
            if (name.find_first_not_of(" \t") == std::string::npos) { status("Enter a session name."); return false; }
            session().name = std::move(name);
            sync_sessions();
            persist("Session renamed.");
            return true;
        });
        ui->on_delete_session([this] {
            if (!idle() || ui->get_editor()) return;
            settings.sessions.erase(settings.sessions.begin() + session_index);
            if (settings.sessions.empty()) settings.sessions.emplace_back();
            session_index = std::min(session_index, int(settings.sessions.size()) - 1);
            source_index = -1;
            sync_sessions();
            persist("Session deleted.");
        });
        ui->on_edit_source([this](bool adding) { begin_editor(adding); });
        ui->on_refresh_devices([this](int kind) { refresh_devices(kind); });
        ui->on_save_source([this] { save_source(); });
        ui->on_dismiss_editor([this] { ui->set_editor(0); status("Ready"); });
        ui->on_delete_source([this] {
            if (!idle() || !selected() || ui->get_editor()) return;
            auto& sources = session().sources;
            sources.erase(sources.begin() + source_index);
            source_index = std::min(source_index, int(sources.size()) - 1);
            sync_sources();
            persist("Source deleted.");
        });
        ui->on_toggle_preview([this] { toggle_preview(); });
        ui->on_start([this] { start_recording(); });
        ui->on_finish([this](bool cancel) { finish(cancel); });
        ui->on_pause_resume([this] { pause_resume(); });
        ui->on_save_preferences([this] { read_preferences(); persist("Settings saved."); });
        ui->on_save_hotkeys([this] { save_shortcuts(); });
        ui->window().on_close_requested([this] {
            if (ui->get_close_to_tray()) return slint::CloseRequestResponse::HideWindow;
            quitting = true;
            return slint::CloseRequestResponse::KeepWindowShown;
        });
        tray->on_open([this] { ui->show(); });
        tray->on_start_stop([this] {
            if (mode == Mode::Recording) finish(false);
            else start_recording();
        });
        tray->on_pause_resume([this] { pause_resume(); });
        tray->on_exit_app([this] { quitting = true; });
    }
};
} // namespace
} // namespace audiopix

int main(int argc, char** argv) {
    try {
        audiopix::App app;
        app.run(argc == 2 && std::string_view(argv[1]) == "--smoke-test");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "AudioPix: " << error.what() << '\n';
        return 1;
    }
}
