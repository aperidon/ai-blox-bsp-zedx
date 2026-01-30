/*
 * ZEDX Daemon - Pure C++/POSIX implementation
 * Monitors ZED SDK messages and manages nvargus-daemon recovery
 */

#include "MainProcess.h"

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "Debug.h"

MainProcess::MainProcess() {
    // Read port configuration from service file
    try {
        mTCPPort = std::stoi(parseServiceFile("/etc/systemd/system/zed_x_daemon.service", "Port", ZMQ_DAEMON_SOCKET_PORT));
    } catch (...) {
        mTCPPort = ZMQ_DAEMON_SOCKET_PORT;
        std::cerr << "[ZED-X Daemon] Warning: invalid Port in service file, using default " << mTCPPort << std::endl;
    }
    try {
        mTCPPortSub = std::stoi(parseServiceFile("/etc/systemd/system/zed_x_daemon.service", "PortSub", ZMQ_DAEMON_SOCKET_PORT_SUB));
    } catch (...) {
        mTCPPortSub = ZMQ_DAEMON_SOCKET_PORT_SUB;
        std::cerr << "[ZED-X Daemon] Warning: invalid PortSub in service file, using default " << mTCPPortSub << std::endl;
    }

    ::Debug::debugPrint("[ZED-X Daemon] Using TCP port (Read/Write): %d and %d\n", mTCPPort, mTCPPortSub);
}

MainProcess::~MainProcess() {
    keep_running = false;
    zmq_socket_sub.reset();
    zmq_socket_pub.reset();
}


std::string MainProcess::parseServiceFile(const std::string& filepath, const std::string& key, int default_val) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return std::to_string(default_val);
    }

    std::string line;
    while (std::getline(file, line)) {
        // Look for "Key=Value" pattern
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string k = line.substr(0, eq_pos);
            // Trim whitespace
            size_t start = k.find_first_not_of(" \t");
            size_t end = k.find_last_not_of(" \t");
            if (start != std::string::npos) {
                k = k.substr(start, end - start + 1);
            }
            if (k == key) {
                std::string v = line.substr(eq_pos + 1);
                start = v.find_first_not_of(" \t");
                end = v.find_last_not_of(" \t\n\r");
                if (start != std::string::npos) {
                    return v.substr(start, end - start + 1);
                }
            }
        }
    }
    return std::to_string(default_val);
}

std::vector<std::string> MainProcess::split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void MainProcess::reportL4TVersion() {
    std::string l4t_version_raw = startBlockingProcess(".", "cat /etc/nv_tegra_release", false, false);

    // Parse: '# R35 (release), REVISION: 4.1, GCID: ...'
    std::string result = "L4T_VERSION#";
    bool del_end = false;
    int commas = 0;

    for (size_t i = 0; i < l4t_version_raw.size(); i++) {
        char c = l4t_version_raw[i];
        if (c == '#' || c == ' ' || c == 'R')
            continue;
        if (c == ',') {
            commas++;
            if (commas == 2)
                break;
        }
        if (c == '(') {
            del_end = true;
            result += ".";
            continue;
        }
        if (c == ':') {
            del_end = false;
            continue;
        }
        if (!del_end)
            result += c;
    }

    ::Debug::debugPrint("[ZED-X Daemon] Reporting L4T version: %s\n", result.c_str());
    zmq_socket_pub->send(zmq::message_t(result), zmq::send_flags::dontwait);
}

std::string MainProcess::startBlockingProcess(const std::string& workingdir, const std::string& cmd, bool verbose, bool error_only) {
    std::string full_cmd = "cd " + workingdir + " && " + cmd + " 2>&1";

    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) {
        return "";
    }

    std::string result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    int status = pclose(pipe);
    int exit_code = -1;
    if (status != -1) {
        exit_code = WEXITSTATUS(status);
    }

    if (verbose && !result.empty() && (!error_only || (error_only && exit_code != 0))) {
        ::Debug::debugPrint("[ZED-X Daemon] Process %s outputs %s\n", cmd.c_str(), result.c_str());
    }

    return result;
}

void MainProcess::update() {
    zmq::message_t message;

    try {
        auto result = zmq_socket_sub->recv(message, zmq::recv_flags::none);
        if (!result) return;
    } catch (zmq::error_t& ex) {
        if (ex.num() != EINTR)
            throw;
        return;
    }

    std::string msg_recv = message.to_string();
    auto current_time_sc = static_cast<uint64_t>(std::time(nullptr));

    if (msg_recv.empty())
        return;

    // Message format: "ZEDX#<GMSL_PORT>#<SUB_MODEL>#<STATUS>#<FRAME_COUNT>#..."
    std::vector<std::string> fields = split(msg_recv, '#');

    if (fields.size() < 4)  // Need at least 4 fields
        return;

    // Check prefix
    if (fields[0].find("ZEDX") == std::string::npos)
        return;

    int gmsl_port = -1;
    int submodel = -1;
    try {
        gmsl_port = std::stoi(fields[1]);
        submodel = std::stoi(fields[2]);
    } catch (...) {
        std::cerr << "[ZED-X Daemon] Error parsing message fields" << std::endl;
        return;
    }

    if (gmsl_port < 0 || gmsl_port >= MAX_GMSL_CAM) {
        std::cerr << "[ZED-X Daemon] Invalid GMSL port: " << gmsl_port << std::endl;
        return;
    }

    std::string state = fields[3];

    if (state == "REPORT") {
        reportL4TVersion();
    }
    else if (state == "RUN" && fields.size() >= 5) {
        if (!mPortAcquiring[gmsl_port]) {
            ::Debug::debugPrint("[ZED-X Daemon] Port %d Running for CAM ModeliD %d\n", gmsl_port, submodel);
        }
        mPortAcquiring[gmsl_port] = true;
    }
    else if (state == "FROZEN" && current_time_sc > last_restart_time + 60) {
        // Launch argus restart
        startBlockingProcess("./", "service nvargus-daemon restart", false, false);
        ::Debug::debugPrint("[ZED-X Daemon] Restart NVArgus Daemon\n");
        last_restart_time = static_cast<uint64_t>(std::time(nullptr));
    }
    else if (state == "OPENING" || state == "OPEN") {
        ::Debug::debugPrint("[ZED-X Daemon] Port %d OPENING for CAM ModeliD %d\n", gmsl_port, submodel);
        mPortAcquiring[gmsl_port] = false;
    }
    else if (state == "CLOSING" || state == "OFF") {
        ::Debug::debugPrint("[ZED-X Daemon] Port %d CLOSING for CAM ModeliD %d\n", gmsl_port, submodel);
        mPortAcquiring[gmsl_port] = false;
    }
    else {
        ::Debug::debugPrint("[ZED-X Daemon] Received invalid message: %s\n", msg_recv.c_str());
    }
}

int MainProcess::construct() {
    // Create ZMQ TCP communication with ZED SDK
    zmq_context_sub = zmq::context_t(1);
    endpoint_sub = "tcp://127.0.0.1:" + std::to_string(mTCPPort);
    zmq_type_sub = zmq::socket_type::pull;

    try {
        zmq_socket_sub = std::make_unique<zmq::socket_t>(zmq_context_sub, zmq_type_sub);
        zmq_socket_sub->set(zmq::sockopt::linger, 0);
        zmq_socket_sub->bind(endpoint_sub);
        ::Debug::debugPrint("[ZED-X Daemon] ** Created Sub Endpoint %s\n", endpoint_sub.c_str());
    } catch (zmq::error_t& ex) {
        std::cerr << "Failed to create sub socket: " << ex.what() << std::endl;
        return -1;
    }

    // Create the publisher part to report L4T version
    zmq_context_pub = zmq::context_t(1);
    zmq_type_pub = zmq::socket_type::push;
    endpoint_pub = "tcp://127.0.0.1:" + std::to_string(mTCPPortSub);

    try {
        zmq_socket_pub = std::make_unique<zmq::socket_t>(zmq_context_pub, zmq_type_pub);
        zmq_socket_pub->set(zmq::sockopt::linger, 0);
        zmq_socket_pub->connect(endpoint_pub);
        ::Debug::debugPrint("[ZED-X Daemon] ** Created Pub Endpoint %s\n", endpoint_pub.c_str());
    } catch (zmq::error_t& ex) {
        std::cerr << "Failed to create pub socket: " << ex.what() << std::endl;
        return -1;
    }

    return 0;
}

void MainProcess::run() {
    ::Debug::debugPrint("[ZED-X Daemon] Entering main loop\n");

    zmq::pollitem_t items[] = {
        { zmq_socket_sub->handle(), 0, ZMQ_POLLIN, 0 }
    };

    while (keep_running) {
        try {
            int rc = zmq::poll(items, 1, std::chrono::milliseconds(100));

            if (rc > 0 && (items[0].revents & ZMQ_POLLIN)) {
                update();
            }
        } catch (zmq::error_t& ex) {
            if (ex.num() == EINTR) {
                continue;  // Interrupted, retry
            }
            std::cerr << "ZMQ poll error: " << ex.what() << std::endl;
            break;
        }
    }

    ::Debug::debugPrint("[ZED-X Daemon] Exiting main loop\n");
}