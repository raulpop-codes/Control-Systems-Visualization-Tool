#ifndef TIME_RESPONSE_HPP
#define TIME_RESPONSE_HPP

#include <vector>
#include "control/StateSpace.hpp"

// One point of a simulated time response.
struct ResponseSample {
    double t;
    double y;
};

// Numerically simulates x' = Ax + Bu, y = Cx + Du using the classical
// fourth-order Runge-Kutta method (RK4). Forward Euler -- x_{k+1} = x_k + dt*x'_k --
// uses one slope per step, so its error shrinks only in proportion to dt and
// it blows up on fast poles unless dt is tiny. RK4 samples the slope four
// times across each step (start, twice in the middle, end) and takes their
// weighted average 1:2:2:1, so the error shrinks like dt^4. dt should still
// be small relative to the system's fastest time constant (as a rule of
// thumb, dt well under 1 / (10 * |fastest pole|)), but it forgives a much
// coarser dt than Euler does.
class TimeResponse{
public:
    // Step response: u(t) = 1 for all t >= 0, x(0) = 0.
    static std::vector<ResponseSample> step(const StateSpace& ss, double tFinal, double dt);

    // Impulse response: rather than simulating an infinite, instantaneous
    // spike (which a fixed-step integrator cannot represent), this uses
    // the standard equivalence for a continuous system with u = delta(t):
    // integrating the impulse over an infinitesimal time jumps the state
    // from 0 to B, so the impulse response equals the free (u=0) response
    // starting from x(0) = B. 
    static std::vector<ResponseSample> impulse(const StateSpace& ss, double tFinal, double dt);

private:
    static std::vector<ResponseSample> simulate(const StateSpace& ss, double tFinal, double dt,
                                                  Matrix x0, bool applyStepInput);
};

#endif