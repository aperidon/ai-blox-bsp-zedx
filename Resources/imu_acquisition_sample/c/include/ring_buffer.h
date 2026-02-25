#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "bmi_protocol.h"
#include <stddef.h>

long get_ring_buffer_mmap_size(const char *device_name);
int get_target_cpu(const char *device_name);
int set_cpu_affinity(int target_cpu);

/**
 * @brief Wait for producer to write initial samples and sync tail
 */
void sync_ring_buffer_consumer(struct spsc_ring *ring);

#endif
