#include "MainProcess.h"
#include <csignal>
#include <iostream>

#include "Debug.h"

#define APP_VERSION "v0.2.0"

static MainProcess* g_daemon = nullptr;

void signal_handler(int signum) {
    (void)signum;
    if (g_daemon) {
        std::cerr << "[ZED-X Daemon] Received signal, shutting down..." << std::endl;
    }
    std::exit(0);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    ::Debug::debugEnabled();
    ::Debug::debugPrint("[ZED-X Daemon] Version %s\n", APP_VERSION);

    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    MainProcess daemon;
    g_daemon = &daemon;

    if (daemon.construct() != 0) {
        std::cerr << "[ZED-X Daemon] Failed to initialize" << std::endl;
        return -1;
    }

    daemon.run();  // Blocking ZMQ poll loop
    return 0;
}
