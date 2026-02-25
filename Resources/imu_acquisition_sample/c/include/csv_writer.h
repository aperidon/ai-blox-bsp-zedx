#ifndef CSV_WRITER_H
#define CSV_WRITER_H

#include "bmi_protocol.h"

void init_csv_writer(const char *filename);
void close_csv_writer(void);
void add_sample_fast(struct bmi_sensor_data *sample);
void write_buffered_samples_to_csv(void);

#endif
