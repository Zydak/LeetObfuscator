// test_threading.cpp - Multi-threaded application test
//
// Compile:  g++ -O2 -std=c++17 -fno-exceptions -pthread -o test_threading test_threading.cpp
//
// Design notes (these are the rules that make a threaded program's output
// reproducible even though thread *scheduling* is not):
//  1. All input data is generated single-threaded, up front, by a seeded
//     deterministic LCG -- never by std::rand/random_device.
//  2. Every worker thread writes ONLY to a memory location that no other
//     thread touches (its own slot in a results array, or disjoint rows of
//     a matrix). This means there are no data races, and the *content* of
//     the result never depends on which thread happened to finish first.
//  3. Wherever threads must share mutable state (the mutex-protected
//     counter, the producer/consumer queue), the shared state is updated
//     with operations that are either (a) commutative/associative integer
//     operations, so any interleaving yields the same final value, or
//     (b) ordered by a mutex + condition variable so the logical order is
//     fixed regardless of physical thread timing.
//  4. Combination of per-thread results into the final answer always
//     happens on the main thread, AFTER every worker has been joined, and
//     always in a fixed index order (thread 0, 1, 2, ... N-1) -- never in
//     "whichever finished first" order.
//  5. No thread ever calls std::cout directly. Only the main thread prints,
//     after all joins, so there is no possibility of interleaved /
//     torn output between runs.
//  6. Thread count is a fixed compile-time constant (not
//     hardware_concurrency()), so the partitioning of work is identical on
//     every run and every machine.

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

// ---------------------------------------------------------------------------
// Deterministic generator (LCG)
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Deterministic chunk boundaries: splits [0, total) into NUM_THREADS
// contiguous ranges as evenly as possible. Purely a function of `total`
// and NUM_THREADS, so identical on every run/machine.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// 1. Parallel sum of squares. Each thread writes to its own results[i]
//    slot -- no shared mutable state, no locking needed, no races.
// ---------------------------------------------------------------------------
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
        total += partial[static_cast<size_t>(i)]; // fixed order: thread 0..N-1
    }
    return total;
}

// ---------------------------------------------------------------------------
// 2. Parallel prime counting over [2, bound). Each thread counts primes in
//    its own disjoint sub-range via trial division, writing to its own slot.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// 3. Parallel max-finding. Combination via max() is commutative/associative
//    so combining order truly does not matter here -- a good example of a
//    reduction that stays correct under any interleaving.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// 4. Mutex-protected shared counter. Every increment is a plain integer add
//    (associative & commutative), so no matter how the threads interleave,
//    the final total is always NUM_THREADS * incrementsPerThread.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// 5. Producer/consumer with a mutex + condition_variable bounded queue. The
//    single producer pushes 0..N-1 in that exact order; the single consumer
//    pops in FIFO order. Because there is exactly one producer and one
//    consumer and the queue preserves FIFO order, the sequence of values
//    consumed is always 0, 1, 2, ..., N-1 regardless of the relative speed
//    of the two threads -- so the resulting sum is fully deterministic.
// ---------------------------------------------------------------------------
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

    // Returns false when the queue is empty AND the producer signalled done.
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

// ---------------------------------------------------------------------------
// 6. Parallel matrix multiplication (int). Rows of the result are
//    partitioned across threads; each thread writes only to its own
//    disjoint set of rows, so there is no need for locking.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// 7. std::call_once demonstration: a shared piece of "expensive" one-time
//    initialization state, guaranteed to run exactly once no matter how
//    many threads race to trigger it.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// 8. Parallel Collatz step counting. Each thread writes into its own
//    disjoint slice of a shared results vector -- disjoint indices mean no
//    race even without a lock.
// ---------------------------------------------------------------------------
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
        if (steps > 10000) break; // safety bound; not expected to trigger for our range
    }
    return steps;
}

__attribute__((noinline))
void collatzWorker(std::vector<int>& results, Range range) {
    for (size_t n = range.begin; n < range.end; ++n) {
        results[n] = collatzSteps(static_cast<long long>(n + 1)); // avoid n=0
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

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    auto start = std::chrono::high_resolution_clock::now();

    uint64_t checksum = 0;

    std::cout << "=== Multi-threaded Application Test ===\n";
    std::cout << "(fixed thread count: " << NUM_THREADS << ")\n\n";

    // All data generated single-threaded and deterministically.
    Lcg rng(88172645463325252ULL);

    // 1. Parallel sum of squares
    std::cout << "-- Parallel sum of squares --\n";
    std::vector<long long> squareData;
    squareData.reserve(2000000);
    for (int i = 0; i < 2000000; ++i) {
        squareData.push_back(static_cast<long long>(rng.nextInt(-1000, 1000)));
    }
    long long sumSquares = parallelSumOfSquares(squareData);
    std::cout << "sum of squares: " << sumSquares << "\n";
    checksum += static_cast<uint64_t>(sumSquares);

    // 2. Parallel prime counting
    std::cout << "\n-- Parallel prime counting --\n";
    long long primeCount = parallelPrimeCount(300000);
    std::cout << "primes below 300000: " << primeCount << "\n";
    checksum += static_cast<uint64_t>(primeCount);

    // 3. Parallel max
    std::cout << "\n-- Parallel max --\n";
    long long maxValue = parallelMax(squareData);
    std::cout << "max value in dataset: " << maxValue << "\n";
    checksum += static_cast<uint64_t>(maxValue + 1000);

    // 4. Mutex-protected counter
    std::cout << "\n-- Mutex-protected shared counter --\n";
    long long counterResult = runMutexCounter(200000);
    long long expectedCounter = static_cast<long long>(NUM_THREADS) * 200000LL;
    std::cout << "counter result: " << counterResult << " (expected " << expectedCounter << ")\n";
    std::cout << "matches expected: " << (counterResult == expectedCounter ? "true" : "false") << "\n";
    checksum += static_cast<uint64_t>(counterResult);

    // 5. Producer/consumer
    std::cout << "\n-- Producer/consumer (bounded queue) --\n";
    BoundedQueue queue(64);
    long long producedCount = 500000;
    long long consumedSum = 0;
    long long consumedCount = 0;
    std::thread producer(producerThread, std::ref(queue), producedCount);
    std::thread consumer(consumerThread, std::ref(queue), std::ref(consumedSum), std::ref(consumedCount));
    producer.join();
    consumer.join();
    long long expectedSum = (producedCount - 1) * producedCount / 2; // sum(0..N-1)
    std::cout << "consumed count: " << consumedCount << " (expected " << producedCount << ")\n";
    std::cout << "consumed sum: " << consumedSum << " (expected " << expectedSum << ")\n";
    checksum += static_cast<uint64_t>(consumedSum % 1000000007LL);

    // 6. Parallel matrix multiplication
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

    // 7. call_once demonstration
    std::cout << "\n-- std::call_once demonstration --\n";
    long long onceResult = runCallOnceDemo();
    std::cout << "one-time-init value: " << onceResult << "\n";
    checksum += static_cast<uint64_t>(onceResult);

    // 8. Parallel Collatz
    std::cout << "\n-- Parallel Collatz step counting --\n";
    std::vector<int> collatzResults = parallelCollatz(200000);
    long long collatzTotal = std::accumulate(collatzResults.begin(), collatzResults.end(), 0LL);
    int collatzMaxSteps = *std::max_element(collatzResults.begin(), collatzResults.end());
    std::cout << "total collatz steps (n=1..200000): " << collatzTotal << "\n";
    std::cout << "max collatz steps in range: " << collatzMaxSteps << "\n";
    checksum += static_cast<uint64_t>(collatzTotal % 1000000007LL);
    checksum += static_cast<uint64_t>(collatzMaxSteps);

    // Final checksum
    std::cout << "\n=== Final checksum ===\n";
    std::cout << "TOTAL_CHECKSUM: " << checksum << "\n";

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << diff << "\n";

    return 0;
}