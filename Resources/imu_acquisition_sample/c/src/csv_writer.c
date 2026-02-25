#include "csv_writer.h"
#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_BUFFER_SIZE (1000000)

static FILE *g_csv_file = NULL;
static struct bmi_sensor_data *g_sample_buffer = NULL;
static int g_buffer_write_idx = 0;
static unsigned long g_samples_written_total = 0;

void init_csv_writer(const char *filename) {
    g_csv_file = fopen(filename, "w");
    if (!g_csv_file) {
        perror("fopen csv");
        exit(1);
    }
    
    fprintf(g_csv_file, "sample_id,timestamp_ns,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z\n");
    fflush(g_csv_file);

    g_sample_buffer = malloc(SAMPLE_BUFFER_SIZE * sizeof(struct bmi_sensor_data));
    if (!g_sample_buffer) {
        fprintf(stderr, "Failed to allocate CSV sample buffer\n");
        exit(1);
    }
    g_buffer_write_idx = 0;
}

void close_csv_writer(void) {
    write_buffered_samples_to_csv();
    if (g_csv_file) fclose(g_csv_file);
    if (g_sample_buffer) free(g_sample_buffer);
}

void add_sample_fast(struct bmi_sensor_data *sample) {
    if (g_buffer_write_idx < SAMPLE_BUFFER_SIZE) {
        g_sample_buffer[g_buffer_write_idx++] = *sample;
    }
}

void write_buffered_samples_to_csv(void) {
    if (!g_csv_file || g_buffer_write_idx == 0) return;

    for (int i = 0; i < g_buffer_write_idx; i++) {
        struct bmi_sensor_data *s = &g_sample_buffer[i];
        g_samples_written_total++;
        fprintf(g_csv_file, "%lu,%llu,%d,%d,%d,%d,%d,%d\n",
                g_samples_written_total,
                (unsigned long long)s->timestamp,
                s->ax, s->ay, s->az,
                s->gx, s->gy, s->gz);
    }
    fflush(g_csv_file);
    g_buffer_write_idx = 0;
}
