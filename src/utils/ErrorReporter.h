#pragma once
#include "MediaCap_types.h"
#include "Utf8.h"
#include <atomic>
#include <cstdio>
#include <exception>
#include <functional>
#include <mutex>

namespace MediaCap::Errors {
void publish(const MediaCap_Error& error) noexcept;
uint64_t next_session_id() noexcept;

// Session state outlives every worker reporting into it. First failure is sticky
// so a later generic readiness failure cannot replace the backend root cause.
class State {
public:
    const uint64_t id = next_session_id();
    std::function<void()> on_failure; // Set once before any worker starts.
    bool failed() const noexcept { return m_failed.load(); }
    MediaCap_Error get() const { std::lock_guard<std::mutex> lock(m_mutex); return m_error; }
    void reset() { std::lock_guard<std::mutex> lock(m_mutex); m_error = {}; m_failed.store(false); }
    void record(MediaCap_Error error) noexcept {
        bool first = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_failed.load()) { m_error = error; m_failed.store(true); first = true; }
        }
        if (first && on_failure) { try { on_failure(); } catch (...) {} }
    }
private:
    mutable std::mutex m_mutex;
    MediaCap_Error m_error{};
    std::atomic<bool> m_failed{false};
};

// One reporting path for all components. Configure before starting workers.
class Reporter {
public:
    explicit Reporter(MediaCap_ErrorComponent component) : m_component(component) {}
    void configure(State* state, MediaCap_SourceType source = MEDIACAP_SOURCE_TYPE_UNKNOWN, int index = -1) {
        m_state = state; m_source = source; m_index = index;
    }
    void report(MediaCap_ErrorCode code, const char* message, int64_t native = 0) const noexcept {
        MediaCap_Error error{};
        error.code = code; error.component = m_component; error.source_type = m_source;
        error.source_index = m_index; error.session_id = m_state ? m_state->id : 0; error.native_code = native;
        Utf8::copy(error.message, message ? message : "Unknown failure");
        if (m_state) m_state->record(error);
        publish(error);
    }
    void exception(const char* fallback) const noexcept {
        try { throw; }
        catch (const std::exception& error) { report(MEDIACAP_ERROR_INTERNAL, error.what()); }
        catch (...) { report(MEDIACAP_ERROR_INTERNAL, fallback); }
    }
private:
    MediaCap_ErrorComponent m_component;
    State* m_state = nullptr;
    MediaCap_SourceType m_source = MEDIACAP_SOURCE_TYPE_UNKNOWN;
    int m_index = -1;
};
}
