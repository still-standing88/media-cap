/** @file MediaCap_export.h
 * @brief Public C symbol visibility for shared and static builds.
 */
#ifndef MEDIACAP_EXPORT_H
#define MEDIACAP_EXPORT_H

/**
 * @brief Cross-platform symbol visibility macro for MediaCap's public API.
 *
 * Expands to the correct dllexport/dllimport attribute on Windows (or nothing when building/
 * consuming the static library), and to a default-visibility attribute on non-Windows shared
 * builds. Exported C functions use this macro. The C++ Session wrapper is inline and
 * invokes those functions; STL ownership remains in the application. Static consumers
 * define MEDIACAP_STATIC (provided by the installed build-system target).
 */
#if defined(_WIN32)
    #if defined(MEDIACAP_EXPORT)      // Defined when building the shared library (DLL)
        #define MEDIACAP_API __declspec(dllexport)
    #elif defined(MEDIACAP_STATIC)    // Defined when building or using the static library
        #define MEDIACAP_API
    #else                             // Default: defined when using the shared library (DLL)
        #define MEDIACAP_API __declspec(dllimport)
    #endif
#else // Non-Windows (GCC, Clang, etc.)
    #if defined(MEDIACAP_EXPORT)      // Defined when building a shared library (.so, .dylib)
        #define MEDIACAP_API __attribute__((visibility("default")))
    #else                             // Default: static library or executable using the library
        #define MEDIACAP_API
    #endif
#endif

#endif // MEDIACAP_EXPORT_H
