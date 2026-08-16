// main_gui.cpp
//
// Standalone quick-launch: opens the root locus, step-response, and
// impulse-response windows directly for an example plant, without going
// through the console menu. Useful for testing the view/ classes in
// isolation. The console menu (main.cpp) opens the same windows
// (showRootLocusPlot / showTimeResponsePlot) for whatever system you
// actually typed in -- both entry points call the same functions, so
// there's only one implementation of each window to maintain.
#include "view/RootLocusWindow.hpp"
#include "view/TimeResponseWindow.hpp"
#include "math/Polynomial.hpp"
#include "control/TransferFunction.hpp"
#include "control/Conversions.hpp"
#include "control/TimeResponse.hpp"
#include <iostream>

int main() {
    std::cout << "gui.exe: quick-launch demo, always uses H(s) = 1/(s(s+2)(s+4)).\n"
              << "For YOUR OWN system (matrices or transfer function), run main.exe "
              << "and choose an option from the menu instead.\n\n";

    // H(s) = 1 / (s(s+2)(s+4))
    Polynomial num({1.0});
    Polynomial den = Polynomial({0.0, 1.0}) * Polynomial({2.0, 1.0}) * Polynomial({4.0, 1.0});
    TransferFunction plant(num, den);

    std::cout << "1/3: Root locus window (close it to continue)...\n";
    showRootLocusPlot(plant, 120.0);

    StateSpace ss = Conversions::toStateSpace(plant);

    std::cout << "2/3: Step response window (close it to continue)...\n";
    auto stepSamples = TimeResponse::step(ss, 10.0, 0.01);
    showTimeResponsePlot(stepSamples, "Step Response");

    std::cout << "3/3: Impulse response window (close it to finish)...\n";
    auto impulseSamples = TimeResponse::impulse(ss, 10.0, 0.01);
    showTimeResponsePlot(impulseSamples, "Impulse Response");

    return 0;
}
