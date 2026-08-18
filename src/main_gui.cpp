// main_gui.cpp
//
// Entry point for gui.exe -- the point-and-click app. No console I/O
// anywhere: define your system and run an analysis using on-screen text
// fields and buttons (view/LauncherWindow.cpp). This is what most people
// should run day to day; main.exe (the console menu) is still there for
// anyone who prefers typing values at a prompt.
#include "view/LauncherWindow.hpp"

int main() {
    runLauncherApp();
    return 0;
}
