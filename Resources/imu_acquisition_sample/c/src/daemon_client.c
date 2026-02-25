#include "daemon_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_PATH "/tmp/imu_daemon.sock"

/*
 * send_daemon_command -- send a simple "set <type> <dev> <value>" to the
 * imu daemon socket.
 *
 * NOTE: the SDK intentionally exposes only range/scale controls for the
 * accelerometer/gyroscope (via CLI or programmatic calls). Sampling
 * frequency (ODR) and timer configuration are driver/daemon-controlled and
 * therefore are NOT exposed as user-settable options here to avoid
 * misconfiguration on kernels/drivers that don't support runtime ODR changes.
 */
int send_daemon_command(int device_id, const char *command_type, int value) {
    int sock_fd;
    struct sockaddr_un addr;
    char command[256];

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    /* CRITICAL FIX #4: strcpy → strncpy (security fix) */
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';  /* Ensure null termination */

    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect to daemon");
        close(sock_fd);
        return -1;
    }

    snprintf(command, sizeof(command), "set %s %d %d\n", command_type, device_id, value);
    if (write(sock_fd, command, strlen(command)) < 0) {
        perror("write to daemon");
        close(sock_fd);
        return -1;
    }

    usleep(10000); // 10ms wait
    close(sock_fd);
    return 0;
}

int initialize_device(int device_id, int accel_range_idx, int gyro_range_idx) {
    /* 1) startup */
    if (send_daemon_command(device_id, "startup", 1) < 0) return -1;

    /* 2) apply ranges (if requested) */
    if (accel_range_idx >= 0) {
        if (send_daemon_command(device_id, "accel_range", accel_range_idx) < 0) return -1;
    }
    if (gyro_range_idx >= 0) {
        if (send_daemon_command(device_id, "gyro_range", gyro_range_idx) < 0) return -1;
    }

    /* 3) settle then enable control */
    usleep(40000); /* 40 ms */
    if (send_daemon_command(device_id, "control", 1) < 0) return -1;
    return 0;
}
