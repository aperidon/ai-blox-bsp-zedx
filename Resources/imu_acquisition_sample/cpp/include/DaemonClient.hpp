#pragma once
#include <string>

class DaemonClient {
public:
    static bool initializeDevice(int deviceId, int accelRangeIdx = -1, int gyroRangeIdx = -1);
    static bool sendCommand(int deviceId, const std::string& type, int value);
};
