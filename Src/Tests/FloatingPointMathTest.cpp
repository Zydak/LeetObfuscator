// Floating-point math stress test for the Leet obfuscator.
// Focuses on complex floating-point operations including trigonometry,
// logarithms, exponentiation, and precision-sensitive computations.
// Deterministic output for verification.

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
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

// Basic arithmetic operations
static uint64_t basicArithmetic(double a, double b) {
    uint64_t h = 0xdeadbeefcafebabeULL;
    
    h = add_d(h, a + b);
    h = add_d(h, a - b);
    h = add_d(h, a * b);
    
    if (b != 0.0) {
        h = add_d(h, a / b);
    }
    
    h = add_d(h, std::fmod(a, b));
    
    return h;
}

// Trigonometric functions
static uint64_t trigonometricFunctions(double angle) {
    uint64_t h = 0xfeedfacebadc0ffeULL;
    
    h = add_d(h, std::sin(angle));
    h = add_d(h, std::cos(angle));
    h = add_d(h, std::tan(angle));
    
    if (std::cos(angle) != 0.0) {
        h = add_d(h, 1.0 / std::tan(angle));
    }
    
    h = add_d(h, std::asin(std::sin(angle * 0.5)));
    h = add_d(h, std::acos(std::cos(angle * 0.5)));
    h = add_d(h, std::atan(std::tan(angle * 0.3)));
    
    return h;
}

// Hyperbolic functions
static uint64_t hyperbolicFunctions(double value) {
    uint64_t h = 0x0badf00d1337c0deULL;
    
    h = add_d(h, std::sinh(value));
    h = add_d(h, std::cosh(value));
    h = add_d(h, std::tanh(value));
    
    if (value > 1.0) {
        h = add_d(h, std::asinh(value));
    }
    
    if (value >= 1.0) {
        h = add_d(h, std::acosh(value));
    }
    
    if (value < 1.0 && value > -1.0) {
        h = add_d(h, std::atanh(value));
    }
    
    return h;
}

// Exponential and logarithmic functions
static uint64_t exponentialLogarithmic(double value) {
    uint64_t h = 0xcafebabedeadfaceULL;
    
    h = add_d(h, std::exp(value));
    h = add_d(h, std::exp2(value));
    h = add_d(h, std::expm1(value));
    
    if (value > 0.0) {
        h = add_d(h, std::log(value));
        h = add_d(h, std::log10(value));
        h = add_d(h, std::log2(value));
        h = add_d(h, std::log1p(value));
    }
    
    return h;
}

// Power functions
static uint64_t powerFunctions(double base, double exponent) {
    uint64_t h = 0xfacefeed0ddba11eULL;
    
    h = add_d(h, std::pow(base, exponent));
    h = add_d(h, std::sqrt(base));
    
    if (base >= 0.0) {
        h = add_d(h, std::cbrt(base));
    }
    
    h = add_d(h, std::hypot(base, exponent));
    
    return h;
}

// Rounding functions
static uint64_t roundingFunctions(double value) {
    uint64_t h = 0xbaddcafebabe1337ULL;
    
    h = add_d(h, std::ceil(value));
    h = add_d(h, std::floor(value));
    h = add_d(h, std::trunc(value));
    h = add_d(h, std::round(value));
    
    return h;
}

// Absolute and difference functions
static uint64_t absoluteDifference(double a, double b) {
    uint64_t h = 0xc0dedeadbadf00dULL;
    
    h = add_d(h, std::fabs(a));
    h = add_d(h, std::fabs(b));
    h = add_d(h, std::fabs(a - b));
    h = add_d(h, std::fdim(a, b));
    
    return h;
}

// Minimum and maximum functions
static uint64_t minMaxFunctions(double a, double b, double c) {
    uint64_t h = 0xfeedfacedeadbeefULL;
    
    h = add_d(h, std::fmin(a, b));
    h = add_d(h, std::fmax(a, b));
    h = add_d(h, std::fmin(a, std::fmin(b, c)));
    h = add_d(h, std::fmax(a, std::fmax(b, c)));
    
    return h;
}

// Classification functions
static uint64_t classificationFunctions(double value) {
    uint64_t h = 0xfaceface12345678ULL;
    
    h = mix64(h ^ (uint64_t)std::isfinite(value));
    h = mix64(h ^ (uint64_t)std::isinf(value));
    h = mix64(h ^ (uint64_t)std::isnan(value));
    h = mix64(h ^ (uint64_t)std::isnormal(value));
    
    return h;
}

// Sign manipulation
static uint64_t signManipulation(double value) {
    uint64_t h = 0xdead1234face5678ULL;
    
    h = add_d(h, std::copysign(1.0, value));
    h = add_d(h, std::copysign(2.0, value));
    h = add_d(h, std::copysign(value, 1.0));
    h = add_d(h, std::copysign(value, -1.0));
    
    return h;
}

// Error functions
static uint64_t errorFunctions(double value) {
    uint64_t h = 0xcafe1234dead5678ULL;
    
    h = add_d(h, std::erf(value));
    h = add_d(h, std::erfc(value));
    
    return h;
}

// Gamma functions
static uint64_t gammaFunctions(double value) {
    uint64_t h = 0x1337deadbeef1337ULL;
    
    if (value > 0.0) {
        h = add_d(h, std::tgamma(value));
        h = add_d(h, std::lgamma(value));
    }
    
    return h;
}

// Bessel functions
static uint64_t besselFunctions(double value) {
    uint64_t h = 0xfacedeadbeefcafeULL;
    
    h = add_d(h, j0(value));
    h = add_d(h, j1(value));
    h = add_d(h, y0(value));
    h = add_d(h, y1(value));
    
    return h;
}

// Complex arithmetic simulation
static uint64_t complexArithmetic(double ar, double ai, double br, double bi) {
    uint64_t h = 0xdeadcafe12345678ULL;
    
    // Complex addition: (a + bi) + (c + di) = (a+c) + (b+d)i
    double addReal = ar + br;
    double addImag = ai + bi;
    h = add_d(h, addReal);
    h = add_d(h, addImag);
    
    // Complex subtraction: (a + bi) - (c + di) = (a-c) + (b-d)i
    double subReal = ar - br;
    double subImag = ai - bi;
    h = add_d(h, subReal);
    h = add_d(h, subImag);
    
    // Complex multiplication: (a + bi)(c + di) = (ac-bd) + (ad+bc)i
    double mulReal = ar * br - ai * bi;
    double mulImag = ar * bi + ai * br;
    h = add_d(h, mulReal);
    h = add_d(h, mulImag);
    
    // Complex magnitude: |a + bi| = sqrt(a^2 + b^2)
    double magnitudeA = std::sqrt(ar * ar + ai * ai);
    double magnitudeB = std::sqrt(br * br + bi * bi);
    h = add_d(h, magnitudeA);
    h = add_d(h, magnitudeB);
    
    return h;
}

// Polynomial evaluation
static uint64_t polynomialEvaluation(const std::array<double, 10> &coeffs, double x) {
    uint64_t h = 0xbeefdeadface1234ULL;
    double result = 0.0;
    
    // Horner's method
    for (int i = (int)coeffs.size() - 1; i >= 0; --i) {
        result = result * x + coeffs[i];
    }
    
    h = add_d(h, result);
    
    // Direct evaluation
    double directResult = 0.0;
    for (unsigned i = 0; i < coeffs.size(); ++i) {
        directResult += coeffs[i] * std::pow(x, (double)i);
    }
    
    h = add_d(h, directResult);
    
    return h;
}

// Numerical differentiation
static uint64_t numericalDifferentiation(double (*func)(double), double x, double h) {
    uint64_t checksum = 0xcafebeefdeadfaceULL;
    
    // Central difference
    double derivative = (func(x + h) - func(x - h)) / (2.0 * h);
    checksum = add_d(checksum, derivative);
    
    // Forward difference
    double forwardDerivative = (func(x + h) - func(x)) / h;
    checksum = add_d(checksum, forwardDerivative);
    
    // Backward difference
    double backwardDerivative = (func(x) - func(x - h)) / h;
    checksum = add_d(checksum, backwardDerivative);
    
    return checksum;
}

// Helper function for differentiation
static double testFunction(double x) {
    return std::sin(x) * std::cos(x) + std::exp(x * 0.1);
}

// Numerical integration (Simpson's rule)
static uint64_t numericalIntegration(double (*func)(double), double a, double b, int n) {
    uint64_t h = 0xfeeddeadcafebeefULL;
    double step = (b - a) / n;
    double sum = 0.0;
    
    for (int i = 0; i <= n; ++i) {
        double x = a + i * step;
        double fx = func(x);
        
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

// Root finding (Newton-Raphson)
static uint64_t newtonRaphson(double (*func)(double), double (*deriv)(double), 
                              double initial, int iterations) {
    uint64_t h = 0x0badf00d12345678ULL;
    double x = initial;
    
    for (int i = 0; i < iterations; ++i) {
        double fx = func(x);
        double dfx = deriv(x);
        
        if (std::abs(dfx) < 1e-10) break;
        
        x = x - fx / dfx;
        h = add_d(h, x);
    }
    
    h = add_d(h, x);
    return h;
}

// Helper functions for Newton-Raphson
static double nrFunction(double x) {
    return x * x - 2.0;
}

static double nrDerivative(double x) {
    return 2.0 * x;
}

// Matrix operations (floating-point)
static uint64_t matrixOperations(const std::array<std::array<double, 4>, 4> &a,
                                 const std::array<std::array<double, 4>, 4> &b) {
    uint64_t h = 0xfacefacecafe1234ULL;
    
    // Matrix addition
    std::array<std::array<double, 4>, 4> sum{};
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned j = 0; j < 4; ++j) {
            sum[i][j] = a[i][j] + b[i][j];
            h = add_d(h, sum[i][j]);
        }
    }
    
    // Matrix multiplication
    std::array<std::array<double, 4>, 4> product{};
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned j = 0; j < 4; ++j) {
            for (unsigned k = 0; k < 4; ++k) {
                product[i][j] += a[i][k] * b[k][j];
            }
            h = add_d(h, product[i][j]);
        }
    }
    
    // Matrix transpose
    std::array<std::array<double, 4>, 4> transpose{};
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned j = 0; j < 4; ++j) {
            transpose[i][j] = a[j][i];
            h = add_d(h, transpose[i][j]);
        }
    }
    
    return h;
}

// Vector operations
static uint64_t vectorOperations(const std::array<double, 8> &a,
                                const std::array<double, 8> &b) {
    uint64_t h = 0xdeadbeeffacefaceULL;
    
    // Dot product
    double dotProduct = 0.0;
    for (unsigned i = 0; i < a.size(); ++i) {
        dotProduct += a[i] * b[i];
    }
    h = add_d(h, dotProduct);
    
    // Element-wise operations
    for (unsigned i = 0; i < a.size(); ++i) {
        h = add_d(h, a[i] + b[i]);
        h = add_d(h, a[i] - b[i]);
        h = add_d(h, a[i] * b[i]);
        
        if (b[i] != 0.0) {
            h = add_d(h, a[i] / b[i]);
        }
    }
    
    // Vector magnitude
    double magnitudeA = 0.0;
    double magnitudeB = 0.0;
    for (unsigned i = 0; i < a.size(); ++i) {
        magnitudeA += a[i] * a[i];
        magnitudeB += b[i] * b[i];
    }
    h = add_d(h, std::sqrt(magnitudeA));
    h = add_d(h, std::sqrt(magnitudeB));
    
    return h;
}

// Statistical functions
static uint64_t statisticalFunctions(const std::array<double, 20> &data) {
    uint64_t h = 0xcafebabedead1234ULL;
    
    // Mean
    double sum = 0.0;
    for (double value : data) {
        sum += value;
    }
    double mean = sum / data.size();
    h = add_d(h, mean);
    
    // Variance
    double variance = 0.0;
    for (double value : data) {
        variance += (value - mean) * (value - mean);
    }
    variance /= data.size();
    h = add_d(h, variance);
    
    // Standard deviation
    double stdDev = std::sqrt(variance);
    h = add_d(h, stdDev);
    
    // Min and max
    double minVal = data[0];
    double maxVal = data[0];
    for (double value : data) {
        minVal = std::min(minVal, value);
        maxVal = std::max(maxVal, value);
    }
    h = add_d(h, minVal);
    h = add_d(h, maxVal);
    
    // Median
    std::array<double, 20> sorted = data;
    std::sort(sorted.begin(), sorted.end());
    double median;
    if (sorted.size() % 2 == 0) {
        median = (sorted[sorted.size() / 2 - 1] + sorted[sorted.size() / 2]) / 2.0;
    } else {
        median = sorted[sorted.size() / 2];
    }
    h = add_d(h, median);
    
    return h;
}

// Interpolation
static uint64_t interpolation(const std::array<double, 10> &x,
                             const std::array<double, 10> &y,
                             double query) {
    uint64_t h = 0xfeedfacecafe5678ULL;
    
    // Linear interpolation
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

// Distance calculations
static uint64_t distanceCalculations(double x1, double y1, double z1,
                                     double x2, double y2, double z2) {
    uint64_t h = 0xdeadcafeface1234ULL;
    
    // Euclidean distance (2D)
    double dist2D = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    h = add_d(h, dist2D);
    
    // Euclidean distance (3D)
    double dist3D = std::sqrt((x2 - x1) * (x2 - x1) + 
                              (y2 - y1) * (y2 - y1) + 
                              (z2 - z1) * (z2 - z1));
    h = add_d(h, dist3D);
    
    // Manhattan distance (2D)
    double manhattan2D = std::abs(x2 - x1) + std::abs(y2 - y1);
    h = add_d(h, manhattan2D);
    
    // Manhattan distance (3D)
    double manhattan3D = std::abs(x2 - x1) + std::abs(y2 - y1) + std::abs(z2 - z1);
    h = add_d(h, manhattan3D);
    
    return h;
}

// Angle calculations
static uint64_t angleCalculations(double x1, double y1, double x2, double y2) {
    uint64_t h = 0xbeefdeadcafe5678ULL;
    
    // Dot product
    double dotProduct = x1 * x2 + y1 * y2;
    h = add_d(h, dotProduct);
    
    // Cross product (2D)
    double crossProduct = x1 * y2 - y1 * x2;
    h = add_d(h, crossProduct);
    
    // Angle between vectors
    double magnitude1 = std::sqrt(x1 * x1 + y1 * y1);
    double magnitude2 = std::sqrt(x2 * x2 + y2 * y2);
    
    if (magnitude1 > 0.0 && magnitude2 > 0.0) {
        double cosAngle = dotProduct / (magnitude1 * magnitude2);
        cosAngle = std::max(-1.0, std::min(1.0, cosAngle)); // Clamp
        double angle = std::acos(cosAngle);
        h = add_d(h, angle);
    }
    
    return h;
}

// Coordinate transformations
static uint64_t coordinateTransformations(double x, double y, double angle) {
    uint64_t h = 0xfacefeeddead5678ULL;
    
    // Rotation
    double cosA = std::cos(angle);
    double sinA = std::sin(angle);
    
    double rotatedX = x * cosA - y * sinA;
    double rotatedY = x * sinA + y * cosA;
    
    h = add_d(h, rotatedX);
    h = add_d(h, rotatedY);
    
    // Scaling
    double scaleX = 2.0;
    double scaleY = 3.0;
    
    double scaledX = x * scaleX;
    double scaledY = y * scaleY;
    
    h = add_d(h, scaledX);
    h = add_d(h, scaledY);
    
    // Translation
    double translateX = 5.0;
    double translateY = 10.0;
    
    double translatedX = x + translateX;
    double translatedY = y + translateY;
    
    h = add_d(h, translatedX);
    h = add_d(h, translatedY);
    
    return h;
}

// Polar to Cartesian conversion
static uint64_t polarToCartesian(double radius, double angle) {
    uint64_t h = 0x13371337deadfaceULL;
    
    double x = radius * std::cos(angle);
    double y = radius * std::sin(angle);
    
    h = add_d(h, x);
    h = add_d(h, y);
    
    return h;
}

// Cartesian to Polar conversion
static uint64_t cartesianToPolar(double x, double y) {
    uint64_t h = 0xc0dec0dedeadbeefULL;
    
    double radius = std::sqrt(x * x + y * y);
    double angle = std::atan2(y, x);
    
    h = add_d(h, radius);
    h = add_d(h, angle);
    
    return h;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    // Initialize test data
    std::array<double, 10> polyCoeffs;
    for (unsigned i = 0; i < polyCoeffs.size(); ++i)
        polyCoeffs[i] = (i % 2 == 0) ? 1.0 : -1.0;

    std::array<std::array<double, 4>, 4> matrixA{};
    std::array<std::array<double, 4>, 4> matrixB{};
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned j = 0; j < 4; ++j) {
            matrixA[i][j] = 1.0 + i * 0.5 + j * 0.3;
            matrixB[i][j] = 2.0 + i * 0.3 + j * 0.5;
        }
    }

    std::array<double, 8> vectorA, vectorB;
    for (unsigned i = 0; i < 8; ++i) {
        vectorA[i] = 0.5 * i;
        vectorB[i] = 0.3 * i;
    }

    std::array<double, 20> statisticalData;
    for (unsigned i = 0; i < statisticalData.size(); ++i)
        statisticalData[i] = std::sin(0.5 * i) + std::cos(0.3 * i);

    std::array<double, 10> interpX, interpY;
    for (unsigned i = 0; i < 10; ++i) {
        interpX[i] = (double)i;
        interpY[i] = i * i;
    }

    uint64_t checksum = 0xdeadbeefdeadbeefULL;
    
    // Run all floating-point math tests
    checksum ^= basicArithmetic(3.5, 2.7);
    checksum ^= basicArithmetic(10.0, 4.0);
    checksum ^= trigonometricFunctions(1.5);
    checksum ^= trigonometricFunctions(2.5);
    checksum ^= hyperbolicFunctions(1.0);
    checksum ^= hyperbolicFunctions(2.0);
    checksum ^= exponentialLogarithmic(2.5);
    checksum ^= exponentialLogarithmic(1.5);
    checksum ^= powerFunctions(4.0, 2.0);
    checksum ^= powerFunctions(8.0, 3.0);
    checksum ^= roundingFunctions(3.7);
    checksum ^= roundingFunctions(-2.3);
    checksum ^= absoluteDifference(5.5, 3.2);
    checksum ^= absoluteDifference(-4.1, 2.8);
    checksum ^= minMaxFunctions(3.5, 7.2, 1.9);
    checksum ^= minMaxFunctions(10.0, 5.0, 15.0);
    checksum ^= classificationFunctions(42.0);
    checksum ^= classificationFunctions(std::sqrt(-1.0));
    checksum ^= signManipulation(-5.5);
    checksum ^= signManipulation(3.2);
    checksum ^= errorFunctions(1.0);
    checksum ^= errorFunctions(2.0);
    checksum ^= gammaFunctions(2.0);
    checksum ^= gammaFunctions(5.0);
    checksum ^= besselFunctions(1.5);
    checksum ^= besselFunctions(2.5);
    checksum ^= complexArithmetic(3.0, 4.0, 1.0, 2.0);
    checksum ^= complexArithmetic(5.0, 2.0, 3.0, 1.0);
    checksum ^= polynomialEvaluation(polyCoeffs, 2.5);
    checksum ^= polynomialEvaluation(polyCoeffs, 1.5);
    checksum ^= numericalDifferentiation(testFunction, 1.5, 0.001);
    checksum ^= numericalDifferentiation(testFunction, 2.5, 0.0001);
    checksum ^= numericalIntegration(testFunction, 0.0, 3.1415926535, 100);
    checksum ^= numericalIntegration(testFunction, 0.0, 2.7182818284, 50);
    checksum ^= newtonRaphson(nrFunction, nrDerivative, 1.5, 10);
    checksum ^= newtonRaphson(nrFunction, nrDerivative, 2.0, 15);
    checksum ^= matrixOperations(matrixA, matrixB);
    checksum ^= vectorOperations(vectorA, vectorB);
    checksum ^= statisticalFunctions(statisticalData);
    checksum ^= interpolation(interpX, interpY, 5.5);
    checksum ^= distanceCalculations(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    checksum ^= distanceCalculations(0.0, 0.0, 0.0, 3.0, 4.0, 5.0);
    checksum ^= angleCalculations(1.0, 0.0, 0.0, 1.0);
    checksum ^= angleCalculations(1.0, 1.0, -1.0, 1.0);
    checksum ^= coordinateTransformations(3.0, 4.0, 0.7853981634);
    checksum ^= coordinateTransformations(5.0, 2.0, 1.5707963268);
    checksum ^= polarToCartesian(5.0, 0.7853981634);
    checksum ^= polarToCartesian(10.0, 1.5707963268);
    checksum ^= cartesianToPolar(3.0, 4.0);
    checksum ^= cartesianToPolar(1.0, 1.0);
    
    checksum = mix64(checksum);

    std::printf("CHECKSUM: 0x%016llx\n", (unsigned long long)checksum);

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("%ld\n", diff);
    return 0;
}