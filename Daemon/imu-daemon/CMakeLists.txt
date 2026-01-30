cmake_minimum_required(VERSION 3.10)
project(IMU_Daemon)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add executable
set(SOURCES 
    IMU_Daemon.cpp
    IMU_Daemon.h
)

# Create the executable
add_executable(IMU_Daemon ${SOURCES})

# Link against pthread (required for std::thread)
find_package(Threads REQUIRED)
target_link_libraries(IMU_Daemon Threads::Threads)

# Install the executable with proper permissions
install(TARGETS IMU_Daemon 
        DESTINATION /usr/sbin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                  GROUP_READ GROUP_EXECUTE
                  WORLD_READ WORLD_EXECUTE)
# Install service file
install(FILES IMU_Daemon.service DESTINATION /etc/systemd/system)
