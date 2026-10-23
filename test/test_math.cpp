#include <iostream>
#include <iomanip>
#include <cmath>
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

static void printRoots(const vector<ComplexNumber>& r) {
    for (const auto& c : r) cout << "  " << c << "\n";
}

static void printRouth(const RouthResult& res) {
    for (const auto& row : res.table) {
        for (double v : row) cout << setw(10) << v;
        cout << "\n";
    }
    cout << "RHP roots: " << res.rightHalfPlaneRoots
         << "  stable: " << (res.stable ? "yes" : "no")
         << "  marginal: " << (res.marginallyStable ? "yes" : "no") << "\n\n";
}


// Prints the margins the way the course reports them (m_k, not MATLAB's 1/m_k).
static void printMargins(const FrequencyResponse& fr, double K) {
    Margins m = fr.margins(K);
    cout << "  K=" << K << ":  ";
    if(m.wt) cout << "wt=" << *m.wt << "  gamma_k=" << *m.phaseMargin << " deg  ";
    else     cout << "wt=none  ";
    if(m.wPi) cout << "w_-pi=" << *m.wPi << "  m_k=" << *m.gainMargin
                   << " (" << 20.0 * log10(*m.gainMargin) << " dB)  ";
    else      cout << "w_-pi=none  ";
    if(m.criticalGain()) cout << "Kcrit=" << *m.criticalGain();
    cout << "  -> " << (m.stable() ? "stable" : "unstable") << "\n";
}

static void printNyquist(const FrequencyResponse& fr, const TransferFunction& G, double K, bool withRouth) {
    NyquistVerdict v = fr.nyquist(K);
    cout << "  K=" << K << ":  delta arg=" << v.deltaDeg << " deg, required " << v.requiredDeg
         << " (N0=" << v.N0 << ", N+=" << v.Nplus << ") -> " << (v.stable ? "stable" : "unstable");
    if(withRouth)
        cout << "   | Routh: " << (Stability::routhHurwitz(G.closedLoopCharacteristicPolynomial(K)).stable ? "stable" : "unstable");
    cout << "\n";
}

static FrequencyResponse bodeOf(const TransferFunction& G, double tau = 0.0) {
    auto range = FrequencyResponse::autoRange(G, tau);
    FrequencyResponse fr(G, tau);
    fr.compute(range.first, range.second);
    return fr;
}

int main() {
    cout << fixed << setprecision(4);

    cout << "=== Matrix: determinant ===\n";
    Matrix m2(2, 2);
    m2.at(0,0)=1; m2.at(0,1)=2; m2.at(1,0)=3; m2.at(1,1)=4;
    cout << "2x2 det (expect -2): " << m2.determinant() << "\n";

    Matrix m3(3, 3);
    double vals[3][3] = {{6,1,1},{4,-2,5},{2,8,7}};
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m3.at(i,j) = vals[i][j];
    cout << "3x3 det (expect -306): " << m3.determinant() << "\n\n";

    cout << "=== Polynomial::roots (Durand-Kerner) ===\n";
    cout << "s^2-5s+6 (expect 2, 3):\n";
    printRoots(Polynomial({6,-5,1}).roots());
    cout << "s^2+1 (expect +-i):\n";
    printRoots(Polynomial({1,0,1}).roots());

    cout << "\n=== Stability::routhHurwitz vs Indrumar TS1, sectiunea 7.4 ===\n";
    cout << "lambda^4 - 0.5*lambda^2 + 1/16 (expect table to match the book exactly, 2 RHP roots):\n";
    printRouth(Stability::routhHurwitz(Polynomial({1.0/16, 0, -0.5, 0, 1})));

    cout << "=== TransferFunction + RootLocus vs Indrumar TS1, exemplul (e), 11.2/11.3 ===\n";
    cout << "H(s) = 1 / (s(s+2)(s+4))\n";
    Polynomial num({1.0});
    Polynomial den = Polynomial({0.0,1.0}) * Polynomial({2.0,1.0}) * Polynomial({4.0,1.0});
    TransferFunction G(num, den);
    RootLocus rl(G);

    cout << "poles (expect 0, -2, -4):\n";
    printRoots(G.poles());
    cout << "asymptote centroid (expect -2): " << rl.asymptoteCentroid() << "\n";
    cout << "asymptote angles in degrees (expect 60, 180, 300):\n";
    for (double a : rl.asymptoteAngles()) cout << "  " << a * 180.0 / M_PI << "\n";

    double kc;
    if (rl.findCriticalK(200.0, kc))
        cout << "critical K (book says 48): " << kc << "\n";
    else
        cout << "no critical K found in range\n";

    cout << "\nclosed-loop poles, K=10 (< 48, expect all Re<0):\n";
    printRoots(G.closedLoopPoles(10.0));
    cout << "closed-loop poles, K=100 (> 48, expect two with Re>0):\n";
    printRoots(G.closedLoopPoles(100.0));

    cout << "\n=== StateSpace::characteristicPolynomial (Faddeev-LeVerrier) ===\n";
    Matrix A(2, 2);
    A.at(0,0)=0; A.at(0,1)=1; A.at(1,0)=-2; A.at(1,1)=-3;
    Matrix B(2, 1); B.at(1,0) = 1;
    Matrix C(1, 2); C.at(0,0) = 1;
    Matrix D(1, 1);
    StateSpace ss(A, B, C, D);
    Polynomial charPoly = ss.characteristicPolynomial();
    cout << "companion of (s+1)(s+2): coeffs ascending (expect 2,3,1): ";
    for (double c : charPoly.coefficients()) cout << c << " ";
    cout << "\nroots (expect -1,-2):\n";
    printRoots(charPoly.roots());

    cout << "\n=== Conversions: TF -> StateSpace (FCC) -> TF round trip ===\n";
    StateSpace ssFromTf = Conversions::toStateSpace(G);
    TransferFunction backToTf = Conversions::toTransferFunction(ssFromTf);
    cout << "round-trip numerator (expect [1]): ";
    for (double c : backToTf.numerator().coefficients()) cout << c << " ";
    cout << "\nround-trip denominator (expect [0,8,6,1]): ";
    for (double c : backToTf.denominator().coefficients()) cout << c << " ";
    cout << "\n";

    cout << "\n=== TimeResponse (RK4) vs analytical, H(s)=1/(s+1) ===\n";
    Matrix Af(1,1); Af.at(0,0) = -1;
    Matrix Bf(1,1); Bf.at(0,0) = 1;
    Matrix Cf(1,1); Cf.at(0,0) = 1;
    Matrix Df(1,1);
    StateSpace firstOrder(Af, Bf, Cf, Df);
    auto stepSamples = TimeResponse::step(firstOrder, 3.0, 0.001);
    auto impulseSamples = TimeResponse::impulse(firstOrder, 3.0, 0.001);
    int idxAt2 = 2000; // t = 2.0 at dt = 0.001
    cout << "step at t=2 (expect ~" << (1.0 - exp(-2.0)) << "): " << stepSamples[idxAt2].y << "\n";
    cout << "impulse at t=2 (expect ~" << exp(-2.0) << "): " << impulseSamples[idxAt2].y << "\n";

    // A step 100x coarser: forward Euler would be off in the 2nd decimal
    // here (about 0.8784 instead of 0.8647); RK4 stays accurate.
    auto coarseSamples = TimeResponse::step(firstOrder, 3.0, 0.1);
    cout << "step at t=2, dt=0.1 (expect ~" << (1.0 - exp(-2.0)) << "): " << coarseSamples[20].y << "\n";

    cout << "\n=== Repeated roots no longer throw ===\n";
    cout << "(s+1)^2 (expect -1, -1):\n";
    printRoots(Polynomial({1,2,1}).roots());
    cout << "s(s+2)^2 (expect 0, -2, -2):\n";
    printRoots(Polynomial({0,4,4,1}).roots());

    cout << "\n=== FrequencyResponse: Bode, phase unwrapping, automatic range ===\n";
    TransferFunction lead(Polynomial({40.0, 2.0}), Polynomial({70.0, 1.0})); // 2(s+20)/(s+70)
    FrequencyResponse frLead = bodeOf(lead);
    for(double w : {1.0, 300.0}){
        ComplexNumber h = frLead.at(w);
        cout << "2(s+20)/(s+70) at w=" << w << ": " << 20.0 * log10(h.magnitude()) << " dB, "
             << h.phase() * 180.0 / M_PI << " deg  (expect -4.85 dB, 2.0 deg  /  5.81 dB, 9.3 deg)\n";
    }
    TransferFunction triple(Polynomial({1.0}), Polynomial({1.0, 3.0, 3.0, 1.0})); // 1/(s+1)^3
    FrequencyResponse frTriple(triple);
    frTriple.compute(1e-2, 1e2);
    cout << "1/(s+1)^3 phase at w=100 (expect about -268.3, continuous -- atan2 alone would give +91.7): "
         << frTriple.samples().back().phaseDeg << " deg\n";
    auto range = FrequencyResponse::autoRange(G);
    cout << "autoRange of 1/(s(s+2)(s+4)) (expect 0.01 .. 1000): " << range.first << " .. " << range.second << "\n";

    cout << "\n=== Margins (course convention) ===\n";
    cout << "1/(s(s+2)(s+4)): expect w_-pi=2.8284 (=sqrt 8), m_k=1/48=0.02083, Kcrit=48 (same as the root locus)\n";
    FrequencyResponse frBook = bodeOf(G);
    printMargins(frBook, 1.0);

    TransferFunction nmp(Polynomial({2.0, -1.0}), Polynomial({0.0, 4.0, 4.0, 1.0})); // (-s+2)/(s(s+2)^2)
    FrequencyResponse frNmp = bodeOf(nmp);
    cout << "(-s+2)/(s(s+2)^2): K=1 expect wt=0.4859 gamma_k=49.0 w_-pi=1.1547 m_k=0.375 (-8.52 dB) Kcrit=2.667\n"
         << "                   K=5 expect wt=1.8399 gamma_k=-37.8 m_k=1.875 (5.46 dB) -> unstable\n";
    printMargins(frNmp, 1.0);
    printMargins(frNmp, 5.0);

    TransferFunction nmp2(Polynomial({4.0, -2.0}), Polynomial({0.0, 6.0, 1.0})); // -2(s-2)/(s(s+6))
    cout << "-2(s-2)/(s(s+6)): expect wt=0.7017 gamma_k=64.0 w_-pi=3.4641 (=2 sqrt 3) m_k=0.3333 (-9.54 dB)\n";
    printMargins(bodeOf(nmp2), 1.0);

    TransferFunction delayed(Polynomial({72.0, 8.0}), Polynomial({0.0, 50.0, 1.0})); // 8(s+9)/(s(s+50)) e^{-2s}
    cout << "8(s+9)/(s(s+50)) e^{-2s}: expect wt=1.4582 gamma_k=-69.6 first w_-pi=0.8228 m_k=1.757 -> unstable (wt > w_-pi)\n";
    printMargins(bodeOf(delayed, 2.0), 1.0);

    cout << "\n=== Nyquist: counting encirclements, cross-checked with Routh-Hurwitz ===\n";
    cout << "(-s+2)/(s(s+2)^2): stable for K < 8/3, required delta = 90 deg\n";
    printNyquist(frNmp, nmp, 1.0, true);
    printNyquist(frNmp, nmp, 2.5, true);
    printNyquist(frNmp, nmp, 2.8, true);
    printNyquist(frNmp, nmp, 5.0, true);
    cout << "1/(s(s+2)(s+4)): stable for K < 48\n";
    printNyquist(frBook, G, 10.0, true);
    printNyquist(frBook, G, 100.0, true);
    TransferFunction unstableOl(Polynomial({1.0}), Polynomial({-1.0, 1.0})); // 1/(s-1): needs K > 1
    cout << "1/(s-1) (N+=1, required 180 deg): stable for K > 1\n";
    FrequencyResponse frUnst = bodeOf(unstableOl);
    printNyquist(frUnst, unstableOl, 0.5, true);
    printNyquist(frUnst, unstableOl, 3.0, true);
    cout << "8(s+9)/(s(s+50)) e^{-2s} (no Routh with dead time): ";
    printNyquist(bodeOf(delayed, 2.0), delayed, 1.0, false);

    return 0;
}
