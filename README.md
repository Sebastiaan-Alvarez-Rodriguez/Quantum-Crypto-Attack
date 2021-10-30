# Quantum-Crypto-Attack
Fundamental quantum simulation to attack symmetric-key cryptography primitives.
This project is based on a paper I read about [here](https://arxiv.org/pdf/1603.07856.pdf):
Using Simon’s algorithm to attack symmetric-key cryptographic primitives.

The paper describes the use of quantum computers to determine whether a symmetric-key cryptography system is using a 
[Feistel network](https://en.wikipedia.org/wiki/Feistel_cipher) or
a random swapping algorithm.

The paper shows that we can accurately decide whether Feistel networks or random swapping algorithms are used **in linear time**, technically breaking said symmetric-key cryptography system.

A cryptography system is considered broken when an attacker can tell whether a given output string `s` is encrypted or a random collection of tokens.
When thinking informally about it:
If an attacker can see the difference between encrypted strings `s` and random strings `r`, there must exist a fundamental, recognizable pattern in `s`.


## Requirements
To compile a binary, you need:
 - `make`
 - The libquantum development library (version >= 1.1.1). Note: Some linux distributions have this package in their repositories, usually named `libquantum-dev`
 - A modern C++ compiler capable of compiling C++17

## Compilation & Execution
Compile the binary by using the provided Makefile.
Simply type `make` in the project root directory once all requirements are satisfied. 

Execute the program by executing the binary, named `qa_distinguish`.

## Program
The program will run 2 tests when booted:
 1. Feistel detection routine, which uses a Feistel network as parameter.
 2. Feistel detection routine, which uses a **random permutation-based encryption routine**.
    - Random values will be generated for the `alpha` and `beta` values used by the `f` function.
    - The program outputs one of the following 3 outputs:
        - 3-round Feistel (solved equation): `n` linearly-independent equations were obtained and the equation was solved. 
          In this case, the program detected the encryption routine is a Feistel network after verifying `f(u) = f(u ^ s)`.
        - 3-round Feistel (more than `2n` equations attempted): 
          `2n` equations were obtained, containing less than `n` linearly independent equations.
          In this case, the program guesses the routine to be a 3-round Feistel network, since random swapping algorithms are unlikely to show this behaviour.
        - random permutation: `n` linearly independent equations were obtained and the equation was solved.
          In this case, the program tested `f(u) != f(u^s)`, and as such detected a random swapping algorithm.