#include "DaemonClient.hpp"
#include "RingBuffer.hpp"
#include "CsvWriter.hpp"
#include <iostream>
#include <poll.h>
#include <csignal>
#include <atomic>
#include <unistd.h>

std::atomic<bool> g_running(true);
static unsigned long g_overflow_count = 0;

/* CRITICAL FIX: Signal-safe handler - only set flag */
void sig_handler(int) { 
    g_running.store(false, std::memory_order_release); 
}

int main(int argc, char** argv) {
    std::string deviceName = "spsc_bmi0";
    std::string csvName = "bmi_data_cpp.csv";
    int accelRangeIdx = -1;
    int gyroRangeIdx = -1;

    /* Parse args: first non-option token is device name. Options:
     *  --accel-range <idx>   set accel range index
     *  --gyro-range  <idx>   set gyro range index
     *  --csv <filename>      set output CSV filename
     * NOTE: frequency/ODR is intentionally not exposed here.
     */
    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);
        if (a.rfind("--accel-range", 0) == 0) {
            if (a.size() > 13 && a[13] == '=') accelRangeIdx = std::stoi(a.substr(14));
            else if (i + 1 < argc) accelRangeIdx = std::stoi(argv[++i]);
            continue;
        }
        if (a.rfind("--gyro-range", 0) == 0) {
            if (a.size() > 12 && a[12] == '=') gyroRangeIdx = std::stoi(a.substr(13));
            else if (i + 1 < argc) gyroRangeIdx = std::stoi(argv[++i]);
            continue;
        }
        if (a.rfind("--csv", 0) == 0) {
            if (a.size() > 5 && a[5] == '=') csvName = a.substr(6);
            else if (i + 1 < argc) csvName = argv[++i];
            continue;
        }
        deviceName = a;
    }

    int deviceId = 0;
    if (deviceName.find("spsc_bmi") == 0) {
        deviceId = std::stoi(deviceName.substr(8));
    }

    std::signal(SIGINT, sig_handler);

    // CPU affinity pinning
    int cpu = RingBuffer::getTargetCpu(deviceName);
    if (cpu >= 0) {
        std::cout << "Pinning to CPU " << cpu << std::endl;
        if (!RingBuffer::setCpuAffinity(cpu)) {
            std::cerr << "Warning: Failed to set CPU affinity" << std::endl;
        }
    }

    CsvWriter writer(csvName);
    RingBuffer ring(deviceName);

    if (!ring.isValid()) {
        std::cerr << "Failed to open ring buffer" << std::endl;
        return 1;
    }

    ring.sync();

    // Correct startup sequence:
    // 1) startup
    // 2) apply ranges
    // 3) wait 40 ms and enable control

    /* Initialize device (startup → optional ranges → 40ms → control 1) */
    if (!DaemonClient::initializeDevice(deviceId, accelRangeIdx, gyroRangeIdx)) {
        std::cerr << "Failed to init daemon" << std::endl;
    }

    /* CRITICAL FIX #2: Wait for stale data to flush */
    std::cout << "Waiting for fresh samples..." << std::endl;
    sleep(1);
    ring.sync();  /* Re-sync after stale flush */

    struct pollfd pfd = {ring.getFd(), POLLIN, 0};
    const uint32_t MASK = BMI_SPSC_BUFFER_SIZE - 1;

    std::cout << "Collecting data..." << std::endl;

    unsigned long sample_count = 0;

    while (g_running) {
        uint32_t tail = __atomic_load_n(ring.getTailPtr(), __ATOMIC_RELAXED);
        uint32_t head = __atomic_load_n(ring.getHeadPtr(), __ATOMIC_ACQUIRE);
        uint32_t count = (head - tail) & MASK;

        if (count > BMI_SPSC_BUFFER_SIZE) {
            /* CRITICAL FIX #3: Log buffer overflow */
            std::cerr << "[OVERFLOW] Buffer wrapped! Lost samples. Total: " 
                      << ++g_overflow_count << std::endl;
            tail = (head - BMI_SPSC_BUFFER_SIZE) & MASK;
            count = BMI_SPSC_BUFFER_SIZE;
        }

        if (count > 0) {
            for (uint32_t i = 0; i < count; i++) {
                uint32_t idx = (tail + i) & MASK;
                writer.addSample(*ring.getDataPtr(idx));
                sample_count++;
            }
            ring.storeTail(head);
            
            if (sample_count % 1000 == 0) {
                writer.flush(); // Flush regularly
                std::cout << "Samples: " << sample_count << "\r" << std::flush;
            }
        } else {
            poll(&pfd, 1, 100);
        }
    }

    /* CRITICAL FIX #1: Final flush on normal/signal exit */
    std::cout << "\nFlushing final samples..." << std::endl;

    // Ensure daemon/timer are turned off when exiting
    if (deviceId >= 0) {
        DaemonClient::sendCommand(deviceId, "control", 0);
    }

    writer.flush();
    if (g_overflow_count > 0) {
        std::cerr << "Final stats: " << g_overflow_count << " buffer overflows" << std::endl;
    }

    return 0;
}
