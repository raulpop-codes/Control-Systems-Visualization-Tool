#ifndef ROOT_LOCUS_WINDOW_HPP
#define ROOT_LOCUS_WINDOW_HPP

#include "control/TransferFunction.hpp"

// Opens an interactive window: the static root locus curves for
// k in [0, kMax], plus a slider that moves K and redraws the
// closed-loop poles live. Blocks until the window is closed, then
// control returns to the caller -- so calling this from the
// console menu just pauses the menu until you close the plot.
//
// Pulled out into its own function so both main.cpp's "Root locus" menu
// option and the standalone main_gui.cpp entry point share one
// implementation instead of two copies of the same window loop.
void showRootLocusPlot(const TransferFunction& plant, double kMax, double initialK = 1.0);

#endif
