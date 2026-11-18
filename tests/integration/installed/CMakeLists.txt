cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(MediaCapInstalledConsumer LANGUAGES C CXX)

find_package(MediaCap CONFIG REQUIRED PATHS "${MEDIACAP_PACKAGE}" NO_DEFAULT_PATH)
add_executable(installed_consumer smoke.c)
set_target_properties(installed_consumer PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(installed_consumer PRIVATE MediaCap::MediaCap)

get_target_property(package_type MediaCap::MediaCap TYPE)
if(WIN32 AND package_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET installed_consumer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:MediaCap::MediaCap> $<TARGET_FILE_DIR:installed_consumer>)
endif()
enable_testing()
add_test(NAME installed-consumer COMMAND installed_consumer)
