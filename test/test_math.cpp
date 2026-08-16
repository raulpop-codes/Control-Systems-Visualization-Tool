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

#define M_PI 3.141592

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

    cout << "\n=== TimeResponse (Euler) vs analytical, H(s)=1/(s+1) ===\n";
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

    return 0;
}
