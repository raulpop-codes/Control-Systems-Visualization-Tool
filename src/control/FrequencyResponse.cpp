#include "control/FrequencyResponse.hpp"
#include "control/Stability.hpp"
#include "math/Constants.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

using namespace std;

static const double RAD_TO_DEG = 180.0 / M_PI;

FrequencyResponse::FrequencyResponse(const TransferFunction& plant, double tau)
    : G(plant), deadTime(tau), pointsPerDecade(200) {}

// beta(jw) / alpha(jw), divided by hand instead of with ComplexNumber's
// operator/: that one refuses denominators with |den|^2 < 1e-12, which a slow
// integrator like 1/(s(s+0.001)) already has at w = 1e-5 (|den| = 1e-8) --
// perfectly fine numbers for a frequency response. Only an exactly zero
// denominator (a pole sitting right on the jw axis at this very w) is a
// problem; it gets a huge finite value instead of a crash.
ComplexNumber FrequencyResponse::rational(double w) const {
    ComplexNumber s(0.0, w);
    ComplexNumber num = G.numerator().evaluate(s);
    ComplexNumber den = G.denominator().evaluate(s);
    double d2 = den.getReal() * den.getReal() + den.getImag() * den.getImag();
    if(d2 == 0.0) return ComplexNumber(1e12, 0.0);
    return ComplexNumber(
        (num.getReal() * den.getReal() + num.getImag() * den.getImag()) / d2,
        (num.getImag() * den.getReal() - num.getReal() * den.getImag()) / d2
    );
}

// H(jw) = beta(jw)/alpha(jw) * e^{-j*tau*w}. The dead time has modulus 1 and
// phase -tau*w, so it doesn't change the magnitude curve, it only rotates.
ComplexNumber FrequencyResponse::at(double w) const {
    ComplexNumber h = rational(w);
    if(deadTime > 0.0) h = h * ComplexNumber(cos(deadTime * w), -sin(deadTime * w));
    return h;
}

// atan2 only gives the phase modulo 360 degrees, so 1/(s+1)^3 would jump from
// -180 to +180 on its way down to -270. As long as two neighbouring points
// differ by less than 180 degrees, the true difference is the one closest to
// zero modulo 2*pi: subtract the nearest multiple of 2*pi. p[i-1] is already
// corrected, so the corrections accumulate along the curve.
void FrequencyResponse::unwrapPhase(vector<double>& p) {
    for(size_t i = 1; i < p.size(); i++){
        double d = p[i] - p[i - 1];
        p[i] -= 2.0 * M_PI * round(d / (2.0 * M_PI));
    }
}

void FrequencyResponse::compute(double wMin, double wMax, int ppd) {
    data.clear();
    if(!(wMin > 0.0) || !(wMax > wMin) || ppd < 1) return;
    pointsPerDecade = ppd;

    double lg0 = log10(wMin), lg1 = log10(wMax);
    int n = max(2, static_cast<int>(ceil((lg1 - lg0) * ppd)) + 1);

    // Logarithmic grid: the same number of points per decade however wide the
    // range is, which is what a Bode plot (log w axis) needs.
    vector<double> w(n), phase(n);
    for(int i = 0; i < n; i++){
        w[i] = pow(10.0, lg0 + (lg1 - lg0) * i / (n - 1));
        // Unwrap the RATIONAL part only. The dead time adds -tau*w radians,
        // which between two neighbouring points at a large w can exceed 180
        // degrees and would fool the unwrapping -- it is subtracted afterwards,
        // analytically.
        phase[i] = rational(w[i]).phase();
    }
    unwrapPhase(phase);

    for(int i = 0; i < n; i++){
        ComplexNumber h = at(w[i]);
        double phi = phase[i] - deadTime * w[i];
        data.push_back({w[i], h, 20.0 * log10(h.magnitude()), phi * RAD_TO_DEG});
    }
}

const vector<FreqSample>& FrequencyResponse::samples() const { return data; }

double FrequencyResponse::phaseDegAt(double w) const {
    if(data.empty()) return rational(w).phase() * RAD_TO_DEG;

    // Last grid point at or below w: its (unwrapped) rational phase tells
    // which 360-degree branch the exact atan2 value below belongs to.
    size_t i = upper_bound(data.begin(), data.end(), w,
                           [](double v, const FreqSample& s){ return v < s.w; }) - data.begin();
    if(i > 0) i--;
    double ratRef = data[i].phaseDeg + deadTime * data[i].w * RAD_TO_DEG;
    double raw = rational(w).phase() * RAD_TO_DEG;
    double rat = raw + 360.0 * round((ratRef - raw) / 360.0);
    return rat - deadTime * w * RAD_TO_DEG;
}

pair<double, double> FrequencyResponse::autoRange(const TransferFunction& plant, double deadTime) {
    vector<double> moduli;
    for(auto& p : plant.poles()) moduli.push_back(p.magnitude());
    for(auto& z : plant.zeros()) moduli.push_back(z.magnitude());
    // Dead time has a characteristic frequency of its own, 1/tau.
    if(deadTime > 0.0) moduli.push_back(1.0 / deadTime);

    // The origin doesn't count: log10(0) = -inf, and a pole "at 0" comes out
    // of Durand-Kerner as something like 4e-11.
    vector<double> useful;
    for(double m : moduli) if(isfinite(m) && m > 1e-6) useful.push_back(m);
    if(useful.empty()) return {1e-2, 1e2}; // e.g. K, 1/s, 1/s^2

    double lo = *min_element(useful.begin(), useful.end());
    double hi = *max_element(useful.begin(), useful.end());

    // Snapped to whole decades so the axis starts and ends on a labelled
    // tick. The 1e-6 keeps a pole at -1 that came out as 0.9999999 from
    // costing an extra decade.
    double decLo = floor(log10(lo) + 1e-6) - 2.0;
    double decHi = ceil(log10(hi) - 1e-6) + 2.0;
    return {pow(10.0, decLo), pow(10.0, decHi)};
}

// Root finding on the already-computed samples, in two steps:
//  1) `values[i]` is f(data[i].w), read from the stored samples (no new
//     evaluations): wherever it changes sign, a root is bracketed in that
//     interval;
//  2) each bracket is refined by bisection with the exact function f.
// Shared by the margins (|H| - 1, Im H) and any other "where does the curve
// cross ..." question.
static vector<double> findCrossings(const vector<FreqSample>& d, const vector<double>& values,
                                    const function<double(double)>& f) {
    vector<double> roots;
    for(size_t i = 0; i + 1 < d.size(); i++){
        double a = values[i], b = values[i + 1];
        if(!isfinite(a) || !isfinite(b)) continue;
        if((a > 0.0) == (b > 0.0)) continue; // same side of zero: no root here

        double lo = d[i].w, hi = d[i + 1].w;
        bool loPositive = f(lo) > 0.0;
        for(int it = 0; it < 60; it++){
            double mid = sqrt(lo * hi); // geometric mean: the grid is logarithmic
            if((f(mid) > 0.0) == loPositive) lo = mid;
            else                              hi = mid;
        }
        roots.push_back(sqrt(lo * hi));
    }
    return roots;
}

Margins FrequencyResponse::margins(double K) const {
    Margins m;
    m.K = K;
    if(data.size() < 2 || !(K > 0.0)) return m;

    size_t n = data.size();
    double kDb = 20.0 * log10(K);
    vector<double> db(n), im(n);
    for(size_t i = 0; i < n; i++){
        db[i] = data[i].magDb + kDb;
        im[i] = data[i].H.getImag(); // K > 0 doesn't change the sign of Im
    }

    // Gain crossover: |K*H(jw)| = 1  <=>  0 dB. More than one is possible;
    // the one with the smallest phase margin is the one that matters.
    auto wts = findCrossings(data, db, [&](double w){ return 20.0 * log10(K * at(w).magnitude()); });
    for(double w : wts){
        double g = fmod(180.0 + phaseDegAt(w), 360.0);
        if(g > 180.0)   g -= 360.0;
        if(g <= -180.0) g += 360.0;
        if(!m.phaseMargin || g < *m.phaseMargin){
            m.wt = w;
            m.phaseMargin = g;
        }
    }

    // Phase crossover: angle = -180 (mod 360)  <=>  Im H = 0 with Re H < 0
    // (Im H = 0 with Re H > 0 is the positive real axis, not the critical
    // point). With dead time there are infinitely many; m_k shrinks as w
    // grows, so the biggest m_k -- the first crossing -- is the one that matters.
    auto cands = findCrossings(data, im, [&](double w){ return at(w).getImag(); });
    for(double w : cands){
        ComplexNumber h = at(w);
        if(h.getReal() >= 0.0) continue;
        double mk = K * h.magnitude();
        if(!m.gainMargin || mk > *m.gainMargin){
            m.wPi = w;
            m.gainMargin = mk;
        }
    }
    return m;
}

NyquistVerdict FrequencyResponse::nyquist(double K) const {
    NyquistVerdict v{0.0, 0.0, 0, 0, false, false};
    if(data.empty() || !(K > 0.0)) return v;

    // Continuous angle of the vector from -1/K to the locus, i.e. of 1 + K*H.
    // Same unwrapping as the Bode phase.
    vector<double> angle;
    double closest = numeric_limits<double>::max();
    auto addPoint = [&](const ComplexNumber& h) {
        ComplexNumber t = ComplexNumber(1.0) + h * ComplexNumber(K);
        angle.push_back(t.phase());
        closest = min(closest, t.magnitude());
    };
    for(auto& s : data) addPoint(s.H);

    // The grid ends at a finite w. If K*|H| is still not small there (a big
    // K), the angle has not reached its final value yet: keep going until the
    // locus has collapsed onto the origin.
    double w = data.back().w, ratio = pow(10.0, 1.0 / pointsPerDecade);
    while(w < 1e12){
        w *= ratio;
        ComplexNumber h = at(w);
        if(K * h.magnitude() <= 1e-3) break;
        addPoint(h);
    }
    unwrapPhase(angle);
    v.deltaDeg = (angle.back() - angle.front()) * RAD_TO_DEG;

    // N0: leading zero coefficients of alpha(s) (poles at the origin).
    // N+: the Routh table of alpha(s) -- no root finding needed.
    const auto& c = G.denominator().coefficients();
    while(v.N0 < static_cast<int>(c.size()) && fabs(c[v.N0]) < 1e-12) v.N0++;
    v.Nplus = Stability::routhHurwitz(G.denominator()).rightHalfPlaneRoots;

    v.requiredDeg = 90.0 * (v.N0 + 2 * v.Nplus);
    v.marginal = closest < 1e-3;
    // The variation always comes in multiples of 90 degrees, so "close
    // enough" is anything within half of that.
    v.stable = !v.marginal && fabs(v.deltaDeg - v.requiredDeg) < 45.0;
    return v;
}
