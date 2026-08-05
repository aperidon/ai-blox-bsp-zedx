#ifndef DAEMON_DEBUG_H
#define DAEMON_DEBUG_H

#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <chrono>
#include <ctime>

namespace Debug {

inline bool debugEnabled() {
    // Simple, explicit caching: evaluate getenv once on first call and store the result.
    static bool enabled_computed = false;
    static bool enabled = false;
    if (!enabled_computed) {
        const char* v = std::getenv("ZED_DAEMON_DEBUG");
        enabled = (v != nullptr) && (v[0] == '1');
        enabled_computed = true;
    }
    return enabled;
}

inline std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_t = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_t));
    return std::string(buf);
}

inline void debugPrint(const char* fmt, ...) {
    if (!debugEnabled()) return;
    fprintf(stderr, "[%s]", getTimestamp().c_str());
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

} // namespace Debug

#endif // DAEMON_DEBUG_H
