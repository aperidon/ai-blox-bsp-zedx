#ifndef __ZED_GMSL_DAEMON__MAINPROCESS__
#define __ZED_GMSL_DAEMON__MAINPROCESS__

#include <string>
#include <memory>
#include <atomic>
#include <cstdint>
#include "zmq.hpp"

#define MAX_GMSL_CAM 8
const int ZMQ_DAEMON_SOCKET_PORT = 20206;
const int ZMQ_DAEMON_SOCKET_PORT_SUB = 20207;

class MainProcess {
public:
    MainProcess();
    ~MainProcess();
    int construct();
    void run();  // Main event loop

private:
    std::string startBlockingProcess(const std::string& workingdir, const std::string& cmd, bool verbose = false, bool error_only = false);
    void reportL4TVersion();
    void update();
    std::string parseServiceFile(const std::string& filepath, const std::string& key, int default_val);

    // String utilities
    std::vector<std::string> split(const std::string& s, char delimiter);

private:
    std::atomic<bool> keep_running{true};
    uint64_t last_restart_time = 0ULL;

    std::string endpoint_sub;
    std::string endpoint_pub;
    zmq::socket_type zmq_type_pub = zmq::socket_type::push;
    zmq::socket_type zmq_type_sub = zmq::socket_type::pull;
    zmq::context_t zmq_context_pub;
    zmq::context_t zmq_context_sub;
    std::unique_ptr<zmq::socket_t> zmq_socket_pub;
    std::unique_ptr<zmq::socket_t> zmq_socket_sub;
    int mTCPPort = ZMQ_DAEMON_SOCKET_PORT;
    int mTCPPortSub = ZMQ_DAEMON_SOCKET_PORT_SUB;
    bool mPortAcquiring[MAX_GMSL_CAM] = {false};
};

#endif /*__ZED_GMSL_DAEMON__MAINPROCESS__*/
