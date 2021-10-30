#ifndef QUANTUM_CRYPTO_ATTACK_MATRIX
#define QUANTUM_CRYPTO_ATTACK_MATRIX

#include <array>
#include <vector>
#include <memory>
#include <cassert>

//Xors the destination register with the src register
template <size_t Width>
inline void xor_vectors(std::array<bool, Width>& dest, const std::array<bool, Width>& src) {
    for(size_t i = 0; i < Width; ++i)
        dest[i] ^= src[i];
}

//Checks whether a given register is all zero
template <size_t Width>
inline bool zero_vector(const std::array<bool, Width>& src) {
    for(size_t i = 0; i < Width; ++i)
        if(src[i])
            return false;
    return true;
}

//Checks whether a given register contains the value 1
template <size_t Width>
inline bool one_vector(const std::array<bool, Width>& src) {
    for(size_t i = 0; i < Width - 1; ++i)
        if(src[i])
            return false;
    return src[Width-1];
}

//Class to solve matrix equations
template <size_t Width>
class MatrixSolver {
    private:
        //The number of independent equations in this matrix
        size_t independent_rows;
        //Matrix contents
        std::vector<std::array<bool, Width>> contents;
        //Target values for every equation
        std::vector<bool> targets;

        //Checks whether a given row is independent from all other independent equations in this matrix
        bool independent(size_t row) {
            //Copy the contents of the independent equations into a new matrix, adding the target bits as a final column
            std::unique_ptr<std::array<bool, Width+1>[]> content_copy(new std::array<bool, Width+1>[this->independent_rows + 1]);
            for(size_t i = 0; i < this->independent_rows; ++i) {
                for(size_t j = 0; j < Width; ++j)
                    content_copy[i][j] = this->contents[i][j];
                content_copy[i][Width] = this->targets[i];
            }
            for(size_t j = 0; j < Width; ++j)
                content_copy[this->independent_rows][j] = this->contents[row][j];
            content_copy[this->independent_rows][Width] = this->targets[row];

            //Perform gausssian elimination
            size_t row_offset = 0;
            for(size_t j = 0; j < Width + 1; ++j) {
                //Find a pivot
                for(size_t k = row_offset; k < this->independent_rows + 1; ++k) {
                    if(content_copy[k][j]) {
                        std::swap(content_copy[k], content_copy[row_offset]);
                        ++row_offset;
                        break;
                    }
                }

                //Perform addition modulo 2 for every row, which is equal to xoring the rows
                for(size_t k = row_offset; k < this->independent_rows + 1; ++k) {
                    if(content_copy[k][j] && content_copy[row_offset-1][j])
                        xor_vectors(content_copy[k], content_copy[row_offset-1]);
                }
            }

            //Check whether we end up with an equation of the form 0 = 1
            if(one_vector(content_copy[this->independent_rows]))
                throw std::runtime_error("Invalid equation");

            //Check whether independent
            return !zero_vector(content_copy[this->independent_rows]);
        }

        //Performs gaussian elimination on the internal matrix
        void gaussianEliminate() {
            for(size_t i = 0; i < Width; ++i) {
                //Find a pivot row
                for(size_t j = i; j < this->independent_rows; ++j) {
                    if(this->contents[j][i]) {
                        std::swap(this->contents[j], this->contents[i]);
                        std::swap(this->targets[j], this->targets[i]);
                        break;
                    }
                }

                //Eliminate using addition modulo 2, which is equal to a xor
                for(size_t j = i + 1; j < this->independent_rows; ++j) {
                    if(this->contents[j][i]) {
                        xor_vectors(this->contents[j], this->contents[i]);
                        this->targets[j] = this->targets[i] ^ this->targets[j];
                    }
                }
            }
        }

        //Uses backward substitution to solve the given equation
        std::array<bool, Width> backwardSubstitute() {
            std::array<bool, Width> result;
            //Start at the bottom row, and calculate the value of every row in the solution vector
            for(size_t j = this->independent_rows; j > 0; --j) {
                size_t index = j - 1;
                bool sub_result = 0;

                for(size_t i = index + 1; i < this->independent_rows; ++i)
                    sub_result ^= result[i] & this->contents[index][i];

                result[index] = sub_result ^ this->targets[index];
            }
            return result;
        }
    public:
        MatrixSolver() : independent_rows(0) {}
        ~MatrixSolver() = default;

        inline size_t getIndependent() const {
            return this->independent_rows;
        }

        //Adds a row in masked encoding, bit 0 denotes the target, the other bits the factors in the equation
        void addRow(size_t encoded) {
            std::array<bool, Width> new_row;

            for(size_t i = 0; i < Width; ++i) {
                size_t mask = (1 << (i+1));
                new_row[i] = (encoded & mask) != 0;
            }

            this->addRow(new_row, encoded & 1);
        }

        //Adds a row, the target is given seperately, and the array denotes the factors of the equation
        void addRow(const std::array<bool, Width>& new_row, bool solution) {
            this->contents.push_back(new_row);
            this->targets.push_back(solution);

            //In case the equation is independent, swap it to the top to use it in later equations
            if(this->independent(this->contents.size() - 1)) {
                std::swap(this->contents[this->independent_rows], this->contents[this->contents.size()-1]);
                std::swap(this->targets[this->independent_rows], this->targets[this->contents.size()-1]);
                ++this->independent_rows;
            }
        }

        //Solves the set of equations
        std::array<bool, Width> solve() {
            //Assert a valid matrix
            if(this->independent_rows != Width)
                throw std::runtime_error("Could not solve, invalid number of rows");

            //Gaussian elimination to rewrite the matrix the equation set into a valid set
            this->gaussianEliminate();

            //Perform backward substitution to obtain the solution
            return this->backwardSubstitute();
        }

        //Solves the set of equations, and converts it back into masked encoding
        //Bit 0 will always be set to 1, the other bits contain the solution bits
        size_t solveEncoded() {
            std::array<bool, Width> solution = this->solve();

            size_t result = 1;
            for(size_t i = 0; i < solution.size(); ++i)
                result |= solution[i] << (i+1);
            return result;
        }
};

#endif
