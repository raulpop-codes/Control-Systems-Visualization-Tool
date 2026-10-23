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

        // RK4 for x' = f(x) = A*x + B*u. The input u is constant over a
        // step (0 or 1), so it's the same in all four slope samples:
        //   k1 = f(x_k)
        //   k2 = f(x_k + dt/2 * k1)
        //   k3 = f(x_k + dt/2 * k2)
        //   k4 = f(x_k + dt   * k3)
        //   x_{k+1} = x_k + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
        Matrix k1 = A * x + B * u;
        Matrix k2 = A * (x + k1 * (dt / 2.0)) + B * u;
        Matrix k3 = A * (x + k2 * (dt / 2.0)) + B * u;
        Matrix k4 = A * (x + k3 * dt) + B * u;
        x = x + (k1 + k2 * 2.0 + k3 * 2.0 + k4) * (dt / 6.0);
    }
    return samples;
}