#ifndef LAUNCHER_WINDOW_HPP
#define LAUNCHER_WINDOW_HPP

// Runs the whole point-and-click app: define a system (state-space
// matrices or transfer function coefficients) using on-screen text
// fields, then pick Stability / Step Response / Impulse Response / Root
// Locus from a menu of buttons. No console I/O anywhere -- this is the
// only thing gui.exe does. Blocks until the launcher window is closed.
void runLauncherApp();

#endif
