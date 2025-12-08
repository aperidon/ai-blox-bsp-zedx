CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
project(ZEDX_Daemon)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)

# Mandatory
find_package(Qt5 5 REQUIRED COMPONENTS Core)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/zmq)
include_directories(${Qt5Core_INCLUDE_DIRS})
FILE(GLOB_RECURSE SRC_FILES src/*.c*)
FILE(GLOB_RECURSE HDR_FILES include/*.h*)


add_definitions(-fpermissive -O3 -Wendif-labels)
add_definitions(-DUNICODE -DQT_NO_DEBUG -DQT_CORE_LIB)

if(UNIX)
    message("** L4T Version : ${RT_L4T_VERSION}")
    add_definitions(-DL4T_VERSION=${RT_L4T_VERSION})
    add_definitions(-Werror=return-type -Werror=uninitialized
        -Werror=switch
        -Werror=strict-overflow=1)

    IF (BUILD_WITH_SANITIZER)
        message("!! Building with address sanitizer and -g !!")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address -Wall -Wextra -g")
        set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address -Wall -Wextra -g")
        add_definitions(-g -fsanitize=address -fno-omit-frame-pointer -static-libasan -Wall -Wextra)
    ENDIF()

    IF (BUILD_WITH_DEBUGINFOS)
        message("!! Building with -g !!")
        add_definitions(-g)
    ENDIF()
ENDIF()


ADD_EXECUTABLE(${PROJECT_NAME} ${SRC_FILES} ${HDR_FILES})

# Add the required libraries for linking:
if(WIN32)
    message("platform not supported")
else()
    SET(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} "-no-pie")
    TARGET_LINK_LIBRARIES(${PROJECT_NAME}
        Qt5::Core
        pthread
        )
endif()


