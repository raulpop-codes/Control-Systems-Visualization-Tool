#ifndef TIME_RESPONSE_WINDOW_HPP
#define TIME_RESPONSE_WINDOW_HPP

#include <string>
#include "control/TimeResponse.hpp"

// Opens a window plotting a simulated time response (step or impulse) as a
// simple line graph: time t on the x axis, output y(t) on the y axis.
// Blocks until the window is closed, then control returns to the caller --
// same blocking behaviour as showRootLocusPlot, so it can be called
// directly from the console menu once the samples have been computed
// (TimeResponse::step / TimeResponse::impulse).
void showTimeResponsePlot(const std::vector<ResponseSample>& samples, const std::string& windowTitle);

#endif
