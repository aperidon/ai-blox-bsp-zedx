#include "CsvWriter.hpp"

CsvWriter::CsvWriter(const std::string& filename) : totalSamples_(0) {
    file_ = fopen(filename.c_str(), "w");
    if (file_) {
        fprintf(file_, "sample_id,timestamp_ns,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z\n");
    }
    buffer_.reserve(1000000);
}

CsvWriter::~CsvWriter() {
    flush();
    if (file_) fclose(file_);
}

void CsvWriter::addSample(const struct bmi_sensor_data& sample) {
    buffer_.push_back(sample);
}

void CsvWriter::flush() {
    if (!file_ || buffer_.empty()) return;
    
    for (const auto& s : buffer_) {
        totalSamples_++;
        fprintf(file_, "%lu,%llu,%d,%d,%d,%d,%d,%d\n",
                totalSamples_,
                (unsigned long long)s.timestamp,
                s.ax, s.ay, s.az,
                s.gx, s.gy, s.gz);
    }
    fflush(file_);
    buffer_.clear();
}
