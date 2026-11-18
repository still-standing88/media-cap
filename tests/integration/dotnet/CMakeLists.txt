cmake_minimum_required(VERSION 3.20)
project(MediaCapAbiProbe LANGUAGES C)
add_executable(abi_probe abi_probe.c)
target_include_directories(abi_probe PRIVATE ../../../include)
