cmake_minimum_required(VERSION 3.20)
project(rak_minimal_c_client C)

add_executable(rak_minimal_c_client main.c)
target_compile_features(rak_minimal_c_client PRIVATE c_std_11)

