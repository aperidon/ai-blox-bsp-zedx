#ifndef __ZED_GMSL_DAEMON__MAINPROCESS__
#define __ZED_GMSL_DAEMON__MAINPROCESS__

#include <string>
#include <vector>
#include <cstdint>

class MainProcess final {
public:
    MainProcess();
    ~MainProcess();

private:
    std::string startBlockingProcess(const std::string& workingdir, const std::string& cmd, bool verbose = false, bool error_only = false);
    void reportL4TVersion();
    void setISPClockBooster();
    void setVICBooster();
    void stopAllSPSTimers();
    std::vector<int> getAvailableSPSCDevices();

    std::string trim(const std::string &s);

    // Config parsing
    std::string parseServiceFile(const std::string& filepath, const std::string& key, const std::string& default_val);
    int parseServiceFileInt(const std::string& filepath, const std::string& key, int default_val);
    bool fileExists(const std::string& path);

    int mActivateVICBooster = 0;
    int mSynch_mode = 0; // 0 = no sync, 1 = master/slave mode, 2 = slave mode only
};

#endif /*__ZED_GMSL_DAEMON__MAINPROCESS__*/
