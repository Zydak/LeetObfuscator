// test_float.cpp - Floating point operations stress/determinism test
//
// Compile:  g++ -O2 -std=c++17 -fno-exceptions -o test_float test_float.cpp
//
// Design notes for the obfuscator author:
//  - Every "random-looking" value is produced by a fixed, seeded, integer LCG
//    (never std::rand / std::random_device), so the numeric inputs are 100%
//    reproducible.
//  - All iterative numeric methods (Newton's method, series expansions, etc.)
//    run for a FIXED number of iterations rather than "until convergence
//    within epsilon", so control flow never depends on floating point
//    rounding noise.
//  - All output is printed with std::fixed + std::setprecision so textual
//    output is stable.
//  - No UB: no signed overflow, no uninitialized reads, no strict-aliasing
//    violations (memcpy is used for the raw bit-pattern demo), no invalid
//    casts. Floating point divide-by-zero is well-defined (produces +-inf or
//    NaN under IEC 60559, which virtually every real target implements) and
//    is not integer division, so it is not UB.

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <string>
#include <chrono>

#define LEET_IMPLEMENTATION
#include "../Leet.h"

// ---------------------------------------------------------------------------
// Deterministic pseudo-random generator (PCG/LCG style). Never seeded from
// time or hardware entropy, so the sequence is identical on every run.
// ---------------------------------------------------------------------------
struct Lcg {
    uint64_t state;
    explicit Lcg(uint64_t seed) : state(seed) {}

    uint64_t nextRaw() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    }

    // Deterministic double in [0, 1)
    double nextDouble() {
        uint64_t bits = nextRaw() >> 11; // top 53 bits
        return static_cast<double>(bits) * (1.0 / 9007199254740992.0);
    }

    // Deterministic double in [lo, hi)
    double nextInRange(double lo, double hi) {
        return lo + nextDouble() * (hi - lo);
    }
};

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
__attribute__((noinline))
void printResult(const std::string& label, double value, int precision = 6) {
    std::cout << label << ": " << std::fixed << std::setprecision(precision) << value << "\n";
}

__attribute__((noinline))
void printResultI(const std::string& label, long long value) {
    std::cout << label << ": " << value << "\n";
}

__attribute__((noinline))
bool approxEqual(double a, double b, double eps) {
    double diff = a - b;
    if (diff < 0.0) diff = -diff;
    return diff <= eps;
}

__attribute__((noinline))
double roundToDecimals(double value, int decimals) {
    static const double table[7] = {1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0};
    double scale = (decimals >= 0 && decimals <= 6) ? table[decimals] : 1.0;
    double scaled = value * scale;
    double rounded = std::floor(scaled + 0.5);
    return rounded / scale;
}

// ---------------------------------------------------------------------------
// Root finding via fixed-iteration Newton's method (deterministic step count)
// ---------------------------------------------------------------------------
__attribute__((noinline))
double newtonSqrt(double x) {
    if (x <= 0.0) return 0.0;
    double guess = x * 0.5 + 0.5; // arbitrary deterministic starting point
    for (int i = 0; i < 40; ++i) {
        guess = 0.5 * (guess + x / guess);
    }
    return guess;
}

__attribute__((noinline))
double newtonCbrt(double x) {
    if (x == 0.0) return 0.0;
    double sign = (x < 0.0) ? -1.0 : 1.0;
    double a = (x < 0.0) ? -x : x;
    double guess = a * 0.5 + 0.25;
    for (int i = 0; i < 50; ++i) {
        double g2 = guess * guess;
        guess = guess - (guess * g2 - a) / (3.0 * g2);
    }
    return sign * guess;
}

// ---------------------------------------------------------------------------
// Series expansions (fixed term count -> fully deterministic result)
// ---------------------------------------------------------------------------
__attribute__((noinline))
double piLeibniz(int terms) {
    double sum = 0.0;
    double sign = 1.0;
    for (int k = 0; k < terms; ++k) {
        sum += sign / (2.0 * static_cast<double>(k) + 1.0);
        sign = -sign;
    }
    return sum * 4.0;
}

__attribute__((noinline))
double piNilakantha(int terms) {
    double pi = 3.0;
    double sign = 1.0;
    double n = 2.0;
    for (int k = 0; k < terms; ++k) {
        double denom = n * (n + 1.0) * (n + 2.0);
        pi += sign * (4.0 / denom);
        sign = -sign;
        n += 2.0;
    }
    return pi;
}

__attribute__((noinline))
double eulerSeries(int terms) {
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k < terms; ++k) {
        term /= static_cast<double>(k);
        sum += term;
    }
    return sum;
}

__attribute__((noinline))
double ln2Series(int terms) {
    double sum = 0.0;
    double sign = 1.0;
    for (int k = 1; k <= terms; ++k) {
        sum += sign / static_cast<double>(k);
        sign = -sign;
    }
    return sum;
}

// ---------------------------------------------------------------------------
// Trig / hyperbolic identity checks over a fixed set of angles
// ---------------------------------------------------------------------------
__attribute__((noinline))
double trigIdentityChecksum() {
    double checksum = 0.0;
    for (int i = 0; i <= 36; ++i) {
        double angle = (static_cast<double>(i) / 36.0) * 2.0 * M_PI;
        double s = std::sin(angle);
        double c = std::cos(angle);
        double identity = s * s + c * c; // should be ~1.0
        checksum += identity;

        double t = std::tan(angle);
        (void)t; // computed for cost / coverage, not used further to avoid /0 sensitivity

        bool ok = approxEqual(identity, 1.0, 1e-9);
        checksum += ok ? 1.0 : 0.0;
    }
    return checksum;
}

__attribute__((noinline))
double hyperbolicIdentityChecksum() {
    double checksum = 0.0;
    for (int i = -10; i <= 10; ++i) {
        double x = static_cast<double>(i) * 0.3;
        double sh = std::sinh(x);
        double ch = std::cosh(x);
        double identity = ch * ch - sh * sh; // should be ~1.0
        checksum += identity;

        double th = std::tanh(x);
        checksum += th;
    }
    return checksum;
}

// ---------------------------------------------------------------------------
// Statistics over a deterministically generated dataset
// ---------------------------------------------------------------------------
__attribute__((noinline))
struct Stats {
    double mean;
    double variance;
    double stddev;
    double minVal;
    double maxVal;
};

__attribute__((noinline))
std::vector<double> generateDataset(int n) {
    std::vector<double> data;
    data.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        double v = std::sin(static_cast<double>(i) * 0.37) * 50.0 +
                   std::cos(static_cast<double>(i) * 0.13) * 20.0 + 100.0;
        data.push_back(v);
    }
    return data;
}

__attribute__((noinline))
Stats computeStats(const std::vector<double>& data) {
    Stats result{};
    result.mean = 0.0;
    result.minVal = data.empty() ? 0.0 : data[0];
    result.maxVal = data.empty() ? 0.0 : data[0];

    double sum = 0.0;
    for (double v : data) {
        sum += v;
        if (v < result.minVal) result.minVal = v;
        if (v > result.maxVal) result.maxVal = v;
    }
    result.mean = data.empty() ? 0.0 : sum / static_cast<double>(data.size());

    double sqSum = 0.0;
    for (double v : data) {
        double d = v - result.mean;
        sqSum += d * d;
    }
    result.variance = data.empty() ? 0.0 : sqSum / static_cast<double>(data.size());
    result.stddev = newtonSqrt(result.variance);
    return result;
}

// Two-pass variance re-check (naive method), used only as a cross-check value
__attribute__((noinline))
double naiveVariance(const std::vector<double>& data, double mean) {
    double sq = 0.0;
    for (double v : data) {
        sq += (v - mean) * (v - mean);
    }
    return data.empty() ? 0.0 : sq / static_cast<double>(data.size());
}

// ---------------------------------------------------------------------------
// Small linear algebra: Vector3, Matrix4x4
// ---------------------------------------------------------------------------
struct Vector3 {
    double x, y, z;
};

__attribute__((noinline))
Vector3 vecAdd(const Vector3& a, const Vector3& b) {
    return Vector3{a.x + b.x, a.y + b.y, a.z + b.z};
}

__attribute__((noinline))
double vecDot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

__attribute__((noinline))
Vector3 vecCross(const Vector3& a, const Vector3& b) {
    return Vector3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

__attribute__((noinline))
double vecLength(const Vector3& a) {
    return newtonSqrt(vecDot(a, a));
}

__attribute__((noinline))
Vector3 vecNormalize(const Vector3& a) {
    double len = vecLength(a);
    if (len == 0.0) {
        return Vector3{0.0, 0.0, 0.0};
    }
    return Vector3{a.x / len, a.y / len, a.z / len};
}

struct Matrix4x4 {
    double m[4][4];
};

__attribute__((noinline))
Matrix4x4 matIdentity() {
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            result.m[i][j] = (i == j) ? 1.0 : 0.0;
    return result;
}

__attribute__((noinline))
Matrix4x4 matMultiply(const Matrix4x4& a, const Matrix4x4& b) {
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[i][k] * b.m[k][j];
            }
            result.m[i][j] = sum;
        }
    }
    return result;
}

__attribute__((noinline))
Matrix4x4 matTranspose(const Matrix4x4& a) {
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            result.m[j][i] = a.m[i][j];
    return result;
}

__attribute__((noinline))
double matTrace(const Matrix4x4& a) {
    double sum = 0.0;
    for (int i = 0; i < 4; ++i) sum += a.m[i][i];
    return sum;
}

__attribute__((noinline))
double det3(double a, double b, double c,
            double d, double e, double f,
            double g, double h, double i) {
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}

__attribute__((noinline))
double matDeterminant4(const Matrix4x4& mat) {
    const double (&m)[4][4] = mat.m;
    double d0 = det3(m[1][1], m[1][2], m[1][3],
                      m[2][1], m[2][2], m[2][3],
                      m[3][1], m[3][2], m[3][3]);
    double d1 = det3(m[1][0], m[1][2], m[1][3],
                      m[2][0], m[2][2], m[2][3],
                      m[3][0], m[3][2], m[3][3]);
    double d2 = det3(m[1][0], m[1][1], m[1][3],
                      m[2][0], m[2][1], m[2][3],
                      m[3][0], m[3][1], m[3][3]);
    double d3 = det3(m[1][0], m[1][1], m[1][2],
                      m[2][0], m[2][1], m[2][2],
                      m[3][0], m[3][1], m[3][2]);
    return m[0][0] * d0 - m[0][1] * d1 + m[0][2] * d2 - m[0][3] * d3;
}

__attribute__((noinline))
Matrix4x4 makeDeterministicMatrix(Lcg& rng) {
    Matrix4x4 mat{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            mat.m[i][j] = roundToDecimals(rng.nextInRange(-5.0, 5.0), 3);
        }
    }
    return mat;
}

// ---------------------------------------------------------------------------
// Complex numbers
// ---------------------------------------------------------------------------
struct Complex {
    double re, im;
};

__attribute__((noinline))
Complex complexAdd(const Complex& a, const Complex& b) {
    return Complex{a.re + b.re, a.im + b.im};
}

__attribute__((noinline))
Complex complexMul(const Complex& a, const Complex& b) {
    return Complex{a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re};
}

__attribute__((noinline))
Complex complexDiv(const Complex& a, const Complex& b) {
    double denom = b.re * b.re + b.im * b.im;
    if (denom == 0.0) {
        return Complex{0.0, 0.0};
    }
    return Complex{
        (a.re * b.re + a.im * b.im) / denom,
        (a.im * b.re - a.re * b.im) / denom
    };
}

__attribute__((noinline))
double complexMagnitude(const Complex& a) {
    return newtonSqrt(a.re * a.re + a.im * a.im);
}

// ---------------------------------------------------------------------------
// Polynomial evaluation via Horner's method
// ---------------------------------------------------------------------------
__attribute__((noinline))
double hornerEval(const std::vector<double>& coeffs, double x) {
    // coeffs[0] is the highest degree coefficient
    double result = 0.0;
    for (double c : coeffs) {
        result = result * x + c;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Numerical integration
// ---------------------------------------------------------------------------
__attribute__((noinline))
double integrateTrapezoid(double a, double b, int steps) {
    double h = (b - a) / static_cast<double>(steps);
    double sum = 0.5 * (std::sin(a) + std::sin(b));
    for (int i = 1; i < steps; ++i) {
        double x = a + static_cast<double>(i) * h;
        sum += std::sin(x);
    }
    return sum * h;
}

__attribute__((noinline))
double integrateSimpson(double a, double b, int steps) {
    if (steps % 2 != 0) steps += 1; // Simpson requires even interval count
    double h = (b - a) / static_cast<double>(steps);
    double sum = a * a + b * b; // f(x) = x^2 endpoints
    for (int i = 1; i < steps; ++i) {
        double x = a + static_cast<double>(i) * h;
        double fx = x * x;
        sum += (i % 2 == 0) ? 2.0 * fx : 4.0 * fx;
    }
    return sum * h / 3.0;
}

// ---------------------------------------------------------------------------
// Fixed point conversion
// ---------------------------------------------------------------------------
__attribute__((noinline))
int64_t doubleToFixedPoint(double value, int scaleBits) {
    double scale = static_cast<double>(1LL << scaleBits);
    double scaled = value * scale;
    double rounded = std::floor(scaled + 0.5);
    return static_cast<int64_t>(rounded);
}

__attribute__((noinline))
double fixedPointToDouble(int64_t fixed, int scaleBits) {
    double scale = static_cast<double>(1LL << scaleBits);
    return static_cast<double>(fixed) / scale;
}

// ---------------------------------------------------------------------------
// Raw bit-pattern inspection (uses memcpy, never reinterpret_cast, to avoid
// strict-aliasing UB)
// ---------------------------------------------------------------------------
__attribute__((noinline))
uint64_t doubleBits(double value) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// ---------------------------------------------------------------------------
// Float classification
// ---------------------------------------------------------------------------
__attribute__((noinline))
double classificationChecksum() {
    double checksum = 0.0;

    double posInf = 1.0 / 0.0;   // well-defined for floating point, gives +inf
    double negInf = -1.0 / 0.0;  // gives -inf
    double nanVal = 0.0 / 0.0;   // gives NaN under IEC 60559

    checksum += std::isinf(posInf) ? 1.0 : 0.0;
    checksum += std::isinf(negInf) ? 1.0 : 0.0;
    checksum += std::isnan(nanVal) ? 1.0 : 0.0;
    checksum += std::isnormal(1.5) ? 1.0 : 0.0;
    checksum += std::isnormal(0.0) ? 1.0 : 0.0; // zero is not "normal"
    checksum += std::signbit(-0.0) ? 1.0 : 0.0;
    checksum += std::signbit(0.0) ? 1.0 : 0.0;

    return checksum;
}

// ---------------------------------------------------------------------------
// Golden ratio via Fibonacci term ratio (fixed iteration count)
// ---------------------------------------------------------------------------
__attribute__((noinline))
double goldenRatioApprox(int iterations) {
    double a = 1.0;
    double b = 1.0;
    for (int i = 0; i < iterations; ++i) {
        double next = a + b;
        a = b;
        b = next;
    }
    return b / a;
}

// ---------------------------------------------------------------------------
// Rounding function demonstration
// ---------------------------------------------------------------------------
__attribute__((noinline))
double roundingChecksum() {
    double checksum = 0.0;
    double values[8] = {2.3, 2.5, 2.7, -2.3, -2.5, -2.7, 0.5, -0.5};
    for (double v : values) {
        checksum += std::floor(v);
        checksum += std::ceil(v);
        checksum += std::round(v);
        checksum += std::trunc(v);
    }
    return checksum;
}

// ---------------------------------------------------------------------------
// Linear interpolation
// ---------------------------------------------------------------------------
__attribute__((noinline))
double lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

__attribute__((noinline))
double lerpChecksum() {
    double checksum = 0.0;
    for (int i = 0; i <= 10; ++i) {
        double t = static_cast<double>(i) / 10.0;
        checksum += lerp(-100.0, 250.0, t);
    }
    return checksum;
}

// ---------------------------------------------------------------------------
// Weighted dot product / projection over fixed vectors
// ---------------------------------------------------------------------------
__attribute__((noinline))
double weightedDotProduct(const std::array<double, 10>& a, const std::array<double, 10>& b,
                           const std::array<double, 10>& weights) {
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        sum += a[i] * b[i] * weights[i];
    }
    return sum;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    auto start = std::chrono::high_resolution_clock::now();

    double checksum = 0.0;

    std::cout << "=== Floating Point Operations Test ===\n\n";

    // Newton's method root finding
    std::cout << "-- Root finding --\n";
    for (int i = 1; i <= 10; ++i) {
        double x = static_cast<double>(i) * 3.5;
        double sq = newtonSqrt(x);
        double cb = newtonCbrt(x);
        printResult("sqrt(" + std::to_string(i) + " * 3.5)", sq);
        printResult("cbrt(" + std::to_string(i) + " * 3.5)", cb);
        checksum += sq + cb;
    }

    // Series expansions
    std::cout << "\n-- Series expansions --\n";
    double piL = piLeibniz(200000);
    double piN = piNilakantha(2000);
    double eSeries = eulerSeries(30);
    double ln2 = ln2Series(200000);
    printResult("pi (Leibniz, 200000 terms)", piL);
    printResult("pi (Nilakantha, 2000 terms)", piN);
    printResult("e (series, 30 terms)", eSeries);
    printResult("ln(2) (alternating harmonic, 200000 terms)", ln2);
    checksum += piL + piN + eSeries + ln2;

    // Trig / hyperbolic identities
    std::cout << "\n-- Identity checks --\n";
    double trigChk = trigIdentityChecksum();
    double hypChk = hyperbolicIdentityChecksum();
    printResult("trig identity checksum", trigChk);
    printResult("hyperbolic identity checksum", hypChk);
    checksum += trigChk + hypChk;

    // Statistics
    std::cout << "\n-- Statistics --\n";
    std::vector<double> dataset = generateDataset(500);
    Stats stats = computeStats(dataset);
    double naiveVar = naiveVariance(dataset, stats.mean);
    printResult("dataset mean", stats.mean);
    printResult("dataset variance", stats.variance);
    printResult("dataset variance (naive cross-check)", naiveVar);
    printResult("dataset stddev", stats.stddev);
    printResult("dataset min", stats.minVal);
    printResult("dataset max", stats.maxVal);
    checksum += stats.mean + stats.variance + stats.stddev + stats.minVal + stats.maxVal;

    // Linear algebra
    std::cout << "\n-- Linear algebra --\n";
    Lcg rng(0x9E3779B97F4A7C15ULL);
    Vector3 v1{1.0, 2.0, 3.0};
    Vector3 v2{4.0, -5.0, 6.0};
    Vector3 sum = vecAdd(v1, v2);
    double dot = vecDot(v1, v2);
    Vector3 cross = vecCross(v1, v2);
    Vector3 normalized = vecNormalize(v1);
    printResult("v1+v2 x", sum.x);
    printResult("v1+v2 y", sum.y);
    printResult("v1+v2 z", sum.z);
    printResult("v1 . v2", dot);
    printResult("v1 x v2 (x)", cross.x);
    printResult("v1 x v2 (y)", cross.y);
    printResult("v1 x v2 (z)", cross.z);
    printResult("normalize(v1) x", normalized.x);
    checksum += sum.x + sum.y + sum.z + dot + cross.x + cross.y + cross.z + normalized.x;

    Matrix4x4 matA = makeDeterministicMatrix(rng);
    Matrix4x4 matB = makeDeterministicMatrix(rng);
    Matrix4x4 product = matMultiply(matA, matB);
    Matrix4x4 transposed = matTranspose(matA);
    double trace = matTrace(product);
    double determinant = matDeterminant4(matA);
    printResult("matrix product trace", trace);
    printResult("matrix A determinant", determinant);
    printResult("matrix A transposed [0][1]", transposed.m[0][1]);
    checksum += trace + determinant + transposed.m[0][1];

    Matrix4x4 identity = matIdentity();
    Matrix4x4 idProduct = matMultiply(matA, identity);
    bool identityOk = approxEqual(idProduct.m[2][2], matA.m[2][2], 1e-9);
    printResultI("A * I == A (sanity check)", identityOk ? 1 : 0);
    checksum += identityOk ? 1.0 : 0.0;

    // Complex numbers
    std::cout << "\n-- Complex numbers --\n";
    Complex c1{3.0, 4.0};
    Complex c2{1.0, -2.0};
    Complex cSum = complexAdd(c1, c2);
    Complex cProd = complexMul(c1, c2);
    Complex cQuot = complexDiv(c1, c2);
    double cMag = complexMagnitude(c1);
    printResult("c1+c2 re", cSum.re);
    printResult("c1+c2 im", cSum.im);
    printResult("c1*c2 re", cProd.re);
    printResult("c1*c2 im", cProd.im);
    printResult("c1/c2 re", cQuot.re);
    printResult("c1/c2 im", cQuot.im);
    printResult("|c1|", cMag);
    checksum += cSum.re + cSum.im + cProd.re + cProd.im + cQuot.re + cQuot.im + cMag;

    // Polynomial evaluation
    std::cout << "\n-- Polynomial evaluation (Horner) --\n";
    std::vector<double> coeffs{2.0, -3.0, 0.5, 7.0, -1.0}; // 2x^4 -3x^3 +0.5x^2 +7x -1
    for (int i = -3; i <= 3; ++i) {
        double x = static_cast<double>(i);
        double val = hornerEval(coeffs, x);
        printResult("P(" + std::to_string(i) + ")", val);
        checksum += val;
    }

    // Numerical integration
    std::cout << "\n-- Numerical integration --\n";
    double trapResult = integrateTrapezoid(0.0, M_PI, 100000);
    double simpsonResult = integrateSimpson(0.0, 10.0, 100000);
    printResult("integral of sin(x) over [0, pi] (trapezoid)", trapResult);
    printResult("integral of x^2 over [0, 10] (simpson)", simpsonResult);
    checksum += trapResult + simpsonResult;

    // Fixed point conversion
    std::cout << "\n-- Fixed point conversion --\n";
    for (int i = 0; i < 5; ++i) {
        double original = 3.14159265 * static_cast<double>(i + 1);
        int64_t fixed = doubleToFixedPoint(original, 16);
        double roundTrip = fixedPointToDouble(fixed, 16);
        printResult("original", original);
        printResultI("fixed (Q16)", static_cast<long long>(fixed));
        printResult("round-trip", roundTrip);
        checksum += roundTrip;
    }

    // Bit pattern inspection
    std::cout << "\n-- Bit pattern inspection --\n";
    double sample = 1.5;
    uint64_t bits = doubleBits(sample);
    std::cout << "bit pattern of 1.5: 0x" << std::hex << std::setw(16) << std::setfill('0')
              << bits << std::dec << std::setfill(' ') << "\n";
    checksum += static_cast<double>(bits % 1000000ULL);

    // Classification
    std::cout << "\n-- Float classification --\n";
    double clsChk = classificationChecksum();
    printResult("classification checksum", clsChk);
    checksum += clsChk;

    // Golden ratio
    std::cout << "\n-- Golden ratio approximation --\n";
    double golden = goldenRatioApprox(60);
    printResult("golden ratio approx (60 Fibonacci steps)", golden, 10);
    checksum += golden;

    // Rounding
    std::cout << "\n-- Rounding functions --\n";
    double roundChk = roundingChecksum();
    printResult("rounding checksum", roundChk);
    checksum += roundChk;

    // Interpolation
    std::cout << "\n-- Linear interpolation --\n";
    double lerpChk = lerpChecksum();
    printResult("lerp checksum", lerpChk);
    checksum += lerpChk;

    // Weighted dot product
    std::cout << "\n-- Weighted dot product --\n";
    std::array<double, 10> va{}, vb{}, vw{};
    for (size_t i = 0; i < 10; ++i) {
        va[i] = static_cast<double>(i) + 1.0;
        vb[i] = static_cast<double>(10 - i);
        vw[i] = 0.1 * static_cast<double>(i + 1);
    }
    double wdp = weightedDotProduct(va, vb, vw);
    printResult("weighted dot product", wdp);
    checksum += wdp;

    // Final checksum
    std::cout << "\n=== Final checksum ===\n";
    printResult("CHECKSUM", checksum, 4);

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << diff << "\n";

    return 0;
}