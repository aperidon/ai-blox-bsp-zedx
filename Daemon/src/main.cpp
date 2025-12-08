#include "MainProcess.h"

#define APP_VERSION "v0.1.1"

int main(int argc, char* argv[]) {

    QCoreApplication app(argc,argv);
    app.setApplicationVersion(APP_VERSION);
    MainProcess Kitapp(app);
    if (Kitapp.construct()!=0)
        return -1;
    int result = app.exec();
    return result;

}
