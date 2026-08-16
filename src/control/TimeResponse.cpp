#include "control/TimeResponse.hpp"

using namespace std;

vector<ResponseSample> TimeResponse::step(const StateSpace& ss, double tFinal, double dt) {
    Matrix x0(ss.order(), 1, 0.0);
    return simulate(ss, tFinal, dt, x0, /*applyStepInput=*/true);
}

vector<ResponseSample> TimeResponse::impulse(const StateSpace& ss, double tFinal, double dt) {
    return simulate(ss, tFinal, dt, ss.getB(), /*applyStepInput=*/false);
}

vector<ResponseSample> TimeResponse::simulate(const StateSpace& ss, double tFinal, double dt,
                                               Matrix x, bool applyStepInput) {
    const Matrix& A = ss.getA();
    const Matrix& B = ss.getB();
    const Matrix& C = ss.getC();
    double D = ss.getD().at(0, 0);
    double u = applyStepInput ? 1.0 : 0.0;

    vector<ResponseSample> samples;
    int steps = static_cast<int>(tFinal / dt);
    samples.reserve(steps + 1);

    for(int k = 0; k <= steps; k++){
        double t = k * dt;
        Matrix y = C * x; // 1x1
        samples.push_back({ t, y.at(0, 0) + D * u });

        // Forward Euler: x_{k+1} = x_k + dt * (A*x_k + B*u)
        Matrix xDot = A * x + B * u;
        x = x + xDot * dt;
    }
    return samples;
}