// Deterministic containers test for the Leet obfuscator.
// Uses std::vector, std::map, std::deque, std::set, std::array, and std::string.
// Output is always the same for validation.
// Expanded to stress test STL containers with complex operations.

#include <array>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <list>
#include <forward_list>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <queue>
#include <tuple>
#include <cstring>
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

static uint64_t hashString(const std::string &value) {
    uint64_t h = 0x13579bdf2468ace0ULL;
    for (unsigned char c : value) {
        h = mix64(h ^ (uint64_t)c);
    }
    return h;
}

static uint64_t hashContainer(const std::vector<int> &values,
                              const std::deque<double> &doubles,
                              const std::array<uint32_t, 8> &fixedArray,
                              const std::set<char> &letters,
                              const std::map<std::string, int> &mapping,
                              const std::string &label) {
    uint64_t h = 0xabcdef0123456789ULL;
    h = mix64(h ^ (uint64_t)values.size());
    for (int value : values) {
        h = mix64(h ^ (uint64_t)value);
    }
    h = mix64(h ^ (uint64_t)doubles.size());
    for (double value : doubles) {
        h = add_d(h, value);
    }
    for (uint32_t value : fixedArray) {
        h = mix64(h ^ (uint64_t)value);
    }
    for (char value : letters) {
        h = mix64(h ^ (uint64_t)value);
    }
    for (auto const &entry : mapping) {
        h = mix64(h ^ hashString(entry.first) ^ (uint64_t)entry.second);
    }
    h = mix64(h ^ hashString(label));
    return h;
}

static uint64_t combineSequences(const std::string &a,
                                 const std::string &b,
                                 const std::vector<std::pair<std::string, int>> &pairs,
                                 const std::map<int, int> &indexMap,
                                 const std::array<int, 6> &values) {
    uint64_t h = 0x1234deadbeef5678ULL;
    h = mix64(h ^ hashString(a));
    h = mix64(h ^ hashString(b));
    for (auto const &element : pairs) {
        h = mix64(h ^ hashString(element.first) ^ (uint64_t)element.second);
    }
    for (auto const &entry : indexMap) {
        h = mix64(h ^ (uint64_t)entry.first ^ (uint64_t)entry.second);
    }
    for (int x : values) {
        h = mix64(h ^ (uint64_t)x);
    }
    return h;
}

// List operations
static uint64_t listOperations(const std::list<int> &data) {
    uint64_t h = 0xfee1deadbadc0ffeULL;
    
    for (int value : data) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    // Simulate various list operations
    std::list<int> working = data;
    working.reverse();
    
    for (int value : working) {
        h = mix64(h ^ (uint64_t)(value + 1));
    }
    
    working.sort();
    for (int value : working) {
        h = mix64(h ^ (uint64_t)(value + 2));
    }
    
    return h;
}

// Forward list operations
static uint64_t forwardListOperations(const std::forward_list<int> &data) {
    uint64_t h = 0xdeadcafebabefaceULL;
    
    for (int value : data) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    std::forward_list<int> working = data;
    working.reverse();
    
    for (int value : working) {
        h = mix64(h ^ (uint64_t)(value + 3));
    }
    
    working.sort();
    for (int value : working) {
        h = mix64(h ^ (uint64_t)(value + 4));
    }
    
    return h;
}

// Unordered map operations
static uint64_t unorderedMapOperations(const std::unordered_map<std::string, int> &data) {
    uint64_t h = 0xcafebabedeadbeefULL;
    
    for (auto const &entry : data) {
        h = mix64(h ^ hashString(entry.first) ^ (uint64_t)entry.second);
    }
    
    return h;
}

// Unordered set operations
static uint64_t unorderedSetOperations(const std::unordered_set<int> &data) {
    uint64_t h = 0xfacedeadcafebabeULL;
    
    for (int value : data) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    return h;
}

// Stack operations
static uint64_t stackOperations(const std::vector<int> &input) {
    uint64_t h = 0x0badf00d1337c0deULL;
    std::stack<int> s;
    
    for (int value : input) {
        s.push(value);
    }
    
    while (!s.empty()) {
        h = mix64(h ^ (uint64_t)s.top());
        s.pop();
    }
    
    return h;
}

// Queue operations
static uint64_t queueOperations(const std::vector<int> &input) {
    uint64_t h = 0xc0deface12345678ULL;
    std::queue<int> q;
    
    for (int value : input) {
        q.push(value);
    }
    
    while (!q.empty()) {
        h = mix64(h ^ (uint64_t)q.front());
        q.pop();
    }
    
    return h;
}

// Priority queue operations
static uint64_t priorityQueueOperations(const std::vector<int> &input) {
    uint64_t h = 0x1337beefdeadfaceULL;
    std::priority_queue<int> pq;
    
    for (int value : input) {
        pq.push(value);
    }
    
    while (!pq.empty()) {
        h = mix64(h ^ (uint64_t)pq.top());
        pq.pop();
    }
    
    return h;
}

// Tuple operations
static uint64_t tupleOperations(const std::vector<std::tuple<int, double, std::string>> &data) {
    uint64_t h = 0xfacefeed0ddba11eULL;
    
    for (auto const &t : data) {
        int i = std::get<0>(t);
        double d = std::get<1>(t);
        std::string s = std::get<2>(t);
        
        h = mix64(h ^ (uint64_t)i);
        h = add_d(h, d);
        h = mix64(h ^ hashString(s));
    }
    
    return h;
}

// Multi-map operations
static uint64_t multimapOperations(const std::multimap<int, std::string> &data) {
    uint64_t h = 0xbaddcafebabe1337ULL;
    
    for (auto const &entry : data) {
        h = mix64(h ^ (uint64_t)entry.first);
        h = mix64(h ^ hashString(entry.second));
    }
    
    return h;
}

// Multi-set operations
static uint64_t multisetOperations(const std::multiset<int> &data) {
    uint64_t h = 0xc0dedeadbadf00dULL;
    
    for (int value : data) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    return h;
}

// Vector transformations
static uint64_t vectorTransformations(const std::vector<int> &input) {
    uint64_t h = 0xfeedfacedeadbeefULL;
    std::vector<int> transformed;
    
    // Transform: multiply by 2
    std::transform(input.begin(), input.end(), std::back_inserter(transformed),
                   [](int x) { return x * 2; });
    
    for (int value : transformed) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    // Accumulate
    int sum = std::accumulate(transformed.begin(), transformed.end(), 0);
    h = mix64(h ^ (uint64_t)sum);
    
    // Count if
    int count = std::count_if(transformed.begin(), transformed.end(),
                             [](int x) { return x > 10; });
    h = mix64(h ^ (uint64_t)count);
    
    return h;
}

// String operations
static uint64_t stringOperations(const std::vector<std::string> &strings) {
    uint64_t h = 0xfaceface12345678ULL;
    
    for (auto const &s : strings) {
        h = mix64(h ^ hashString(s));
        
        // String length
        h = mix64(h ^ (uint64_t)s.length());
        
        // Find operation
        size_t pos = s.find('a');
        if (pos != std::string::npos) {
            h = mix64(h ^ (uint64_t)pos);
        }
        
        // Substring
        if (s.length() > 3) {
            std::string sub = s.substr(0, 3);
            h = mix64(h ^ hashString(sub));
        }
    }
    
    return h;
}

// Map transformations
static uint64_t mapTransformations(const std::map<int, double> &input) {
    uint64_t h = 0xdeadbeeffacefeedULL;
    
    for (auto const &entry : input) {
        h = mix64(h ^ (uint64_t)entry.first);
        h = add_d(h, entry.second);
    }
    
    // Find and count
    auto it = input.find(5);
    if (it != input.end()) {
        h = add_d(h, it->second);
    }
    
    size_t count = input.count(10);
    h = mix64(h ^ (uint64_t)count);
    
    return h;
}

// Set operations
static uint64_t setOperations(const std::set<int> &a, const std::set<int> &b) {
    uint64_t h = 0xc0ffeebabebad0ULL;
    
    // Union operation simulation
    std::vector<int> unionResult;
    std::set_union(a.begin(), a.end(), b.begin(), b.end(),
                  std::back_inserter(unionResult));
    
    for (int value : unionResult) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    // Intersection operation simulation
    std::vector<int> intersectResult;
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                         std::back_inserter(intersectResult));
    
    for (int value : intersectResult) {
        h = mix64(h ^ (uint64_t)(value + 1000));
    }
    
    return h;
}

// Deque operations
static uint64_t dequeOperations(std::deque<int> data) {
    uint64_t h = 0x13371337deadbeefULL;
    
    // Push front and back
    data.push_front(100);
    data.push_back(200);
    
    for (int value : data) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    // Pop operations
    if (!data.empty()) {
        data.pop_front();
        data.pop_back();
    }
    
    for (int value : data) {
        h = mix64(h ^ (uint64_t)(value + 1));
    }
    
    return h;
}

// Array operations
static uint64_t arrayOperations(const std::array<int, 10> &data) {
    uint64_t h = 0xbadfeedfacade123ULL;
    
    for (size_t i = 0; i < data.size(); ++i) {
        h = mix64(h ^ (uint64_t)(data[i] + i));
    }
    
    // Array fill
    std::array<int, 10> filled;
    filled.fill(42);
    
    for (int value : filled) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    return h;
}

// Nested containers
static uint64_t nestedContainerOperations(const std::vector<std::vector<int>> &matrix) {
    uint64_t h = 0xface1234dead5678ULL;
    
    for (auto const &row : matrix) {
        int rowSum = 0;
        for (int value : row) {
            rowSum += value;
            h = mix64(h ^ (uint64_t)value);
        }
        h = mix64(h ^ (uint64_t)rowSum);
    }
    
    return h;
}

// Map of vectors
static uint64_t mapOfVectors(const std::map<std::string, std::vector<int>> &data) {
    uint64_t h = 0xcafe1234dead5678ULL;
    
    for (auto const &entry : data) {
        h = mix64(h ^ hashString(entry.first));
        
        for (int value : entry.second) {
            h = mix64(h ^ (uint64_t)value);
        }
    }
    
    return h;
}

// Vector of maps
static uint64_t vectorOfMaps(const std::vector<std::map<int, std::string>> &data) {
    uint64_t h = 0xdead1234face5678ULL;
    
    for (auto const &m : data) {
        for (auto const &entry : m) {
            h = mix64(h ^ (uint64_t)entry.first);
            h = mix64(h ^ hashString(entry.second));
        }
    }
    
    return h;
}

// Complex container algorithm
static uint64_t complexContainerAlgorithm(const std::vector<int> &data) {
    uint64_t h = 0x1337deadbeef1337ULL;
    
    // Create frequency map
    std::map<int, int> frequency;
    for (int value : data) {
        frequency[value]++;
    }
    
    for (auto const &entry : frequency) {
        h = mix64(h ^ (uint64_t)entry.first);
        h = mix64(h ^ (uint64_t)entry.second);
    }
    
    // Find most frequent
    int maxFreq = 0;
    int mostFrequent = 0;
    for (auto const &entry : frequency) {
        if (entry.second > maxFreq) {
            maxFreq = entry.second;
            mostFrequent = entry.first;
        }
    }
    
    h = mix64(h ^ (uint64_t)mostFrequent);
    h = mix64(h ^ (uint64_t)maxFreq);
    
    return h;
}

// Container comparison
static uint64_t containerComparison(const std::vector<int> &a, const std::vector<int> &b) {
    uint64_t h = 0xfacedeadbeefcafeULL;
    
    // Lexicographical comparison
    bool less = std::lexicographical_compare(a.begin(), a.end(),
                                            b.begin(), b.end());
    h = mix64(h ^ (uint64_t)less);
    
    // Element-wise comparison
    size_t minSize = std::min(a.size(), b.size());
    for (size_t i = 0; i < minSize; ++i) {
        h = mix64(h ^ (uint64_t)(a[i] == b[i]));
    }
    
    return h;
}

// Container rotation
static uint64_t containerRotation(std::vector<int> data, int positions) {
    uint64_t h = 0xdeadfacecafe1234ULL;
    
    std::rotate(data.begin(), data.begin() + positions, data.end());
    
    for (int value : data) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    return h;
}

// Container partition
static uint64_t containerPartition(std::vector<int> data) {
    uint64_t h = 0xcafebeefdeadfaceULL;
    
    auto pivot = std::partition(data.begin(), data.end(), [](int x) { return x % 2 == 0; });
    
    for (auto it = data.begin(); it != pivot; ++it) {
        h = mix64(h ^ (uint64_t)(*it + 100));
    }
    
    for (auto it = pivot; it != data.end(); ++it) {
        h = mix64(h ^ (uint64_t)(*it + 200));
    }
    
    return h;
}

// Container merge
static uint64_t containerMerge(const std::vector<int> &a, const std::vector<int> &b) {
    uint64_t h = 0xfeeddeadcafebeefULL;
    
    std::vector<int> merged;
    std::merge(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(merged));
    
    for (int value : merged) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    return h;
}

// Container unique
static uint64_t containerUnique(std::vector<int> data) {
    uint64_t h = 0xbadfeedfacadebadULL;
    
    std::sort(data.begin(), data.end());
    auto last = std::unique(data.begin(), data.end());
    
    for (auto it = data.begin(); it != last; ++it) {
        h = mix64(h ^ (uint64_t)(*it));
    }
    
    return h;
}

// Container reverse
static uint64_t containerReverse(std::vector<int> data) {
    uint64_t h = 0xfacadebadfeed123ULL;
    
    std::reverse(data.begin(), data.end());
    
    for (int value : data) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    return h;
}

// Container shuffle simulation (deterministic)
static uint64_t containerShuffleSimulation(std::vector<int> data) {
    uint64_t h = 0xdeadcafe12345678ULL;
    
    // Deterministic "shuffle" using swap pattern
    for (size_t i = 0; i < data.size(); ++i) {
        size_t j = (i * 7) % data.size();
        std::swap(data[i], data[j]);
    }
    
    for (int value : data) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    return h;
}

// Container search
static uint64_t containerSearch(const std::vector<int> &data, int target) {
    uint64_t h = 0xbeefdeadface1234ULL;
    
    auto it = std::find(data.begin(), data.end(), target);
    if (it != data.end()) {
        h = mix64(h ^ (uint64_t)std::distance(data.begin(), it));
    } else {
        h = mix64(h ^ 0xFFFFFFFFULL);
    }
    
    // Binary search (requires sorted container)
    std::vector<int> sorted = data;
    std::sort(sorted.begin(), sorted.end());
    bool found = std::binary_search(sorted.begin(), sorted.end(), target);
    h = mix64(h ^ (uint64_t)found);
    
    return h;
}

// Container fill
static uint64_t containerFill(std::vector<int> data, int value, size_t count) {
    uint64_t h = 0xcafe1234dead5678ULL;
    
    if (count <= data.size()) {
        std::fill_n(data.begin(), count, value);
    }
    
    for (int v : data) {
        h = mix64(h ^ (uint64_t)v);
    }
    
    return h;
}

// Container generate
static uint64_t containerGenerate(std::vector<int> data) {
    uint64_t h = 0xface5678dead1234ULL;
    
    int seed = 42;
    std::generate(data.begin(), data.end(), [&seed]() {
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        return seed;
    });
    
    for (int value : data) {
        h = mix64(h ^ (uint64_t)value);
    }
    
    return h;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<int> values(16);
    std::iota(values.begin(), values.end(), 1);

    std::deque<double> doubles;
    for (int i = 0; i < 12; ++i)
        doubles.push_back(1.25 + 0.75 * i);

    std::array<uint32_t, 8> fixedArray = {0u, 1u, 2u, 3u, 5u, 8u, 13u, 21u};

    std::set<char> letters = {'A', 'B', 'C', 'D', 'E', 'F'};

    std::map<std::string, int> mapping;
    mapping["alpha"] = 10;
    mapping["beta"] = 20;
    mapping["gamma"] = 30;
    mapping["delta"] = 40;

    std::vector<std::pair<std::string, int>> pairs;
    pairs.emplace_back("one", 1);
    pairs.emplace_back("two", 2);
    pairs.emplace_back("three", 3);
    pairs.emplace_back("four", 4);

    std::map<int, int> indexMap;
    indexMap[0] = 100;
    indexMap[1] = 101;
    indexMap[2] = 103;
    indexMap[3] = 107;

    std::array<int, 6> values6 = {2, 3, 5, 7, 11, 13};

    // Additional test data
    std::list<int> listData = {5, 3, 8, 1, 9, 2, 7, 4, 6};
    std::forward_list<int> fwdListData = {10, 20, 30, 40, 50, 60, 70, 80};

    std::unordered_map<std::string, int> unorderedMap;
    unorderedMap["apple"] = 5;
    unorderedMap["banana"] = 3;
    unorderedMap["cherry"] = 8;
    unorderedMap["date"] = 2;

    std::unordered_set<int> unorderedSet = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    std::vector<std::tuple<int, double, std::string>> tupleData;
    tupleData.emplace_back(1, 1.5, "first");
    tupleData.emplace_back(2, 2.5, "second");
    tupleData.emplace_back(3, 3.5, "third");
    tupleData.emplace_back(4, 4.5, "fourth");

    std::multimap<int, std::string> multimap;
    multimap.insert({1, "one"});
    multimap.insert({2, "two"});
    multimap.insert({1, "uno"});
    multimap.insert({3, "three"});
    multimap.insert({2, "dos"});

    std::multiset<int> multiset = {1, 2, 2, 3, 3, 3, 4, 4, 4, 4};

    std::vector<std::string> strings = {"hello", "world", "test", "container", "obfuscation"};

    std::map<int, double> intDoubleMap;
    for (int i = 0; i < 10; ++i) {
        intDoubleMap[i] = i * 1.5;
    }

    std::set<int> setA = {1, 2, 3, 4, 5, 6, 7, 8};
    std::set<int> setB = {5, 6, 7, 8, 9, 10, 11, 12};

    std::array<int, 10> arrayData;
    for (int i = 0; i < 10; ++i) {
        arrayData[i] = i * i;
    }

    std::vector<std::vector<int>> matrix = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };

    std::map<std::string, std::vector<int>> mapOfVec;
    mapOfVec["first"] = {1, 2, 3};
    mapOfVec["second"] = {4, 5, 6};
    mapOfVec["third"] = {7, 8, 9};

    std::vector<std::map<int, std::string>> vecOfMap;
    std::map<int, std::string> temp1;
    temp1[1] = "a";
    temp1[2] = "b";
    vecOfMap.push_back(temp1);
    std::map<int, std::string> temp2;
    temp2[3] = "c";
    temp2[4] = "d";
    vecOfMap.push_back(temp2);

    std::vector<int> freqData = {1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5};

    std::vector<int> compA = {1, 3, 5, 7, 9};
    std::vector<int> compB = {2, 4, 6, 8, 10};

    std::vector<int> mergeA = {1, 3, 5, 7, 9};
    std::vector<int> mergeB = {2, 4, 6, 8, 10};

    std::vector<int> uniqueData = {1, 2, 2, 3, 3, 3, 4, 4, 4, 4};

    uint64_t checksum = 0x0ULL;
    
    // Original tests
    checksum ^= hashContainer(values, doubles, fixedArray, letters, mapping, "container-test");
    checksum ^= combineSequences("first-label", "second-label", pairs, indexMap, values6);
    checksum ^= hashContainer({3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3}, doubles, fixedArray, letters, mapping, "std-containers");
    
    // New container tests
    checksum ^= listOperations(listData);
    checksum ^= forwardListOperations(fwdListData);
    checksum ^= unorderedMapOperations(unorderedMap);
    checksum ^= unorderedSetOperations(unorderedSet);
    checksum ^= stackOperations(values);
    checksum ^= queueOperations(values);
    checksum ^= priorityQueueOperations(values);
    checksum ^= tupleOperations(tupleData);
    checksum ^= multimapOperations(multimap);
    checksum ^= multisetOperations(multiset);
    checksum ^= vectorTransformations(values);
    checksum ^= stringOperations(strings);
    checksum ^= mapTransformations(intDoubleMap);
    checksum ^= setOperations(setA, setB);
    checksum ^= dequeOperations({1, 2, 3, 4, 5, 6, 7, 8});
    checksum ^= arrayOperations(arrayData);
    checksum ^= nestedContainerOperations(matrix);
    checksum ^= mapOfVectors(mapOfVec);
    checksum ^= vectorOfMaps(vecOfMap);
    checksum ^= complexContainerAlgorithm(freqData);
    checksum ^= containerComparison(compA, compB);
    checksum ^= containerRotation(values, 3);
    checksum ^= containerPartition(values);
    checksum ^= containerMerge(mergeA, mergeB);
    checksum ^= containerUnique(uniqueData);
    checksum ^= containerReverse(values);
    checksum ^= containerShuffleSimulation(values);
    checksum ^= containerSearch(values, 5);
    checksum ^= containerFill(values, 42, 5);
    checksum ^= containerGenerate(values);
    
    checksum = mix64(checksum);

    std::printf("CHECKSUM: 0x%016llx\n", (unsigned long long)checksum);

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("%ld\n", diff);
    return 0;
}
