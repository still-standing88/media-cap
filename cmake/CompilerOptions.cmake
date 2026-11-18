target_compile_definitions(MediaCap PRIVATE NOMINMAX)

if(MSVC)
    target_compile_options(MediaCap PRIVATE
        /W4 /MP /Zc:__cplusplus /Zc:preprocessor
        /permissive-      # standards-conforming parsing (matches gcc/clang default behavior)
        /utf-8            # source *and* execution charset - vendored ccap files contain UTF-8 comments
        /EHsc
        /wd4100           # unreferenced formal parameter - very noisy across the vendored capture backends
        /wd4127           # conditional expression is constant
    )
    target_compile_definitions(MediaCap PRIVATE _CRT_SECURE_NO_WARNINGS)
    # CRT selection uses CMP0091 and CMAKE_MSVC_RUNTIME_LIBRARY.
    target_compile_options(MediaCap PRIVATE $<$<CONFIG:Release>:/Oi> $<$<CONFIG:Release>:/Gy>)
    target_link_options(MediaCap PRIVATE $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
else()
    target_compile_options(MediaCap PRIVATE -Wall -Wextra -pedantic -Wshadow)
    target_compile_options(MediaCap PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>)
    target_compile_options(MediaCap PRIVATE -fstack-protector-strong)
    target_compile_options(MediaCap PRIVATE $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>)
    if(UNIX AND NOT APPLE)
        target_compile_definitions(MediaCap PRIVATE _FILE_OFFSET_BITS=64)
    endif()
endif()
