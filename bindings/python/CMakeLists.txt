find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
execute_process(
    COMMAND "${Python_EXECUTABLE}" -m pybind11 --cmakedir
    OUTPUT_VARIABLE pybind11_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
find_package(pybind11 3 CONFIG REQUIRED HINTS "${pybind11_dir}")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS sources.txt package_files.txt)
file(STRINGS sources.txt binding_sources)
file(STRINGS package_files.txt binding_package_files)
pybind11_add_module(_native ${binding_sources})
target_link_libraries(_native PRIVATE MediaCap::MediaCap)
target_compile_features(_native PRIVATE cxx_std_17)
target_compile_definitions(_native PRIVATE NOMINMAX)
if(MSVC)
    target_compile_options(_native PRIVATE /bigobj)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT BUILD_SHARED_LIBS)
    target_link_options(_native PRIVATE "LINKER:--exclude-libs,ALL")
endif()

set(binding_output "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/mediacap")
set_target_properties(_native PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${binding_output}"
    RUNTIME_OUTPUT_DIRECTORY "${binding_output}"
)
foreach(package_file IN LISTS binding_package_files)
    file(GENERATE OUTPUT "${binding_output}/${package_file}"
        INPUT "${CMAKE_CURRENT_SOURCE_DIR}/mediacap/${package_file}")
    list(APPEND binding_package_sources "mediacap/${package_file}")
endforeach()
if(BUILD_SHARED_LIBS)
    if(WIN32)
        add_custom_command(OUTPUT "${binding_output}/MediaCap.dll"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${binding_output}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:MediaCap>" "${binding_output}/MediaCap.dll"
            DEPENDS MediaCap
            VERBATIM)
        add_custom_target(mediacap_python_runtime DEPENDS "${binding_output}/MediaCap.dll")
        add_dependencies(_native mediacap_python_runtime)
    elseif(APPLE)
        set_target_properties(_native PROPERTIES INSTALL_RPATH "@loader_path")
    else()
        set_target_properties(_native PROPERTIES INSTALL_RPATH "$ORIGIN")
    endif()
endif()

execute_process(
    COMMAND "${Python_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/python_config.py" platlib
    OUTPUT_VARIABLE binding_platlib
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
set(MEDIACAP_PYTHON_INSTALL_DIR "${binding_platlib}" CACHE STRING "Python installation path relative to prefix")
set(binding_install "${MEDIACAP_PYTHON_INSTALL_DIR}/mediacap")
install(TARGETS _native
    LIBRARY DESTINATION "${binding_install}" COMPONENT python
    RUNTIME DESTINATION "${binding_install}" COMPONENT python)
if(BUILD_SHARED_LIBS)
    install(TARGETS MediaCap
        LIBRARY DESTINATION "${binding_install}" COMPONENT python
        RUNTIME DESTINATION "${binding_install}" COMPONENT python)
endif()
install(FILES ${binding_package_sources} DESTINATION "${binding_install}" COMPONENT python)
if(MEDIACAP_BUILD_TESTS)
    add_test(NAME python-bindings COMMAND "${CMAKE_COMMAND}" -E env
        "PYTHONPATH=$<TARGET_FILE_DIR:_native>/.." "${Python_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/integration/python_bindings.py")
endif()
