add_library(mediacap_test_main STATIC support/main.cpp)
target_include_directories(mediacap_test_main PUBLIC third_party/doctest)
target_compile_features(mediacap_test_main PUBLIC cxx_std_17)

function(mediacap_internal_test target)
    target_compile_definitions(${target} PRIVATE MEDIACAP_STATIC NOMINMAX)
    target_include_directories(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include" ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE mediacap_test_main ${FFMPEG_LIBS} Threads::Threads)
    if(UNIX AND NOT APPLE)
        target_link_libraries(${target} PRIVATE atomic)
    endif()
endfunction()

add_subdirectory(unit)
add_subdirectory(integration)
