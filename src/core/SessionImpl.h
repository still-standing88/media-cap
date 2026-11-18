#pragma once
#include "capture/Capturer.h"
#include "utils/RecordingTimeline.h"
#include "utils/FrameViews.h"
#include "audio/AudioMixer.h"
#include "integrations/FFmpegMuxer.h"
#include <condition_variable>
#include <future>
#include <mutex>
#include <iostream>
#include <stdexcept>

namespace MediaCap {
namespace SessionDetail {
inline size_t sample_bytes(MediaCap_SampleFormat format) {
    switch (format) {
    case MEDIACAP_SAMPLE_FORMAT_U8: return 1;
    case MEDIACAP_SAMPLE_FORMAT_S16: return 2;
    case MEDIACAP_SAMPLE_FORMAT_S32:
    case MEDIACAP_SAMPLE_FORMAT_FLT: return 4;
    default: return 0;
    }
}
struct AudioSlice {
    std::shared_ptr<PackedFrame> backing;
    PackedFrame frame;
    uint8_t* plane = nullptr;
};
}
class SessionImpl : public std::enable_shared_from_this<SessionImpl> {
public:
    Errors::State errors;
    Errors::Reporter reporter{MEDIACAP_ERROR_COMPONENT_SESSION};
    explicit SessionImpl(std::function<int64_t()> service_clock = RecordingTimeline::now_ns)
        : service_clock(std::move(service_clock)) {
        reporter.configure(&errors);
        errors.on_failure = [this] {
            std::lock_guard<std::mutex> lock(timer_mutex); timer_cv.notify_all();
        };
    }
    ~SessionImpl() { ensure_stopped(); }
    bool is_capturing() const { return recording.load() || capture_only.load(); }
    bool set_frame_callbacks(MediaCap_AudioFrameCallback audio, MediaCap_VideoFrameCallback video, void* user_data) {
        if (is_capturing()) return false;
        audio_callback = audio;
        video_callback = video;
        callback_data = user_data;
        return true;
    }
    bool start_capture() { return start(nullptr, std::nullopt, std::chrono::seconds(10)); }
    bool start_recording(const MediaCap_RecordingConfig& config,
        std::optional<std::chrono::milliseconds> duration = std::nullopt,
        std::chrono::milliseconds preparation_timeout = std::chrono::seconds(10)) {
        return start(&config, duration, preparation_timeout);
    }

private:
    bool start(const MediaCap_RecordingConfig* config, std::optional<std::chrono::milliseconds> duration,
        std::chrono::milliseconds preparation_timeout) {
        if (is_capturing()) {
            if (config ? recording.load() : capture_only.load()) return true;
            Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT,
                "Stop the active capture mode before starting a different mode");
            return false;
        }
        if (timer.valid()) timer.wait();
        errors.reset();
        { std::lock_guard<std::mutex> lock(service_mutex); service_active = false; interrupted_at = -1; }
        if (capturers.empty()) {
            reporter.report(MEDIACAP_ERROR_INVALID_ARGUMENT, "No capture sources were added"); return false;
        }
        size_t audio_count = 0, video_count = 0;
        for (const auto& cap : capturers) {
            if (is_audio(*cap)) ++audio_count; else ++video_count;
        }
        if (config && audio_count && !config->audio_config) {
            reporter.report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Audio sources require an audio encoder configuration");
            return false;
        }
        if (config && video_count && !config->video_config) {
            reporter.report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Video sources require a video encoder configuration");
            return false;
        }
        if (config) {
            Errors::Reporter format_errors(MEDIACAP_ERROR_COMPONENT_ENCODER);
            format_errors.configure(&errors);
            if (!FFmpegMuxer::validate_format(*config, format_errors)) return false;
        }
        if (!config && !(audio_count && audio_callback) && !(video_count && video_callback)) {
            reporter.report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Capture requires a callback for a configured source");
            return false;
        }
        try {
            if (config) {
                muxer = std::make_unique<FFmpegMuxer>(*config);
                muxer->errors.configure(&errors);
                muxer->set_media_clock([this] { return timeline.elapsed(observe_service().host_ns); });
            }
            if (config && audio_count) {
                mixer = std::make_unique<AudioMixer>(MediaCap_AudioFormat{
                    MEDIACAP_SAMPLE_FORMAT_FLT, config->audio_config->sample_rate, config->audio_config->channels});
                mixer->errors.configure(&errors);
                mixer->set_output_callback([this](auto frame) { muxer->add_frame(std::move(frame)); });
            }
            {
                std::lock_guard<std::mutex> lock(delivery_mutex);
                prepared.clear(); prepared.resize(capturers.size()); phase = Phase::Preparing;
                for (size_t i = 0; i < capturers.size(); ++i) {
                    prepared[i].audio = is_audio(*capturers[i]);
                    prepared[i].type = capturers[i]->get_source_type();
                    capturers[i]->set_error_context(&errors, int(i));
                    if (prepared[i].audio && mixer) prepared[i].input = mixer->add_input();
                    capturers[i]->set_frame_callback([this, i](auto frame, int64_t observation) {
                        receive(i, std::move(frame), observation);
                    });
                }
            }
            for (auto& cap : capturers) cap->start();
            {
                std::unique_lock<std::mutex> lock(delivery_mutex);
                const auto deadline = std::chrono::steady_clock::now() + preparation_timeout;
                for (;;) {
                    check_sources_running();
                    if (std::all_of(prepared.begin(), prepared.end(), [](const auto& source) { return bool(source.latest); })) break;
                    if (std::chrono::steady_clock::now() >= deadline) {
                        for (size_t i = 0; i < prepared.size(); ++i)
                            if (!prepared[i].latest) capturers[i]->report_startup_timeout();
                        throw std::runtime_error("Timed out waiting for every capture source's first frame");
                    }
                    delivery_cv.wait_until(lock, std::min(deadline,
                        std::chrono::steady_clock::now() + std::chrono::milliseconds(20)));
                }
            }
            if (muxer && !muxer->start()) throw std::runtime_error("Could not start the media encoder");
            if (mixer) mixer->start();
            {
                std::lock_guard<std::mutex> lock(delivery_mutex);
                check_sources_running();
                if (config) begin_recording(duration);
                for (auto& source : prepared) source.latest.reset();
                phase = Phase::Live;
                paused.store(false);
                if (config) recording.store(true);
                else capture_only.store(true);
            }
            arm_timer(duration);
            return true;
        } catch (const std::exception& error) {
            if (!errors.failed()) reporter.report(MEDIACAP_ERROR_INTERNAL, error.what());
            if (is_capturing()) ensure_stopped();
            else abort_preparation();
            return false;
        } catch (...) {
            reporter.exception("Unknown recording startup failure");
            if (is_capturing()) ensure_stopped(); else abort_preparation();
            return false;
        }
    }
    void begin_recording(std::optional<std::chrono::milliseconds> duration) {
        const int64_t epoch = RecordingTimeline::now_ns();
        timeline.start(epoch);
        {
            std::lock_guard<std::mutex> service(service_mutex);
            last_service = service_clock(); service_active = true;
        }
        if (duration) timeline.set_deadline(epoch + std::max<int64_t>(0, duration->count()) * 1000000LL);
        for (auto& source : prepared) {
            if (source.audio) audio_frame(source.input, source.latest, source.observation);
            else {
                auto initial = std::make_shared<PackedFrame>(*source.latest);
                std::get<MediaCap_VideoFrame>(*initial).timestamp_ns = epoch;
                auto owner = source.latest;
                video_frame(std::shared_ptr<PackedFrame>(initial.get(),
                    [initial, owner](PackedFrame*) {}));
            }
        }
    }

public:
    void arm_timer(std::optional<std::chrono::milliseconds> duration) {
        if (timer.valid()) timer.wait();
        { std::lock_guard<std::mutex> lock(timer_mutex); timer_cancelled = false; }
        timer = std::async(std::launch::async, [weak = weak_from_this(), duration] {
            auto owner = weak.lock();
            if (!owner) return;
            {
                std::unique_lock<std::mutex> lock(owner->timer_mutex);
                const auto done = [&] { return owner->timer_cancelled || owner->errors.failed(); };
                if (duration) owner->timer_cv.wait_for(lock, *duration, done);
                else owner->timer_cv.wait(lock, done);
                if (owner->timer_cancelled) return;
            }
            owner->ensure_stopped();
        });
    }
    void cancel_timer() {
        { std::lock_guard<std::mutex> lock(timer_mutex); timer_cancelled = true; }
        timer_cv.notify_all();
    }
    void ensure_stopped() {
        cancel_timer();
        std::lock_guard<std::mutex> lock(stop_mutex);
        if (!is_capturing()) return;
        ServiceTime service{};
        int64_t end = -1;
        if (recording.load()) {
            service = observe_service();
            end = timeline.stop(service.host_ns);
        } else {
            std::lock_guard<std::mutex> delivery(delivery_mutex);
            phase = Phase::Closed;
        }
        for (auto& cap : capturers) cap->stop();
        { std::lock_guard<std::mutex> delivery(delivery_mutex); phase = Phase::Closed; prepared.clear(); }
        const int64_t flush_end = service.interrupted || !errors.failed() ? end : -1;
        if (mixer) mixer->stop(flush_end);
        if (muxer) muxer->stop(flush_end);
        capturers.clear(); mixer.reset(); muxer.reset();
        { std::lock_guard<std::mutex> lock_service(service_mutex); service_active = false; }
        paused.store(false); recording.store(false);
        capture_only.store(false);
    }
    void shutdown() {
        ensure_stopped();
        if (timer.valid()) timer.wait();
    }
    void audio_frame(size_t source, std::shared_ptr<PackedFrame> frame, int64_t observation) {
        const auto& audio = std::get<MediaCap_AudioFrame>(*frame);
        const size_t bytes = SessionDetail::sample_bytes(audio.format.format);
        if (!bytes || !mixer) return;
        for (const auto& span : timeline.map_audio(audio.timestamp_ns, audio.num_samples, audio.format.sample_rate)) {
            auto owner = std::make_shared<SessionDetail::AudioSlice>();
            owner->backing = frame;
            owner->plane = audio.data[0] + size_t(span.offset) * audio.format.channels * bytes;
            auto& sliced = owner->frame.emplace<MediaCap_AudioFrame>(audio);
            sliced.data = &owner->plane; sliced.num_samples = span.count; sliced.timestamp_ns = span.timestamp_ns;
            const int64_t observed = observation < 0 ? -1 :
                timeline.elapsed(observation + samples_to_ns(span.offset, audio.format.sample_rate));
            mixer->add_frame(source, std::shared_ptr<PackedFrame>(owner, &owner->frame), observed);
        }
    }
    void video_frame(std::shared_ptr<PackedFrame> frame) {
        const auto timestamp = timeline.map_video(std::get<MediaCap_VideoFrame>(*frame).timestamp_ns);
        if (!timestamp) return;
        auto* copy = new PackedFrame(*frame);
        std::get<MediaCap_VideoFrame>(*copy).timestamp_ns = *timestamp;
        muxer->add_frame(std::shared_ptr<PackedFrame>(copy, [frame](PackedFrame* p) { delete p; }));
    }

    std::vector<std::unique_ptr<Capturer>> capturers;
    std::unique_ptr<AudioMixer> mixer;
    std::unique_ptr<FFmpegMuxer> muxer;
    RecordingTimeline timeline;
    std::atomic<bool> recording{false}, paused{false};
    std::future<void> timer;
    std::mutex stop_mutex, timer_mutex;
    std::condition_variable timer_cv;
    bool timer_cancelled = false;

private:
    struct ServiceTime { int64_t host_ns; bool interrupted; };
    ServiceTime observe_service() {
        bool report = false;
        ServiceTime result;
        {
            std::lock_guard<std::mutex> lock(service_mutex);
            const auto now = service_clock();
            constexpr auto grace = std::chrono::seconds(3);
            if (service_active && interrupted_at < 0) {
                if (now < last_service || now - last_service >
                    std::chrono::duration_cast<std::chrono::nanoseconds>(grace).count()) {
                    interrupted_at = last_service; report = true;
                } else last_service = now;
            }
            result = {interrupted_at >= 0 ? interrupted_at : now, interrupted_at >= 0};
        }
        if (report) reporter.report(MEDIACAP_ERROR_DEVICE_LOST,
            "Capture execution was interrupted; recording stopped before the gap. Restart the session to continue.");
        return result;
    }
    std::function<int64_t()> service_clock;
    std::mutex service_mutex;
    int64_t last_service = 0, interrupted_at = -1;
    bool service_active = false;
    enum class Phase { Closed, Preparing, Live };
    struct PreparedSource {
        bool audio = false;
        MediaCap_SourceType type = MEDIACAP_SOURCE_TYPE_UNKNOWN;
        size_t input = 0;
        std::shared_ptr<PackedFrame> latest;
        int64_t observation = -1;
    };
    static bool is_audio(const Capturer& cap) {
        const auto type = cap.get_source_type();
        return type == MEDIACAP_SOURCE_TYPE_AUDIO_INPUT || type == MEDIACAP_SOURCE_TYPE_AUDIO_OUTPUT ||
            type == MEDIACAP_SOURCE_TYPE_AUDIO_PROCESS || type == MEDIACAP_SOURCE_TYPE_AUDIO_MIX;
    }
    void check_sources_running() const {
        if (errors.failed()) throw std::runtime_error("Capture preparation failed");
        for (size_t i = 0; i < capturers.size(); ++i)
            if (!capturers[i]->is_running()) {
                reporter.report(MEDIACAP_ERROR_CAPTURE_FAILED, "Capture source stopped during preparation");
                throw std::runtime_error("Capture source " + std::to_string(i) + " failed during preparation");
            }
    }
    void receive(size_t source, std::shared_ptr<PackedFrame> frame, int64_t observation) {
        std::unique_lock<std::mutex> lock(delivery_mutex);
        if (phase == Phase::Closed || !frame) return;
        if (phase == Phase::Live && muxer && observe_service().interrupted) return;
        auto& input = prepared[source];
        const auto type = input.type;
        if (input.audio) {
            const auto* audio = std::get_if<MediaCap_AudioFrame>(frame.get());
            if (!audio || !audio->num_samples || !audio->format.sample_rate || !audio->format.channels ||
                !SessionDetail::sample_bytes(audio->format.format) || !audio->data || !audio->data[0]) return;
        } else {
            const auto* video = std::get_if<MediaCap_VideoFrame>(frame.get());
            if (!video || !video->width || !video->height || !video->data[0]) return;
        }
        if (phase == Phase::Preparing) {
            input.latest = frame; input.observation = observation;
            delivery_cv.notify_one();
        } else if (muxer) {
            if (input.audio) audio_frame(input.input, frame, observation);
            else video_frame(frame);
        }
        lock.unlock();
        try {
            if (const auto* audio = std::get_if<MediaCap_AudioFrame>(frame.get())) {
                if (audio_callback) {
                    const MediaCap_AudioFrameView view{audio->data[0], audio->num_samples, audio->format, audio->timestamp_ns};
                    audio_callback(source, type, &view, callback_data);
                }
            } else if (video_callback) {
                const auto view = video_frame_view(std::get<MediaCap_VideoFrame>(*frame));
                video_callback(source, type, &view, callback_data);
            }
        } catch (...) {
            reporter.exception("Raw frame callback threw an exception");
        }
    }
    void abort_preparation() {
        { std::lock_guard<std::mutex> lock(delivery_mutex); phase = Phase::Closed; prepared.clear(); }
        for (auto& cap : capturers) cap->stop();
        if (mixer) mixer->stop(0);
        if (muxer) muxer->stop(0);
        capturers.clear(); mixer.reset(); muxer.reset();
        paused.store(false); recording.store(false);
    }
    std::mutex delivery_mutex;
    std::condition_variable delivery_cv;
    std::vector<PreparedSource> prepared;
    Phase phase = Phase::Closed;
    std::atomic<bool> capture_only{false};
    MediaCap_AudioFrameCallback audio_callback = nullptr;
    MediaCap_VideoFrameCallback video_callback = nullptr;
    void* callback_data = nullptr;
};

}
