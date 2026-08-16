#include <iostream>
#include <iomanip>
#include <optional>
#include <limits>
#include <cmath>
#include "math/Matrix.hpp"
#include "math/Polynomial.hpp"
#include "control/TransferFunction.hpp"
#include "control/StateSpace.hpp"
#include "control/Conversions.hpp"
#include "control/Stability.hpp"
#include "control/RootLocus.hpp"
#include "control/TimeResponse.hpp"
#include "view/RootLocusWindow.hpp"
#include "view/TimeResponseWindow.hpp"

#include "math/Constants.hpp"

using namespace std;

static Matrix readMatrix(int rows, int cols, const string& name) {
    Matrix m(rows, cols);
    cout << "Enter " << name << " (" << rows << "x" << cols << "), row by row:\n";
    for(int i = 0; i < rows; i++){
        for(int j = 0; j < cols; j++){
            cout << "  " << name << "(" << i << "," << j << ") = ";
            cin >> m.at(i, j);
        }
    }
    return m;
}

// Reads coefficients in descending order [a_n, ..., a_0] (the natural way
// most people would read them off a page) and returns the ascending-order
// Polynomial our classes expect.
static Polynomial readPolynomialDescending(int degree, const string& name) {
    vector<double> descending(degree + 1);
    cout << "Enter " << name << " coefficients, highest power first "
         << "(degree " << degree << " down to 0):\n";
    for(int i = 0; i <= degree; i++){
        cout << "  coefficient of s^" << (degree - i) << " = ";
        cin >> descending[i];
    }
    vector<double> ascending(descending.rbegin(), descending.rend());
    return Polynomial(ascending);
}

static void printRoots(const vector<ComplexNumber>& roots) {
    for(auto& r : roots) cout << "    " << r << "\n";
}

static void runStabilityCheck(optional<StateSpace>& ss, optional<TransferFunction>& tf) {
    RouthResult result;
    if(ss.has_value()){
        cout << "Checking stability from the state matrix A\n";
        result = Stability::routhHurwitz(ss->characteristicPolynomial());
    } else {
        cout << "Checking stability from the denominator alpha(s)\n";
        result = Stability::routhHurwitz(tf->denominator());
    }

    cout << "Routh table:\n";
    for(auto& row : result.table){
        for(double v : row) cout << setw(10) << v;
        cout << "\n";
    }
    cout << "Right-half-plane roots: " << result.rightHalfPlaneRoots << "\n";
    if(result.marginallyStable) cout << "Verdict: MARGINALLY STABLE (poles on the imaginary axis)\n";
    else if(result.stable) cout << "Verdict: STABLE\n";
    else cout << "Verdict: UNSTABLE\n";
}

static void runTimeResponse(optional<StateSpace>& ss, optional<TransferFunction>& tf, bool isStep) {
    if(!ss.has_value()){
        cout << "Converting transfer function to state-space via FCC...\n";
        ss = Conversions::toStateSpace(*tf);
    }

    double tFinal, dt;
    cout << "Simulation length (seconds)? ";
    cin >> tFinal;
    cout << "Time step dt (try something well under 1/(10*|fastest pole|), e.g. 0.01)? ";
    cin >> dt;

    auto samples = isStep ? TimeResponse::step(*ss, tFinal, dt) : TimeResponse::impulse(*ss, tFinal, dt);

    cout << "Opening " << (isStep ? "step" : "impulse") << " response plot "
            "(close it to return to the menu)...\n";
    showTimeResponsePlot(samples, isStep ? "Step Response" : "Impulse Response");
}

static void runRootLocus(optional<StateSpace>& ss, optional<TransferFunction>& tf) {
    if(!tf.has_value()){
        cout << "Converting state-space to transfer function via Faddeev-LeVerrier...\n";
        tf = Conversions::toTransferFunction(*ss);
    }

    RootLocus rl(*tf);
    cout << "Open-loop poles:\n";
    printRoots(tf->poles());
    cout << "Open-loop zeros:\n";
    printRoots(tf->zeros());
    cout << "Asymptote centroid: " << rl.asymptoteCentroid() << "\n";
    cout << "Asymptote angles (degrees): ";
    for(double a : rl.asymptoteAngles()) cout << (a * 180.0 / M_PI) << "  ";
    cout << "\n";

    double kMax;
    cout << "Search for the critical gain up to K = ? ";
    cin >> kMax;
    double kc;
    if(rl.findCriticalK(kMax, kc))
        cout << "Critical gain: K = " << kc << " (Routh-Hurwitz boundary)\n";
    else
        cout << "No stability-crossing gain found in [0, " << kMax << "]\n";

    double k;
    cout << "Show closed-loop poles at K = ? ";
    cin >> k;
    cout << "Closed-loop poles at K=" << k << ":\n";
    printRoots(tf->closedLoopPoles(k));
     cout << "Opening root locus plot window (close it to return to the menu)...\n";
    // Sweep the locus up to the same kMax used for the critical-gain search
    // above, and start the slider at the K the user just asked about.
     showRootLocusPlot(*tf, kMax, k);
}

int main() {
    cout << "=== Control Systems Analysis ===\n\n";

    optional<StateSpace> ss;
    optional<TransferFunction> tf;

    bool running = true;
    while(running){
        cout << "How do you want to describe the system?\n";
        cout << "  1) State-space matrices (A, B, C, D)\n";
        cout << "  2) Transfer function coefficients\n";
        cout << "  Other) Exit\n";

        cout << "Choice: ";
        int inputChoice;
        cin >> inputChoice;

        if(inputChoice == 1){
            int n;
            cout << "System order (number of state variables), n = ";
            cin >> n;
            Matrix A = readMatrix(n, n, "A");
            Matrix B = readMatrix(n, 1, "B");
            Matrix C = readMatrix(1, n, "C");
            Matrix D = readMatrix(1, 1, "D");
            ss = StateSpace(A, B, C, D);
            running = false;
        } else if(inputChoice == 2){
            int n, m;
            cout << "Denominator degree, n = ";
            cin >> n;
            Polynomial den = readPolynomialDescending(n, "denominator alpha(s)");
            cout << "Numerator degree, m (m <= n) = ";
            cin >> m;
            Polynomial num = readPolynomialDescending(m, "numerator beta(s)");
            tf = TransferFunction(num, den);
            running = false;
        }
        else{
           return 0;
        }
    }

    running = true;
    while(running){
        cout << "\nWhat do you want to check?\n";
        cout << "  1) Stability\n";
        cout << "  2) Step response\n";
        cout << "  3) Impulse response\n";
        cout << "  4) Root locus\n";
        cout << "  5) Exit\n";
        cout << "Choice: ";
        int choice;
        cin >> choice;

        switch(choice){
            case 1: runStabilityCheck(ss, tf); break;
            case 2: runTimeResponse(ss, tf, /*isStep=*/true); break;
            case 3: runTimeResponse(ss, tf, /*isStep=*/false); break;
            case 4: runRootLocus(ss, tf); break;
            case 5: running = false; break;
            default: cout << "Not a valid option.\n";
        }
    }

    return 0;
}
