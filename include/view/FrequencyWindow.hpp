#ifndef FREQUENCY_WINDOW_HPP
#define FREQUENCY_WINDOW_HPP

#include "control/TransferFunction.hpp"

// Opens an interactive window with the frequency-domain views of the open
// loop H(s) = plant(s) * e^{-s*deadTime}: Bode (magnitude + phase), Nichols
// and Nyquist, on tabs (buttons, or the keys 1 / 2 / 3).
//
// A K slider (the loop gain, like the one in the root locus window) moves the
// curves live -- up and down on Bode and Nichols, scaling the locus on
// Nyquist -- and a corner panel lists wt, gamma_k, w_-pi, m_k and K_critic
// together with three verdicts on the closed loop (margins, Nyquist
// encirclements, Routh-Hurwitz) that should always agree.
//
// The frequency range is chosen automatically from the poles and zeros
// (FrequencyResponse::autoRange). Blocks until the window is closed, same as
// showRootLocusPlot / showTimeResponsePlot.
void showFrequencyResponse(const TransferFunction& plant, double deadTime = 0.0);

#endif
