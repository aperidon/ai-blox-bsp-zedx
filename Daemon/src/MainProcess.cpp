#include "MainProcess.h"
#include <QDebug>
#include <sys/utsname.h>

MainProcess::MainProcess(QCoreApplication& app) {
    qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] "<<"** Start ZED-X Daemon";


    //// Get the tegra release number to create the path where drivers are located : /usr/lib/modules/<xx-xxx-xxxx>/kernel/drivers
    utsname result;      // declare the variable to hold the result
    uname(&result);
    QRegExp rx("tegra");
    QString release = result.release;
    release.chop(release.length()-rx.indexIn(release)-rx.pattern().length());
    qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Found tegra release : "<<rx.indexIn(release);

    //// ZED-X Daemon supports command that can be trigger before the rmmod/insmod of the drivers (gpio trigger and else)
    /// Pre-command are specified in /etc/systemd/system/zed_x_dameon.preload
    QString preload_command_file = "/etc/systemd/system/zed_x_daemon.preload";
    qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Searching for Preload file "<<preload_command_file<<", Found= "<<QFile(preload_command_file).exists();
    if (QFile(preload_command_file).exists())
        startBlockingProcess(QString("./"),preload_command_file,true);

    //// Set the booster clock
    setISPClockBooster();

    /// Make sure to restart argus first
    qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Restarting NVArgus";
    startBlockingProcess(QString("./"),QString("service nvargus-daemon restart"));

    //// Start to rm driver module in the following order (important!)
    /// sensors --> serializer --> deserializer

    ///////////////////////////////////////////
    ///////////////// SENSOR //////////////////
    ///////////////////////////////////////////

    /// IMX678 ZED-XOne UHD
    QString zedxone_uhd_driver_v1 = QString("/usr/lib/modules/%1/kernel/drivers/stereolabs/zedone4k/sl_zedxone_uhd.ko").arg(release);
    if (QFile(zedxone_uhd_driver_v1).exists())
        startBlockingProcess(QString("./"),QString("rmmod sl_zedxone_uhd"),true);

    /// ARO234 ZED-XOne GS or ZED-X
    QString zedx_driver_v1 = QString("/usr/lib/modules/%1/kernel/drivers/stereolabs/zedx/sl_zedx.ko").arg(release);
    if (QFile(zedx_driver_v1).exists())
        startBlockingProcess(QString("./"),QString("rmmod sl_zedx"),true);


    /// ISX031 ZED-XOne Pro or ZED-X Pro
    QString zedxpro_driver_v1 = QString("/usr/lib/modules/%1/kernel/drivers/stereolabs/zedxpro/sl_zedxpro.ko").arg(release);
    if (QFile(zedxpro_driver_v1).exists())
        startBlockingProcess(QString("./"),QString("rmmod sl_zedxpro"),true);

    ///////////////////////////////////////////
    ///////////////// SER /////////////////////
    ///////////////////////////////////////////

    //// MAX9295 serializer
    QString max9295_driver_inmaxx = QString("/usr/lib/modules/%1/kernel/drivers/stereolabs/max9295/sl_max9295.ko").arg(release);
    if (QFile(max9295_driver_inmaxx).exists())
        startBlockingProcess(QString("./"),QString("rmmod sl_max9295"),true);

    ///////////////////////////////////////////
    ///////////////// DESER ///////////////////
    ///////////////////////////////////////////

    /// MAX96712 Deserializer
    QString max96712_driver_inmaxx = QString("/usr/lib/modules/%1/kernel/drivers/stereolabs/max96712/sl_max96712.ko").arg(release);
    if (QFile(max96712_driver_inmaxx).exists())
        startBlockingProcess(QString("./"),QString("rmmod sl_max96712"),true);

    // MAX96724 Deserializer
    QString max96724_driver_inmaxx = QString("/usr/lib/modules/%1/kernel/drivers/stereolabs/max96724/sl_max96724.ko").arg(release);
    if (QFile(max96724_driver_inmaxx).exists())
        startBlockingProcess(QString("./"),QString("rmmod sl_max96724"),true);

    /// MAX9296 Deserializer
    QString max9296_driver_inmaxx = QString("/usr/lib/modules/%1/kernel/drivers/stereolabs/max9296/sl_max9296.ko").arg(release);
    if (QFile(max9296_driver_inmaxx).exists())
        startBlockingProcess(QString("./"),QString("rmmod sl_max9296"),true);



    qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] ZED-X Driver removed";

    //// Now Start to insert driver module in the following order (important!)
    /// deserializer --> serializer --> sensors

    ///////////////////////////////////////////
    ///////////////// DESER ///////////////////
    ///////////////////////////////////////////
    if (QFile(max96712_driver_inmaxx).exists())
        startBlockingProcess(QString("./"),QString("insmod  %1").arg(max96712_driver_inmaxx),true);

    if (QFile(max96724_driver_inmaxx).exists())
        startBlockingProcess(QString("./"),QString("insmod  %1").arg(max96724_driver_inmaxx),true);

    if (QFile(max9296_driver_inmaxx).exists())
        startBlockingProcess(QString("./"),QString("insmod  %1").arg(max9296_driver_inmaxx),true);


    ///////////////////////////////////////////
    ///////////////// SER   ///////////////////
    ///////////////////////////////////////////
    if (QFile(max9295_driver_inmaxx).exists())
        startBlockingProcess(QString("./"),QString("insmod  %1").arg(max9295_driver_inmaxx),true);

    ///////////////////////////////////////////
    ///////////////// SENSOR //////////////////
    ///////////////////////////////////////////
    /// zedx pro and one pro driver ///
    if (QFile(zedxpro_driver_v1).exists())
        startBlockingProcess(QString("./"),QString("insmod  %1").arg(zedxpro_driver_v1),true);


    /// zedx and zedx one gs driver ///
    if (QFile(zedx_driver_v1).exists())
        startBlockingProcess(QString("./"),QString("insmod  %1").arg(zedx_driver_v1),true);


    /// zedxone uhd driver ///
    if (QFile(zedxone_uhd_driver_v1).exists())
        startBlockingProcess(QString("./"),QString("insmod  %1").arg(zedxone_uhd_driver_v1),true);


   qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] ZED-X Driver loaded";

   /// ZED-X Daemon and SDK communicates together through local TCP comm.
   /// Port for read/write can be specified in the zed-x-daemon service and read here
   ///Reading Port in service file
   QSettings service_file("/etc/systemd/system/zed_x_daemon.service",QSettings::IniFormat);
   service_file.beginGroup("Service");
   mTCPPort = service_file.value("Port",ZMQ_DAEMON_SOCKET_PORT).toInt();
   mTCPPortSub = service_file.value("PortSub",ZMQ_DAEMON_SOCKET_PORT).toInt();
   mActivateVICBooster = service_file.value("VICBooster",0).toInt();

   service_file.endGroup();
   qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Using TCP port (Read/Write): "<<mTCPPort<<" and "<<mTCPPortSub;
   /// <<--

   /// Post-command that are specified in /etc/systemd/system/zed_x_dameon.postload
   QString postload_command_file = "/etc/systemd/system/zed_x_daemon.postload";
   qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Searching for Postload file "<<postload_command_file<<", Found=  "<<QFile(postload_command_file).exists();
   if (QFile(postload_command_file).exists())
        startBlockingProcess(QString("./"),postload_command_file,true);


   if (mActivateVICBooster)
       setVICBooster();

    qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Restarting NVArgus";
    startBlockingProcess(QString("./"),QString("service nvargus-daemon restart"));
}

MainProcess::~MainProcess() {
    appTimer.stop();
    zmq_socket_sub.reset(0);
}

void MainProcess::reportL4TVersion() {
    std::string l4t_version_raw = startBlockingProcess(".", "cat /etc/nv_tegra_release", false);
    // parsing '# R35 (release), REVISION: 4.1, GCID: 33958178, BOARD: t186ref, EABI: aarch64, DATE: Tue Aug  1 19:57:35 UTC 2023'
    // code form zediot
    std::string result = "L4T_VERSION#";
    int col_it = 0;
    bool del_end = false;
    int commas = 0;
    for (int i = 0; i < l4t_version_raw.size(); i++)
    {
        if (l4t_version_raw.at(i) == '#' || l4t_version_raw.at(i) == ' ' || l4t_version_raw.at(i) == 'R')
            continue;
        if (l4t_version_raw.at(i) == ',')
        {
            commas++;
            if (commas == 2)
                break;
        }
        if (l4t_version_raw.at(i) == '(')
        {
            del_end = true;
            result += ".";
            continue;
        }
        if (l4t_version_raw.at(i) == ':')
        {
            del_end = false;
            continue;
        }
        if (!del_end)
            result += l4t_version_raw.at(i);
    }
    
    qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Reporting L4T version: " << QString::fromStdString(result);
    zmq::send_result_t rr = zmq_socket_pub->send(zmq::message_t(result),zmq::send_flags::dontwait);
}

std::string MainProcess::startBlockingProcess(QString workingdir_, QString cmd_,bool verbose,bool error_only)
{
    proc.setWorkingDirectory(workingdir_);
    proc.start(cmd_);
    proc.waitForStarted();
    proc.waitForFinished();
    QByteArray tm_bytes;
    if (!error_only)
        tm_bytes = proc.readAllStandardOutput();
    tm_bytes+=proc.readAllStandardError();
    if (verbose && !tm_bytes.isEmpty())
        qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Process "<<cmd_<<"outputs "<<tm_bytes;
    proc.kill();
    return tm_bytes.toStdString();
}

void MainProcess::update()
{
    zmq::message_t message;
    // decompose the message
    //// With ZEDLink Quad, some interrupt are triggered and catched by zmq which throw an error
    /// This specific interrupt is catched so that it does not exit the daemon
    try{
        zmq_socket_sub->recv(message,zmq::recv_flags::none);
    }
    catch (zmq::error_t &ex)
    {
        if (ex.num() != EINTR)
            throw;
    }


    QString msg_recv = QString(message.to_string().c_str());
    uint64_t current_time_sc = QDateTime::currentSecsSinceEpoch();
    if (!msg_recv.isEmpty())
    {
       ///Message is formatted this way
       /// --> "ZEDX#<GMSL_PORT>#<SUB_MODEL>#<STATUS>#<FRAME_COUNT>#...."
       ///
       QStringList fields_ = msg_recv.split("#");
       if (fields_.size()<4) // We should at least at 4 fields, otherwise it's not valid
         return;

       /// Check that message starts with ZEDX
       QString prefix_ = fields_.at(0);
       if (!prefix_.contains("ZEDX"))
         return;

       int gmsl_port_ = fields_.at(1).toInt();
       int submodel_ = fields_.at(2).toInt();
       QString state_ = fields_.at(3);

       if (state_ == "REPORT")
       {
           // report L4T version to the SDK, because in docker the host version can be different from the container version and we need to warn the user.
           reportL4TVersion();
       }
       else if (state_ == "RUN" && fields_.size()>=5)
       {
           if (!mPortAcquiring[gmsl_port_])
            qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Port "<<gmsl_port_<<" Running for CAM ModeliD "<<submodel_;
           mPortAcquiring[gmsl_port_] = true;
       }
       else if (state_=="FROZEN" && current_time_sc>last_restart_time+60) //FROZEN and last restart was more than 60secs ago
       {
           if (current_time_sc>last_restart_time+60)
           {
               /// Launch argus restart ///
              startBlockingProcess(QString("./"),QString("service nvargus-daemon restart"));
              qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Restart NVArgus Daemon";
              last_restart_time = QDateTime::currentSecsSinceEpoch();
           }
           else
               qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Received a RESTART cmd but discard as already done";
       }
       else if (state_=="OPENING" || state_=="OPEN")
       {
          qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Port "<<gmsl_port_<<" OPENING for CAM ModeliD "<<submodel_;
          mPortAcquiring[gmsl_port_] = false;
       }
       else if (state_=="CLOSING" || state_=="OFF")
       {
          qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Port "<<gmsl_port_<<" CLOSING for CAM ModeliD "<<submodel_;
          mPortAcquiring[gmsl_port_] = false;
       }
       else
          qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Received invalid message : "<<msg_recv;
    }
}


int MainProcess::construct()
{
    ///Create ZMQ TCP communication with ZED SDK
    zmq_context_sub = zmq::context_t(1);
    endpoint_sub=QString("tcp://127.0.0.1:%1").arg(mTCPPort);//+std::to_string(i);
    zmq_type_sub = zmq::socket_type::pull;

    try{
        zmq_socket_sub.reset( new zmq::socket_t(zmq_context_sub, zmq_type_sub));
        zmq_socket_sub->setsockopt( ZMQ_LINGER, 0 );
        zmq_socket_sub->bind(endpoint_sub.toStdString());
        qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] "<<"** Created Sub Endpoint "<<endpoint_sub;
    }
    catch (zmq::error_t &ex)
    {
        if (ex.num() != EINTR)
            throw;
    }

    // create the publisher part, to report the L4T version
    zmq_context_pub = zmq::context_t(1);
    zmq_type_pub = zmq::socket_type::push;
    endpoint_pub =QString("tcp://127.0.0.1:%1").arg(mTCPPortSub);//+std::to_string(i);
    try{
        zmq_socket_pub.reset( new zmq::socket_t(zmq_context_pub, zmq_type_pub));
        zmq_socket_pub->setsockopt( ZMQ_LINGER, 0 );
        zmq_socket_pub->connect(endpoint_pub.toStdString());
        qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] "<<"** Created Pub Endpoint "<<endpoint_pub;
    }
    catch (zmq::error_t &ex)
    {
        if (ex.num() != EINTR)
            throw;
    }

    ///Connect a loop timer
    connect(&appTimer,SIGNAL(timeout()),this,SLOT(update()));
    appTimer.start(50);
    return zmq_socket_sub->connected()?0:1;
}

void MainProcess::setISPClockBooster()
{
    qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Activate Boosting clock control";

    startBlockingProcess(QString("./"),QString("sh -c \"echo 1 > /sys/kernel/debug/bpmp/debug/clk/vi/mrq_rate_locked\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"echo 1 > /sys/kernel/debug/bpmp/debug/clk/isp/mrq_rate_locked\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"echo 1 > /sys/kernel/debug/bpmp/debug/clk/nvcsi/mrq_rate_locked\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"echo 1 > /sys/kernel/debug/bpmp/debug/clk/emc/mrq_rate_locked\""),true,true);

    startBlockingProcess(QString("./"),QString("sh -c \"cat /sys/kernel/debug/bpmp/debug/clk/vi/max_rate |tee /sys/kernel/debug/bpmp/debug/clk/vi/rate\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"cat /sys/kernel/debug/bpmp/debug/clk/isp/max_rate | tee /sys/kernel/debug/bpmp/debug/clk/isp/rate\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"cat /sys/kernel/debug/bpmp/debug/clk/nvcsi/max_rate | tee /sys/kernel/debug/bpmp/debug/clk/nvcsi/rate\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"cat /sys/kernel/debug/bpmp/debug/clk/emc/max_rate | tee /sys/kernel/debug/bpmp/debug/clk/emc/rate\""),true,true);
}

void MainProcess::setVICBooster()
{
    /// JP 6
    #if L4T_VERSION>=360
    qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Activate Boosting VIC control (JP6.x)";
    startBlockingProcess(QString("./"),QString("sh -c \"echo on > /sys/devices/platform/bus@0/13e00000.host1x/15340000.vic/power/control\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"echo userspace > /sys/devices/platform/bus@0/13e00000.host1x/15340000.vic/devfreq/15340000.vic/governor\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"echo 729600000 > /sys/devices/platform/bus@0/13e00000.host1x/15340000.vic/devfreq/15340000.vic/max_freq\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"echo 729600000 > /sys/devices/platform/bus@0/13e00000.host1x/15340000.vic/devfreq/15340000.vic/userspace/set_freq\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"echo auto > /sys/devices/platform/bus@0/13e00000.host1x/15340000.vic/power/control\""),true,true);
    #endif

    /// JP 5
    #if (L4T_VERSION>=350 && L4T_VERSION<360)
    qDebug()<<"["<<QDateTime::currentDateTime().toString()<<"][ZED-X Daemon] Activate Boosting VIC control (JP5.x)";
    startBlockingProcess(QString("./"),QString("sh -c \"echo on > /sys/devices/platform/13e40000.host1x/15340000.vic/power/control\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"echo userspace > /sys/devices/platform/13e40000.host1x/15340000.vic/devfreq/15340000.vic/governor\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"echo 729600000 > /sys/devices/platform/13e40000.host1x/15340000.vic/devfreq/15340000.vic/max_freq\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"echo 729600000 > /sys/devices/platform/13e40000.host1x/15340000.vic/devfreq/15340000.vic/userspace/set_freq\""),true,true);
    startBlockingProcess(QString("./"),QString("sh -c \"echo auto > /sys/devices/platform/13e40000.host1x/15340000.vic/power/control\""),true,true);
    #endif
}

