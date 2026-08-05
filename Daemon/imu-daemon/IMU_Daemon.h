#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <iostream>
#include <regex>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#define SOCK_PATH "/tmp/imu_daemon.sock"
#define BUFSZ 256

enum CommandType {
  CMD_BMIFREQ,
  CMD_STARTUP,
  CMD_CONTROL,
  CMD_TIMER,
  CMD_USAGE
};

struct ParsedCommand {
  CommandType type = CMD_USAGE;
  int device = 0;
  int value = 0;
};

class IMU_Daemon {
public:
  IMU_Daemon();
  ~IMU_Daemon();

  void run();

private:
  std::atomic<bool> keep_running{true};

  // Socket server
  void daemon_server();
  void handle_client(int client_fd);

  // Command processing
  ParsedCommand parse_command(const std::string &cmd);
  void handle_command(const std::string &cmd);
  void handle_command_with_response(const std::string &cmd, int client_fd);

  // SPSC device discovery
  std::vector<int> getAvailableSPSCDevices();

  // Helper
  std::string startBlockingProcess(const std::string &workingdir,
                                   const std::string &cmd, bool verbose = false,
                                   bool error_only = false);
};
