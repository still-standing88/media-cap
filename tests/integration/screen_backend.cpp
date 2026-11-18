#include "../support/test.hpp"
#include "internal/ThreadManager.h"
#include <stdexcept>
using namespace SL::Screen_Capture;
namespace {
std::atomic<int> attempts{0}, frames{0};
std::atomic<bool> fail_always{false};
std::atomic<bool> throw_on_init{false}, reported{false};
struct Processor : BaseFrameProcessor {
    DUPL_RETURN Init(std::shared_ptr<Thread_Data> data, const Window&) {
        Data = data;
        if (throw_on_init) throw std::runtime_error("injected worker failure");
        if (++attempts == 1 || fail_always) return DUPL_RETURN_ERROR_EXPECTED;
        return DUPL_RETURN_SUCCESS;
    }
    DUPL_RETURN ProcessFrame(Window&) { ++frames; return DUPL_RETURN_SUCCESS; }
};
template<class F> bool until(F predicate) {
    const auto end = std::chrono::steady_clock::now() + 5s;
    while (!predicate() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(5ms);
    return predicate();
}
}
// Replace only OS entry points; exercise the production manager and capture loop.
namespace SL::Screen_Capture {
std::vector<Monitor> GetMonitors() { return {}; }
std::vector<Window> GetWindows() { return {}; }
void RunCaptureMonitor(std::shared_ptr<Thread_Data>, Monitor) {}
void RunCaptureMouse(std::shared_ptr<Thread_Data>) {}
void RunCaptureWindow(std::shared_ptr<Thread_Data> data, Window window) {
    TryCaptureWindow<Processor>(data, window);
}
}
TEST_CASE("Screen backend retries startup and joins a failing worker") {
    auto make = [] {
        return CreateCaptureConfiguration(WindowCallback([] { return std::vector<Window>{Window{}}; }))
            ->onError([](bool retryable, const char* operation, int64_t) {
                if (!retryable && std::string(operation) == "injected worker failure") reported = true;
            })->onNewFrame([](const Image&, const Window&) {})->start_capturing();
    };
    auto manager = make();
    CHECK_MESSAGE(until([] { return frames.load() > 0; }), "failed initialization never retried");
    CHECK_MESSAGE(attempts >= 2, "retry did not recreate worker");
    manager->pause();
    manager.reset();
    fail_always = true;
    for (int i = 0; i < 10; ++i) {
        attempts = 0;
        manager = make();
        CHECK_MESSAGE(until([] { return attempts.load() > 0; }), "worker not started");
        const auto start = std::chrono::steady_clock::now();
        manager.reset();
        CHECK_MESSAGE(std::chrono::steady_clock::now() - start < 500ms, "shutdown blocked in retry delay");
        CHECK_MESSAGE(attempts == 1, "worker restarted during shutdown");
    }
    throw_on_init = true;
    manager = make();
    CHECK_MESSAGE(until([] { return reported.load(); }), "worker exception not reported");
    manager.reset();
}
