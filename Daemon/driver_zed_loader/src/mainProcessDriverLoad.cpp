/*
 * ZEDX Driver Loader - Pure C++/POSIX implementation
 * Loads/unloads ZED camera kernel modules in correct order
 */

#include "MainProcess.h"

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <chrono>
#include <cstdlib>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <regex>
#include <dirent.h>

#include "Debug.h"

std::string MainProcess::trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\n\r");
  size_t end = s.find_last_not_of(" \t\n\r");
  return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

bool MainProcess::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

std::string MainProcess::parseServiceFile(const std::string& filepath, const std::string& key, const std::string& default_val) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return default_val;
    }

    std::string line;
    while (std::getline(file, line)) {
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
    return default_val;
}

int MainProcess::parseServiceFileInt(const std::string& filepath, const std::string& key, int default_val) {
    std::string val = parseServiceFile(filepath, key, std::to_string(default_val));
    try {
        return std::stoi(val);
    } catch (...) {
        return default_val;
    }
}

MainProcess::MainProcess() {
    auto t_start = std::chrono::steady_clock::now();
    ::Debug::debugPrint("[ZED-X Daemon] ** Start ZED-X Daemon, Driver Loader Service\n");

    const std::string service_file = "/etc/systemd/system/zed_x_daemon.service";

    mActivateVICBooster = parseServiceFileInt(service_file, "VICBooster", 0);
    mSynch_mode = parseServiceFileInt(service_file, "sync_mode", 0);

    if (mSynch_mode < 0 || mSynch_mode > 2) {
        mSynch_mode = 0;
    }

    // Get tegra release to find driver paths
    struct utsname result;
    uname(&result);
    std::string release = result.release;
    
    // Find "tegra" and truncate after it
    size_t tegra_pos = release.find("tegra");
    if (tegra_pos != std::string::npos) {
        release = release.substr(0, tegra_pos + 5);  // include "tegra"
    }
    ::Debug::debugPrint("[ZED-X Daemon] Found tegra release: %s\n", release.c_str());

    // Pre-command support
    std::string preload_file = "/etc/systemd/system/zed_x_daemon.preload";
    ::Debug::debugPrint("[ZED-X Daemon] Searching for Preload file %s, Found= %s\n", preload_file.c_str(), (fileExists(preload_file) ? "true" : "false"));
    if (fileExists(preload_file)) {
        startBlockingProcess("./", preload_file, true, false);
    }

    // Set booster clock
    setISPClockBooster();

    // Restart argus first
    ::Debug::debugPrint("[ZED-X Daemon] Restarting NVArgus\n");
    startBlockingProcess("./", "service nvargus-daemon restart", false, false);

    // Remove drivers in order: sensors → serializer → deserializer
    std::string base_path = "/usr/lib/modules/" + release + "/kernel/drivers/stereolabs/";

    // SENSORS
    std::string zedxone_uhd_driver = base_path + "zedone4k/sl_zedxone_uhd.ko";
    if (fileExists(zedxone_uhd_driver))
        startBlockingProcess("./", "rmmod sl_zedxone_uhd", true, false);

    std::string zedx_driver = base_path + "zedx/sl_zedx.ko";
    if (fileExists(zedx_driver))
        startBlockingProcess("./", "rmmod sl_zedx", true, false);

    // ISX031 ZED-XOne HDR or ZED-X HDR
    std::string zedxhdr_driver = base_path + "zedxhdr/sl_zedxhdr.ko";
    if (fileExists(zedxhdr_driver))
        startBlockingProcess("./", "rmmod sl_zedxhdr", true, false);

    // SERIALIZER
    std::string max9295_driver = base_path + "max9295/sl_max9295.ko";
    if (fileExists(max9295_driver))
        startBlockingProcess("./", "rmmod sl_max9295", true, false);

    // DESERIALIZERS
    std::string max96724_driver = base_path + "max96724/sl_max96724.ko";
    if (fileExists(max96724_driver))
        startBlockingProcess("./", "rmmod sl_max96724", true, false);

    std::string max96712_driver = base_path + "max96712/sl_max96712.ko";
    if (fileExists(max96712_driver))
        startBlockingProcess("./", "rmmod sl_max96712", true, false);

    std::string max9296_driver = base_path + "max9296/sl_max9296.ko";
    if (fileExists(max9296_driver))
        startBlockingProcess("./", "rmmod sl_max9296", true, false);

    ::Debug::debugPrint("[ZED-X Daemon] ZED-X Daemon removed\n");

    // Insert drivers: deserializer → serializer → sensors
    // DESERIALIZERS
    if (fileExists(max96724_driver))
        startBlockingProcess("./", "insmod " + max96724_driver + " sync_mode=" + std::to_string(mSynch_mode), true, false);

    if (fileExists(max96712_driver))
        startBlockingProcess("./", "insmod " + max96712_driver + " sync_mode=" + std::to_string(mSynch_mode), true, false);

    if (fileExists(max9296_driver))
        startBlockingProcess("./", "insmod " + max9296_driver + " sync_mode=" + std::to_string(mSynch_mode), true, false);

    // SERIALIZER
    if (fileExists(max9295_driver))
        startBlockingProcess("./", "insmod " + max9295_driver, true, false);

    // SENSORS
    // zedx pro and one pro driver (HDR)
    if (fileExists(zedxhdr_driver))
        startBlockingProcess("./", "insmod " + zedxhdr_driver, true, false);

    // zedx and zedx one gs driver
    if (fileExists(zedx_driver))
        startBlockingProcess("./", "insmod " + zedx_driver, true, false);

    if (fileExists(zedxone_uhd_driver))
        startBlockingProcess("./", "insmod " + zedxone_uhd_driver, true, false);

    ::Debug::debugPrint("[ZED-X Daemon] ZED-X Daemon loaded\n");

    // Check if SPSC modules are loaded
    std::string lsmod_result = startBlockingProcess("./", "lsmod | grep bmi_spsc", false, false);
    if (!lsmod_result.empty()) {
        ::Debug::debugPrint("### [TRACE] SPSC modules found, stopping all timers first\n");
        stopAllSPSTimers();
    }

    // Unload modules in reverse dependency order
    std::vector<std::string> modules = {"bmi_spsc", "bmi088"};
    for (const auto &mod : modules) {
        std::string check =
            startBlockingProcess("./", "lsmod | grep " + mod, false, false);
        if (!trim(check).empty()) {
            ::Debug::debugPrint("### [TRACE] Removing module: %s\n", mod.c_str());
            startBlockingProcess("./", "sudo rmmod " + mod, true, false);
        }
    }

    // Load modules in correct order
    std::vector<std::string> spscModules = {"bmi_spsc.ko", "bmi088.ko"};
    for (const auto &mod : spscModules) {
        std::string modPath = base_path + "bmi088/" + mod;
        ::Debug::debugPrint("### [TRACE] Loading SPSC module: %s\n", modPath.c_str());
        startBlockingProcess("./", "insmod " + modPath, true, false);
    }

    ::Debug::debugPrint("[ZED-X Daemon] IMU Drivers loaded\n");

    // Post-command support
    std::string postload_file = "/etc/systemd/system/zed_x_daemon.postload";
    ::Debug::debugPrint("[ZED-X Daemon] Searching for Postload file %s, Found= %s\n", postload_file.c_str(), (fileExists(postload_file) ? "true" : "false"));
    if (fileExists(postload_file)) {
        startBlockingProcess("./", postload_file, true, false);
    }

    if (mActivateVICBooster)
        setVICBooster();

    ::Debug::debugPrint("[ZED-X Daemon] Restarting NVArgus\n");
    startBlockingProcess("./", "service nvargus-daemon restart", false, false);

    auto t_end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
    ::Debug::debugPrint("[ZED-X Daemon] Initialization completed in %lld ms\n", (long long)elapsed_ms);
}

void MainProcess::stopAllSPSTimers() {
  ::Debug::debugPrint("### [TRACE] Stopping all SPSC timers\n");

  std::vector<int> devices = getAvailableSPSCDevices();
  for (int dev : devices) {
    std::string path = "/sys/devices/virtual/bmi_spsc/spsc_bmi" +
                       std::to_string(dev) + "/timer_control";
    FILE *f = fopen(path.c_str(), "w");
    if (f) {
      fprintf(f, "0\n");
      fclose(f);
    }
  }
}

std::vector<int> MainProcess::getAvailableSPSCDevices() {
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

MainProcess::~MainProcess() {
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

void MainProcess::setISPClockBooster() {
    ::Debug::debugPrint("[ZED-X Daemon] Activate Boosting clock control\n");

    startBlockingProcess("./", "sh -c \"echo 1 > /sys/kernel/debug/bpmp/debug/clk/vi/mrq_rate_locked\"", true, true);
    startBlockingProcess("./", "sh -c \"echo 1 > /sys/kernel/debug/bpmp/debug/clk/isp/mrq_rate_locked\"", true, true);
    startBlockingProcess("./", "sh -c \"echo 1 > /sys/kernel/debug/bpmp/debug/clk/nvcsi/mrq_rate_locked\"", true, true);
    startBlockingProcess("./", "sh -c \"echo 1 > /sys/kernel/debug/bpmp/debug/clk/emc/mrq_rate_locked\"", true, true);

    startBlockingProcess("./", "sh -c \"cat /sys/kernel/debug/bpmp/debug/clk/vi/max_rate | tee /sys/kernel/debug/bpmp/debug/clk/vi/rate\"", true, true);
    startBlockingProcess("./", "sh -c \"cat /sys/kernel/debug/bpmp/debug/clk/isp/max_rate | tee /sys/kernel/debug/bpmp/debug/clk/isp/rate\"", true, true);
    startBlockingProcess("./", "sh -c \"cat /sys/kernel/debug/bpmp/debug/clk/nvcsi/max_rate | tee /sys/kernel/debug/bpmp/debug/clk/nvcsi/rate\"", true, true);
    startBlockingProcess("./", "sh -c \"cat /sys/kernel/debug/bpmp/debug/clk/emc/max_rate | tee /sys/kernel/debug/bpmp/debug/clk/emc/rate\"", true, true);
}

void MainProcess::setVICBooster() {
#if L4T_VERSION >= 360
    ::Debug::debugPrint("[ZED-X Daemon] Activate Boosting VIC control (JP6.x)\n");
    startBlockingProcess("./", "sh -c \"echo on > /sys/devices/platform/bus@0/13e00000.host1x/15340000.vic/power/control\"", true, true);
    startBlockingProcess("./", "sh -c \"echo userspace > /sys/devices/platform/bus@0/13e00000.host1x/15340000.vic/devfreq/15340000.vic/governor\"", true, true);
    startBlockingProcess("./", "sh -c \"echo 729600000 > /sys/devices/platform/bus@0/13e00000.host1x/15340000.vic/devfreq/15340000.vic/max_freq\"", true, true);
    startBlockingProcess("./", "sh -c \"echo 729600000 > /sys/devices/platform/bus@0/13e00000.host1x/15340000.vic/devfreq/15340000.vic/userspace/set_freq\"", true, true);
    startBlockingProcess("./", "sh -c \"echo auto > /sys/devices/platform/bus@0/13e00000.host1x/15340000.vic/power/control\"", true, true);
#elif (L4T_VERSION >= 350 && L4T_VERSION < 360)
    ::Debug::debugPrint("[ZED-X Daemon] Activate Boosting VIC control (JP5.x)\n");
    startBlockingProcess("./", "sh -c \"echo on > /sys/devices/platform/13e40000.host1x/15340000.vic/power/control\"", true, true);
    startBlockingProcess("./", "sh -c \"echo userspace > /sys/devices/platform/13e40000.host1x/15340000.vic/devfreq/15340000.vic/governor\"", true, true);
    startBlockingProcess("./", "sh -c \"echo 729600000 > /sys/devices/platform/13e40000.host1x/15340000.vic/devfreq/15340000.vic/max_freq\"", true, true);
    startBlockingProcess("./", "sh -c \"echo 729600000 > /sys/devices/platform/13e40000.host1x/15340000.vic/devfreq/15340000.vic/userspace/set_freq\"", true, true);
    startBlockingProcess("./", "sh -c \"echo auto > /sys/devices/platform/13e40000.host1x/15340000.vic/power/control\"", true, true);
#endif
}
