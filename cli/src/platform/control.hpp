#pragma once
#include "../cli.hpp"
#include <csignal>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <conio.h>
#include <io.h>
#else
#include <cerrno>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif
namespace mccli {
inline volatile std::sig_atomic_t stop_requested = 0;
inline void on_signal(int) { stop_requested = 1; }
#ifdef _WIN32
inline volatile LONG console_stop = 0;
inline BOOL WINAPI on_console(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) { InterlockedExchange(&console_stop, 1); return TRUE; }
    return FALSE;
}
#endif
inline bool interrupted() {
#ifdef _WIN32
    return stop_requested || InterlockedCompareExchange(&console_stop, 0, 0);
#else
    return stop_requested != 0;
#endif
}
class Signals {
    using Handler = void (*)(int);
    Handler old_int_, old_term_;
public:
    Signals() {
        stop_requested = 0;
        old_int_ = std::signal(SIGINT, on_signal); old_term_ = std::signal(SIGTERM, on_signal);
#ifdef _WIN32
        InterlockedExchange(&console_stop, 0);
        SetConsoleCtrlHandler(on_console, TRUE);
#endif
    }
    ~Signals() {
#ifdef _WIN32
        SetConsoleCtrlHandler(on_console, FALSE);
#endif
        std::signal(SIGINT, old_int_); std::signal(SIGTERM, old_term_);
    }
};
class Input {
    bool terminal_, lines_, enabled_;
    std::string pending_;
#ifndef _WIN32
    termios saved_{};
    bool changed_ = false;
#endif
public:
    Input(bool lines, bool disabled) : lines_(lines) {
#ifdef _WIN32
        terminal_ = _isatty(_fileno(stdin)) != 0;
#else
        terminal_ = isatty(STDIN_FILENO) != 0;
#endif
        enabled_ = lines_ || (terminal_ && !disabled);
#ifndef _WIN32
        if (enabled_ && terminal_ && tcgetattr(STDIN_FILENO, &saved_) == 0) {
            auto raw = saved_; raw.c_lflag &= ~(ICANON | ECHO); raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 0;
            changed_ = tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
        }
#endif
    }
    ~Input() {
#ifndef _WIN32
        if (changed_) tcsetattr(STDIN_FILENO, TCSANOW, &saved_);
#endif
    }
    std::vector<std::string> poll() {
        std::vector<std::string> commands;
        if (!enabled_) return commands;
        std::string bytes;
        bool eof = false;
#ifdef _WIN32
        if (terminal_) {
            while (_kbhit() && bytes.size() < 4096) {
                const int c = _getch();
                if (c == 0 || c == 224) { (void)_getch(); continue; }
                bytes += static_cast<char>(c == '\r' ? '\n' : c);
            }
        } else {
            HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
            DWORD available = 0, count = 0;
            char buffer[4096];
            const auto type = GetFileType(h);
            if (type == FILE_TYPE_PIPE) {
                if (!PeekNamedPipe(h, nullptr, 0, nullptr, &available, nullptr)) {
                    if (GetLastError() == ERROR_BROKEN_PIPE) eof = true;
                    else throw std::runtime_error("Cannot read stdin control pipe");
                }
            } else if (type == FILE_TYPE_DISK) available = sizeof(buffer);
            else eof = true;
            if (available) {
                if (!ReadFile(h, buffer, (std::min)(available, static_cast<DWORD>(sizeof(buffer))), &count, nullptr)) {
                    if (GetLastError() == ERROR_BROKEN_PIPE) eof = true;
                    else throw std::runtime_error("Cannot read stdin controls");
                } else { bytes.assign(buffer, count); eof = count == 0; }
            }
        }
#else
        pollfd descriptor{STDIN_FILENO, POLLIN, 0};
        if (::poll(&descriptor, 1, 0) > 0 && (descriptor.revents & (POLLIN | POLLHUP | POLLERR))) {
            char buffer[4096];
            const auto count = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (count > 0) bytes.assign(buffer, static_cast<size_t>(count));
            else if (count == 0) eof = true;
            else if (errno != EINTR && errno != EAGAIN) throw std::runtime_error("Cannot read stdin controls");
        }
#endif
        if (lines_) {
            pending_ += bytes;
            size_t end;
            while ((end = pending_.find('\n')) != std::string::npos) {
                auto line = pending_.substr(0, end); pending_.erase(0, end + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) commands.push_back(line);
            }
            if (pending_.size() > 4096) throw UsageError("stdin control line exceeds 4096 bytes");
            if (eof && !pending_.empty()) { commands.push_back(pending_); pending_.clear(); }
        } else for (char c : bytes) {
            if (c == 'p' || c == ' ') commands.push_back("toggle");
            else if (c == 'r') commands.push_back("resume");
            else if (c == 's') commands.push_back("status");
            else if (c == 'q') commands.push_back("stop");
        }
        if (eof) { commands.push_back("stop"); enabled_ = false; }
        return commands;
    }
};
}
