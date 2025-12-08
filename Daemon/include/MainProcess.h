#ifndef __ZED_GMSL_DAEMON__MAINPROCESS__
#define __ZED_GMSL_DAEMON__MAINPROCESS__
#include <QtCore>
#include <QSettings>
#include "zmq.hpp"
#define MAX_GMSL_CAM 8
const int ZMQ_DAEMON_SOCKET_PORT = 20206;
const int  ZMQ_DAEMON_SOCKET_PORT_SUB = 20207;

class MainProcess: public QObject {
    Q_OBJECT
public:
    MainProcess(QCoreApplication& app);
    ~MainProcess();
    int construct();

private :
    std::string startBlockingProcess(QString workingdir_, QString cmd_, bool verbose=false, bool error_only=false);
    void reportL4TVersion();

public slots:
    void update();


private:
    void setISPClockBooster();
    void setVICBooster();

    QTimer appTimer;
    QProcess proc;
    uint64_t last_restart_time = 0ULL;

    QString endpoint_sub="";
    QString endpoint_pub="";
    zmq::socket_type zmq_type_pub = zmq::socket_type::pub;
    zmq::socket_type zmq_type_sub = zmq::socket_type::sub;
    zmq::context_t zmq_context_pub;
    zmq::context_t zmq_context_sub;
    std::unique_ptr<zmq::socket_t> zmq_socket_pub;
    std::unique_ptr<zmq::socket_t> zmq_socket_sub;
    int mTCPPort = ZMQ_DAEMON_SOCKET_PORT;
    int mTCPPortSub = ZMQ_DAEMON_SOCKET_PORT_SUB;
    int mActivateVICBooster = false;
    bool mPortAcquiring[MAX_GMSL_CAM] = {false};



};
#endif /*__STEREOLABS__MAINPROCESS__*/
