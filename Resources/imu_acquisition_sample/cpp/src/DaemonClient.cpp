#include "DaemonClient.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

#define SOCK_PATH "/tmp/imu_daemon.sock"

/*
 * sendCommand -- send "set <type> <dev> <value>" to imu daemon.
 *
 * Policy: SDK exposes range/scale selection for accel/gyro but intentionally
 * does NOT expose sampling-frequency (ODR) configuration here. Frequency
 * changes are handled by the daemon/driver and may not be supported at
 * runtime on all kernels.
 */
bool DaemonClient::sendCommand(int deviceId, const std::string& type, int value) {
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path)-1);

    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock_fd);
        return false;
    }

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "set %s %d %d\n", type.c_str(), deviceId, value);
    if (write(sock_fd, cmd, strlen(cmd)) < 0) {
        close(sock_fd);
        return false;
    }

    usleep(10000);
    close(sock_fd);
    return true;

}

bool DaemonClient::initializeDevice(int deviceId, int accelRangeIdx, int gyroRangeIdx) {
    /* 1) startup */
    if (!sendCommand(deviceId, "startup", 1)) return false;

    /* 2) apply ranges (if requested) */
    if (accelRangeIdx >= 0) {
        if (!sendCommand(deviceId, "accel_range", accelRangeIdx)) return false;
    }
    if (gyroRangeIdx >= 0) {
        if (!sendCommand(deviceId, "gyro_range", gyroRangeIdx)) return false;
    }

    /* 3) settle then enable control */
    usleep(40000); /* 40 ms */
    if (!sendCommand(deviceId, "control", 1)) return false;
    return true;
}
