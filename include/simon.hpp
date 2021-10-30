#ifndef QUANTUM_CRYPTO_ATTACK_SIMON
#define QUANTUM_CRYPTO_ATTACK_SIMON

#include "quantum.hpp"

//Runs Simon's algorithm
//uf_callback has to be a function satisfying Uf|x>|y> -> |x>|y xor f(x)>
template <size_t N, size_t M, typename T>
std::pair<size_t, size_t> run_simon(T uf_callback) {
    quantum_reg reg = quantum_new_qureg(0, N + M);

    for(size_t i = 0; i < N; ++i)
        quantum_hadamard(i, &reg);

    uf_callback(&reg);

    for(size_t i = 0; i < N; ++i)
        quantum_hadamard(i, &reg);

    size_t result = quantum_measure(reg);

    quantum_delete_qureg(&reg);

    size_t result_x = result & ((1ull << N) - 1);
    size_t result_y = result >> N;

    return std::make_pair(result_x, result_y);
}

#endif