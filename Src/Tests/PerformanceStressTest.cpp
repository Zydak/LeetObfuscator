// Deterministic performance-intensive test for the Leet obfuscator.
// Uses repeated arithmetic and matrix-like computation with fixed values.
// Output is deterministic across all runs.
// Expanded to stress test performance with complex computational patterns.

#include <array>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <chrono>

#include "../Leet.h"

static inline uint64_t mix64(uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

static inline uint64_t add_f(uint64_t h, float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    return mix64(h ^ u);
}

static inline uint64_t add_d(uint64_t h, double d) {
    uint64_t u;
    std::memcpy(&u, &d, sizeof(u));
    return mix64(h ^ u);
}

// Matrix multiplication
static uint64_t multiplyMatrix(const std::array<std::array<uint32_t, 8>, 8> &a,
                               const std::array<std::array<uint32_t, 8>, 8> &b) {
    std::array<std::array<uint32_t, 8>, 8> c{};
    for (unsigned r = 0; r < 8; ++r) {
        for (unsigned ccol = 0; ccol < 8; ++ccol) {
            uint32_t acc = 0;
            for (unsigned k = 0; k < 8; ++k)
                acc += a[r][k] * b[k][ccol];
            c[r][ccol] = acc;
        }
    }
    uint64_t h = 0xfeedfacecafebabeULL;
    for (unsigned r = 0; r < 8; ++r) {
        for (unsigned ccol = 0; ccol < 8; ++ccol) {
            h = mix64(h ^ (uint64_t)c[r][ccol]);
        }
    }
    return h;
}

// Polynomial sweep computation
static uint64_t polynomialSweep(const std::array<double, 16> &coeffs,
                                const std::array<double, 16> &input,
                                int rounds,
                                double scale,
                                uint32_t offset) {
    uint64_t h = 0x0badf00d12345678ULL;
    for (int pass = 0; pass < rounds; ++pass) {
        double value = 0.0;
        for (unsigned i = 0; i < coeffs.size(); ++i)
            value += coeffs[i] * std::pow(input[i] + pass * 0.001, (double)(i % 3 + 1));
        value = std::fmod(value * scale + offset, 1e9);
        h = add_d(h, value);
    }
    return h;
}

// Strided sum computation
static uint64_t computeStridedSum(const std::array<uint32_t, 32> &data,
                                  uint32_t stride,
                                  uint32_t seed,
                                  uint32_t mask) {
    uint64_t h = 0xabbaabbaabbaabbaULL;
    for (unsigned i = 0; i < data.size(); i += 2) {
        uint32_t value = data[i] ^ seed;
        value += data[(i + stride) % data.size()];
        value &= mask;
        h = mix64(h ^ (uint64_t)value);
    }
    return h;
}

// FFT-like butterfly computation
static uint64_t butterflyComputation(const std::array<std::array<double, 4>, 4> &data,
                                     int stages) {
    uint64_t h = 0x1234567890abcdefULL;
    std::array<std::array<double, 4>, 4> working = data;
    
    for (int stage = 0; stage < stages; ++stage) {
        for (unsigned i = 0; i < working.size(); ++i) {
            for (unsigned j = 0; j < working[i].size() / 2; ++j) {
                double temp = working[i][j];
                working[i][j] = temp + working[i][j + working[i].size() / 2];
                working[i][j + working[i].size() / 2] = temp - working[i][j + working[i].size() / 2];
                
                h = add_d(h, working[i][j]);
                h = add_d(h, working[i][j + working[i].size() / 2]);
            }
        }
    }
    
    for (unsigned i = 0; i < working.size(); ++i) {
        for (unsigned j = 0; j < working[i].size(); ++j) {
            h = add_d(h, working[i][j]);
        }
    }
    
    return h;
}

// Convolution computation
static uint64_t convolutionComputation(const std::array<double, 16> &signal,
                                       const std::array<double, 8> &kernel) {
    uint64_t h = 0xabcdef0123456789ULL;
    std::array<double, 23> output{};
    
    for (unsigned i = 0; i < output.size(); ++i) {
        double sum = 0.0;
        for (unsigned j = 0; j < kernel.size(); ++j) {
            if (i >= j && i - j < signal.size()) {
                sum += signal[i - j] * kernel[j];
            }
        }
        output[i] = sum;
        h = add_d(h, output[i]);
    }
    
    return h;
}

// Sorting network simulation
static uint64_t sortingNetwork(const std::array<int, 16> &input) {
    uint64_t h = 0xdeadbeefcafebabeULL;
    std::array<int, 16> data = input;
    
    // Bitonic sort simulation
    for (unsigned k = 2; k <= data.size(); k *= 2) {
        for (unsigned j = k / 2; j > 0; j /= 2) {
            for (unsigned i = 0; i < data.size(); ++i) {
                unsigned l = i ^ j;
                if (l > i) {
                    if ((i & k) == 0 && data[i] > data[l]) {
                        std::swap(data[i], data[l]);
                    } else if ((i & k) != 0 && data[i] < data[l]) {
                        std::swap(data[i], data[l]);
                    }
                    h = mix64(h ^ (uint64_t)(data[i] + data[l]));
                }
            }
        }
    }
    
    for (int value : data) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    return h;
}

// Numerical integration (Simpson's rule)
static uint64_t numericalIntegration(double a, double b, int n) {
    uint64_t h = 0xfee1deadbadc0ffeULL;
    double step = (b - a) / n;
    double sum = 0.0;
    
    for (int i = 0; i <= n; ++i) {
        double x = a + i * step;
        double fx = std::sin(x) * std::cos(x) + std::exp(x * 0.1);
        
        if (i == 0 || i == n) {
            sum += fx;
        } else if (i % 2 == 0) {
            sum += 2.0 * fx;
        } else {
            sum += 4.0 * fx;
        }
        
        h = add_d(h, fx);
    }
    
    double result = sum * step / 3.0;
    h = add_d(h, result);
    
    return h;
}

// Monte Carlo simulation (pi estimation)
static uint64_t monteCarloPi(int samples) {
    uint64_t h = 0x13579bdf2468ace0ULL;
    int inside = 0;
    
    for (int i = 0; i < samples; ++i) {
        // Use deterministic pseudo-random values
        double x = ((i * 7919) % 10000) / 10000.0;
        double y = ((i * 7907) % 10000) / 10000.0;
        
        if (x * x + y * y <= 1.0) {
            inside++;
        }
        
        h = mix64(h ^ (uint64_t)(i * 7919));
        h = add_d(h, x);
        h = add_d(h, y);
    }
    
    double piEstimate = 4.0 * inside / samples;
    h = add_d(h, piEstimate);
    h = mix64(h ^ (uint64_t)inside);
    
    return h;
}

// Matrix transpose and multiplication
static uint64_t matrixTransposeMultiply(const std::array<std::array<uint32_t, 8>, 8> &a,
                                        const std::array<std::array<uint32_t, 8>, 8> &b) {
    uint64_t h = 0xaaaabbbbccccddddULL;
    std::array<std::array<uint32_t, 8>, 8> bTransposed{};
    
    // Transpose b
    for (unsigned i = 0; i < 8; ++i) {
        for (unsigned j = 0; j < 8; ++j) {
            bTransposed[i][j] = b[j][i];
            h = mix64(h ^ (uint64_t)bTransposed[i][j]);
        }
    }
    
    // Multiply a by bTransposed
    std::array<std::array<uint32_t, 8>, 8> result{};
    for (unsigned i = 0; i < 8; ++i) {
        for (unsigned j = 0; j < 8; ++j) {
            uint32_t sum = 0;
            for (unsigned k = 0; k < 8; ++k) {
                sum += a[i][k] * bTransposed[j][k];
            }
            result[i][j] = sum;
            h = mix64(h ^ (uint64_t)result[i][j]);
        }
    }
    
    return h;
}

// Fast exponentiation
static uint64_t fastExponentiation(double base, int exp) {
    uint64_t h = 0x1111222233334444ULL;
    double result = 1.0;
    double current = base;
    
    while (exp > 0) {
        if (exp % 2 == 1) {
            result *= current;
            h = add_d(h, result);
        }
        current *= current;
        h = add_d(h, current);
        exp /= 2;
    }
    
    h = add_d(h, result);
    return h;
}

// GCD computation (Euclidean algorithm)
static uint64_t gcdComputation(uint64_t a, uint64_t b) {
    uint64_t h = 0x5555666677778888ULL;
    
    while (b != 0) {
        uint64_t temp = b;
        b = a % b;
        a = temp;
        h = mix64(h ^ a);
        h = mix64(h ^ b);
    }
    
    h = mix64(h ^ a);
    return h;
}

// Prime sieve simulation
static uint64_t primeSieve(int limit) {
    uint64_t h = 0x9999aaaabbbbccccULL;
    std::array<bool, 1000> sieve{};
    sieve.fill(true);
    
    sieve[0] = false;
    sieve[1] = false;
    
    for (int i = 2; i * i <= limit; ++i) {
        if (sieve[i]) {
            for (int j = i * i; j <= limit; j += i) {
                sieve[j] = false;
                h = mix64(h ^ (uint64_t)j);
            }
        }
        h = mix64(h ^ (uint64_t)i);
    }
    
    int count = 0;
    for (int i = 2; i <= limit; ++i) {
        if (sieve[i]) {
            count++;
            h = mix64(h ^ (uint64_t)i);
        }
    }
    
    h = mix64(h ^ (uint64_t)count);
    return h;
}

// Newton-Raphson square root
static uint64_t newtonRaphsonSqrt(double number, int iterations) {
    uint64_t h = 0xddddeeeeffff0000ULL;
    double guess = number / 2.0;
    
    for (int i = 0; i < iterations; ++i) {
        guess = 0.5 * (guess + number / guess);
        h = add_d(h, guess);
    }
    
    h = add_d(h, guess);
    return h;
}

// Mandelbrot set computation
static uint64_t mandelbrotComputation(double cx, double cy, int maxIter) {
    uint64_t h = 0x0011223344556677ULL;
    double x = 0.0, y = 0.0;
    int iter = 0;
    
    while (x * x + y * y <= 4.0 && iter < maxIter) {
        double xTemp = x * x - y * y + cx;
        y = 2.0 * x * y + cy;
        x = xTemp;
        iter++;
        
        h = add_d(h, x);
        h = add_d(h, y);
    }
    
    h = mix64(h ^ (uint64_t)iter);
    return h;
}

// LU decomposition simulation
static uint64_t luDecomposition(const std::array<std::array<double, 6>, 6> &matrix) {
    uint64_t h = 0x778899aabbccdde0ULL;
    std::array<std::array<double, 6>, 6> L{};
    std::array<std::array<double, 6>, 6> U{};
    
    // Initialize L as identity and U as zero
    for (unsigned i = 0; i < 6; ++i) {
        L[i][i] = 1.0;
        for (unsigned j = 0; j < 6; ++j) {
            U[i][j] = 0.0;
        }
    }
    
    // Perform LU decomposition
    for (unsigned i = 0; i < 6; ++i) {
        for (unsigned j = i; j < 6; ++j) {
            double sum = 0.0;
            for (unsigned k = 0; k < i; ++k) {
                sum += L[i][k] * U[k][j];
            }
            U[i][j] = matrix[i][j] - sum;
            h = add_d(h, U[i][j]);
        }
        
        for (unsigned j = i + 1; j < 6; ++j) {
            double sum = 0.0;
            for (unsigned k = 0; k < i; ++k) {
                sum += L[j][k] * U[k][i];
            }
            L[j][i] = (matrix[j][i] - sum) / U[i][i];
            h = add_d(h, L[j][i]);
        }
    }
    
    return h;
}

// Cholesky decomposition simulation
static uint64_t choleskyDecomposition(const std::array<std::array<double, 5>, 5> &matrix) {
    uint64_t h = 0xffeeddccbbaa9988ULL;
    std::array<std::array<double, 5>, 5> L{};
    
    for (unsigned i = 0; i < 5; ++i) {
        for (unsigned j = 0; j <= i; ++j) {
            double sum = 0.0;
            
            if (j == i) {
                for (unsigned k = 0; k < j; ++k) {
                    sum += L[j][k] * L[j][k];
                }
                L[i][j] = std::sqrt(matrix[i][i] - sum);
            } else {
                for (unsigned k = 0; k < j; ++k) {
                    sum += L[i][k] * L[j][k];
                }
                L[i][j] = (matrix[i][j] - sum) / L[j][j];
            }
            
            h = add_d(h, L[i][j]);
        }
    }
    
    return h;
}

// Vector dot product
static uint64_t dotProduct(const std::array<double, 32> &a,
                           const std::array<double, 32> &b) {
    uint64_t h = 0x8899aabbccddeeffULL;
    double result = 0.0;
    
    for (unsigned i = 0; i < a.size(); ++i) {
        result += a[i] * b[i];
        h = add_d(h, a[i] * b[i]);
    }
    
    h = add_d(h, result);
    return h;
}

// Cross product (3D)
static uint64_t crossProduct(const std::array<double, 3> &a,
                             const std::array<double, 3> &b) {
    uint64_t h = 0x1122334455667788ULL;
    
    std::array<double, 3> result{
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    };
    
    for (unsigned i = 0; i < result.size(); ++i) {
        h = add_d(h, result[i]);
    }
    
    return h;
}

// Linear interpolation
static uint64_t linearInterpolation(const std::array<double, 10> &x,
                                    const std::array<double, 10> &y,
                                    double query) {
    uint64_t h = 0x2233445566778899ULL;
    
    for (unsigned i = 0; i < x.size() - 1; ++i) {
        if (query >= x[i] && query <= x[i + 1]) {
            double t = (query - x[i]) / (x[i + 1] - x[i]);
            double result = y[i] + t * (y[i + 1] - y[i]);
            h = add_d(h, result);
            return h;
        }
    }
    
    return h;
}

// Cumulative sum
static uint64_t cumulativeSum(const std::array<int, 20> &data) {
    uint64_t h = 0x3344556677889900ULL;
    std::array<int, 20> result{};
    result[0] = data[0];
    
    for (unsigned i = 1; i < data.size(); ++i) {
        result[i] = result[i - 1] + data[i];
        h = mix64(h ^ (uint64_t)result[i]);
    }
    
    return h;
}

// Moving average
static uint64_t movingAverage(const std::array<double, 15> &data, int window) {
    uint64_t h = 0x4455667788990011ULL;
    std::array<double, 15> result{};
    
    for (unsigned i = 0; i < data.size(); ++i) {
        double sum = 0.0;
        int count = 0;
        
        for (int j = std::max(0, (int)i - window + 1); j <= (int)i; ++j) {
            sum += data[j];
            count++;
        }
        
        result[i] = sum / count;
        h = add_d(h, result[i]);
    }
    
    return h;
}

// Histogram equalization simulation
static uint64_t histogramEqualization(const std::array<uint8_t, 64> &image) {
    uint64_t h = 0x5566778899001122ULL;
    std::array<int, 256> histogram{};
    histogram.fill(0);
    
    // Build histogram
    for (unsigned i = 0; i < image.size(); ++i) {
        histogram[image[i]]++;
        h = mix64(h ^ (uint64_t)image[i]);
    }
    
    // Compute cumulative distribution
    std::array<int, 256> cdf{};
    cdf[0] = histogram[0];
    for (unsigned i = 1; i < 256; ++i) {
        cdf[i] = cdf[i - 1] + histogram[i];
        h = mix64(h ^ (uint64_t)cdf[i]);
    }
    
    return h;
}

// DCT (Discrete Cosine Transform) simulation
static uint64_t dctSimulation(const std::array<double, 64> &block) {
    uint64_t h = 0x6677889900112233ULL;
    std::array<double, 64> dctCoeffs{};
    
    for (unsigned u = 0; u < 8; ++u) {
        for (unsigned v = 0; v < 8; ++v) {
            double sum = 0.0;
            
            for (unsigned x = 0; x < 8; ++x) {
                for (unsigned y = 0; y < 8; ++y) {
                    double cu = (u == 0) ? 1.0 / std::sqrt(2.0) : 1.0;
                    double cv = (v == 0) ? 1.0 / std::sqrt(2.0) : 1.0;
                    sum += block[x * 8 + y] * cu * cv *
                           std::cos((2.0 * x + 1.0) * u * 3.1415926535 / 16.0) *
                           std::cos((2.0 * y + 1.0) * v * 3.1415926535 / 16.0);
                }
            }
            
            dctCoeffs[u * 8 + v] = 0.25 * sum;
            h = add_d(h, dctCoeffs[u * 8 + v]);
        }
    }
    
    return h;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    // Initialize test data
    std::array<std::array<uint32_t, 8>, 8> a{};
    std::array<std::array<uint32_t, 8>, 8> b{};
    for (unsigned r = 0; r < 8; ++r) {
        for (unsigned c = 0; c < 8; ++c) {
            a[r][c] = (uint32_t)(r * 11 + c * 7 + 13);
            b[r][c] = (uint32_t)(r * 5 + c * 17 + 19);
        }
    }

    std::array<double, 16> coeffs;
    std::array<double, 16> input;
    for (unsigned i = 0; i < coeffs.size(); ++i) {
        coeffs[i] = 1.1 + i * 0.3;
        input[i] = 2.2 + i * 0.5;
    }

    std::array<uint32_t, 32> data;
    for (unsigned i = 0; i < data.size(); ++i)
        data[i] = (uint32_t)(i * i + 7);

    std::array<std::array<double, 4>, 4> butterflyData{};
    for (unsigned i = 0; i < butterflyData.size(); ++i) {
        for (unsigned j = 0; j < butterflyData[i].size(); ++j) {
            butterflyData[i][j] = 1.0 + i * 0.5 + j * 0.3;
        }
    }

    std::array<double, 16> signal;
    std::array<double, 8> kernel;
    for (unsigned i = 0; i < signal.size(); ++i)
        signal[i] = std::sin(0.5 * i);
    for (unsigned i = 0; i < kernel.size(); ++i)
        kernel[i] = 1.0 / (i + 1);

    std::array<int, 16> sortInput;
    for (unsigned i = 0; i < sortInput.size(); ++i)
        sortInput[i] = (int)((i * 13) % 16);

    std::array<std::array<double, 6>, 6> luMatrix{};
    for (unsigned i = 0; i < 6; ++i) {
        for (unsigned j = 0; j < 6; ++j) {
            luMatrix[i][j] = (i == j) ? 4.0 : 1.0;
        }
    }

    std::array<std::array<double, 5>, 5> choleskyMatrix{};
    for (unsigned i = 0; i < 5; ++i) {
        for (unsigned j = 0; j < 5; ++j) {
            choleskyMatrix[i][j] = (i >= j) ? (double)(i + j + 2) : (double)(j + 1);
        }
    }

    std::array<double, 32> vecA, vecB;
    for (unsigned i = 0; i < 32; ++i) {
        vecA[i] = 0.1 * i;
        vecB[i] = 0.2 * i;
    }

    std::array<double, 3> crossA{1.0, 2.0, 3.0};
    std::array<double, 3> crossB{4.0, 5.0, 6.0};

    std::array<double, 10> interpX;
    std::array<double, 10> interpY;
    for (unsigned i = 0; i < 10; ++i) {
        interpX[i] = (double)i;
        interpY[i] = i * i;
    }

    std::array<int, 20> cumsumData;
    for (unsigned i = 0; i < cumsumData.size(); ++i)
        cumsumData[i] = (int)(i * 3 + 1);

    std::array<double, 15> movingData;
    for (unsigned i = 0; i < movingData.size(); ++i)
        movingData[i] = std::sin(0.3 * i);

    std::array<uint8_t, 64> imageData;
    for (unsigned i = 0; i < imageData.size(); ++i)
        imageData[i] = (uint8_t)(i % 256);

    std::array<double, 64> dctBlock;
    for (unsigned i = 0; i < dctBlock.size(); ++i)
        dctBlock[i] = (double)i;

    uint64_t checksum = 0xdeadbeefdeadbeefULL;
    
    // Original tests
    checksum ^= multiplyMatrix(a, b);
    checksum ^= polynomialSweep(coeffs, input, 128, 3.1415926535, 123u);
    checksum ^= computeStridedSum(data, 3u, 0x55aa55aau, 0xffffffffu);
    checksum ^= polynomialSweep(coeffs, input, 64, 2.7182818284, 456u);
    checksum ^= multiplyMatrix(b, a);
    
    // New performance tests
    checksum ^= butterflyComputation(butterflyData, 4);
    checksum ^= convolutionComputation(signal, kernel);
    checksum ^= sortingNetwork(sortInput);
    checksum ^= numericalIntegration(0.0, 3.1415926535, 100);
    checksum ^= monteCarloPi(1000);
    checksum ^= matrixTransposeMultiply(a, b);
    checksum ^= fastExponentiation(1.5, 20);
    checksum ^= gcdComputation(123456789ULL, 987654321ULL);
    checksum ^= primeSieve(100);
    checksum ^= newtonRaphsonSqrt(2.0, 10);
    checksum ^= mandelbrotComputation(-0.7, 0.27015, 100);
    checksum ^= luDecomposition(luMatrix);
    checksum ^= choleskyDecomposition(choleskyMatrix);
    checksum ^= dotProduct(vecA, vecB);
    checksum ^= crossProduct(crossA, crossB);
    checksum ^= linearInterpolation(interpX, interpY, 5.5);
    checksum ^= cumulativeSum(cumsumData);
    checksum ^= movingAverage(movingData, 3);
    checksum ^= histogramEqualization(imageData);
    checksum ^= dctSimulation(dctBlock);
    
    checksum = mix64(checksum);

    std::printf("CHECKSUM: 0x%016llx\n", (unsigned long long)checksum);

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("%ld\n", diff);
    return 0;
}
