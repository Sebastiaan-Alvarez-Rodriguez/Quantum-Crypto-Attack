#ifndef QUANTUM_CRYPTO_ATTACK_TOFFOLI
#define QUANTUM_CRYPTO_ATTACK_TOFFOLI

//Utility functions for creating toffoli-based constructions

#include <array>
#include <cstdarg>
#include <iostream>

#include "quantum.hpp"

//Creates an n-bit toffoli, using the args register to denote which bits to include in the toffoli, and using target_bit as the target
//This function is used internally by create_nbit_toffoli to expand the argument list into a compile time list of arguments
template <size_t N, size_t... Args>
void create_nbit_toffoli_internal(std::index_sequence<Args...>, quantum_reg* target_register, size_t target_bit, const std::array<size_t, N>& args) {
    quantum_unbounded_toffoli(N, target_register, std::get<Args>(args)..., target_bit);
}

//Creates an n-bit toffoli, using the args register to denote which bits to include in the toffoli, and using target_bit as the target
template <size_t N>
void create_nbit_toffoli(quantum_reg* target_register, size_t target_bit, const std::array<size_t, N>& args) {
    create_nbit_toffoli_internal<N>(std::make_index_sequence<N>(), target_register, target_bit, args);
}

//Calculates the number of bits set in a given mask
template <size_t N>
constexpr size_t num_bits_set(size_t mask) {
    size_t set_bits = 0;
    for(size_t i = 0; i < N; ++i) {
        if(mask & (1ull << i))
            ++set_bits;
    }
    return set_bits;
}

//Creates a toffoli-construction, using the Mask parameter to determine which bits to include in the toffoli
//For internal use by create_masked_toffoli_base, used to expand the template parameter denoting the number of bits set in the mask
template <size_t N, size_t Set, size_t Mask>
void create_masked_toffoli_internal(quantum_reg* target_register, size_t target_bit, size_t offset) {
    std::array<size_t, Set> content = {0};
    size_t set_bits = 0;
    for(size_t j = 0; j < N; ++j) {
        if(Mask & (1ull << j))
            content[set_bits++] = j + offset;
    }
    create_nbit_toffoli<Set>(target_register, target_bit, content);
}

//Creates a toffoli-construction, using the Mask parameter to deterime which bits to include in the toffoli
//This function is for internal use by create_masked_toffoli
//This function cannot handle cases where Mask == 0, which are handled by create_masked_toffoli
template <size_t N, size_t Mask>
inline void create_masked_toffoli_base(quantum_reg* target_register, size_t target_bit, size_t offset) {
    create_masked_toffoli_internal<N, num_bits_set<N>(Mask), Mask>(target_register, target_bit, offset);
}

//Creates a toffoli-construction, using the Mask paramter to determine which bits to include in the toffoli
template <size_t N, size_t Mask>
void create_masked_toffoli(quantum_reg* target_register, size_t target_bit, size_t offset) {
    if constexpr(Mask == 0) {
        ((void)target_register);
        ((void)target_bit);
        ((void)offset);
    }
    else {
        create_masked_toffoli_base<N, Mask>(target_register, target_bit, offset);
    }
}

//Type defintions for the toffoli creation lookup table defined below
using toffoli_create_callback = void(quantum_reg*, size_t, size_t);

//Utility structure to create pointers to functions generate toffoli functions from given bitmasks
template <size_t N, size_t Mask>
struct ToffoliCallbackGenerator {
    const static constexpr toffoli_create_callback* callback = &create_masked_toffoli<N, Mask>;
};

//Generates a lookup table of toffoli creation routines, where callback i creates the masked-toffoli callback defined by Mask parameter i
template <size_t N, size_t... Masks>
struct ToffoliSequenceGenerator {
    const static constexpr toffoli_create_callback* callbacks[] = {
        ToffoliCallbackGenerator<N, Masks>::callback...
    };
};

//Creates a toffoli-construction using a mask provided to it at runtime
//Uses the compile-time generated toffoli-creation lookup table to call the toffoli-creation routine which creates the toffoli with the given mask
//For internal use by create_masked_toffoli_runtime
template <size_t N, size_t... Masks>
inline void create_masked_toffoli_runtime_internal(std::index_sequence<Masks...>, size_t mask, quantum_reg* target_register, size_t target_bit, size_t offset) {
    ToffoliSequenceGenerator<N, Masks...>::callbacks[mask](target_register, target_bit, offset);
}

//Creates a toffoli gate, where every bit in mask defines whether to include the bit as a source operand in the toffoli
//The target_bit paramter denotes the bit to write toggle
//The offset parameter defines from which bit to start generating the mask (e.g. offset = 2, mask = 5) would create a toffoli involving bits 2 and 4
template <size_t N>
inline void create_masked_toffoli_runtime(size_t mask, quantum_reg* target_register, size_t target_bit, size_t offset) {
    create_masked_toffoli_runtime_internal<N>(std::make_index_sequence<1ull << N>(), mask, target_register, target_bit, offset);
}

#endif
