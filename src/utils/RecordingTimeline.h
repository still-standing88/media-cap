#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <vector>

namespace MediaCap {
inline int64_t samples_to_ns(int64_t samples, int64_t rate) {
    return (samples / rate) * 1000000000LL + (samples % rate) * 1000000000LL / rate;
}
inline int64_t ns_to_samples(int64_t ns, int64_t rate) {
    return (ns / 1000000000LL) * rate + (ns % 1000000000LL) * rate / 1000000000LL;
}
// Capturers report host-clock time; the session maps it to media time.
class RecordingTimeline {
public:
    struct AudioSpan { uint32_t offset, count; int64_t timestamp_ns; };
    static int64_t now_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    void start(int64_t now = now_ns()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_epoch = now; m_stop = infinity; m_stopped = false; m_pauses.clear();
    }
    void set_deadline(int64_t host_ns) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = std::max(m_epoch, host_ns);
    }
    void set_paused(bool paused, int64_t now = now_ns()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopped) return;
        const bool was_paused = !m_pauses.empty() && m_pauses.back().end == infinity;
        if (paused && !was_paused) m_pauses.push_back({now, infinity});
        if (!paused && was_paused) m_pauses.back().end = now;
    }
    int64_t stop(int64_t now = now_ns()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = std::min(m_stop, now);
        m_stopped = true;
        return elapsed_unlocked(m_stop);
    }
    std::optional<int64_t> map_video(int64_t host_ns) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (host_ns < m_epoch || host_ns >= m_stop) return std::nullopt;
        for (const auto& p : m_pauses)
            if (host_ns >= p.begin && host_ns < p.end) return std::nullopt;
        return elapsed_unlocked(host_ns);
    }
    int64_t elapsed(int64_t host_ns) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return elapsed_unlocked(std::min(host_ns, m_stop));
    }
    // Split a block at start/pause/resume/stop boundaries, retaining ownership
    // in the caller. Samples whose start falls in an excluded interval vanish.
    std::vector<AudioSpan> map_audio(int64_t host_ns, uint32_t frames, uint32_t rate) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<AudioSpan> spans;
        if (!rate || !frames) return spans;
        const auto index_at = [&](int64_t boundary) -> uint32_t {
            if (boundary <= host_ns) return 0;
            const int64_t delta = boundary - host_ns;
            const int64_t whole = ns_to_samples(delta, rate);
            const int64_t ceil = whole + (samples_to_ns(whole, rate) < delta ? 1 : 0);
            return static_cast<uint32_t>(std::min<int64_t>(frames, ceil));
        };
        const auto append = [&](int64_t begin, int64_t end) {
            const auto first = index_at(begin);
            const auto last = end == infinity ? frames : index_at(end);
            if (last > first) spans.push_back({first, last - first,
                elapsed_unlocked(host_ns + samples_to_ns(first, rate))});
        };
        int64_t begin = m_epoch;
        for (const auto& p : m_pauses) {
            append(begin, std::min(p.begin, m_stop));
            begin = p.end;
            if (begin >= m_stop) return spans;
        }
        if (begin < m_stop) append(begin, m_stop);
        return spans;
    }
private:
    struct Pause { int64_t begin, end; };
    static constexpr int64_t infinity = (std::numeric_limits<int64_t>::max)();
    int64_t elapsed_unlocked(int64_t time) const {
        int64_t result = std::max<int64_t>(0, time - m_epoch);
        for (const auto& p : m_pauses) {
            if (time <= p.begin) break;
            result -= std::min(time, p.end) - p.begin;
        }
        return std::max<int64_t>(0, result);
    }
    mutable std::mutex m_mutex;
    int64_t m_epoch = 0, m_stop = infinity;
    bool m_stopped = false;
    std::vector<Pause> m_pauses;
};
}
