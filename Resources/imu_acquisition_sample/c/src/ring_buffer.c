#define _GNU_SOURCE
#include "ring_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>

long get_ring_buffer_mmap_size(const char *device_name) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/bmi_spsc/%s/ring_buffer_size", device_name);
    
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    unsigned long val = 0;
    if (fscanf(f, "%lu", &val) != 1) val = -1;
    fclose(f);
    return (long)val;
}

int get_target_cpu(const char *device_name) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/bmi_spsc/%s/target_cpu", device_name);
    
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    int cpu = -1;
    if (fscanf(f, "%d", &cpu) != 1) cpu = -1;
    fclose(f);
    return cpu;
}

int set_cpu_affinity(int target_cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(target_cpu, &cpuset);
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0) {
        perror("sched_setaffinity");
        return -1;
    }
    return 0;
}

void sync_ring_buffer_consumer(struct spsc_ring *ring) {
    // Current approach: Sync tail to head to discard stale data
    asm volatile("" : : : "memory");
    ring->tail = ring->head;
    asm volatile("" : : : "memory");
}
