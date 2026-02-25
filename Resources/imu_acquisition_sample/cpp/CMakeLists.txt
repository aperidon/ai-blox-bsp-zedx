cmake_minimum_required(VERSION 3.10)
project(bmi_cpp_consumer CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -pthread")

include_directories(include)

file(GLOB SOURCES "src/*.cpp")

add_executable(bmi_consumer_cpp ${SOURCES})
target_link_libraries(bmi_consumer_cpp pthread)
