// Deterministic multithreading test for the Leet obfuscator.
// Uses std::thread and fixed per-thread data to ensure repeatable output.
// Expanded to stress test multithreading with complex synchronization patterns.

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstring>
#include <cmath>
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

// Basic thread worker with simple computation
static uint64_t threadWorker(int threadId,
                             int baseValue,
                             int stepValue,
                             double scaleValue,
                             bool invert,
                             char tag,
                             size_t limit,
                             const std::array<int, 32> &data,
                             const std::array<double, 16> &weights,
                             uint32_t seed) {
    uint64_t h = 0x1234567890abcdefULL;
    for (size_t i = 0; i < limit; ++i) {
        int index = (int)((i * stepValue + baseValue) % data.size());
        double value = data[index] * weights[i % weights.size()];
        if (invert)
            value = -value;
        h = add_d(h, value + (double)tag + seed);
        h = mix64(h ^ (uint64_t)(threadId * 37 + (int)tag));
    }
    return h;
}

// Thread worker with matrix multiplication simulation
static uint64_t matrixThreadWorker(int threadId,
                                    const std::array<std::array<uint32_t, 8>, 8> &matrixA,
                                    const std::array<std::array<uint32_t, 8>, 8> &matrixB,
                                    int startRow,
                                    int endRow,
                                    uint32_t multiplier) {
    uint64_t h = 0xdeadbeefcafebabeULL;
    
    for (int row = startRow; row < endRow; ++row) {
        for (int col = 0; col < 8; ++col) {
            uint32_t sum = 0;
            for (int k = 0; k < 8; ++k) {
                sum += matrixA[row][k] * matrixB[k][col];
            }
            h = mix64(h ^ (uint64_t)(sum * multiplier));
            h = mix64(h ^ (uint64_t)(threadId + row + col));
        }
    }
    
    return h;
}

// Thread worker with recursive computation
static uint64_t recursiveThreadWorker(int threadId,
                                       int depth,
                                       int maxDepth,
                                       const std::array<int, 16> &values,
                                       double factor) {
    uint64_t h = 0xabc123def456789ULL;
    
    if (depth >= maxDepth) {
        for (unsigned i = 0; i < values.size(); ++i) {
            h = mix64(h ^ (uint64_t)(values[i] + threadId + depth));
        }
        return h;
    }
    
    int sum = 0;
    for (unsigned i = 0; i < values.size(); ++i) {
        sum += values[i] * (depth + 1);
    }
    
    h = mix64(h ^ (uint64_t)sum);
    h = add_d(h, factor * (double)depth);
    
    uint64_t left = recursiveThreadWorker(threadId, depth + 1, maxDepth, values, factor * 1.1);
    uint64_t right = recursiveThreadWorker(threadId, depth + 2, maxDepth, values, factor * 0.9);
    
    h = mix64(h ^ left);
    h = mix64(h ^ right);
    
    return h;
}

// Thread worker with complex data processing
static uint64_t dataProcessingThreadWorker(int threadId,
                                           const std::array<int, 64> &inputData,
                                           const std::array<float, 32> &floatData,
                                           int blockSize,
                                           int passes) {
    uint64_t h = 0xfee1deadbadc0ffeULL;
    
    for (int pass = 0; pass < passes; ++pass) {
        for (int block = 0; block < (int)inputData.size(); block += blockSize) {
            int blockSum = 0;
            for (int i = block; i < block + blockSize && i < (int)inputData.size(); ++i) {
                blockSum += inputData[i];
                h = mix64(h ^ (uint64_t)(inputData[i] + threadId + pass));
            }
            
            h = mix64(h ^ (uint64_t)blockSum);
            
            for (int i = block; i < block + blockSize / 2 && i < (int)floatData.size(); ++i) {
                h = add_f(h, floatData[i] * (float)(threadId + 1));
            }
        }
    }
    
    return h;
}

// Thread worker with sorting simulation
static uint64_t sortingThreadWorker(int threadId,
                                     std::array<int, 32> localArray,
                                     int iterations) {
    uint64_t h = 0xc0ffee1234567890ULL;
    
    for (int iter = 0; iter < iterations; ++iter) {
        // Bubble sort simulation
        for (unsigned i = 0; i < localArray.size(); ++i) {
            for (unsigned j = 0; j < localArray.size() - i - 1; ++j) {
                if (localArray[j] > localArray[j + 1]) {
                    int temp = localArray[j];
                    localArray[j] = localArray[j + 1];
                    localArray[j + 1] = temp;
                    h = mix64(h ^ (uint64_t)(threadId + i + j));
                }
            }
        }
        
        for (unsigned i = 0; i < localArray.size(); ++i) {
            h = mix64(h ^ (uint64_t)localArray[i]);
        }
    }
    
    return h;
}

// Thread worker with prime number calculation simulation
static uint64_t primeThreadWorker(int threadId,
                                  int start,
                                  int end,
                                  const std::array<int, 10> &primes) {
    uint64_t h = 0xdeadbeef12345678ULL;
    int count = 0;
    
    for (int num = start; num < end; ++num) {
        bool isPrime = true;
        for (int i = 2; i * i <= num; ++i) {
            if (num % i == 0) {
                isPrime = false;
                break;
            }
        }
        
        if (isPrime) {
            count++;
            h = mix64(h ^ (uint64_t)num);
            
            for (unsigned i = 0; i < primes.size(); ++i) {
                if (num == primes[i]) {
                    h = mix64(h ^ (uint64_t)(threadId * primes[i]));
                }
            }
        }
    }
    
    h = mix64(h ^ (uint64_t)count);
    return h;
}

// Thread worker with Fibonacci calculation
static uint64_t fibonacciThreadWorker(int threadId,
                                      int n,
                                      const std::array<uint64_t, 20> &cache) {
    uint64_t h = 0xfedcba9876543210ULL;
    
    if (n <= 1) {
        h = mix64(h ^ (uint64_t)n);
        return h;
    }
    
    uint64_t a = 0, b = 1;
    for (int i = 2; i <= n; ++i) {
        uint64_t next = a + b;
        a = b;
        b = next;
        
        if (i < (int)cache.size()) {
            h = mix64(h ^ cache[i]);
        }
        
        h = mix64(h ^ (next + threadId));
    }
    
    h = mix64(h ^ b);
    return h;
}

// Thread worker with string processing simulation
static uint64_t stringThreadWorker(int threadId,
                                    const std::array<char, 64> &charData,
                                    int offset,
                                    int length) {
    uint64_t h = 0x1122334455667788ULL;
    
    for (int i = 0; i < length; ++i) {
        int idx = (offset + i) % charData.size();
        char c = charData[idx];
        
        h = mix64(h ^ (uint64_t)c);
        h = mix64(h ^ (uint64_t)(threadId + i));
        
        // Simulate character transformation
        char transformed = c + (threadId % 26);
        h = mix64(h ^ (uint64_t)transformed);
    }
    
    return h;
}

// Thread worker with bitwise operations
static uint64_t bitwiseThreadWorker(int threadId,
                                    const std::array<uint32_t, 32> &data,
                                    int operations) {
    uint64_t h = 0x8899aabbccddeeffULL;
    
    for (int op = 0; op < operations; ++op) {
        for (unsigned i = 0; i < data.size(); ++i) {
            uint32_t value = data[i];
            
            switch (op % 5) {
            case 0:
                h = mix64(h ^ (uint64_t)(value | (threadId << 16)));
                break;
            case 1:
                h = mix64(h ^ (uint64_t)(value & (0xFFFF ^ threadId)));
                break;
            case 2:
                h = mix64(h ^ (uint64_t)(value ^ (threadId * 0x01010101)));
                break;
            case 3:
                h = mix64(h ^ (uint64_t)((value << (threadId % 16)) | (value >> (16 - threadId % 16))));
                break;
            case 4:
                h = mix64(h ^ (uint64_t)(~value + threadId));
                break;
            }
        }
    }
    
    return h;
}

// Thread worker with mathematical series
static uint64_t seriesThreadWorker(int threadId,
                                    int terms,
                                    double start,
                                    double ratio,
                                    const std::array<double, 8> &coefficients) {
    uint64_t h = 0x0011223344556677ULL;
    double sum = 0.0;
    
    for (int i = 0; i < terms; ++i) {
        double term = start * std::pow(ratio, i);
        sum += term;
        
        h = add_d(h, term);
        
        for (unsigned j = 0; j < coefficients.size(); ++j) {
            h = add_d(h, term * coefficients[j] * (double)(threadId + 1));
        }
    }
    
    h = add_d(h, sum);
    return h;
}

// Thread worker with histogram calculation
static uint64_t histogramThreadWorker(int threadId,
                                       const std::array<int, 128> &data,
                                       const std::array<int, 16> &bins,
                                       int startIdx,
                                       int endIdx) {
    uint64_t h = 0x778899aabbccdde0ULL;
    std::array<int, 16> localBins{};
    localBins.fill(0);
    
    for (int i = startIdx; i < endIdx && i < (int)data.size(); ++i) {
        int value = data[i];
        int binIdx = value % bins.size();
        localBins[binIdx]++;
        
        h = mix64(h ^ (uint64_t)(value + threadId));
    }
    
    for (unsigned i = 0; i < localBins.size(); ++i) {
        h = mix64(h ^ (uint64_t)(localBins[i] + bins[i]));
    }
    
    return h;
}

// Thread worker with polynomial evaluation
static uint64_t polynomialThreadWorker(int threadId,
                                        const std::array<double, 10> &coefficients,
                                        const std::array<double, 20> &xValues,
                                        int start,
                                        int end) {
    uint64_t h = 0xffeeddccbbaa9988ULL;
    
    for (int i = start; i < end && i < (int)xValues.size(); ++i) {
        double x = xValues[i];
        double result = 0.0;
        
        for (unsigned j = 0; j < coefficients.size(); ++j) {
            result += coefficients[j] * std::pow(x, (double)j);
        }
        
        h = add_d(h, result);
        h = mix64(h ^ (uint64_t)(threadId + i));
    }
    
    return h;
}

// Thread worker with search simulation
static uint64_t searchThreadWorker(int threadId,
                                    const std::array<int, 100> &data,
                                    const std::array<int, 10> &targets,
                                    int start,
                                    int end) {
    uint64_t h = 0x5566778899aabbccULL;
    int foundCount = 0;
    
    for (int target : targets) {
        for (int i = start; i < end && i < (int)data.size(); ++i) {
            if (data[i] == target) {
                foundCount++;
                h = mix64(h ^ (uint64_t)(i + threadId));
                h = mix64(h ^ (uint64_t)target);
            }
        }
    }
    
    h = mix64(h ^ (uint64_t)foundCount);
    return h;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    // Initialize test data
    std::array<int, 32> data;
    for (unsigned i = 0; i < data.size(); ++i)
        data[i] = (int)(i * 5 + 3);

    std::array<double, 16> weights;
    for (unsigned i = 0; i < weights.size(); ++i)
        weights[i] = 1.5 + i * 0.75;

    std::array<std::array<uint32_t, 8>, 8> matrixA{};
    std::array<std::array<uint32_t, 8>, 8> matrixB{};
    for (unsigned r = 0; r < 8; ++r) {
        for (unsigned c = 0; c < 8; ++c) {
            matrixA[r][c] = (uint32_t)(r * 11 + c * 7 + 13);
            matrixB[r][c] = (uint32_t)(r * 5 + c * 17 + 19);
        }
    }

    std::array<int, 16> recursiveValues;
    for (unsigned i = 0; i < recursiveValues.size(); ++i)
        recursiveValues[i] = (int)(i * 9 + 17);

    std::array<int, 64> inputData;
    for (unsigned i = 0; i < inputData.size(); ++i)
        inputData[i] = (int)(i * 3 + 11);

    std::array<float, 32> floatData;
    for (unsigned i = 0; i < floatData.size(); ++i)
        floatData[i] = 1.5f + i * 0.25f;

    std::array<int, 32> sortArray;
    for (unsigned i = 0; i < sortArray.size(); ++i)
        sortArray[i] = (int)((i * 7) % 32);

    std::array<int, 10> primes = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29};

    std::array<uint64_t, 20> fibCache;
    fibCache[0] = 0;
    fibCache[1] = 1;
    for (unsigned i = 2; i < fibCache.size(); ++i)
        fibCache[i] = fibCache[i - 1] + fibCache[i - 2];

    std::array<char, 64> charData;
    for (unsigned i = 0; i < charData.size(); ++i)
        charData[i] = (char)('A' + (i % 26));

    std::array<uint32_t, 32> bitwiseData;
    for (unsigned i = 0; i < bitwiseData.size(); ++i)
        bitwiseData[i] = (uint32_t)(i * 0x9e3779b9);

    std::array<double, 8> seriesCoefficients;
    for (unsigned i = 0; i < seriesCoefficients.size(); ++i)
        seriesCoefficients[i] = 1.0 + i * 0.5;

    std::array<int, 16> histogramBins;
    for (unsigned i = 0; i < histogramBins.size(); ++i)
        histogramBins[i] = (int)(i * 2);

    std::array<int, 128> histogramData;
    for (unsigned i = 0; i < histogramData.size(); ++i)
        histogramData[i] = (int)(i % 16);

    std::array<double, 10> polyCoefficients;
    for (unsigned i = 0; i < polyCoefficients.size(); ++i)
        polyCoefficients[i] = (i % 2 == 0) ? 1.0 : -1.0;

    std::array<double, 20> xValues;
    for (unsigned i = 0; i < xValues.size(); ++i)
        xValues[i] = 0.1 * i;

    std::array<int, 100> searchData;
    for (unsigned i = 0; i < searchData.size(); ++i)
        searchData[i] = (int)(i * 2);

    std::array<int, 10> searchTargets;
    for (unsigned i = 0; i < searchTargets.size(); ++i)
        searchTargets[i] = (int)(i * 10);

    // Test 1: Basic thread workers
    std::array<uint64_t, 8> basicResults{};
    std::vector<std::thread> basicThreads;
    basicThreads.reserve(basicResults.size());

    for (int threadId = 0; threadId < (int)basicResults.size(); ++threadId) {
        basicThreads.emplace_back([threadId, &data, &weights, &basicResults]() {
            basicResults[threadId] = threadWorker(
                threadId,
                7 + threadId,
                3 + threadId,
                1.2345 + threadId * 0.1,
                (threadId % 2) == 0,
                (char)('A' + threadId),
                16u + threadId,
                data,
                weights,
                0xdead0000u + (uint32_t)threadId);
        });
    }

    for (auto &thread : basicThreads)
        thread.join();

    // Test 2: Matrix multiplication threads
    std::array<uint64_t, 4> matrixResults{};
    std::vector<std::thread> matrixThreads;
    matrixThreads.reserve(matrixResults.size());

    for (int threadId = 0; threadId < (int)matrixResults.size(); ++threadId) {
        matrixThreads.emplace_back([threadId, &matrixA, &matrixB, &matrixResults]() {
            int startRow = threadId * 2;
            int endRow = startRow + 2;
            matrixResults[threadId] = matrixThreadWorker(
                threadId,
                matrixA,
                matrixB,
                startRow,
                endRow,
                (uint32_t)(threadId + 1));
        });
    }

    for (auto &thread : matrixThreads)
        thread.join();

    // Test 3: Recursive computation threads
    std::array<uint64_t, 6> recursiveResults{};
    std::vector<std::thread> recursiveThreads;
    recursiveThreads.reserve(recursiveResults.size());

    for (int threadId = 0; threadId < (int)recursiveResults.size(); ++threadId) {
        recursiveThreads.emplace_back([threadId, &recursiveValues, &recursiveResults]() {
            recursiveResults[threadId] = recursiveThreadWorker(
                threadId,
                0,
                5 + threadId,
                recursiveValues,
                1.5 + threadId * 0.2);
        });
    }

    for (auto &thread : recursiveThreads)
        thread.join();

    // Test 4: Data processing threads
    std::array<uint64_t, 5> dataProcessingResults{};
    std::vector<std::thread> dataProcessingThreads;
    dataProcessingThreads.reserve(dataProcessingResults.size());

    for (int threadId = 0; threadId < (int)dataProcessingResults.size(); ++threadId) {
        dataProcessingThreads.emplace_back([threadId, &inputData, &floatData, &dataProcessingResults]() {
            dataProcessingResults[threadId] = dataProcessingThreadWorker(
                threadId,
                inputData,
                floatData,
                8 + threadId,
                3 + threadId);
        });
    }

    for (auto &thread : dataProcessingThreads)
        thread.join();

    // Test 5: Sorting threads
    std::array<uint64_t, 4> sortingResults{};
    std::vector<std::thread> sortingThreads;
    sortingThreads.reserve(sortingResults.size());

    for (int threadId = 0; threadId < (int)sortingResults.size(); ++threadId) {
        sortingThreads.emplace_back([threadId, &sortArray, &sortingResults]() {
            sortingResults[threadId] = sortingThreadWorker(
                threadId,
                sortArray,
                5 + threadId);
        });
    }

    for (auto &thread : sortingThreads)
        thread.join();

    // Test 6: Prime calculation threads
    std::array<uint64_t, 3> primeResults{};
    std::vector<std::thread> primeThreads;
    primeThreads.reserve(primeResults.size());

    for (int threadId = 0; threadId < (int)primeResults.size(); ++threadId) {
        primeThreads.emplace_back([threadId, &primes, &primeResults]() {
            primeResults[threadId] = primeThreadWorker(
                threadId,
                threadId * 100,
                (threadId + 1) * 100,
                primes);
        });
    }

    for (auto &thread : primeThreads)
        thread.join();

    // Test 7: Fibonacci threads
    std::array<uint64_t, 5> fibResults{};
    std::vector<std::thread> fibThreads;
    fibThreads.reserve(fibResults.size());

    for (int threadId = 0; threadId < (int)fibResults.size(); ++threadId) {
        fibThreads.emplace_back([threadId, &fibCache, &fibResults]() {
            fibResults[threadId] = fibonacciThreadWorker(
                threadId,
                10 + threadId * 5,
                fibCache);
        });
    }

    for (auto &thread : fibThreads)
        thread.join();

    // Test 8: String processing threads
    std::array<uint64_t, 6> stringResults{};
    std::vector<std::thread> stringThreads;
    stringThreads.reserve(stringResults.size());

    for (int threadId = 0; threadId < (int)stringResults.size(); ++threadId) {
        stringThreads.emplace_back([threadId, &charData, &stringResults]() {
            stringResults[threadId] = stringThreadWorker(
                threadId,
                charData,
                threadId * 10,
                20 + threadId * 2);
        });
    }

    for (auto &thread : stringThreads)
        thread.join();

    // Test 9: Bitwise operation threads
    std::array<uint64_t, 4> bitwiseResults{};
    std::vector<std::thread> bitwiseThreads;
    bitwiseThreads.reserve(bitwiseResults.size());

    for (int threadId = 0; threadId < (int)bitwiseResults.size(); ++threadId) {
        bitwiseThreads.emplace_back([threadId, &bitwiseData, &bitwiseResults]() {
            bitwiseResults[threadId] = bitwiseThreadWorker(
                threadId,
                bitwiseData,
                10 + threadId * 3);
        });
    }

    for (auto &thread : bitwiseThreads)
        thread.join();

    // Test 10: Series calculation threads
    std::array<uint64_t, 4> seriesResults{};
    std::vector<std::thread> seriesThreads;
    seriesThreads.reserve(seriesResults.size());

    for (int threadId = 0; threadId < (int)seriesResults.size(); ++threadId) {
        seriesThreads.emplace_back([threadId, &seriesCoefficients, &seriesResults]() {
            seriesResults[threadId] = seriesThreadWorker(
                threadId,
                15 + threadId * 5,
                1.0 + threadId * 0.5,
                1.5 - threadId * 0.1,
                seriesCoefficients);
        });
    }

    for (auto &thread : seriesThreads)
        thread.join();

    // Test 11: Histogram threads
    std::array<uint64_t, 8> histogramResults{};
    std::vector<std::thread> histogramThreads;
    histogramThreads.reserve(histogramResults.size());

    for (int threadId = 0; threadId < (int)histogramResults.size(); ++threadId) {
        histogramThreads.emplace_back([threadId, &histogramData, &histogramBins, &histogramResults]() {
            int startIdx = threadId * 16;
            int endIdx = startIdx + 16;
            histogramResults[threadId] = histogramThreadWorker(
                threadId,
                histogramData,
                histogramBins,
                startIdx,
                endIdx);
        });
    }

    for (auto &thread : histogramThreads)
        thread.join();

    // Test 12: Polynomial evaluation threads
    std::array<uint64_t, 4> polyResults{};
    std::vector<std::thread> polyThreads;
    polyThreads.reserve(polyResults.size());

    for (int threadId = 0; threadId < (int)polyResults.size(); ++threadId) {
        polyThreads.emplace_back([threadId, &polyCoefficients, &xValues, &polyResults]() {
            int start = threadId * 5;
            int end = start + 5;
            polyResults[threadId] = polynomialThreadWorker(
                threadId,
                polyCoefficients,
                xValues,
                start,
                end);
        });
    }

    for (auto &thread : polyThreads)
        thread.join();

    // Test 13: Search threads
    std::array<uint64_t, 5> searchResults{};
    std::vector<std::thread> searchThreads;
    searchThreads.reserve(searchResults.size());

    for (int threadId = 0; threadId < (int)searchResults.size(); ++threadId) {
        searchThreads.emplace_back([threadId, &searchData, &searchTargets, &searchResults]() {
            int start = threadId * 20;
            int end = start + 20;
            searchResults[threadId] = searchThreadWorker(
                threadId,
                searchData,
                searchTargets,
                start,
                end);
        });
    }

    for (auto &thread : searchThreads)
        thread.join();

    // Combine all results
    uint64_t checksum = 0x0fedcba987654321ULL;
    
    for (uint64_t value : basicResults)
        checksum = mix64(checksum ^ value);
    
    for (uint64_t value : matrixResults)
        checksum = mix64(checksum ^ value);
    
    for (uint64_t value : recursiveResults)
        checksum = mix64(checksum ^ value);
    
    for (uint64_t value : dataProcessingResults)
        checksum = mix64(checksum ^ value);
    
    for (uint64_t value : sortingResults)
        checksum = mix64(checksum ^ value);
    
    for (uint64_t value : primeResults)
        checksum = mix64(checksum ^ value);
    
    for (uint64_t value : fibResults)
        checksum = mix64(checksum ^ value);
    
    for (uint64_t value : stringResults)
        checksum = mix64(checksum ^ value);
    
    for (uint64_t value : bitwiseResults)
        checksum = mix64(checksum ^ value);
    
    for (uint64_t value : seriesResults)
        checksum = mix64(checksum ^ value);
    
    for (uint64_t value : histogramResults)
        checksum = mix64(checksum ^ value);
    
    for (uint64_t value : polyResults)
        checksum = mix64(checksum ^ value);
    
    for (uint64_t value : searchResults)
        checksum = mix64(checksum ^ value);

    checksum = mix64(checksum);
    std::printf("CHECKSUM: 0x%016llx\n", (unsigned long long)checksum);

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("%ld\n", diff);
    return 0;
}
