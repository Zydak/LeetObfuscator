#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>
#include <cstdint>

#define LEET_IMPLEMENTATION
#include "../Leet.h"

struct Lcg {
    uint64_t state;
    explicit Lcg(uint64_t seed) : state(seed) {}
    uint64_t nextRaw() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    }
    int nextInt(int lo, int hi) {
        uint64_t range = static_cast<uint64_t>(hi - lo + 1);
        return lo + static_cast<int>(nextRaw() % range);
    }
};

__attribute__((noinline))
long long runSieve(size_t limit) {
    std::vector<uint8_t> isComposite(limit + 1, 0);
    long long primeCount = 0;

    for (size_t i = 2; i <= limit; ++i) {
        if (isComposite[i] == 0) {
            ++primeCount;
            if (i <= limit / i) {
                for (size_t j = i * i; j <= limit; j += i) {
                    isComposite[j] = 1;
                }
            }
        }
    }
    return primeCount;
}

using Matrix = std::vector<std::vector<int64_t>>;

__attribute__((noinline))
Matrix makeDeterministicMatrix(int n, Lcg& rng) {
    Matrix mat(static_cast<size_t>(n), std::vector<int64_t>(static_cast<size_t>(n), 0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            mat[static_cast<size_t>(i)][static_cast<size_t>(j)] = rng.nextInt(-9, 9);
        }
    }
    return mat;
}

__attribute__((noinline))
Matrix multiplyMatrices(const Matrix& a, const Matrix& b) {
    size_t n = a.size();
    Matrix result(n, std::vector<int64_t>(n, 0));
    for (size_t i = 0; i < n; ++i) {
        for (size_t k = 0; k < n; ++k) {
            int64_t aik = a[i][k];
            if (aik == 0) continue;
            for (size_t j = 0; j < n; ++j) {
                result[i][j] += aik * b[k][j];
            }
        }
    }
    return result;
}

__attribute__((noinline))
int64_t matrixChecksum(const Matrix& m) {
    int64_t sum = 0;
    for (const auto& row : m) {
        for (int64_t v : row) {
            sum += v;
        }
    }
    return sum;
}

__attribute__((noinline))
bool isPrimeTrialDivision(long long n) {
    if (n < 2) return false;
    if (n % 2 == 0) return n == 2;
    if (n % 3 == 0) return n == 3;
    for (long long d = 5; d * d <= n; d += 6) {
        if (n % d == 0) return false;
        if (n % (d + 2) == 0) return false;
    }
    return true;
}

__attribute__((noinline))
long long countPrimesInRange(long long limit) {
    long long count = 0;
    for (long long n = 2; n < limit; ++n) {
        if (isPrimeTrialDivision(n)) {
            ++count;
        }
    }
    return count;
}

__attribute__((noinline))
uint64_t hashLoop(const std::string& seedText, long long iterations) {
    uint64_t hash = 14695981039346656037ULL;
    const uint64_t prime = 1099511628211ULL;
    size_t textLen = seedText.size();

    for (long long i = 0; i < iterations; ++i) {
        unsigned char c = static_cast<unsigned char>(seedText[static_cast<size_t>(i) % textLen]);
        hash ^= static_cast<uint64_t>(c) ^ static_cast<uint64_t>(i & 0xFF);
        hash *= prime;
        hash = (hash << 13) | (hash >> (64 - 13));
    }
    return hash;
}

__attribute__((noinline))
uint64_t modularRecurrenceLoop(long long iterations) {
    const uint64_t mod = 1000000007ULL;
    uint64_t a = 1;
    uint64_t b = 1;
    uint64_t accumulator = 0;

    for (long long i = 0; i < iterations; ++i) {
        uint64_t next = (a + b) % mod;
        a = b;
        b = next;
        accumulator = (accumulator + b) % mod;
    }
    return accumulator;
}

__attribute__((noinline))
int collatzSteps(long long n) {
    int steps = 0;
    while (n != 1) {
        if (n % 2 == 0) {
            n = n / 2;
        } else {
            n = 3 * n + 1;
        }
        ++steps;
        if (steps > 100000) break;
    }
    return steps;
}

__attribute__((noinline))
long long totalCollatzSteps(long long limit) {
    long long total = 0;
    for (long long n = 1; n <= limit; ++n) {
        total += collatzSteps(n);
    }
    return total;
}

__attribute__((noinline))
long long insertionSortWithCount(std::vector<int32_t>& data) {
    long long comparisons = 0;
    for (size_t i = 1; i < data.size(); ++i) {
        int32_t key = data[i];
        size_t j = i;
        while (j > 0) {
            ++comparisons;
            if (data[j - 1] <= key) break;
            data[j] = data[j - 1];
            --j;
        }
        data[j] = key;
    }
    return comparisons;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();

    const size_t SIEVE_LIMIT = 25000000;
    const int MATRIX_SIZE = 260;
    const long long PRIME_COUNT_LIMIT = 1500000;
    const long long HASH_LOOP_ITERATIONS = 60000000;
    const long long MOD_LOOP_ITERATIONS = 80000000;
    const long long COLLATZ_LIMIT = 400000;
    const size_t SORT_SIZE = 15000;

    uint64_t checksum = 0;

    std::cout << "=== Performance Heavy Test ===\n\n";

    std::cout << "-- Sieve of Eratosthenes --\n";
    long long primeCountFromSieve = runSieve(SIEVE_LIMIT);
    std::cout << "primes below " << SIEVE_LIMIT << ": " << primeCountFromSieve << "\n";
    checksum += static_cast<uint64_t>(primeCountFromSieve);

    std::cout << "\n-- Matrix multiplication --\n";
    Lcg rng(2166136261ULL);
    Matrix matA = makeDeterministicMatrix(MATRIX_SIZE, rng);
    Matrix matB = makeDeterministicMatrix(MATRIX_SIZE, rng);
    Matrix product = multiplyMatrices(matA, matB);
    int64_t matChecksum = matrixChecksum(product);
    std::cout << "matrix size: " << MATRIX_SIZE << "x" << MATRIX_SIZE << "\n";
    std::cout << "result checksum: " << matChecksum << "\n";
    checksum += static_cast<uint64_t>(matChecksum >= 0 ? matChecksum : -matChecksum);

    std::cout << "\n-- Trial-division prime counting --\n";
    long long tdPrimeCount = countPrimesInRange(PRIME_COUNT_LIMIT);
    std::cout << "primes below " << PRIME_COUNT_LIMIT << ": " << tdPrimeCount << "\n";
    checksum += static_cast<uint64_t>(tdPrimeCount);

    std::cout << "\n-- Tight hashing loop --\n";
    std::string seedText = "The quick brown fox jumps over the lazy dog, 1234567890, obfuscation test payload.";
    uint64_t hashResult = hashLoop(seedText, HASH_LOOP_ITERATIONS);
    std::cout << "hash loop iterations: " << HASH_LOOP_ITERATIONS << "\n";
    std::cout << "final hash: " << hashResult << "\n";
    checksum += hashResult % 1000000007ULL;

    std::cout << "\n-- Modular recurrence loop --\n";
    uint64_t modResult = modularRecurrenceLoop(MOD_LOOP_ITERATIONS);
    std::cout << "modular loop iterations: " << MOD_LOOP_ITERATIONS << "\n";
    std::cout << "final accumulator: " << modResult << "\n";
    checksum += modResult;

    std::cout << "\n-- Collatz step counting --\n";
    long long collatzTotal = totalCollatzSteps(COLLATZ_LIMIT);
    std::cout << "total steps for n=1.." << COLLATZ_LIMIT << ": " << collatzTotal << "\n";
    checksum += static_cast<uint64_t>(collatzTotal % 1000000007LL);

    std::cout << "\n-- Insertion sort (O(n^2)) --\n";
    std::vector<int32_t> toSort;
    toSort.reserve(SORT_SIZE);
    Lcg sortRng(9223372036854775783ULL);
    for (size_t i = 0; i < SORT_SIZE; ++i) {
        toSort.push_back(sortRng.nextInt(-100000, 100000));
    }
    long long comparisons = insertionSortWithCount(toSort);
    bool sorted = true;
    for (size_t i = 1; i < toSort.size(); ++i) {
        if (toSort[i - 1] > toSort[i]) { sorted = false; break; }
    }
    std::cout << "elements sorted: " << SORT_SIZE << "\n";
    std::cout << "comparisons made: " << comparisons << "\n";
    std::cout << "result is sorted: " << (sorted ? "true" : "false") << "\n";
    checksum += static_cast<uint64_t>(comparisons % 1000000007LL);

    std::cout << "\n=== Final checksum ===\n";
    std::cout << "TOTAL_CHECKSUM: " << checksum << "\n";

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << diff << "\n";

    return 0;
}
