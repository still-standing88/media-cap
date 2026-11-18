cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(MediaCapCLI VERSION 1.0.0 LANGUAGES CXX)
    find_package(MediaCap CONFIG REQUIRED)
    include(CTest)
endif()

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS sources.txt)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/sources.txt" cli_sources)
add_library(mediacap_cli11 INTERFACE)
target_include_directories(mediacap_cli11 SYSTEM INTERFACE third_party/CLI11 third_party/nlohmann)

function(mediacap_configure_cli target)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_link_libraries(${target} PRIVATE mediacap_cli11)
    if(WIN32)
        target_link_libraries(${target} PRIVATE ole32)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_executable(MediaCap_cli ${cli_sources})
mediacap_configure_cli(MediaCap_cli)
set_target_properties(MediaCap_cli PROPERTIES OUTPUT_NAME MediaCap PDB_NAME MediaCap_cli)
target_link_libraries(MediaCap_cli PRIVATE MediaCap::MediaCap)

get_target_property(package_type MediaCap::MediaCap TYPE)
if(WIN32 AND package_type STREQUAL "SHARED_LIBRARY" AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_custom_command(TARGET MediaCap_cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:MediaCap::MediaCap> $<TARGET_FILE_DIR:MediaCap_cli>)
endif()

if(MEDIACAP_BUILD_TESTS OR (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND BUILD_TESTING))
    include(../tests/integration/cli/CLITests.cmake)
endif()

include(GNUInstallDirs)
if(UNIX AND package_type STREQUAL "SHARED_LIBRARY")
    file(RELATIVE_PATH cli_library_dir "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}")
    if(APPLE)
        set(cli_loader_dir "@loader_path")
    else()
        set(cli_loader_dir "$ORIGIN")
    endif()
    set_target_properties(MediaCap_cli PROPERTIES INSTALL_RPATH "${cli_loader_dir}/${cli_library_dir}")
endif()
install(TARGETS MediaCap_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT cli)
install(FILES third_party/CLI11/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/MediaCapCLI RENAME CLI11-LICENSE.txt COMPONENT cli)
install(FILES third_party/nlohmann/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/MediaCapCLI RENAME nlohmann-json-LICENSE.txt COMPONENT cli)
install(FILES ../LICENSE ../COPYING
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/MediaCapCLI COMPONENT cli)
install(FILES ../THIRD_PARTY_NOTICES.md OPTIONAL
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/MediaCapCLI COMPONENT cli)
install(DIRECTORY ../licenses/ OPTIONAL
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/MediaCapCLI/dependencies COMPONENT cli)
