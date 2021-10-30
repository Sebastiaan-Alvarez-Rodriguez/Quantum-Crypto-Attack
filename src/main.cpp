#include "quantum.hpp"

#include <cstdlib>
#include <ctime>
#include <bitset>
#include <functional>

#include "toffoli.hpp"
#include "simon.hpp"
#include "matrix.hpp"
#include "feistel.hpp"

//Creates a quantum gate that toggles a given target bit if bits [offset, offset+N) match value
template <size_t N>
void create_toggle_if_match(size_t value, quantum_reg* x, size_t target, size_t offset) {
    //Flip all bits that are 0 in the orignal value
    //This would cause the register to contain all ones if and only if the value in the register equals value
    for(size_t i = 0; i < N; ++i) {
        if(!(value & (1ull << i))) {
            quantum_sigma_x(offset + i, x);
        }
    }

    //Use an n-bit toffoli on all N bits, setting the target bit if they are all one
    create_masked_toffoli_runtime<N>((1ull << N) - 1, x, target, offset);

    //Cleanup the flipped bits
    for(size_t i = 0; i < N; ++i) {
        if(!(value & (1ull << i))) {
            quantum_sigma_x(offset + i, x);
        }
    }
}

//Convert a classical function into a bitflip oracle
template <size_t N, size_t M, typename T>
void bitflip_oracle(quantum_reg* x_y, T function) {
    //Evaluate all possible inputs to the function, and calculate their results
    size_t num_posibilities = (1ull << N);
    for(size_t i = 0; i < num_posibilities; ++i) {
        //Calculate the input to the i-th possible input to the function
        size_t result = function(i);
        
        //For every bit which equals 1 in the output, flip it if and only if the value in the quantum register matches the input i to the function
        for(size_t j = 0; j < M; ++j) {
            if(result & (1ull << j))
                create_toggle_if_match<N>(i, x_y, j+N, 0);
        }
    }
}

//Utility function to create a new function f(quantum_reg)
//This function f(quantum_reg) performs a quantum bitflip oracle matching the classical callback function given as a parameter
template <size_t N, size_t M, typename T>
auto bind_to_bitflip_oracle(T callback) {
    return std::bind(bitflip_oracle<N, M, T>, std::placeholders::_1, callback);
}


//Simple test to see whether our Simon implementation only yields strings y satifying y * s = 0
void test_simon() {
    //Secret string for this function is 110
    size_t s = 6;
    //A valid 2-to-1 function with secret string s
    auto function = [=](size_t input) {
        switch(input) {
            case 0:
                return 5;
            case 1:
                return 2;
            case 2:
                return 0;
            case 3:
                return 6;
            case 4:
                return 0;
            case 5:
                return 6;
            case 6:
                return 5;
            case 7:
                return 2;
        }
        return 0;
    };

    //Run simon's algorithm often, verify that the result measured in the first register matches the criteria
    auto oracle = bind_to_bitflip_oracle<3, 3>(function);
    for(size_t i = 0; i < 100000; ++i) {
        //Run the quantum circuit and measure
        std::pair<size_t, size_t> measurements = run_simon<3, 3>(oracle);
        size_t measure_x = measurements.first;

        //Calculate s*x (mod 2), with s the secret string and x the measured register
        size_t merge = measure_x & s;
        bool result = false;
        for(size_t i = 0; i < 3; ++i) {
            if(merge & (1 << i)) {
                result = !result;
            }
        }

        //According to simon's algorithm, s*x (mod 2) should always be zero, we can detect if the circuit failed by seeing if s*x (mod 2) = 1
        if(result) {
            std::cout << "Failed for measurement: " << std::bitset<3>(measure_x) << std::endl;
            return;
        }
    }
    std::cout << "Simon success" << std::endl;
}

//Classical function to test if our feister encrypt and decrypt routines are fuctional
void run_feistel_classic_test() {
    //Encrypt a random input
    size_t input = rand() % 256;

    //The round function used for this test, flips the 2 bit pairs in the 4-bit input, and xors the bottom bits with the key
    auto round_function = [=](size_t input, size_t key) {
        size_t left_part = input & 0x3;
        size_t right_part = (input & 0x3) >> 2;
        right_part ^= key;
        return (left_part << 2) | right_part;
    };

    //Encrypt and decrypt
    std::cout << "Input: " << input << std::endl;
    size_t encrypted = run_feistel_encrypt<4, 3>(input, round_function, {1, 2, 3});
    size_t decrypted = run_feistel_decrypt<4, 3>(encrypted, round_function, {1,2,3});

    //Output results, decrypted should match input
    std::cout << "Encrypted: " << encrypted << std::endl;
    std::cout << "Decrypted: " << decrypted << std::endl;
}

//Runs the feistel detection quantum algorithm as described in section 3 of the paper
//The parameter denotes the function to verify
template <size_t Bits, typename Func>
void run_feistel_detect(Func internal_callback) {
    //Generate random alpha and beta parameters
    const size_t ALPHA = std::rand() % (1ull << Bits);
    const size_t BETA = std::rand() % (1ull << Bits);

    //Generate the f function matching our callback function, using the generated alpha and beta parameters
    auto function = [=](size_t input) {
        return run_f<Bits>(input,
            internal_callback,
            ALPHA,
            BETA
        );
    };

    //Create the bitflip oracle matching the f function
    auto oracle = bind_to_bitflip_oracle<Bits + 1, Bits>(function);

    MatrixSolver<Bits> solver;

    //Attempt at most 2n runs of Simons algorithm
    for(size_t i = 0; i < 2*Bits; ++i) {
        try {
            //Run simons algorithm
            std::pair<size_t, size_t> measurements = run_simon<Bits + 1, Bits>(oracle);

            //Obtain the observed result from the first register, and add it as a linear equation
            size_t j = measurements.first;
            solver.addRow(j);

            //Check the number of linearly independent rows, if this equals n, start equation solving
            size_t independent_rows = solver.getIndependent();
            if(independent_rows == Bits) {
                //Solve the equation
                size_t s = solver.solveEncoded();

                //Generate a random bitstring u
                size_t u = std::rand() % (1ull << (Bits+1));

                //Check if f(u) == f(u ^ s), and draw conclusions
                size_t f_u = function(u);
                size_t f_u_s = function(u ^ s);

                if(f_u == f_u_s) {
                    std::cout << "3-round Feistel (solved equation)" << std::endl;
                    return;
                }
                else {
                    std::cout << "Random permutation" << std::endl;
                    return;
                }
            }
        }
        catch(const std::runtime_error& e) {
            //Skip invalid equations, note that this should not happen for valid Feistel networks
            --i;
        }
    }
    //More than 2n equations tried, guess the function to be a feistel
    std::cout << "3-round Feistel (more than 2n equations attempted)" << std::endl;
}

//Generates a random permutation of integer values
//This creates a lookup table where every integer between 0 and max appears once, at a random position in the table
size_t* generate_permuation_map(size_t max, size_t permutations) {
    size_t* permutation_map = new size_t[max];
    for(size_t i = 0; i < max; ++i) {
        permutation_map[i] = i;
    }

    //Swap random elements
    for(size_t i = 0; i < permutations; ++i) {
        size_t x = std::rand() % max;
        size_t y = std::rand() % max;

        std::swap(permutation_map[x], permutation_map[y]);
    }

    return permutation_map;
}

//Test our feistel detection routines
void run_feistel_tests() {
    //Configuration for our oracles and functions
    const size_t FEISTEL_ROUNDS = 3;
    const size_t BITS = 8;

    //Generates a random permutation map used for the Feistel subkey function
    size_t* feistel_permutation_map = generate_permuation_map(1 << BITS, 100000);
    //Generates a random permutation map used for the random swapping function
    size_t* random_permutation_map = generate_permuation_map(1 << (2 * BITS), 100000);

    //Generate the subkeys for the feistel rounds
    std::array<size_t, FEISTEL_ROUNDS> keys;
    for(size_t i = 0; i < FEISTEL_ROUNDS; ++i) {
        keys[i] = std::rand() % (1ull << BITS);
    }

    //The round function to use, a minimal version of Pearshon hashing
    auto round_function = [=](size_t input, size_t key) {
        return feistel_permutation_map[(input ^ key)];
    };

    //The feistel network function
    auto feistel_function = make_feistel_encrypt<BITS, FEISTEL_ROUNDS>(round_function, keys);

    //Random swap test function, maps any integer to another integer in a one-to-one fashion
    auto random_function = [=](size_t input) {
        return random_permutation_map[input];
    };

    //Run feistel detection on a feistel network
    std::cout << "Running detection for feistel function: " << std::endl;
    run_feistel_detect<BITS>(feistel_function);

    //Run feistel detection on a random permutation
    std::cout << std::endl << "Running detection for random permutation function: " << std::endl;
    run_feistel_detect<BITS>(random_function);

    delete[] random_permutation_map;
    delete[] feistel_permutation_map;
}

int main() {
    //Initialize libquantum seed
    std::srand(std::time(nullptr));

    run_feistel_tests();
    
    return 0;
}