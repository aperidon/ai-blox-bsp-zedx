#ifndef DAEMON_CLIENT_H
#define DAEMON_CLIENT_H

/**
 * @brief Initialize the IMU daemon for the device
 */
int initialize_device(int device_id, int accel_range_idx, int gyro_range_idx);

/**
 * @brief Send a raw command to the daemon
 */
int send_daemon_command(int device_id, const char *command_type, int value);

#endif
