cmake_minimum_required(VERSION 3.10)
project(bmi_c_consumer C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -pthread")

include_directories(include)

file(GLOB SOURCES "src/*.c")

add_executable(bmi_consumer_c ${SOURCES})
target_link_libraries(bmi_consumer_c pthread)
