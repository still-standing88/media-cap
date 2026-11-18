#include "../cli.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <chrono>
#include <thread>

namespace mccli {
namespace {
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
std::wstring wide(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (!size) throw UsageError("Invalid UTF-8 in source filter.");
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}
std::wstring argument(const std::wstring& value) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        result += c; slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}
void pipe(Handle& read, Handle& write, SECURITY_ATTRIBUTES& security) {
    if (!CreatePipe(&read.value, &write.value, &security, 0) ||
        !SetHandleInformation(read.value, HANDLE_FLAG_INHERIT, 0))
        throw std::runtime_error("Cannot open camera query output.");
}
void drain(HANDLE pipe, std::string& destination) {
    char buffer[4096]; DWORD available = 0, read = 0;
    while (PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) && available) {
        if (!ReadFile(pipe, buffer, (std::min)(available, static_cast<DWORD>(sizeof(buffer))), &read, nullptr) || !read) break;
        if (destination.size() + read > 8 * 1024 * 1024) throw std::runtime_error("Camera query output is too large.");
        destination.append(buffer, read);
    }
}
}
DiscoveryQuery query_camera_details(const ListOptions& options, bool json, bool verbose) {
    // Driver activation can hang inside COM. A short-lived child lets the CLI
    // retain basic camera identities and continue listing other categories.
    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size()) throw std::runtime_error("Cannot locate camera query executable.");
    executable.resize(length);
    std::wstring command = argument(executable) + L" --_camera-worker list cameras --details";
    if (json) command += L" --json";
    if (verbose) command += L" --verbose";
    if (!options.name.empty()) command += L" --name " + argument(wide(options.name));
    if (!options.id.empty()) command += L" --_exact-id " + argument(wide(options.id));
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Handle result_read, result_write, diagnostic_read, diagnostic_write, input;
    pipe(result_read, result_write, security); pipe(diagnostic_read, diagnostic_write, security);
    input.value = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
    if (input.value == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open camera query input.");
    HANDLE inherited[]{input.value, result_write.value, diagnostic_write.value};
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<unsigned char> attributes(bytes);
    auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &bytes)) throw std::runtime_error("Cannot prepare camera query.");
    struct Attributes { LPPROC_THREAD_ATTRIBUTE_LIST value; ~Attributes() { DeleteProcThreadAttributeList(value); } } cleanup{list};
    if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr))
        throw std::runtime_error("Cannot prepare camera query handles.");
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.value;
    startup.StartupInfo.hStdOutput = result_write.value;
    startup.StartupInfo.hStdError = diagnostic_write.value;
    startup.lpAttributeList = list;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &process))
        throw std::runtime_error("Cannot start camera query.");
    Handle child{process.hProcess}, thread{process.hThread};
    CloseHandle(result_write.value); result_write.value = nullptr;
    CloseHandle(diagnostic_write.value); diagnostic_write.value = nullptr;
    DiscoveryQuery result;
    try {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (WaitForSingleObject(child.value, 0) == WAIT_TIMEOUT) {
            drain(result_read.value, result.result); drain(diagnostic_read.value, result.diagnostic);
            if (std::chrono::steady_clock::now() >= deadline) {
                TerminateProcess(child.value, 1); WaitForSingleObject(child.value, 2000);
                result.timed_out = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        drain(result_read.value, result.result); drain(diagnostic_read.value, result.diagnostic);
        DWORD status = 1; GetExitCodeProcess(child.value, &status);
        result.status = static_cast<int>(status);
    } catch (...) {
        TerminateProcess(child.value, 1); WaitForSingleObject(child.value, 2000);
        throw;
    }
    while (!result.result.empty() && (result.result.back() == '\n' || result.result.back() == '\r')) result.result.pop_back();
    return result;
}
}
#endif
