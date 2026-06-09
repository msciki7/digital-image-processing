cmake_minimum_required(VERSION 4.1)
project(DIP_Exercise_A C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_executable(ex1_zoom_out    ex1_zoom_out.c)
add_executable(ex2_zoom_in     ex2_zoom_in.c)
add_executable(ex3_avg_filter  ex3_avg_filter.c)
add_executable(ex4_sobel       ex4_sobel.c)
add_executable(ex5_translation ex5_translation.c)
add_executable(ex6_rotation    ex6_rotation.c)
add_executable(ex7_histogram   ex7_histogram.c)

target_link_libraries(ex6_rotation m)
