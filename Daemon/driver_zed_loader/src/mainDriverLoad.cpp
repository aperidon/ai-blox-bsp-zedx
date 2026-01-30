#include "MainProcess.h"
#include <iostream>
#include <cstdlib>
#include <cstdio>

#include "Debug.h"

#define APP_VERSION "v0.2.0"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    ::Debug::debugEnabled();
    ::Debug::debugPrint("[ZED-X Driver] Version %s\n", APP_VERSION);

    MainProcess loader;
    // constructor does all the work
    return 0;
}
