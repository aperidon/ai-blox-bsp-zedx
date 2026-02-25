#pragma once
#include "bmi_protocol.h"
#include <string>
#include <cstddef>

class RingBuffer {
public:
    RingBuffer(const std::string& deviceName);
    ~RingBuffer();

    bool isValid() const;
    void sync();
    
    // CPU affinity
    static int getTargetCpu(const std::string& deviceName);
    static bool setCpuAffinity(int cpu);
    
    // Low level accessors
    volatile uint32_t* getHeadPtr();
    volatile uint32_t* getTailPtr();
    struct bmi_sensor_data* getDataPtr(uint32_t idx);
    
    int getFd() const { return fd_; }
    void storeTail(uint32_t head); // Release

private:
    int fd_;
    size_t size_;
    struct spsc_ring* ring_;
};
