#include "control/StateSpace.hpp"
#include <stdexcept>

using namespace std;

StateSpace::StateSpace(Matrix A_, Matrix B_, Matrix C_, Matrix D_)
    : A(move(A_)), B(move(B_)), C(move(C_)), D(move(D_)) {
    if(A.getRows() != A.getCols())
        throw invalid_argument("A must be square");
    if(B.getRows() != A.getRows() || B.getCols() != 1)
        throw invalid_argument("B must be n x 1");
    if(C.getRows() != 1 || C.getCols() != A.getCols())
        throw invalid_argument("C must be 1 x n");
    if(D.getRows() != 1 || D.getCols() != 1)
        throw invalid_argument("D must be 1 x 1 (SISO)");
}

int StateSpace::order() const { return A.getRows(); }

const Matrix& StateSpace::getA() const { return A; }
const Matrix& StateSpace::getB() const { return B; }
const Matrix& StateSpace::getC() const { return C; }
const Matrix& StateSpace::getD() const { return D; }


// Faddeev-LeVerrier (sign convention verified by hand against a known
    // companion matrix -- see the note in StateSpace.hpp):
    //   M_0 = I
    //   for k = 1..n:
    //       c_k = (1/k) * trace(A * M_{k-1})
    //       M_k = A*M_{k-1} - c_k*I
    // p(s) = s^n - c_1*s^(n-1) - c_2*s^(n-2) - ... - c_n
    // adj(sI-A) = M_0*s^(n-1) + M_1*s^(n-2) + ... + M_(n-1)
Polynomial StateSpace::characteristicPolynomial(vector<Matrix>* adjugateCoeffs) const {
    int n = order();

    vector<Matrix> M;
    M.reserve(n);
    vector<double> c(n + 1, 0.0);

    Matrix Mprev = Matrix::identity(n);
    M.push_back(Mprev);

    for(int k = 1; k <= n; k++){
        c[k] = (1.0 / k) * (A * Mprev).trace();
        Matrix Mk = A * Mprev - Matrix::identity(n) * c[k];
        if(k < n) M.push_back(Mk); // M_n is discarded (Cayley-Hamilton: it's ~0)
        Mprev = Mk;
    }

    if(adjugateCoeffs != nullptr) *adjugateCoeffs = M;

    // p(s) = s^n - c_1*s^(n-1) - ... - c_n; Polynomial wants ascending order.
    vector<double> ascending(n + 1, 0.0);
    ascending[n] = 1.0;
    for(int k = 1; k <= n; k++) ascending[n - k] = -c[k];
    return Polynomial(ascending);
}
