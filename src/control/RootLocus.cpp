#include "control/RootLocus.hpp"
#include "control/Stability.hpp"
#include "math/Constants.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

using namespace std;

RootLocus::RootLocus(const TransferFunction& openLoopPlant) : plant(openLoopPlant) {}

static vector<ComplexNumber> reorderToMatchTarget(const vector<ComplexNumber>& current,
                                                    const vector<ComplexNumber>& target) {
    int n = static_cast<int>(current.size());
    vector<int> indices(n);
    for(int i = 0; i < n; i++) indices[i] = i;

    vector<int> bestOrder = indices;
    double bestCost = numeric_limits<double>::max();

    do {
        double cost = 0.0;
        for(int i = 0; i < n; i++){
            cost += (current[indices[i]] - target[i]).magnitude();
        }
        if(cost < bestCost){
            bestCost = cost;
            bestOrder = indices;
        }
    } while(next_permutation(indices.begin(), indices.end()));

    vector<ComplexNumber> result(n);
    for(int i = 0; i < n; i++) result[i] = current[bestOrder[i]];
    return result;
}

vector<RootLocusPoint> RootLocus::compute(double kMax, int samples) const {
    vector<RootLocusPoint> locus;
    locus.reserve(samples);
    for(int i = 0; i < samples; i++){
        double k = (samples == 1) ? 0.0 : kMax * static_cast<double>(i) / (samples - 1);
        vector<ComplexNumber> poles = plant.closedLoopPoles(k);
        if(i == 1){
            // Only one previous sample exists yet -- nothing to predict
            // a direction from, so match against it directly.
            poles = reorderToMatchTarget(poles, locus[0].poles);
        } else if(i > 1){
            // Matching against the raw previous sample alone breaks down
            // right around breakaway/break-in points: that's exactly
            // where root sensitivity to k blows up (two roots collide and
            // peel off in a new direction), so a branch can move further
            // between two samples than the gap to a *different* branch,
            // and nearest-previous-point picks the wrong one -- visible as
            // a little zigzag/kink in the plotted curve.
            //
            // Instead, linearly extrapolate each branch's next position
            // from its last two samples (constant-velocity guess) and
            // match against that prediction. Using direction of travel,
            // not just position, disambiguates crossing/colliding
            // branches far more reliably than nearest-previous-point.
            vector<ComplexNumber> predicted(poles.size());
            for(size_t b = 0; b < poles.size(); b++){
                predicted[b] = locus[i - 1].poles[b] * 2.0 - locus[i - 2].poles[b];
            }
            poles = reorderToMatchTarget(poles, predicted);
        }
        RootLocusPoint point;
        point.k = k;
        point.poles = poles;
        locus.push_back(point);
    }
    return locus;
}

double RootLocus::asymptoteCentroid() const {
    vector<ComplexNumber> poles = plant.poles();
    vector<ComplexNumber> zeros = plant.zeros();
    int n = static_cast<int>(poles.size());
    int m = static_cast<int>(zeros.size());
    if(n <= m) return 0.0;

    double sumPoles = 0.0, sumZeros = 0.0;
    for(auto& p : poles) sumPoles += p.getReal();
    for(auto& z : zeros) sumZeros += z.getReal();
    return (sumPoles - sumZeros) / static_cast<double>(n - m);
}

vector<double> RootLocus::asymptoteAngles() const {
    vector<ComplexNumber> poles = plant.poles();
    vector<ComplexNumber> zeros = plant.zeros();
    int n = static_cast<int>(poles.size());
    int m = static_cast<int>(zeros.size());
    int diff = n - m;
    vector<double> angles;
    if(diff <= 0) return angles;
    for(int i = 1; i <= diff; i++){
        angles.push_back((2.0 * i - 1.0) * M_PI / diff);
    }
    return angles;
}

bool RootLocus::findCriticalK(double kMax, double& outCriticalK) const {
    const int steps = 2000;
    // Seed from a tiny positive k rather than exactly 0: if the open-loop
    // plant has a pole exactly at the origin (a free integrator, common in
    // these examples), k=0 puts a closed-loop pole exactly on the
    // imaginary axis too, which Routh-Hurwitz correctly reports as
    // marginal -- but that's an artifact of k=0 itself, not the crossing
    // we're looking for.
    bool prevStable = Stability::routhHurwitz(plant.closedLoopCharacteristicPolynomial(kMax * 1e-6)).stable;

    for(int i = 1; i <= steps; i++){
        double k = kMax * static_cast<double>(i) / steps;
        bool nowStable = Stability::routhHurwitz(plant.closedLoopCharacteristicPolynomial(k)).stable;

        if(nowStable != prevStable){
            // Bisect between the previous sample and this one to refine.
            double lo = kMax * static_cast<double>(i - 1) / steps;
            double hi = k;
            for(int b = 0; b < 60; b++){
                double mid = (lo + hi) / 2.0;
                bool midStable = Stability::routhHurwitz(plant.closedLoopCharacteristicPolynomial(mid)).stable;
                if(midStable == prevStable) lo = mid; else hi = mid;
            }
            outCriticalK = (lo + hi) / 2.0;
            return true;
        }
        prevStable = nowStable;
    }
    return false;
}