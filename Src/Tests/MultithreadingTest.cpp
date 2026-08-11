#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <numeric>
#include <algorithm>
#include <chrono>
#include <cstdint>

#define LEET_IMPLEMENTATION
#include "../Leet.h"

constexpr int NUM_THREADS = 6;

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

struct Range {
    size_t begin;
    size_t end;
};

__attribute__((noinline))
std::vector<Range> makeChunks(size_t total, int numChunks) {
    std::vector<Range> chunks;
    chunks.reserve(static_cast<size_t>(numChunks));
    size_t base = total / static_cast<size_t>(numChunks);
    size_t remainder = total % static_cast<size_t>(numChunks);
    size_t cursor = 0;
    for (int i = 0; i < numChunks; ++i) {
        size_t size = base + (static_cast<size_t>(i) < remainder ? 1 : 0);
        chunks.push_back(Range{cursor, cursor + size});
        cursor += size;
    }
    return chunks;
}

__attribute__((noinline))
void sumOfSquaresWorker(const std::vector<long long>& data, Range range, long long& outResult) {
    long long sum = 0;
    for (size_t i = range.begin; i < range.end; ++i) {
        sum += data[i] * data[i];
    }
    outResult = sum;
}

__attribute__((noinline))
long long parallelSumOfSquares(const std::vector<long long>& data) {
    std::vector<Range> chunks = makeChunks(data.size(), NUM_THREADS);
    std::vector<long long> partial(NUM_THREADS, 0);
    std::vector<std::thread> workers;
    workers.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; ++i) {
        workers.emplace_back(sumOfSquaresWorker, std::cref(data), chunks[static_cast<size_t>(i)],
                              std::ref(partial[static_cast<size_t>(i)]));
    }
    for (auto& t : workers) t.join();

    long long total = 0;
    for (int i = 0; i < NUM_THREADS; ++i) {
        total += partial[static_cast<size_t>(i)];
    }
    return total;
}

__attribute__((noinline))
bool isPrimeTrialDivision(long long n) {
    if (n < 2) return false;
    if (n < 4) return true;
    if (n % 2 == 0) return false;
    for (long long d = 3; d * d <= n; d += 2) {
        if (n % d == 0) return false;
    }
    return true;
}

__attribute__((noinline))
void primeCountWorker(Range range, long long& outCount) {
    long long count = 0;
    for (size_t n = range.begin; n < range.end; ++n) {
        if (isPrimeTrialDivision(static_cast<long long>(n))) {
            ++count;
        }
    }
    outCount = count;
}

__attribute__((noinline))
long long parallelPrimeCount(size_t bound) {
    std::vector<Range> chunks = makeChunks(bound, NUM_THREADS);
    std::vector<long long> partial(NUM_THREADS, 0);
    std::vector<std::thread> workers;
    workers.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; ++i) {
        workers.emplace_back(primeCountWorker, chunks[static_cast<size_t>(i)],
                              std::ref(partial[static_cast<size_t>(i)]));
    }
    for (auto& t : workers) t.join();

    long long total = 0;
    for (int i = 0; i < NUM_THREADS; ++i) {
        total += partial[static_cast<size_t>(i)];
    }
    return total;
}

__attribute__((noinline))
void maxFinderWorker(const std::vector<long long>& data, Range range, long long& outMax) {
    long long best = data[range.begin];
    for (size_t i = range.begin; i < range.end; ++i) {
        if (data[i] > best) best = data[i];
    }
    outMax = best;
}

__attribute__((noinline))
long long parallelMax(const std::vector<long long>& data) {
    std::vector<Range> chunks = makeChunks(data.size(), NUM_THREADS);
    std::vector<long long> partial(NUM_THREADS, 0);
    std::vector<std::thread> workers;
    workers.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; ++i) {
        workers.emplace_back(maxFinderWorker, std::cref(data), chunks[static_cast<size_t>(i)],
                              std::ref(partial[static_cast<size_t>(i)]));
    }
    for (auto& t : workers) t.join();

    long long best = partial[0];
    for (int i = 1; i < NUM_THREADS; ++i) {
        if (partial[static_cast<size_t>(i)] > best) best = partial[static_cast<size_t>(i)];
    }
    return best;
}

__attribute__((noinline))
void counterWorker(std::mutex& mtx, long long& sharedCounter, int incrementsPerThread) {
    for (int i = 0; i < incrementsPerThread; ++i) {
        std::lock_guard<std::mutex> lock(mtx);
        sharedCounter += 1;
    }
}

__attribute__((noinline))
long long runMutexCounter(int incrementsPerThread) {
    std::mutex mtx;
    long long sharedCounter = 0;
    std::vector<std::thread> workers;
    workers.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; ++i) {
        workers.emplace_back(counterWorker, std::ref(mtx), std::ref(sharedCounter), incrementsPerThread);
    }
    for (auto& t : workers) t.join();

    return sharedCounter;
}

class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity) : capacity_(capacity), finished_(false) {}

    void push(long long value) {
        std::unique_lock<std::mutex> lock(mtx_);
        notFull_.wait(lock, [this] { return queue_.size() < capacity_; });
        queue_.push(value);
        lock.unlock();
        notEmpty_.notify_one();
    }

    void signalDone() {
        std::lock_guard<std::mutex> lock(mtx_);
        finished_ = true;
        notEmpty_.notify_all();
    }

    bool pop(long long& outValue) {
        std::unique_lock<std::mutex> lock(mtx_);
        notEmpty_.wait(lock, [this] { return !queue_.empty() || finished_; });
        if (queue_.empty()) {
            return false;
        }
        outValue = queue_.front();
        queue_.pop();
        lock.unlock();
        notFull_.notify_one();
        return true;
    }

private:
    std::mutex mtx_;
    std::condition_variable notFull_;
    std::condition_variable notEmpty_;
    std::queue<long long> queue_;
    size_t capacity_;
    bool finished_;
};

__attribute__((noinline))
void producerThread(BoundedQueue& q, long long count) {
    for (long long i = 0; i < count; ++i) {
        q.push(i);
    }
    q.signalDone();
}

__attribute__((noinline))
void consumerThread(BoundedQueue& q, long long& outSum, long long& outCount) {
    long long sum = 0;
    long long consumed = 0;
    long long value = 0;
    while (q.pop(value)) {
        sum += value;
        ++consumed;
    }
    outSum = sum;
    outCount = consumed;
}

using Matrix = std::vector<std::vector<long long>>;

__attribute__((noinline))
Matrix makeDeterministicMatrix(int n, Lcg& rng) {
    Matrix mat(static_cast<size_t>(n), std::vector<long long>(static_cast<size_t>(n), 0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            mat[static_cast<size_t>(i)][static_cast<size_t>(j)] = rng.nextInt(-9, 9);
        }
    }
    return mat;
}

__attribute__((noinline))
void matMulRowRangeWorker(const Matrix& a, const Matrix& b, Matrix& result, Range rowRange) {
    size_t n = a.size();
    for (size_t i = rowRange.begin; i < rowRange.end; ++i) {
        for (size_t j = 0; j < n; ++j) {
            long long sum = 0;
            for (size_t k = 0; k < n; ++k) {
                sum += a[i][k] * b[k][j];
            }
            result[i][j] = sum;
        }
    }
}

__attribute__((noinline))
Matrix parallelMatMul(const Matrix& a, const Matrix& b) {
    size_t n = a.size();
    Matrix result(n, std::vector<long long>(n, 0));
    std::vector<Range> chunks = makeChunks(n, NUM_THREADS);
    std::vector<std::thread> workers;
    workers.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; ++i) {
        workers.emplace_back(matMulRowRangeWorker, std::cref(a), std::cref(b), std::ref(result),
                              chunks[static_cast<size_t>(i)]);
    }
    for (auto& t : workers) t.join();

    return result;
}

__attribute__((noinline))
long long matrixChecksum(const Matrix& m) {
    long long sum = 0;
    for (const auto& row : m) {
        for (long long v : row) {
            sum += v;
        }
    }
    return sum;
}

std::once_flag g_initFlag;
long long g_initializedValue = 0;

__attribute__((noinline))
void expensiveOneTimeInit() {
    long long value = 0;
    for (int i = 1; i <= 1000; ++i) {
        value += i * i;
    }
    g_initializedValue = value;
}

__attribute__((noinline))
void callOnceWorker() {
    std::call_once(g_initFlag, expensiveOneTimeInit);
}

__attribute__((noinline))
long long runCallOnceDemo() {
    std::vector<std::thread> workers;
    workers.reserve(NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; ++i) {
        workers.emplace_back(callOnceWorker);
    }
    for (auto& t : workers) t.join();
    return g_initializedValue;
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
        if (steps > 10000) break;
    }
    return steps;
}

__attribute__((noinline))
void collatzWorker(std::vector<int>& results, Range range) {
    for (size_t n = range.begin; n < range.end; ++n) {
        results[n] = collatzSteps(static_cast<long long>(n + 1));
    }
}

__attribute__((noinline))
std::vector<int> parallelCollatz(size_t count) {
    std::vector<int> results(count, 0);
    std::vector<Range> chunks = makeChunks(count, NUM_THREADS);
    std::vector<std::thread> workers;
    workers.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; ++i) {
        workers.emplace_back(collatzWorker, std::ref(results), chunks[static_cast<size_t>(i)]);
    }
    for (auto& t : workers) t.join();

    return results;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();

    uint64_t checksum = 0;

    std::cout << "=== Multi-threaded Application Test ===\n";
    std::cout << "(fixed thread count: " << NUM_THREADS << ")\n\n";

    Lcg rng(88172645463325252ULL);

    std::cout << "-- Parallel sum of squares --\n";
    std::vector<long long> squareData;
    squareData.reserve(2000000);
    for (int i = 0; i < 2000000; ++i) {
        squareData.push_back(static_cast<long long>(rng.nextInt(-1000, 1000)));
    }
    long long sumSquares = parallelSumOfSquares(squareData);
    std::cout << "sum of squares: " << sumSquares << "\n";
    checksum += static_cast<uint64_t>(sumSquares);

    std::cout << "\n-- Parallel prime counting --\n";
    long long primeCount = parallelPrimeCount(300000);
    std::cout << "primes below 300000: " << primeCount << "\n";
    checksum += static_cast<uint64_t>(primeCount);

    std::cout << "\n-- Parallel max --\n";
    long long maxValue = parallelMax(squareData);
    std::cout << "max value in dataset: " << maxValue << "\n";
    checksum += static_cast<uint64_t>(maxValue + 1000);

    std::cout << "\n-- Mutex-protected shared counter --\n";
    long long counterResult = runMutexCounter(200000);
    long long expectedCounter = static_cast<long long>(NUM_THREADS) * 200000LL;
    std::cout << "counter result: " << counterResult << " (expected " << expectedCounter << ")\n";
    std::cout << "matches expected: " << (counterResult == expectedCounter ? "true" : "false") << "\n";
    checksum += static_cast<uint64_t>(counterResult);

    std::cout << "\n-- Producer/consumer (bounded queue) --\n";
    BoundedQueue queue(64);
    long long producedCount = 500000;
    long long consumedSum = 0;
    long long consumedCount = 0;
    std::thread producer(producerThread, std::ref(queue), producedCount);
    std::thread consumer(consumerThread, std::ref(queue), std::ref(consumedSum), std::ref(consumedCount));
    producer.join();
    consumer.join();
    long long expectedSum = (producedCount - 1) * producedCount / 2;
    std::cout << "consumed count: " << consumedCount << " (expected " << producedCount << ")\n";
    std::cout << "consumed sum: " << consumedSum << " (expected " << expectedSum << ")\n";
    checksum += static_cast<uint64_t>(consumedSum % 1000000007LL);

    std::cout << "\n-- Parallel matrix multiplication --\n";
    const int matSize = 120;
    Matrix matA = makeDeterministicMatrix(matSize, rng);
    Matrix matB = makeDeterministicMatrix(matSize, rng);
    Matrix product = parallelMatMul(matA, matB);
    long long matChecksum = matrixChecksum(product);
    std::cout << "matrix size: " << matSize << "x" << matSize << "\n";
    std::cout << "result matrix checksum (sum of all elements): " << matChecksum << "\n";
    std::cout << "result[0][0]=" << product[0][0] << " result[last][last]=" << product.back().back() << "\n";
    checksum += static_cast<uint64_t>(matChecksum % 1000000007LL + 1000000007LL);

    std::cout << "\n-- std::call_once demonstration --\n";
    long long onceResult = runCallOnceDemo();
    std::cout << "one-time-init value: " << onceResult << "\n";
    checksum += static_cast<uint64_t>(onceResult);

    std::cout << "\n-- Parallel Collatz step counting --\n";
    std::vector<int> collatzResults = parallelCollatz(200000);
    long long collatzTotal = std::accumulate(collatzResults.begin(), collatzResults.end(), 0LL);
    int collatzMaxSteps = *std::max_element(collatzResults.begin(), collatzResults.end());
    std::cout << "total collatz steps (n=1..200000): " << collatzTotal << "\n";
    std::cout << "max collatz steps in range: " << collatzMaxSteps << "\n";
    checksum += static_cast<uint64_t>(collatzTotal % 1000000007LL);
    checksum += static_cast<uint64_t>(collatzMaxSteps);

    std::cout << "\n=== Final checksum ===\n";
    std::cout << "TOTAL_CHECKSUM: " << checksum << "\n";

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << diff << "\n";

    return 0;
}
