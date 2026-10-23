#ifndef FREQUENCY_RESPONSE_HPP
#define FREQUENCY_RESPONSE_HPP

#include <vector>
#include <optional>
#include <utility>
#include "control/TransferFunction.hpp"

// One point of the frequency response H(jw).
struct FreqSample {
    double w;         // rad/s
    ComplexNumber H;  // H(jw), dead time included
    double magDb;     // 20*lg|H|
    double phaseDeg;  // CONTINUOUS (unwrapped) phase, in degrees
};

// Stability margins, in the course's convention (not MATLAB's):
//   wt     : |K*H(j wt)| = 1               (gain crossover)
//   gamma_k: 180 + angle(H(j wt))          (phase margin, degrees)
//   wPi    : angle(H(j wPi)) = -180 (mod 360)
//   m_k    : K*|H(j wPi)|                  (MATLAB's gain margin is 1/m_k)
// Closed loop stable <=> gamma_k > 0 and m_k < 1 (valid when the open loop has
// no right-half-plane poles). An empty optional means "no such crossing",
// i.e. that margin is infinite.
struct Margins {
    double K = 1.0;
    std::optional<double> wt;
    std::optional<double> phaseMargin;
    std::optional<double> wPi;
    std::optional<double> gainMargin;

    bool stable() const {
        return (!phaseMargin || *phaseMargin > 0.0) && (!gainMargin || *gainMargin < 1.0);
    }
    // Gain at which the loop reaches the stability boundary: K * |H| = 1 at wPi.
    std::optional<double> criticalGain() const {
        if(!gainMargin) return std::nullopt;
        return K / *gainMargin;
    }
};

// Nyquist criterion by counting: the closed loop (unity feedback, gain K) is
// stable <=> the angle of the vector from -1/K to the locus, i.e. of 1 + K*H(jw),
// turns by (pi/2)*(N0 + 2*N+) while w goes 0+ -> infinity.
//   N0 : open-loop poles at the origin,  N+ : open-loop poles with Re > 0.
struct NyquistVerdict {
    double deltaDeg;     // measured variation of arg(1 + K*H)
    double requiredDeg;  // 90 * (N0 + 2*N+)
    int N0;
    int Nplus;
    bool stable;
    bool marginal;       // the locus passes (almost) through -1/K
};

// Everything the Bode / Nichols / Nyquist views need, computed once on a
// logarithmic frequency grid:  compute() fills samples(); margins() and
// nyquist() then work on those samples (plus a few bisection refinements).
class FrequencyResponse {
private:
    TransferFunction G;   // rational part beta(s)/alpha(s)
    double deadTime;      // tau >= 0, the extra factor e^{-s*tau}
    int pointsPerDecade;
    std::vector<FreqSample> data;

    // beta(jw)/alpha(jw) without the dead time
    ComplexNumber rational(double w) const;
    static void unwrapPhase(std::vector<double>& phaseRad);

public:
    FrequencyResponse(const TransferFunction& plant, double deadTime = 0.0);

    // Exact H(jw) at any w (not only on the grid) -- used to refine roots.
    ComplexNumber at(double w) const;

    // Log-spaced grid from wMin to wMax; fills samples().
    void compute(double wMin, double wMax, int pointsPerDecade = 200);
    const std::vector<FreqSample>& samples() const;

    // Continuous phase (degrees) at an arbitrary w, on the same branch as
    // samples() -- so wt / wPi report -217.8 deg, not +142.2 deg.
    double phaseDegAt(double w) const;

    // Frequency range from the moduli of the poles and zeros: two decades
    // below the slowest and two above the fastest (poles/zeros at the origin
    // don't count -- they show up as a slope, not as a corner frequency).
    static std::pair<double, double> autoRange(const TransferFunction& plant, double deadTime = 0.0);

    Margins margins(double K = 1.0) const;
    NyquistVerdict nyquist(double K = 1.0) const;
};

#endif
