/*
 * IMU Daemon for SPSC BMI088 IMU Management
 * Pure POSIX implementation (no Qt dependencies)
 */

#include "IMU_Daemon.h"

// Helper: trim whitespace
static std::string trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\n\r");
  size_t end = s.find_last_not_of(" \t\n\r");
  return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

IMU_Daemon::IMU_Daemon() {
  std::cout << "### [TRACE] Starting SPSC IMU Daemon" << std::endl;

  std::cout << "### [TRACE] Setting up default device configurations"
            << std::endl;
  std::vector<int> availableDevices = getAvailableSPSCDevices();
  std::cout << "Found " << availableDevices.size() << " SPSC devices"
            << std::endl;
  for (int deviceNum : availableDevices) {
    std::string startup_cmd = "set startup " + std::to_string(deviceNum) + " 1";
    handle_command(startup_cmd);
  }

  std::cout << "### [TRACE] Initialization complete" << std::endl;
}

IMU_Daemon::~IMU_Daemon() {
  keep_running = false;
  unlink(SOCK_PATH);
}

void IMU_Daemon::run() { daemon_server(); }

void IMU_Daemon::daemon_server() {
  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    return;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);

  unlink(SOCK_PATH);

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(server_fd);
    return;
  }

  // Set socket permissions (rw for all)
  chmod(SOCK_PATH, 0666);

  if (listen(server_fd, 5) < 0) {
    perror("listen");
    close(server_fd);
    return;
  }

  std::cout << "[IMU_Daemon] Listening on " << SOCK_PATH << std::endl;

  while (keep_running) {
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd >= 0) {
      std::thread(&IMU_Daemon::handle_client, this, client_fd).detach();
    }
  }

  close(server_fd);
}

void IMU_Daemon::handle_client(int client_fd) {
  char buffer[BUFSZ] = {0};
  ssize_t n = read(client_fd, buffer, BUFSZ - 1);
  if (n > 0) {
    buffer[n] = '\0';
    std::string cmd = trim(std::string(buffer));
    if (!cmd.empty()) {
      handle_command_with_response(cmd, client_fd);
    }
  }
  close(client_fd);
}

ParsedCommand IMU_Daemon::parse_command(const std::string &cmd) {
  ParsedCommand result;
  std::regex re(R"(set\s+(\w+)\s+(\d+)\s+(\d+))");
  std::smatch match;

  if (!std::regex_search(cmd, match, re)) {
    return result;
  }

  std::string type = match[1].str();
  result.device = std::stoi(match[2].str());
  result.value = std::stoi(match[3].str());

  if (type == "bmifreq")
    result.type = CMD_BMIFREQ;
  else if (type == "startup")
    result.type = CMD_STARTUP;
  else if (type == "control")
    result.type = CMD_CONTROL;
  else if (type == "timer")
    result.type = CMD_TIMER;
  else
    result.type = CMD_USAGE;

  return result;
}

void IMU_Daemon::handle_command(const std::string &cmd) {
  ParsedCommand parsed = parse_command(cmd);

  switch (parsed.type) {
  case CMD_STARTUP: {
    std::cout << "[TRACE] Executing startup for SPSC device " << parsed.device
              << std::endl;

    std::string base = "/sys/devices/virtual/bmi_spsc/spsc_bmi" +
                       std::to_string(parsed.device);

    // Set accelerometer range to ±12G (index 2)
    FILE *f = fopen((base + "/accel_range").c_str(), "w");
    if (f) {
      fprintf(f, "2\n");
      fclose(f);
    }

    // Set gyroscope range to ±1000°/s (index 1)
    f = fopen((base + "/gyro_range").c_str(), "w");
    if (f) {
      fprintf(f, "1\n");
      fclose(f);
    }

    // Set frequency to 200Hz
    f = fopen((base + "/accel_frequency").c_str(), "w");
    if (f) {
      fprintf(f, "200\n");
      fclose(f);
    }

    f = fopen((base + "/gyro_frequency").c_str(), "w");
    if (f) {
      fprintf(f, "200\n");
      fclose(f);
    }

    return;
  }
  case CMD_BMIFREQ: {
    std::cout << "[TRACE] Setting SPSC BMI frequency for device "
              << parsed.device << " to " << parsed.value << std::endl;

    std::string base = "/sys/devices/virtual/bmi_spsc/spsc_bmi" +
                       std::to_string(parsed.device);

    FILE *f = fopen((base + "/accel_frequency").c_str(), "w");
    if (f) {
      fprintf(f, "%d\n", parsed.value);
      fclose(f);
    }

    f = fopen((base + "/gyro_frequency").c_str(), "w");
    if (f) {
      fprintf(f, "%d\n", parsed.value);
      fclose(f);
    }

    return;
  }
  case CMD_CONTROL:
  case CMD_TIMER: {
    std::cout << "[TRACE] Setting SPSC timer control for device "
              << parsed.device << " to " << parsed.value << std::endl;

    std::string path = "/sys/devices/virtual/bmi_spsc/spsc_bmi" +
                       std::to_string(parsed.device) + "/timer_control";
    FILE *f = fopen(path.c_str(), "w");
    if (f) {
      fprintf(f, "%d\n", parsed.value);
      fclose(f);
    }

    return;
  }
  case CMD_USAGE:
  default:
    std::cerr << "Invalid command: " << cmd << std::endl;
    std::cerr
        << "Usage: set [bmifreq|startup|control|timer] <device_number> <value>"
        << std::endl;
    return;
  }
}

void IMU_Daemon::handle_command_with_response(const std::string &cmd,
                                              int client_fd) {
  ParsedCommand parsed = parse_command(cmd);

  if (parsed.type == CMD_TIMER) {
    std::string sysfs_path = "/sys/devices/virtual/bmi_spsc/spsc_bmi" +
                             std::to_string(parsed.device) + "/timer_control";
    std::string val_str = std::to_string(parsed.value);

    FILE *f = fopen(sysfs_path.c_str(), "w");
    if (f) {
      fprintf(f, "%d\n", parsed.value);
      fclose(f);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Verify
    char read_buf[32] = {0};
    f = fopen(sysfs_path.c_str(), "r");
    if (f) {
      if (fgets(read_buf, sizeof(read_buf) - 1, f)) {
        // OK
      }
      fclose(f);
    }

    std::string read_val = trim(std::string(read_buf));
    std::string response = (read_val == val_str) ? "OK\n" : "ERR\n";
    write(client_fd, response.c_str(), response.size());
    return;
  }

  handle_command(cmd);
  write(client_fd, "OK\n", 3);
}

std::vector<int> IMU_Daemon::getAvailableSPSCDevices() {
  std::vector<int> devices;
  const char *spsc_dir = "/sys/devices/virtual/bmi_spsc";

  DIR *dir = opendir(spsc_dir);
  if (!dir) {
    std::cerr << "SPSC devices directory not found: " << spsc_dir << std::endl;
    return devices;
  }

  std::regex re("spsc_bmi(\\d+)");
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    std::smatch match;
    std::string name = entry->d_name;
    if (std::regex_match(name, match, re)) {
      int deviceNum = std::stoi(match[1].str());
      devices.push_back(deviceNum);
      std::cout << "Found SPSC device: " << deviceNum << std::endl;
    }
  }
  closedir(dir);

  std::sort(devices.begin(), devices.end());
  return devices;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  IMU_Daemon daemon;
  daemon.run();
  return 0;
}
