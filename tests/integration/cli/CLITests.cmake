find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(cli_test_dir "${CMAKE_CURRENT_LIST_DIR}")
add_test(NAME integration-cli-contract COMMAND "${Python3_EXECUTABLE}"
    "${cli_test_dir}/test_cli.py" $<TARGET_FILE:MediaCap_cli>)
add_executable(MediaCap_cli_fixture ${cli_sources} "${cli_test_dir}/fake_api.cpp")
mediacap_configure_cli(MediaCap_cli_fixture)
target_compile_definitions(MediaCap_cli_fixture PRIVATE MEDIACAP_STATIC)
target_include_directories(MediaCap_cli_fixture PRIVATE ../include)
add_test(NAME integration-cli-discovery COMMAND "${Python3_EXECUTABLE}"
    "${cli_test_dir}/test_discovery.py" $<TARGET_FILE:MediaCap_cli_fixture>)
add_test(NAME integration-cli-capture COMMAND "${Python3_EXECUTABLE}"
    "${cli_test_dir}/test_capture.py" $<TARGET_FILE:MediaCap_cli_fixture>)
set_tests_properties(integration-cli-contract integration-cli-discovery integration-cli-capture PROPERTIES LABELS integration TIMEOUT 120)
