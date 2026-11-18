#include "ErrorReporter.h"
#include "MediaCap.hpp"
#include "MediaCap_c.h"
#include <memory>

namespace {
struct Registry {
    std::mutex mutex;
    MediaCap_Error latest{};
    std::shared_ptr<const MediaCap::ErrorCallback> callback;
};
Registry& registry() { static Registry value; return value; }
thread_local bool notifying = false;
}
namespace MediaCap {
namespace Native {
void set_error_callback(ErrorCallback callback) {
    auto next = callback ? std::make_shared<const ErrorCallback>(std::move(callback)) : nullptr;
    std::lock_guard<std::mutex> lock(registry().mutex);
    registry().callback.swap(next);
}
Error get_last_error() {
    std::lock_guard<std::mutex> lock(registry().mutex); return registry().latest;
}
void clear_last_error() {
    std::lock_guard<std::mutex> lock(registry().mutex); registry().latest = {};
}
}
namespace Errors {
uint64_t next_session_id() noexcept { static std::atomic<uint64_t> next{0}; return ++next; }
void publish(const MediaCap_Error& error) noexcept {
    try {
        std::shared_ptr<const ErrorCallback> callback;
        {
            std::lock_guard<std::mutex> lock(registry().mutex);
            registry().latest = error; callback = registry().callback;
        }
        std::fprintf(stderr, "MediaCap error [%d/%d]: %s (native=%lld)\n", int(error.component),
            int(error.code), error.message, static_cast<long long>(error.native_code));
        if (callback && !notifying) {
            notifying = true;
            try { (*callback)(error); } catch (...) {}
            notifying = false;
        }
    } catch (...) {}
}
}
}
extern "C" MEDIACAP_API void MediaCap_SetErrorCallback(MediaCap_ErrorCallback callback, void* user_data) {
    try {
        if (callback) MediaCap::Native::set_error_callback([callback, user_data](const MediaCap_Error& error) { callback(&error, user_data); });
        else MediaCap::Native::set_error_callback({});
    } catch (...) { MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Error callback registration failed"); }
}
extern "C" MEDIACAP_API int MediaCap_GetLastError(MediaCap_Error* error) {
    if (!error) return 0;
    *error = MediaCap::Native::get_last_error(); return error->code != MEDIACAP_ERROR_NONE;
}
extern "C" MEDIACAP_API void MediaCap_ClearLastError() { MediaCap::Native::clear_last_error(); }
