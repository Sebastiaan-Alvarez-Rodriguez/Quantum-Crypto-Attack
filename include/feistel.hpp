#ifndef QUANTUM_CRYPTO_ATTACK_FEISTEL
#define QUANTUM_CRYPTO_ATTACK_FEISTEL

#include <array>


//Runs a feistel encryption routine using the given keys and round function
template <size_t Bits, size_t Rounds, typename Func>
size_t run_feistel_encrypt(size_t input, Func round_function, const std::array<size_t, Rounds>& keys) {
    size_t r_mask = (1ull << Bits) - 1;
    size_t l_mask = r_mask << Bits;

    size_t r = input & r_mask;
    size_t l = (input & l_mask) >> Bits;

    for(size_t i = 0; i < keys.size(); ++i) {
        size_t next_l = r;
        size_t next_r = l ^ round_function(r, keys[i]);

        l = next_l;
        r = next_r;
    }

    return r | (l << Bits);
}

//Runs a feistel decryption routine using the given keys and round function
template <size_t Bits, size_t Rounds, typename Func>
size_t run_feistel_decrypt(size_t input, Func round_function, const std::array<size_t, Rounds>& keys) {
    size_t r_mask = (1ull << Bits) - 1;
    size_t l_mask = r_mask << Bits;

    size_t r = input & r_mask;
    size_t l = (input & l_mask) >> Bits;

    for(size_t i = 0; i < keys.size(); ++i) {
        size_t prev_r = l;
        size_t prev_l = r ^ round_function(l, keys[keys.size() - 1 - i]);

        r = prev_r;
        l = prev_l;
    }

    return r | (l << Bits);
}

//Utility function to bind keys and round function to the feistel function, resulting in a function f(input) performing the feistel network
template <size_t Bits, size_t Rounds, typename Func>
auto make_feistel_encrypt(Func round_function, const std::array<size_t, Rounds>& keys) {
    return std::bind(run_feistel_encrypt<Bits, Rounds, Func>, std::placeholders::_1, round_function, keys);
}

//Runs the f function described in section 3 of the paper, using the given alpha and beta values
//In this case, the callback parameter contains the oracle V
template <size_t Bits, typename Func>
size_t run_f(size_t input, Func callback, size_t alpha, size_t beta) {
    auto w = [=](size_t input) {
        const size_t bitmask = ((1ull << Bits) - 1);

        size_t result = callback(input) >> Bits;
        return result & bitmask;
    };

    size_t a = input >> 1;
    size_t b = input & 1;

    size_t mask = b ? beta : alpha;
    return w(a << Bits | mask) ^ mask;
}

#endif
