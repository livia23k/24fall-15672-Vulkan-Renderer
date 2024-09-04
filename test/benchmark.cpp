#include <iostream>
#include <chrono>
#include <array>
#include <stdexcept>

using mat4 = std::array<float, 16>;

inline int index(int r, int c) {
    return c * 4 + r;
}

// inline mat4 operator*(const mat4& A, const mat4& B) {
//     mat4 ret;
//     for (uint32_t c = 0; c < 4; ++c) {
//         for (uint32_t r = 0; r < 4; ++r) {
//             ret[index(r, c)] = 0;
//             for (uint32_t k = 0; k < 4; ++k) {
//                 ret[index(r, c)] += A[index(r, k)] * B[index(k, c)];
//             }
//         }
//     }
//     return ret;
// }

inline mat4 operator*(mat4 const &A, mat4 const &B) {
    mat4 ret;
    float b_kc;
    for (uint32_t k = 0; k < 4; ++ k) { //shared axis
        for (uint32_t c = 0; c < 4; ++ c) { //major axis
            b_kc = B[index(k, c)];
            ret[index(0, c)] = A[index(0, k)] * b_kc;
            for (uint32_t r = 1; r < 4; ++ r) { //minor axis
                ret[index(r, c)] += A[index(r, k)] * b_kc;
            }
        }
    }
    return ret;
}

int main() {
    mat4 A, B, C;
    // Initialize A and B with some values
    for (int i = 0; i < 16; ++i) {
        A[i] = i * 0.5f;
        B[i] = i * 0.2f;
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Perform the multiplication 1000000 times to get a significant measurement
    for (int i = 0; i < 1000000; ++i) {
        C = A * B;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Elapsed time: " << elapsed.count() << " ms\n";

    return 0;
}