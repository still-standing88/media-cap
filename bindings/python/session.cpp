#include "binding.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace mediacap::python {
namespace {
thread_local bool in_callback = false;

struct CallbackScope {
    const bool previous = std::exchange(in_callback, true);
    ~CallbackScope() { in_callback = previous; }
};

void check_mutation() {
    if (in_callback) throw std::runtime_error("Session mutations are unavailable inside MediaCap callbacks");
}

struct SessionState {
    std::mutex mutations;
    std::mutex handle_mutex;
    MediaCap_Session_Handle handle = nullptr;
    py::weakref owner;  // Access and reset only with the GIL held.
    std::shared_ptr<SessionState> deferred_self;

    void close_native() {
        std::lock_guard operation(mutations);
        MediaCap_Session_Handle closing;
        {
            std::lock_guard access(handle_mutex);
            closing = std::exchange(handle, nullptr);
        }
        MediaCap_Session_Destroy(closing);
    }
};

struct Runtime {
    std::atomic<bool> enabled{true};
    std::vector<std::weak_ptr<SessionState>> sessions;  // Protected by the GIL.
    PyObject* error_callback = nullptr;                // Owned reference, protected by the GIL.
    std::mutex cleanup_mutex;
    std::condition_variable cleanup_finished;
    size_t active_cleanups = 0;
    size_t active_callbacks = 0;
};

Runtime& runtime() {
    // The native diagnostic trampoline may still be unwinding during interpreter shutdown.
    static auto* value = new Runtime;
    return *value;
}

class CallbackLease {
public:
    CallbackLease() {
        auto& state = runtime();
        std::lock_guard lock(state.cleanup_mutex);
        active = state.enabled.load();
        if (active) ++state.active_callbacks;
    }

    ~CallbackLease() {
        if (!active) return;
        auto& state = runtime();
        std::lock_guard lock(state.cleanup_mutex);
        --state.active_callbacks;
        state.cleanup_finished.notify_all();
    }

    bool active = false;
};

void finish_cleanup() {
    auto& state = runtime();
    std::lock_guard lock(state.cleanup_mutex);
    --state.active_cleanups;
    state.cleanup_finished.notify_all();
}

void defer_close(std::shared_ptr<SessionState> state) {
    {
        std::lock_guard lock(runtime().cleanup_mutex);
        ++runtime().active_cleanups;
    }
    try {
        std::thread([state] {
            state->close_native();
            finish_cleanup();
        }).detach();
    } catch (...) {
        finish_cleanup();
        // A worker cannot join itself. Keep the state alive for shutdown to close.
        state->deferred_self = state;
    }
}

void report_callback_failure(py::handle callback, const std::exception& failure) {
    PyErr_SetString(PyExc_RuntimeError, failure.what());
    PyErr_WriteUnraisable(callback.ptr());
}

template <class Frame, py::object (*Copy)(const Frame&)>
void frame_callback(size_t source, MediaCap_SourceType type, const Frame* frame, void* data,
                    const char* attribute) noexcept {
    CallbackScope scope;
    const CallbackLease callback;
    if (!callback.active) return;
    py::gil_scoped_acquire acquire;
    if (!runtime().enabled.load()) return;
    const auto& owner = static_cast<SessionState*>(data)->owner;
    if (!owner) return;
    py::object callable;
    try {
        const auto session = owner();
        if (session.is_none()) return;
        callable = session.attr(attribute);
        if (!callable.is_none()) callable(source, type, Copy(*frame));
    } catch (py::error_already_set& error) {
        error.discard_as_unraisable("MediaCap frame callback");
    } catch (const std::exception& error) {
        report_callback_failure(callable ? callable : py::none(), error);
    }
}

void audio_callback(size_t source, MediaCap_SourceType type, const MediaCap_AudioFrameView* frame, void* data) {
    frame_callback<MediaCap_AudioFrameView, copy_audio_frame>(source, type, frame, data, "_audio_callback");
}

void video_callback(size_t source, MediaCap_SourceType type, const MediaCap_VideoFrameView* frame, void* data) {
    frame_callback<MediaCap_VideoFrameView, copy_video_frame>(source, type, frame, data, "_video_callback");
}

void error_callback(const MediaCap_Error* error, void*) noexcept {
    CallbackScope scope;
    const CallbackLease invocation;
    if (!invocation.active) return;
    py::gil_scoped_acquire acquire;
    auto& state = runtime();
    if (!state.enabled.load() || !state.error_callback) return;
    auto callable = py::reinterpret_borrow<py::object>(state.error_callback);
    try { callable(py::cast(*error, py::return_value_policy::copy)); }
    catch (py::error_already_set& failure) { failure.discard_as_unraisable("MediaCap error callback"); }
    catch (const std::exception& failure) { report_callback_failure(callable, failure); }
}

void set_error_callback(const py::object& callback) {
    if (!callback.is_none() && !PyCallable_Check(callback.ptr()))
        throw py::type_error("Expected a callable or None");
    auto& state = runtime();
    if (!state.enabled.load()) throw std::runtime_error("MediaCap is shutting down");
    auto* next = callback.is_none() ? nullptr : callback.inc_ref().ptr();
    auto* previous = std::exchange(state.error_callback, next);
    MediaCap_SetErrorCallback(next ? error_callback : nullptr, nullptr);
    Py_XDECREF(previous);
}

void shutdown() {
    auto& state = runtime();
    if (!state.enabled.exchange(false)) return;
    MediaCap_SetErrorCallback(nullptr, nullptr);
    auto sessions = std::move(state.sessions);
    auto* callback = std::exchange(state.error_callback, nullptr);
    Py_XDECREF(callback);
    for (auto& entry : sessions) {
        if (auto session = entry.lock()) {
            session->owner = py::weakref();
            session->deferred_self.reset();
            py::gil_scoped_release release;
            session->close_native();
        }
    }
    py::gil_scoped_release release;
    std::unique_lock lock(state.cleanup_mutex);
    state.cleanup_finished.wait(lock, [&] { return state.active_cleanups == 0 && state.active_callbacks == 0; });
}

template<class F>
bool with_recording_config(const RecordingConfig& config, F call) {
    auto video = config.video_config;
    auto audio = config.audio_config;
    const auto container = config.container;
    const auto path = py::module_::import("os").attr("fspath")(config.output_filename);
    if (!py::isinstance<py::str>(path)) throw py::type_error("Expected a text output path");
    const auto filename = checked_text(path.cast<py::str>());
    const MediaCap_RecordingConfig settings{filename.c_str(), container,
        video ? &*video : nullptr, audio ? &*audio : nullptr};
    return call(&settings);
}

class Session {
public:
    Session() : state_(std::make_shared<SessionState>()) {
        auto& active = runtime();
        if (!active.enabled.load()) throw std::runtime_error("MediaCap is shutting down");
        active.sessions.erase(std::remove_if(active.sessions.begin(), active.sessions.end(),
            [](const auto& entry) { return entry.expired(); }), active.sessions.end());
        active.sessions.emplace_back(state_);
        state_->handle = MediaCap_Session_Create();
        if (!state_->handle) throw std::bad_alloc();
    }

    ~Session() {
        state_->owner = py::weakref();
        if (in_callback) {
            defer_close(std::move(state_));
        } else {
            py::gil_scoped_release release;
            state_->close_native();
        }
    }

    void close() {
        check_mutation();
        {
            py::gil_scoped_release release;
            state_->close_native();
        }
        state_->owner = py::weakref();
    }

    template <class F> auto read(F fn) const {
        std::lock_guard lock(state_->handle_mutex);
        const auto handle = state_->handle;
        if (!handle) throw std::runtime_error("MediaCap session is closed");
        return fn(handle);
    }

    template <class F> auto mutate(F fn) {
        check_mutation();
        py::gil_scoped_release release;
        std::lock_guard lock(state_->mutations);
        const auto handle = state_->handle;
        if (!handle) throw std::runtime_error("MediaCap session is closed");
        return fn(handle);
    }

    bool set_callbacks(const py::object& owner, const py::object& audio, const py::object& video) {
        check_mutation();
        const bool with_audio = !audio.is_none(), with_video = !video.is_none();
        if ((with_audio && !PyCallable_Check(audio.ptr())) || (with_video && !PyCallable_Check(video.ptr())))
            throw py::type_error("Expected a callable or None");
        auto self = py::weakref(owner);
        // Retain previous references until after the mutation lock is dropped.
        py::object previous_audio, previous_video;
        py::gil_scoped_release release;
        std::lock_guard lock(state_->mutations);
        if (!state_->handle) throw std::runtime_error("MediaCap session is closed");
        if (!MediaCap_Session_SetFrameCallbacks(state_->handle, with_audio ? audio_callback : nullptr,
                                               with_video ? video_callback : nullptr, state_.get()))
            return false;
        py::gil_scoped_acquire acquire;
        previous_audio = py::getattr(owner, "_audio_callback", py::none());
        previous_video = py::getattr(owner, "_video_callback", py::none());
        owner.attr("_audio_callback") = audio;
        owner.attr("_video_callback") = video;
        state_->owner = std::move(self);
        return true;
    }

    bool add_audio_source(const AudioSourceConfig& config) {
        const auto snapshot = config;
        auto settings = snapshot.settings;
        if (snapshot.device_id) settings.device_id = snapshot.device_id->c_str();
        if (snapshot.mix_mic_device_id) settings.mix_mic_device_id = snapshot.mix_mic_device_id->c_str();
        if (snapshot.mix_system_device_id) settings.mix_system_device_id = snapshot.mix_system_device_id->c_str();
        if (!snapshot.exclude_pids.empty()) settings.exclude_pids = snapshot.exclude_pids.data();
        settings.exclude_pid_count = snapshot.exclude_pids.size();
        return mutate([&](auto handle) { return MediaCap_Session_AddAudioCaptureSource(handle, &settings) != 0; });
    }

    bool start_recording(const RecordingConfig& config, std::optional<int64_t> duration_ms) {
        return with_recording_config(config, [&](const MediaCap_RecordingConfig* settings) {
            return mutate([&](auto handle) {
                return duration_ms ? MediaCap_Session_StartRecordingForDuration(handle, settings, *duration_ms) != 0
                                   : MediaCap_Session_StartRecording(handle, settings) != 0;
            });
        });
    }

private:
    std::shared_ptr<SessionState> state_;
};

template <class Source, class Config>
void bind_source(py::class_<Session>& session,
                 void (*add)(MediaCap_Session_Handle, const Source*, const Config*)) {
    session.def("add_source", [add](Session& self, Source source, Config config) {
        self.mutate([&](auto handle) { add(handle, &source, &config); });
    }, py::arg("source"), py::arg("config"));
}
}

void bind_session(py::module_& module) {
    auto session = py::class_<Session>(module, "Session", py::dynamic_attr());
    session.def(py::init<>())
        .def("close", [](const py::object& object) {
            object.cast<Session&>().close();
            object.attr("_audio_callback") = py::none();
            object.attr("_video_callback") = py::none();
        })
        .def("__enter__", [](py::object object) {
            object.cast<Session&>().read(MediaCap_Session_GetId);
            return object;
        })
        .def("__exit__", [](py::object object, py::object, py::object, py::object) { object.attr("close")(); })
        .def("add_audio_source", &Session::add_audio_source, py::arg("config"))
        .def("set_frame_callbacks", [](const py::object& object, const py::object& audio, const py::object& video) {
            return object.cast<Session&>().set_callbacks(object, audio, video);
        }, py::arg("audio_callback") = py::none(), py::arg("video_callback") = py::none())
        .def("start_capture", [](Session& self) { return self.mutate(MediaCap_Session_StartCapture) != 0; })
        .def("stop_capture", [](Session& self) { self.mutate(MediaCap_Session_StopCapture); })
        .def("start_recording", &Session::start_recording, py::arg("config"), py::arg("duration_ms") = py::none())
        .def("stop_recording", [](Session& self) { self.mutate(MediaCap_Session_StopRecording); })
        .def("set_paused", [](Session& self, bool paused) {
            self.mutate([paused](auto handle) { MediaCap_Session_SetPaused(handle, paused); });
        }, py::arg("paused"))
        .def("is_capturing", [](const Session& self) { return self.read(MediaCap_Session_IsCapturing) != 0; })
        .def("is_recording", [](const Session& self) { return self.read(MediaCap_Session_IsRecording) != 0; })
        .def("is_paused", [](const Session& self) { return self.read(MediaCap_Session_IsPaused) != 0; })
        .def("id", [](const Session& self) { return self.read(MediaCap_Session_GetId); })
        .def("has_error", [](const Session& self) { return self.read(MediaCap_Session_HasError) != 0; })
        .def("get_error", [](const Session& self) {
            return self.read([](auto handle) {
                MediaCap_Error result{};
                MediaCap_Session_GetError(handle, &result);
                return result;
            });
        });

    bind_source(session, MediaCap_Session_AddMonitorSource);
    bind_source(session, MediaCap_Session_AddWindowSource);
    bind_source(session, MediaCap_Session_AddCameraSource);
    bind_source(session, MediaCap_Session_AddAudioSource);

    module.def("set_error_callback", &set_error_callback, py::arg("callback"));
    module.def("get_last_error", [] {
        MediaCap_Error result{};
        MediaCap_GetLastError(&result);
        return result;
    });
    module.def("clear_last_error", &MediaCap_ClearLastError);
    module.def("validate_recording_format", [](const RecordingConfig& config) {
        return with_recording_config(config, [](const MediaCap_RecordingConfig* settings) {
            py::gil_scoped_release release;
            return MediaCap_ValidateRecordingFormat(settings) != 0;
        });
    }, py::arg("config"));
    py::module_::import("atexit").attr("register")(py::cpp_function(&shutdown));
}
}
