#define _GNU_SOURCE
#include "bmi_protocol.h"
#include "daemon_client.h"
#include "ring_buffer.h"
#include "csv_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>
#include <signal.h>
#include <errno.h>

volatile int keep_running = 1;
static unsigned long g_overflow_count = 0;

void sig_handler(int sig) {
    /* CRITICAL FIX #1: Flush CSV on exit to prevent lost samples */
    printf("\nSignal received, flushing remaining samples...\n");
    write_buffered_samples_to_csv();
    if (g_overflow_count > 0) {
        fprintf(stderr, "WARNING: Buffer overflowed %lu times!\n", g_overflow_count);
    }
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    const char *device_name = "spsc_bmi0";
    const char *device_path = "/dev/spsc_bmi0";
    const char *csv_filename = "bmi_data_c.csv";
    int device_id = 0;

    /* Parse arguments: first non-option token is device name (e.g. spsc_bmi0).
     * Supported options:
     *   --accel-range <idx>    set accelerometer range index (writes via daemon)
     *   --gyro-range  <idx>    set gyroscope range index (writes via daemon)
     *   --csv <filename>       write samples to given CSV filename
     *
     * Note: Frequency/ODR options are intentionally NOT exposed here; the
     * driver/daemon controls sampling frequency and may not support runtime
     * changes on all platforms.
     */
    int accel_range_idx = -1;
    int gyro_range_idx = -1;

    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--accel-range", 13) == 0) {
            if (argv[i][13] == '=') {
                accel_range_idx = atoi(argv[i] + 14);
            } else if (i + 1 < argc) {
                accel_range_idx = atoi(argv[++i]);
            }
            continue;
        }
        if (strncmp(argv[i], "--gyro-range", 12) == 0) {
            if (argv[i][12] == '=') {
                gyro_range_idx = atoi(argv[i] + 13);
            } else if (i + 1 < argc) {
                gyro_range_idx = atoi(argv[++i]);
            }
            continue;
        }
        if (strncmp(argv[i], "--csv", 5) == 0) {
            if (argv[i][5] == '=') csv_filename = argv[i] + 6;
            else if (i + 1 < argc) csv_filename = argv[++i];
            continue;
        }

        /* treat as device name */
        device_name = argv[i];
        if (strncmp(device_name, "spsc_bmi", 8) == 0) {
            sscanf(device_name, "spsc_bmi%d", &device_id);
        }
    }
    
    signal(SIGINT, sig_handler);

    init_csv_writer(csv_filename);
    
    int cpu = get_target_cpu(device_name);
    if (cpu >= 0) {
        printf("Pinning to CPU %d\n", cpu);
        set_cpu_affinity(cpu);
    }

    int fd = open(device_path, O_RDWR);
    if (fd < 0) {
        perror("open device");
        return 1;
    }

    long mmap_size = get_ring_buffer_mmap_size(device_name);
    if (mmap_size <= 0) mmap_size = 32768;

    struct spsc_ring *ring = mmap(NULL, mmap_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (ring == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    // 1. Sync
    sync_ring_buffer_consumer(ring);

    /* Initialize device (startup → optional ranges → 40ms → control 1) */
    if (initialize_device(device_id, accel_range_idx, gyro_range_idx) < 0) {
        fprintf(stderr, "Daemon init failed\n");
    }

    /* CRITICAL FIX #2: Wait for stale data to flush (original consumer.c pattern) */
    printf("Waiting for fresh samples...\n");
    sleep(1);
    sync_ring_buffer_consumer(ring);  /* Re-sync after stale flush */

    // 3. Loop
    struct pollfd pfd = {fd, POLLIN, 0};
    const uint32_t MASK = BMI_SPSC_BUFFER_SIZE - 1;
    unsigned long samples_count = 0;

    printf("Starting loop...\n");
    while (keep_running) {
        uint32_t tail = __atomic_load_n(&ring->tail, __ATOMIC_RELAXED);
        uint32_t head = __atomic_load_n(&ring->head, __ATOMIC_ACQUIRE);
        uint32_t count = (head - tail) & MASK;

        if (count > BMI_SPSC_BUFFER_SIZE) {
            /* CRITICAL FIX #3: Log buffer overflow (silent failure was unsafe) */
            fprintf(stderr, "[OVERFLOW] Buffer wrapped! Lost samples. Total overflows: %lu\n", ++g_overflow_count);
            tail = (head - BMI_SPSC_BUFFER_SIZE) & MASK;
            count = BMI_SPSC_BUFFER_SIZE;
        }

        if (count > 0) {
            for (uint32_t i = 0; i < count; i++) {
                uint32_t idx = (tail + i) & MASK;
                add_sample_fast(&ring->data[idx]);
                samples_count++;
            }
            __atomic_store_n(&ring->tail, head, __ATOMIC_RELEASE);
            
            if (samples_count % 1000 == 0) {
                 write_buffered_samples_to_csv(); // periodic flush
                 printf("Samples: %lu\r", samples_count);
            }
        } else {
            poll(&pfd, 1, 100);
        }
    }

    printf("\nExiting normally...\n");

    /* Ensure daemon/timer are turned off when exiting */
    send_daemon_command(device_id, "control", 0);

    write_buffered_samples_to_csv();  /* Final flush */
    if (g_overflow_count > 0) {
        fprintf(stderr, "Final stats: %lu buffer overflows detected\n", g_overflow_count);
    }
    close_csv_writer();
    munmap(ring, mmap_size);
    close(fd);
    return 0;
}
