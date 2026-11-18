#pragma once

#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <optional>
#include <thread>

#include "keyboard_manager.hpp"

namespace Keyboard {

using ScanCodes = std::vector<int>;
using HotkeyStep = std::vector<ScanCodes>;
using Hotkey = std::vector<HotkeyStep>;

inline ScanCodes key_to_scan_codes(std::wstring_view name,
                                  KeyboardManager& manager = *KeyboardManager::instance()) {
    ScanCodes codes;
    for (const auto& entry : manager.map_name(name))
        if (std::find(codes.begin(), codes.end(), entry.scan_code) == codes.end())
            codes.push_back(entry.scan_code);
    return codes;
}

inline ScanCodes key_to_scan_codes(int code) { return {code}; }

namespace detail {

template<typename Callback>
void split_keys(std::wstring_view text, wchar_t delimiter, const Callback& callback) {
    for (;;) {
        const auto separator = text.find(delimiter);
        std::wstring token(text.substr(0, separator));
        wstrutil::trim(token);
        if (token.empty())
            throw std::invalid_argument("Empty key or step in hotkey");
        callback(token);
        if (separator == std::wstring_view::npos)
            return;
        text.remove_prefix(separator + 1);
    }
}

inline bool matches(const HotkeyStep& step, const ScanCodes& held, bool allow_extra = false) {
    if (step.size() > held.size() || (!allow_extra && step.size() != held.size()))
        return false;
    // Each physical key can satisfy only one term, even if two aliases overlap.
    std::vector<std::size_t> owner(held.size(), step.size());
    const auto assign = [&](const auto& self, std::size_t term, std::vector<bool>& tried) -> bool {
        for (std::size_t i = 0; i < held.size(); ++i) {
            if (tried[i] || std::find(step[term].begin(), step[term].end(), held[i]) == step[term].end())
                continue;
            tried[i] = true;
            if (owner[i] == step.size() || self(self, owner[i], tried)) {
                owner[i] = term;
                return true;
            }
        }
        return false;
    };
    for (std::size_t term = 0; term < step.size(); ++term) {
        std::vector<bool> tried(held.size());
        if (!assign(assign, term, tried))
            return false;
    }
    return true;
}

} // namespace detail

inline Hotkey parse_hotkey(std::wstring_view expression,
                           KeyboardManager& manager = *KeyboardManager::instance()) {
    if (expression.size() == 1)
        return {{key_to_scan_codes(expression, manager)}};
    Hotkey result;
    detail::split_keys(expression, L',', [&](const std::wstring& chord) {
        HotkeyStep step;
        if (chord.size() == 1)
            step.push_back(key_to_scan_codes(chord, manager));
        else
            detail::split_keys(chord, L'+', [&](const std::wstring& name) {
                step.push_back(key_to_scan_codes(name, manager));
            });
        result.push_back(std::move(step));
    });
    return result;
}

inline Hotkey parse_hotkey(int code) { return {{{code}}}; }

class Listener {
public:
    explicit Listener(std::shared_ptr<KeyboardManager> manager = KeyboardManager::instance(),
                      bool enable_suppression = false)
        : manager_(std::move(manager)), suppression_(enable_suppression) {
        if (!manager_)
            throw std::invalid_argument("Keyboard listener requires a manager");
        if (suppression_ && !manager_->supports_suppression())
            throw std::logic_error("Keyboard event suppression is unavailable on this platform");
    }

    virtual ~Listener() { stop(); }

    void start() {
        if (active_worker_ == this) {
            if (running())
                return;
            throw std::logic_error("Cannot restart a keyboard listener from its stopping callback");
        }
        std::lock_guard<std::mutex> lifecycle_lock(lifecycle_);
        if (running())
            return;
        join_workers();

        auto stop = std::make_shared<std::atomic<bool>>(false);
        std::promise<void> startup;
        auto started = startup.get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = stop;
            ++generation_;
            capture_error_ = nullptr;
            callback_error_ = nullptr;
        }

        try {
            processing_ = std::thread([this, stop] { process_events(*stop); });
            listening_ = std::thread([this, stop, startup = std::move(startup)]() mutable {
                Worker worker(this);
                bool ready = false;
                try {
                    manager_->listen([this, &stop](const KeyboardEvent& event) {
                        return capture_event(event, *stop);
                    }, stop, [&] {
                        startup.set_value();
                        ready = true;
                    }, suppression_);
                    if (!ready)
                        throw std::runtime_error("Keyboard capture stopped during initialization");
                } catch (...) {
                    if (!ready)
                        startup.set_exception(std::current_exception());
                    else {
                        std::lock_guard<std::mutex> lock(mutex_);
                        capture_error_ = std::current_exception();
                    }
                }
                request_stop();
            });
            started.get();
        } catch (...) {
            request_stop();
            join_workers();
            throw;
        }
    }

    void stop() {
        if (active_worker_ == this) {
            request_stop();
            return;
        }
        std::lock_guard<std::mutex> lifecycle_lock(lifecycle_);
        request_stop();
        join_workers();
    }

    bool running() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stop_ && !stop_->load();
    }

    void rethrow_error() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (capture_error_) std::rethrow_exception(capture_error_);
        if (callback_error_) std::rethrow_exception(callback_error_);
    }

    int add_handler(EventHandler handler) {
        return hook(std::move(handler));
    }

    int hook(EventHandler handler, bool suppress = false) {
        if (!handler)
            throw std::invalid_argument("Keyboard hook requires a callback");
        return add_dispatch([handler = std::move(handler)](const Dispatch& input) {
            return handler(input.event);
        }, suppress);
    }

    int hook_key(std::wstring_view key, EventHandler handler, bool suppress = false) {
        if (!handler)
            throw std::invalid_argument("Keyboard hook requires a callback");
        auto codes = key_to_scan_codes(key, *manager_);
        return hook([codes = std::move(codes), handler = std::move(handler), suppress](
                        const KeyboardEvent& event) {
            if (std::find(codes.begin(), codes.end(), event.scan_code) == codes.end())
                return suppress;
            return handler(event);
        }, suppress);
    }

    int block_key(std::wstring_view key) {
        return hook_key(key, [](const KeyboardEvent&) { return false; }, true);
    }

    int add_hotkey(std::wstring_view expression, std::function<void()> callback,
                   std::chrono::milliseconds timeout = std::chrono::seconds(1),
                   bool trigger_on_release = false) {
        return add_hotkey(parse_hotkey(expression, *manager_), std::move(callback),
                          timeout, trigger_on_release);
    }

    int add_hotkey(Hotkey steps, std::function<void()> callback,
                   std::chrono::milliseconds timeout = std::chrono::seconds(1),
                   bool trigger_on_release = false) {
        if (!callback || steps.empty() || timeout.count() < 0 ||
            std::any_of(steps.begin(), steps.end(), [](const auto& step) {
                return step.empty() || std::any_of(step.begin(), step.end(), [](const auto& codes) {
                    return codes.empty();
                });
            }))
            throw std::invalid_argument("Invalid hotkey or callback");
        return add_dispatch([steps = std::move(steps), callback = std::move(callback), timeout,
                             trigger_on_release, index = std::size_t{0}, releasing = ScanCodes{},
                             last = std::chrono::steady_clock::time_point{},
                             generation = std::size_t{0}](const Dispatch& input) mutable {
            if (generation != input.generation) {
                generation = input.generation;
                index = 0;
                releasing.clear();
            }
            const bool up = input.event.type == EventType::Up;
            if (input.event.is_repeat)
                return false;

            if (index && releasing.empty() && timeout.count() && input.arrival - last > timeout)
                index = 0;
            if (!releasing.empty()) {
                if (up) {
                    if (std::find(releasing.begin(), releasing.end(), input.event.scan_code) != releasing.end()) {
                        releasing.clear();
                        last = input.arrival;
                    }
                    return false;
                }
                index = 0;
                releasing.clear();
            }

            const bool final_step = index + 1 == steps.size();
            const auto advance = [&] {
                if (final_step) {
                    index = 0;
                    callback();
                } else {
                    ++index;
                    releasing = input.pressed;
                    last = input.arrival;
                }
            };
            if (up == (final_step && trigger_on_release) && detail::matches(steps[index], input.pressed)) {
                advance();
            } else if (index && !up) {
                const auto& next = steps[index];
                const bool partial = std::all_of(input.pressed.begin(), input.pressed.end(), [&](int code) {
                    return std::any_of(next.begin(), next.end(), [code](const auto& choices) {
                        return std::find(choices.begin(), choices.end(), code) != choices.end();
                    });
                });
                if (!partial || input.pressed.size() >= next.size()) {
                    if (!(final_step && trigger_on_release && detail::matches(next, input.pressed))) {
                        index = 0;
                        if (detail::matches(steps.front(), input.pressed)) {
                            index = 1;
                            releasing = input.pressed;
                            last = input.arrival;
                        }
                    }
                }
            }
            return false;
        }, false);
    }

    void remove_handler(int id) {
        std::shared_ptr<Handler> removed;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto it = handlers_.find(id);
            if (it != handlers_.end()) {
                removed = std::move(it->second);
                handlers_.erase(it);
                removed->enabled.store(false);
            }
        }
    }

    void clear_handlers() {
        std::map<int, std::shared_ptr<Handler>> removed;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            removed.swap(handlers_);
            for (const auto& entry : removed)
                entry.second->enabled.store(false);
        }
    }

    bool is_pressed(int code) {
        start();
        std::lock_guard<std::mutex> lock(mutex_);
        return pressed_.count(code) != 0;
    }

    bool is_pressed(std::wstring_view expression) {
        auto hotkey = parse_hotkey(expression, *manager_);
        if (hotkey.size() != 1)
            throw std::invalid_argument("is_pressed accepts a single hotkey step");
        start();
        std::lock_guard<std::mutex> lock(mutex_);
        // Unlike a hotkey trigger, a state query permits extra held keys.
        ScanCodes held;
        for (const auto& entry : pressed_)
            held.push_back(entry.first);
        return detail::matches(hotkey.front(), held, true);
    }

    KeyboardEvent read_event(bool suppress = false,
                             std::optional<EventType> type = std::nullopt) {
        require_external_thread();
        auto result = std::make_shared<std::promise<KeyboardEvent>>();
        auto future = result->get_future();
        const int id = hook([result, suppress, type](const KeyboardEvent& event) {
            if (type && *type != event.type)
                return suppress;
            try {
                result->set_value(event);
                return false;
            } catch (const std::future_error&) {
                return suppress;
            }
        }, suppress);
        return wait_for_result(id, future);
    }

    void wait(std::wstring_view expression, bool trigger_on_release = false) {
        require_external_thread();
        auto result = std::make_shared<std::promise<void>>();
        auto future = result->get_future();
        const int id = add_hotkey(expression, [result] {
            try { result->set_value(); } catch (const std::future_error&) {}
        }, std::chrono::seconds(1), trigger_on_release);
        wait_for_result(id, future);
    }

private:
    struct Dispatch {
        KeyboardEvent event;
        ScanCodes pressed;
        std::chrono::steady_clock::time_point arrival;
        int last_handler;
        std::size_t generation;
    };

    struct Handler {
        explicit Handler(std::function<bool(const Dispatch&)> fn, bool suppress)
            : fn(std::move(fn)), suppress(suppress) {}
        std::function<bool(const Dispatch&)> fn;
        bool suppress;
        std::atomic<bool> enabled{true};
    };

    struct Worker {
        explicit Worker(Listener* listener) { active_worker_ = listener; }
        ~Worker() { active_worker_ = nullptr; }
    };

    inline static thread_local Listener* active_worker_ = nullptr;
    std::shared_ptr<KeyboardManager> manager_;
    const bool suppression_;
    mutable std::mutex mutex_;
    std::mutex lifecycle_;
    std::condition_variable available_;
    std::thread processing_;
    std::thread listening_;
    std::shared_ptr<std::atomic<bool>> stop_;
    std::exception_ptr capture_error_;
    std::exception_ptr callback_error_;
    std::map<int, std::shared_ptr<Handler>> handlers_;
    std::deque<Dispatch> events_;
    std::map<int, std::unordered_set<std::string>> pressed_;
    int handler_id_ = 0;
    std::size_t generation_ = 0;

    int add_dispatch(std::function<bool(const Dispatch&)> fn, bool suppress) {
        if (suppress && !suppression_)
            throw std::logic_error("Create the listener with suppression enabled to block input");
        int id;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            id = ++handler_id_;
            handlers_.emplace(id, std::make_shared<Handler>(std::move(fn), suppress));
        }
        try {
            start();
        } catch (...) {
            remove_handler(id);
            throw;
        }
        return id;
    }

    void request_stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_) stop_->store(true);
            events_.clear();
            pressed_.clear();
        }
        available_.notify_all();
    }

    void join_workers() {
        if (listening_.joinable()) listening_.join();
        if (processing_.joinable()) processing_.join();
    }

    std::vector<std::shared_ptr<Handler>> matching_handlers(int last_id, bool suppress) {
        std::vector<std::shared_ptr<Handler>> result;
        for (const auto& entry : handlers_) {
            if (entry.first > last_id)
                break;
            if (entry.second->suppress == suppress)
                result.push_back(entry.second);
        }
        return result;
    }

    bool capture_event(const KeyboardEvent& event, const std::atomic<bool>& stop) {
        Dispatch input{event, {}, std::chrono::steady_clock::now(), 0, 0};
        std::vector<std::shared_ptr<Handler>> blocking;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop.load())
                return true;
            if (event.type == EventType::Down)
                pressed_[event.scan_code].insert(event.device);
            for (const auto& entry : pressed_)
                input.pressed.push_back(entry.first);
            if (event.type == EventType::Up) {
                const auto it = pressed_.find(event.scan_code);
                if (it != pressed_.end()) {
                    it->second.erase(event.device);
                    if (it->second.empty())
                        pressed_.erase(it);
                }
            }
            input.last_handler = handler_id_;
            input.generation = generation_;
            if (suppression_)
                blocking = matching_handlers(input.last_handler, true);
        }
        bool forward = true;
        for (const auto& handler : blocking) {
            if (stop.load())
                break;
            if (handler->enabled.load() && !invoke(*handler, input, true))
                forward = false;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!stop.load() && !handlers_.empty())
                events_.push_back(std::move(input));
        }
        available_.notify_one();
        return forward;
    }

    void process_events(const std::atomic<bool>& stop) {
        Worker worker(this);
        std::unique_lock<std::mutex> lock(mutex_);
        while (!stop.load()) {
            available_.wait(lock, [&] { return stop.load() || !events_.empty(); });
            if (stop.load())
                return;
            auto input = std::move(events_.front());
            events_.pop_front();
            auto handlers = matching_handlers(input.last_handler, false);
            lock.unlock();
            for (const auto& handler : handlers) {
                if (stop.load())
                    break;
                if (handler->enabled.load() && invoke(*handler, input, false))
                    break;
            }
            handlers.clear();
            lock.lock();
        }
    }

    bool invoke(const Handler& handler, const Dispatch& event, bool fallback) {
        try {
            return handler.fn(event);
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!callback_error_)
                callback_error_ = std::current_exception();
            return fallback;
        }
    }

    void require_external_thread() const {
        if (active_worker_ == this)
            throw std::logic_error("Cannot wait for keyboard events inside a keyboard callback");
    }

    template<typename Result>
    Result wait_for_result(int id, std::future<Result>& future) {
        try {
            while (future.wait_for(std::chrono::milliseconds(50)) != std::future_status::ready) {
                bool waiting;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    waiting = stop_ && !stop_->load() && handlers_.count(id) != 0;
                }
                if (!waiting &&
                    future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
                    rethrow_error();
                    throw std::runtime_error("Keyboard wait ended before receiving input");
                }
            }
        } catch (...) {
            remove_handler(id);
            throw;
        }
        remove_handler(id);
        return future.get();
    }
};

class KeyboardListener final : public Listener {
public:
    using Listener::Listener;
};

} // namespace Keyboard
