# SPSC IMU Daemon Installation Guide

This section contains commands for compiling and installing the SPSC IMU Daemon (`IMU_Daemon`), 
a privileged service that safely manages SPSC (Single Producer Single Consumer) BMI088 IMU device operations through a Unix domain socket.

## C++ Version with SPSC Framework

The daemon has been migrated from C to C++ using Qt5 and converted to work with the SPSC framework, providing more robust handling of sysfs operations. The C++ SPSC version:

1. Uses Qt's event loop for better concurrency management
2. Implements SPSC-specific command interface for BMI088 IMU devices
3. Uses blocking processes for sysfs operations to ensure reliable operation
4. Supports SPSC timer control instead of IIO buffers
5. Manages SPSC ring buffer operations for high-performance IMU data streaming

## Installation Steps

1. **Compile** the helper daemon from source using CMake
2. **Install** the binary to system location with proper ownership
3. **Configure permissions** to ensure only root can execute the daemon
4. **Enable and start** the systemd service

### Dependencies 
```
sudo apt install qtbase5-dev cmake
```
The daemon provides a secure interface for user-mode applications to perform privileged 
sysfs operations on SPSC BMI088 devices without requiring direct root access.

## Security Notes
- Binary runs with root privileges (711 permissions)
- Only root can read/write the binary, others can only execute
- Uses systemd for service management
- Follows privilege separation best practices

## Command Format

Commands are sent to the Unix socket in the format:

```
set <command_type> <device_number> <value>
```

Where:
- `command_type` can be one of:
  - `startup`: Initialize SPSC device and set default configuration
  - `bmifreq`: Set sampling frequency for BMI088 sensors (e.g., 100, 200, 400 Hz)
  - `control`: Enable or disable SPSC device control
  - `timer`: Set timer interval for data sampling (in milliseconds)

- `device_number`: The SPSC device number (e.g., for spsc_bmi0, use 0)
- `value`: The value to set (frequency in Hz, timer interval in ms, enable/disable flag)

Example:
```bash
# Using nc (ncat) (waits for OK/ERR response)
(echo -n "set startup 0 1" && echo) | nc -U -q 0 /tmp/imu_daemon.sock
(echo -n "set bmifreq 0 200" && echo) | nc -U -q 0 /tmp/imu_daemon.sock
(echo -n "set control 0 1" && echo) | nc -U -q 0 /tmp/imu_daemon.sock
(echo -n "set timer 0 10" && echo) | nc -U -q 0 /tmp/imu_daemon.sock
```

Note: Commands will return "OK" or "ERR" to indicate success or failure. Make sure to include a newline (\n) at the end of the command.

## Building & Installing (C++ SPSC Version)

```bash
# Build with CMake
mkdir -p build
cd build
cmake ..
make
cd ..
```

## Installation

```bash
# Install the binary
sudo cp build/IMU_Daemon /usr/sbin/IMU_Daemon

# Install the systemd service
sudo cp IMU_Daemon.service /etc/systemd/system/

# Reload systemd and enable the service
sudo systemctl daemon-reload
sudo systemctl enable IMU_Daemon.service
sudo systemctl start IMU_Daemon.service
sudo systemctl status IMU_Daemon.service
```

## Legacy C Version

For the legacy C version, use the following build command instead:

```bash
gcc -o IMU_Daemon IMU_Daemon.c
```

If you need to go back to the C version, use the commands in the installation section after building with gcc.