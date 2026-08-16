#ifndef STATE_SPACE_HPP
#define STATE_SPACE_HPP

#include <vector>
#include "math/Matrix.hpp"
#include "math/Polynomial.hpp"

class StateSpace {
private:
    Matrix A, B, C, D;
public:
    StateSpace(Matrix A, Matrix B, Matrix C, Matrix D);

    int order() const;
    const Matrix& getA() const;
    const Matrix& getB() const;
    const Matrix& getC() const;
    const Matrix& getD() const;

    // Faddeev-LeVerrier algorithm: computes p(s) = det(sI - A) using only
    // matrix multiplication and trace -- no eigenvalue decomposition
    // needed. The roots of p(s) are exactly the eigenvalues of A, i.e. the
    // system's poles, and Polynomial::roots() (Durand-Kerner) already knows how to find them.
    //
    // Also fills adjugateCoeffs with the matrices M_0..M_{n-1} such that
    // adj(sI-A) = M_0*s^(n-1) + M_1*s^(n-2) + ... + M_{n-1}; these are
    // what Conversions::toTransferFunction needs to build the numerator.
    // Pass nullptr if you only need the characteristic polynomial.
    Polynomial characteristicPolynomial(std::vector<Matrix>* adjugateCoeffs = nullptr) const;
};

#endif