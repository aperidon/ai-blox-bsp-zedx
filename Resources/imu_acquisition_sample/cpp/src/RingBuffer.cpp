#include "RingBuffer.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sched.h>
#include <fstream>
#include <iostream>

RingBuffer::RingBuffer(const std::string& deviceName) : fd_(-1), ring_(nullptr) {
    std::string path = "/dev/" + deviceName;
    fd_ = open(path.c_str(), O_RDWR);
    if (fd_ < 0) return;

    // Get size
    std::string sysPath = "/sys/class/bmi_spsc/" + deviceName + "/ring_buffer_size";
    std::ifstream f(sysPath);
    unsigned long sz = 32768;
    if (f.is_open()) f >> sz;
    size_ = sz;

    void* map = mmap(NULL, size_, PROT_READ|PROT_WRITE, MAP_SHARED, fd_, 0);
    if (map != MAP_FAILED) {
        ring_ = static_cast<struct spsc_ring*>(map);
    } else {
        /* CRITICAL FIX: Close fd on mmap failure to prevent leak */
        close(fd_);
        fd_ = -1;
    }
}

RingBuffer::~RingBuffer() {
    if (ring_) munmap(ring_, size_);
    if (fd_ >= 0) close(fd_);
}

bool RingBuffer::isValid() const { return ring_ != nullptr; }

void RingBuffer::sync() {
    if (!ring_) return;
    asm volatile("" : : : "memory");
    ring_->tail = ring_->head;
    asm volatile("" : : : "memory");
}

volatile uint32_t* RingBuffer::getHeadPtr() { return &ring_->head; }
volatile uint32_t* RingBuffer::getTailPtr() { return &ring_->tail; }
struct bmi_sensor_data* RingBuffer::getDataPtr(uint32_t idx) { return &ring_->data[idx]; }

void RingBuffer::storeTail(uint32_t head) {
    __atomic_store_n(&ring_->tail, head, __ATOMIC_RELEASE);
}

int RingBuffer::getTargetCpu(const std::string& deviceName) {
    std::string path = "/sys/class/bmi_spsc/" + deviceName + "/target_cpu";
    std::ifstream f(path);
    if (!f.is_open()) return -1;
    int cpu = -1;
    f >> cpu;
    return cpu;
}

bool RingBuffer::setCpuAffinity(int cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0) {
        return false;
    }
    return true;
}
