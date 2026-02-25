#ifndef BMI_PROTOCOL_H
#define BMI_PROTOCOL_H

#include <stdint.h>

#define BMI_SPSC_BUFFER_SIZE 1024

struct bmi_sensor_data {
    uint64_t timestamp;      /* 8 bytes - nanosecond timestamp */
    int16_t gx, gy, gz;     /* 6 bytes - gyroscope data */
    int16_t ax, ay, az;     /* 6 bytes - accelerometer data */
} __attribute__((packed));

struct spsc_ring {
    volatile uint32_t head;  /* Producer index */
    volatile uint32_t tail;  /* Consumer index */
    struct bmi_sensor_data data[BMI_SPSC_BUFFER_SIZE];
};

#endif
