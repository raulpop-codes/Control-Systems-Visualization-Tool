// Validation suite for the math and control layers.
//
// Every reference value below comes either from closed-form analysis or from
// the worked examples in the course guides (Indrumar TS1). Each check prints
// what it got next to what it expected, so the output is still readable by
// hand -- but a mismatch now also sets the exit code, which is what CI reads.
//
// Build and run:  make test_math && ./test_math.exe
// No SFML dependency: this always builds, even without SFML installed.

#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>
#include <vector>
#include <sstream>
#include "math/Matrix.hpp"
#include "math/Polynomial.hpp"
#include "control/TransferFunction.hpp"
#include "control/Stability.hpp"
#include "control/RootLocus.hpp"
#include "control/StateSpace.hpp"
#include "control/Conversions.hpp"
#include "control/TimeResponse.hpp"
#include "control/FrequencyResponse.hpp"
#include "math/Constants.hpp"

using namespace std;

// ---------------------------------------------------------------- harness --

static int checksRun = 0;
static int checksFailed = 0;

static void verdict(bool ok, const string& what, const string& got, const string& want) {
    ++checksRun;
    if (!ok) ++checksFailed;
    cout << (ok ? "  [ok]   " : "  [FAIL] ") << left << setw(46) << what << right
         << "  got " << setw(26) << got;
    if (!ok) cout << "   want " << want;
    cout << "\n";
}

static string num(double v) {
    ostringstream os;
    os << fixed << setprecision(4) << v;
    return os.str();
}

static void checkNear(const string& what, double got, double want, double tol = 1e-3) {
    verdict(fabs(got - want) <= tol, what, num(got), num(want));
}

static void checkInt(const string& what, int got, int want) {
    verdict(got == want, what, to_string(got), to_string(want));
}

static void checkBool(const string& what, bool got, bool want) {
    verdict(got == want, what, got ? "true" : "false", want ? "true" : "false");
}

// Compares two root sets regardless of ordering.
static void checkRoots(const string& what, const vector<ComplexNumber>& got,
                       const vector<ComplexNumber>& want, double tol = 1e-3) {
    bool ok = got.size() == want.size();
    if (ok) {
        vector<bool> used(want.size(), false);
        for (const auto& g : got) {
            bool matched = false;
            for (size_t i = 0; i < want.size() && !matched; ++i) {
                if (used[i]) continue;
                if (fabs(g.getReal() - want[i].getReal()) <= tol &&
                    fabs(g.getImag() - want[i].getImag()) <= tol) {
                    used[i] = true;
                    matched = true;
                }
            }
            if (!matched) { ok = false; break; }
        }
    }
    ostringstream g, w;
    g << fixed << setprecision(3);
    for (const auto& c : got) g << c << " ";
    w << fixed << setprecision(3);
    for (const auto& c : want) w << c << " ";
    verdict(ok, what, g.str(), w.str());
}

static void checkCoeffs(const string& what, const vector<double>& got,
                        const vector<double>& want, double tol = 1e-3) {
    bool ok = got.size() == want.size();
    for (size_t i = 0; ok && i < got.size(); ++i)
        if (fabs(got[i] - want[i]) > tol) ok = false;
    ostringstream g, w;
    g << fixed << setprecision(3);
    for (double v : got) g << v << " ";
    w << fixed << setprecision(3);
    for (double v : want) w << v << " ";
    verdict(ok, what, g.str(), w.str());
}

static void section(const string& title) {
    cout << "\n=== " << title << " ===\n";
}

static FrequencyResponse bodeOf(const TransferFunction& G, double tau = 0.0) {
    auto range = FrequencyResponse::autoRange(G, tau);
    FrequencyResponse fr(G, tau);
    fr.compute(range.first, range.second);
    return fr;
}

// All margins reported the way the course does (m_k, not MATLAB's 1/m_k).
static void checkMargins(const string& label, const FrequencyResponse& fr, double K,
                         double wt, double gammaK, double wPi, double mK,
                         double kCrit, bool stable) {
    Margins m = fr.margins(K);
    verdict(m.wt.has_value(), label + ": wt present", m.wt ? "yes" : "no", "yes");
    verdict(m.wPi.has_value(), label + ": w_-pi present", m.wPi ? "yes" : "no", "yes");
    if (!m.wt || !m.wPi) return;
    checkNear(label + ": wt", *m.wt, wt);
    checkNear(label + ": gamma_k (deg)", *m.phaseMargin, gammaK, 1e-2);
    checkNear(label + ": w_-pi", *m.wPi, wPi);
    checkNear(label + ": m_k", *m.gainMargin, mK);
    checkNear(label + ": K_critic", *m.criticalGain(), kCrit);
    checkBool(label + ": stable", m.stable(), stable);
}

// The point of the Nyquist criterion here: its verdict has to agree with
// Routh-Hurwitz, which is computed by a completely independent route.
static void checkNyquistVsRouth(const string& label, const FrequencyResponse& fr,
                                const TransferFunction& G, double K, bool stable) {
    NyquistVerdict v = fr.nyquist(K);
    bool routh = Stability::routhHurwitz(G.closedLoopCharacteristicPolynomial(K)).stable;
    checkBool(label + ": Nyquist verdict", v.stable, stable);
    checkBool(label + ": Routh agrees with Nyquist", routh, v.stable);
    cout << "          delta arg=" << num(v.deltaDeg) << " deg, required "
         << num(v.requiredDeg) << " (N0=" << v.N0 << ", N+=" << v.Nplus << ")\n";
}

// ------------------------------------------------------------------ tests --

int main() {
    cout << fixed << setprecision(4);
    cout << "Validation suite -- math and control layers\n";

    section("Matrix: determinant");
    Matrix m2(2, 2);
    m2.at(0,0)=1; m2.at(0,1)=2; m2.at(1,0)=3; m2.at(1,1)=4;
    checkNear("2x2 determinant", m2.determinant(), -2.0);

    Matrix m3(3, 3);
    double vals[3][3] = {{6,1,1},{4,-2,5},{2,8,7}};
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m3.at(i,j) = vals[i][j];
    checkNear("3x3 determinant", m3.determinant(), -306.0);

    section("Polynomial::roots (Durand-Kerner)");
    checkRoots("s^2-5s+6", Polynomial({6,-5,1}).roots(),
               {ComplexNumber(2,0), ComplexNumber(3,0)});
    checkRoots("s^2+1", Polynomial({1,0,1}).roots(),
               {ComplexNumber(0,1), ComplexNumber(0,-1)});

    section("Repeated roots do not throw");
    checkRoots("(s+1)^2", Polynomial({1,2,1}).roots(),
               {ComplexNumber(-1,0), ComplexNumber(-1,0)});
    checkRoots("s(s+2)^2", Polynomial({0,4,4,1}).roots(),
               {ComplexNumber(0,0), ComplexNumber(-2,0), ComplexNumber(-2,0)});

    section("Stability::routhHurwitz vs Indrumar TS1, 7.4");
    // lambda^4 - 0.5*lambda^2 + 1/16 -- the book's worked example.
    RouthResult routh = Stability::routhHurwitz(Polynomial({1.0/16, 0, -0.5, 0, 1}));
    for (const auto& row : routh.table) {
        cout << "         ";
        for (double v : row) cout << setw(10) << v;
        cout << "\n";
    }
    checkInt ("right-half-plane roots", routh.rightHalfPlaneRoots, 2);
    checkBool("stable", routh.stable, false);
    checkBool("marginally stable", routh.marginallyStable, false);

    section("TransferFunction + RootLocus vs Indrumar TS1, exemplul (e)");
    // H(s) = 1 / (s(s+2)(s+4))
    Polynomial den = Polynomial({0.0,1.0}) * Polynomial({2.0,1.0}) * Polynomial({4.0,1.0});
    TransferFunction G(Polynomial({1.0}), den);
    RootLocus rl(G);

    checkRoots("open-loop poles", G.poles(),
               {ComplexNumber(0,0), ComplexNumber(-2,0), ComplexNumber(-4,0)});
    checkNear ("asymptote centroid", rl.asymptoteCentroid(), -2.0);

    vector<double> anglesDeg;
    for (double a : rl.asymptoteAngles()) anglesDeg.push_back(a * 180.0 / M_PI);
    checkCoeffs("asymptote angles (deg)", anglesDeg, {60.0, 180.0, 300.0});

    double kc = 0.0;
    bool found = rl.findCriticalK(200.0, kc);
    checkBool("critical K found", found, true);
    if (found) checkNear("critical K (book: 48)", kc, 48.0, 1e-2);

    // Below the critical gain every closed-loop pole must sit in the left half-plane.
    int unstableAt10 = 0;
    for (const auto& p : G.closedLoopPoles(10.0)) if (p.getReal() > 0) ++unstableAt10;
    checkInt("closed-loop poles with Re>0 at K=10", unstableAt10, 0);

    int unstableAt100 = 0;
    for (const auto& p : G.closedLoopPoles(100.0)) if (p.getReal() > 0) ++unstableAt100;
    checkInt("closed-loop poles with Re>0 at K=100", unstableAt100, 2);

    section("StateSpace::characteristicPolynomial (Faddeev-LeVerrier)");
    Matrix A(2, 2);
    A.at(0,0)=0; A.at(0,1)=1; A.at(1,0)=-2; A.at(1,1)=-3;
    Matrix B(2, 1); B.at(1,0) = 1;
    Matrix C(1, 2); C.at(0,0) = 1;
    Matrix D(1, 1);
    Polynomial charPoly = StateSpace(A, B, C, D).characteristicPolynomial();
    checkCoeffs("companion of (s+1)(s+2), ascending", charPoly.coefficients(), {2.0, 3.0, 1.0});
    checkRoots ("its roots", charPoly.roots(), {ComplexNumber(-1,0), ComplexNumber(-2,0)});

    section("Conversions: TF -> StateSpace (FCC) -> TF round trip");
    TransferFunction backToTf = Conversions::toTransferFunction(Conversions::toStateSpace(G));
    checkCoeffs("round-trip numerator", backToTf.numerator().coefficients(), {1.0});
    checkCoeffs("round-trip denominator", backToTf.denominator().coefficients(), {0.0, 8.0, 6.0, 1.0});

    section("TimeResponse (RK4) vs analytical, H(s)=1/(s+1)");
    Matrix Af(1,1); Af.at(0,0) = -1;
    Matrix Bf(1,1); Bf.at(0,0) = 1;
    Matrix Cf(1,1); Cf.at(0,0) = 1;
    Matrix Df(1,1);
    StateSpace firstOrder(Af, Bf, Cf, Df);

    auto stepFine    = TimeResponse::step(firstOrder, 3.0, 0.001);
    auto impulseFine = TimeResponse::impulse(firstOrder, 3.0, 0.001);
    checkNear("step at t=2, dt=0.001",    stepFine[2000].y,    1.0 - exp(-2.0), 1e-5);
    checkNear("impulse at t=2, dt=0.001", impulseFine[2000].y, exp(-2.0),       1e-5);

    // 100x coarser: forward Euler would land near 0.8784 instead of 0.8647.
    auto stepCoarse = TimeResponse::step(firstOrder, 3.0, 0.1);
    checkNear("step at t=2, dt=0.1 (RK4 stays accurate)", stepCoarse[20].y, 1.0 - exp(-2.0), 1e-4);

    section("FrequencyResponse: Bode, phase unwrapping, automatic range");
    TransferFunction lead(Polynomial({40.0, 2.0}), Polynomial({70.0, 1.0})); // 2(s+20)/(s+70)
    FrequencyResponse frLead = bodeOf(lead);
    ComplexNumber h1   = frLead.at(1.0);
    ComplexNumber h300 = frLead.at(300.0);
    checkNear("2(s+20)/(s+70) at w=1: magnitude (dB)",   20.0 * log10(h1.magnitude()),   -4.8508, 1e-3);
    checkNear("2(s+20)/(s+70) at w=1: phase (deg)",      h1.phase() * 180.0 / M_PI,       2.0439, 1e-3);
    checkNear("2(s+20)/(s+70) at w=300: magnitude (dB)", 20.0 * log10(h300.magnitude()),  5.8096, 1e-3);
    checkNear("2(s+20)/(s+70) at w=300: phase (deg)",    h300.phase() * 180.0 / M_PI,     9.3199, 1e-3);

    // Continuous phase: atan2 alone would report +91.7 here instead of -268.3.
    TransferFunction triple(Polynomial({1.0}), Polynomial({1.0, 3.0, 3.0, 1.0})); // 1/(s+1)^3
    FrequencyResponse frTriple(triple);
    frTriple.compute(1e-2, 1e2);
    checkNear("1/(s+1)^3 unwrapped phase at w=100 (deg)",
              frTriple.samples().back().phaseDeg, -268.2812, 1e-2);

    auto range = FrequencyResponse::autoRange(G);
    checkNear("autoRange lower bound", range.first,  0.01,   1e-6);
    checkNear("autoRange upper bound", range.second, 1000.0, 1e-6);

    section("Margins (course convention)");
    FrequencyResponse frBook = bodeOf(G);
    // 1/(s(s+2)(s+4)): w_-pi = sqrt(8), m_k = 1/48, K_critic = 48 -- same 48 the root locus found.
    checkMargins("1/(s(s+2)(s+4)) K=1", frBook, 1.0,
                 0.1247, 84.6467, sqrt(8.0), 1.0/48.0, 48.0, true);

    TransferFunction nmp(Polynomial({2.0, -1.0}), Polynomial({0.0, 4.0, 4.0, 1.0})); // (-s+2)/(s(s+2)^2)
    FrequencyResponse frNmp = bodeOf(nmp);
    checkMargins("(-s+2)/(s(s+2)^2) K=1", frNmp, 1.0,
                 0.4859, 49.0362, 2.0/sqrt(3.0), 0.375, 8.0/3.0, true);
    checkMargins("(-s+2)/(s(s+2)^2) K=5", frNmp, 5.0,
                 1.8399, -37.8367, 2.0/sqrt(3.0), 1.875, 8.0/3.0, false);

    TransferFunction nmp2(Polynomial({4.0, -2.0}), Polynomial({0.0, 6.0, 1.0})); // -2(s-2)/(s(s+6))
    checkMargins("-2(s-2)/(s(s+6)) K=1", bodeOf(nmp2), 1.0,
                 0.7017, 63.9952, 2.0*sqrt(3.0), 1.0/3.0, 3.0, true);

    // Dead time: wt > w_-pi, so the loop is unstable despite a positive gain margin reading.
    TransferFunction delayed(Polynomial({72.0, 8.0}), Polynomial({0.0, 50.0, 1.0})); // 8(s+9)/(s(s+50)) e^{-2s}
    FrequencyResponse frDelayed = bodeOf(delayed, 2.0);
    checkMargins("8(s+9)/(s(s+50)) e^-2s K=1", frDelayed, 1.0,
                 1.4582, -69.5600, 0.8228, 1.7573, 0.5691, false);

    section("Nyquist criterion, cross-checked against Routh-Hurwitz");
    // (-s+2)/(s(s+2)^2) is stable for K < 8/3.
    checkNyquistVsRouth("(-s+2)/(s(s+2)^2) K=1.0", frNmp, nmp, 1.0, true);
    checkNyquistVsRouth("(-s+2)/(s(s+2)^2) K=2.5", frNmp, nmp, 2.5, true);
    checkNyquistVsRouth("(-s+2)/(s(s+2)^2) K=2.8", frNmp, nmp, 2.8, false);
    checkNyquistVsRouth("(-s+2)/(s(s+2)^2) K=5.0", frNmp, nmp, 5.0, false);

    // 1/(s(s+2)(s+4)) is stable for K < 48.
    checkNyquistVsRouth("1/(s(s+2)(s+4)) K=10",  frBook, G, 10.0,  true);
    checkNyquistVsRouth("1/(s(s+2)(s+4)) K=100", frBook, G, 100.0, false);

    // An open-loop unstable plant: 1/(s-1) needs K > 1, so N+=1 and 180 deg are required.
    TransferFunction unstableOl(Polynomial({1.0}), Polynomial({-1.0, 1.0}));
    FrequencyResponse frUnst = bodeOf(unstableOl);
    checkNyquistVsRouth("1/(s-1) K=0.5", frUnst, unstableOl, 0.5, false);
    checkNyquistVsRouth("1/(s-1) K=3.0", frUnst, unstableOl, 3.0, true);

    // With dead time there is no finite characteristic polynomial, so Nyquist stands alone.
    NyquistVerdict delayedVerdict = frDelayed.nyquist(1.0);
    checkBool("8(s+9)/(s(s+50)) e^-2s K=1: Nyquist verdict (no Routh possible)",
              delayedVerdict.stable, false);

    // ------------------------------------------------------------ summary --
    cout << "\n" << string(72, '-') << "\n";
    if (checksFailed == 0)
        cout << "All " << checksRun << " checks passed.\n";
    else
        cout << checksFailed << " of " << checksRun << " checks FAILED.\n";

    return checksFailed == 0 ? 0 : 1;
}
