#pragma once
#include "bmi_protocol.h"
#include <string>
#include <vector>
#include <cstdio>

class CsvWriter {
public:
    CsvWriter(const std::string& filename);
    ~CsvWriter();

    void addSample(const struct bmi_sensor_data& sample);
    void flush();

private:
    FILE* file_;
    std::vector<struct bmi_sensor_data> buffer_;
    unsigned long totalSamples_;
};
