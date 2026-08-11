#include <iostream>
#include <iomanip>
#include <vector>
#include <array>
#include <deque>
#include <list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <stack>
#include <queue>
#include <bitset>
#include <string>
#include <utility>
#include <tuple>
#include <algorithm>
#include <numeric>
#include <functional>
#include <chrono>

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

template <typename Container>
void printIntContainer(const std::string& label, const Container& c) {
    std::cout << label << ": [";
    bool first = true;
    for (const auto& v : c) {
        if (!first) std::cout << ", ";
        std::cout << v;
        first = false;
    }
    std::cout << "]\n";
}

struct Person {
    std::string name;
    int age;
};

bool comparePersonByAgeThenName(const Person& a, const Person& b) {
    if (a.age != b.age) return a.age < b.age;
    return a.name < b.name;
}

uint64_t vectorSection() {
    std::cout << "-- std::vector --\n";
    uint64_t checksum = 0;

    std::vector<int> v;
    v.reserve(20);
    for (int i = 1; i <= 20; ++i) {
        v.push_back(i * i);
    }
    printIntContainer("squares", v);

    v.insert(v.begin() + 5, -1);
    v.erase(v.begin() + 10);
    printIntContainer("after insert/erase", v);

    std::sort(v.begin(), v.end());
    printIntContainer("sorted", v);

    auto newEnd = std::unique(v.begin(), v.end());
    v.erase(newEnd, v.end());
    printIntContainer("unique", v);

    std::reverse(v.begin(), v.end());
    printIntContainer("reversed", v);

    long long sum = std::accumulate(v.begin(), v.end(), 0LL);
    std::cout << "sum: " << sum << "\n";
    checksum += static_cast<uint64_t>(sum < 0 ? -sum : sum);

    auto foundIt = std::find(v.begin(), v.end(), 81);
    std::cout << "found 81: " << (foundIt != v.end() ? "true" : "false") << "\n";

    long countEven = std::count_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
    std::cout << "even count: " << countEven << "\n";
    checksum += static_cast<uint64_t>(countEven);

    v.erase(std::remove_if(v.begin(), v.end(), [](int x) { return x < 0; }), v.end());
    printIntContainer("after remove negatives", v);

    std::vector<std::vector<int>> grid(4, std::vector<int>(4, 0));
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            grid[i][j] = i * 4 + j;
        }
    }
    std::cout << "2D vector diagonal sum: ";
    int diagSum = 0;
    for (int i = 0; i < 4; ++i) diagSum += grid[i][i];
    std::cout << diagSum << "\n";
    checksum += static_cast<uint64_t>(diagSum);

    std::cout << "capacity >= size: " << (v.capacity() >= v.size() ? "true" : "false") << "\n";

    return checksum;
}

uint64_t arraySection() {
    std::cout << "\n-- std::array --\n";
    uint64_t checksum = 0;

    std::array<int, 10> arr{};
    for (size_t i = 0; i < arr.size(); ++i) {
        arr[i] = static_cast<int>((i + 1) * 7 % 17);
    }
    printIntContainer("array", arr);

    std::array<int, 10> sortedArr = arr;
    std::sort(sortedArr.begin(), sortedArr.end());
    printIntContainer("sorted array", sortedArr);

    int arrSum = std::accumulate(arr.begin(), arr.end(), 0);
    std::cout << "array sum: " << arrSum << "\n";
    checksum += static_cast<uint64_t>(arrSum);

    std::array<int, 10> filled{};
    filled.fill(42);
    printIntContainer("filled array", filled);

    return checksum;
}

uint64_t dequeSection() {
    std::cout << "\n-- std::deque --\n";
    uint64_t checksum = 0;

    std::deque<int> dq;
    for (int i = 1; i <= 5; ++i) {
        dq.push_back(i);
        dq.push_front(-i);
    }
    printIntContainer("deque", dq);

    dq.pop_front();
    dq.pop_back();
    printIntContainer("after pop_front/pop_back", dq);

    int dqSum = std::accumulate(dq.begin(), dq.end(), 0);
    std::cout << "deque sum: " << dqSum << "\n";
    checksum += static_cast<uint64_t>(dqSum < 0 ? -dqSum : dqSum);

    return checksum;
}

uint64_t listSection() {
    std::cout << "\n-- std::list --\n";
    uint64_t checksum = 0;

    std::list<int> lst;
    for (int i = 10; i >= 1; --i) {
        lst.push_back(i);
    }
    printIntContainer("list", lst);

    lst.sort();
    printIntContainer("sorted list", lst);

    std::list<int> secondList = {100, 200, 300};
    auto it = lst.begin();
    std::advance(it, 3);
    lst.splice(it, secondList);
    printIntContainer("after splice", lst);

    lst.remove(200);
    printIntContainer("after remove(200)", lst);

    lst.unique();
    printIntContainer("after unique", lst);

    int lstSum = std::accumulate(lst.begin(), lst.end(), 0);
    std::cout << "list sum: " << lstSum << "\n";
    checksum += static_cast<uint64_t>(lstSum);

    return checksum;
}

uint64_t setSection() {
    std::cout << "\n-- std::set / std::multiset --\n";
    uint64_t checksum = 0;

    std::set<int> setA = {5, 3, 8, 1, 9, 3, 5};
    printIntContainer("setA (deduped, ordered)", setA);

    setA.insert(100);
    setA.erase(1);
    printIntContainer("setA after insert/erase", setA);

    bool has8 = setA.find(8) != setA.end();
    std::cout << "contains 8: " << (has8 ? "true" : "false") << "\n";
    checksum += has8 ? 1 : 0;

    std::set<int> setB = {3, 8, 20, 30};
    std::vector<int> unionResult;
    std::set_union(setA.begin(), setA.end(), setB.begin(), setB.end(), std::back_inserter(unionResult));
    printIntContainer("union(setA, setB)", unionResult);

    std::vector<int> intersectionResult;
    std::set_intersection(setA.begin(), setA.end(), setB.begin(), setB.end(), std::back_inserter(intersectionResult));
    printIntContainer("intersection(setA, setB)", intersectionResult);

    std::multiset<int> multi = {4, 4, 2, 2, 2, 7};
    printIntContainer("multiset", multi);
    std::cout << "count of 2 in multiset: " << multi.count(2) << "\n";
    checksum += static_cast<uint64_t>(multi.count(2));

    return checksum;
}

uint64_t mapSection() {
    std::cout << "\n-- std::map / std::multimap --\n";
    uint64_t checksum = 0;

    std::map<std::string, int> ages;
    ages["Alice"] = 30;
    ages["Bob"] = 25;
    ages["Carol"] = 35;
    ages["Dave"] = 28;

    for (const auto& entry : ages) {
        std::cout << "  " << entry.first << " -> " << entry.second << "\n";
        checksum += static_cast<uint64_t>(entry.second);
    }

    auto it = ages.find("Bob");
    bool foundBob = (it != ages.end());
    std::cout << "found Bob: " << (foundBob ? "true" : "false") << "\n";

    ages.erase("Carol");
    std::cout << "size after erase: " << ages.size() << "\n";
    checksum += ages.size();

    if (ages.count("Alice") > 0) {
        std::cout << "Alice via at(): " << ages.at("Alice") << "\n";
    }

    std::multimap<std::string, int> scores;
    scores.insert({"Team A", 10});
    scores.insert({"Team A", 15});
    scores.insert({"Team B", 20});
    for (const auto& entry : scores) {
        std::cout << "  " << entry.first << " -> " << entry.second << "\n";
        checksum += static_cast<uint64_t>(entry.second);
    }

    return checksum;
}

uint64_t unorderedSection() {
    std::cout << "\n-- std::unordered_set / std::unordered_map --\n";
    uint64_t checksum = 0;

    std::unordered_set<int> uset = {15, 3, 27, 9, 42, 1};
    std::vector<int> sortedFromSet(uset.begin(), uset.end());
    std::sort(sortedFromSet.begin(), sortedFromSet.end());
    printIntContainer("unordered_set contents (sorted for determinism)", sortedFromSet);
    checksum += sortedFromSet.size();

    std::unordered_map<std::string, int> umap;
    umap["zebra"] = 1;
    umap["apple"] = 2;
    umap["mango"] = 3;
    umap["kiwi"] = 4;

    std::vector<std::pair<std::string, int>> sortedFromMap(umap.begin(), umap.end());
    std::sort(sortedFromMap.begin(), sortedFromMap.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& entry : sortedFromMap) {
        std::cout << "  " << entry.first << " -> " << entry.second << "\n";
        checksum += static_cast<uint64_t>(entry.second);
    }

    bool hasKiwi = umap.find("kiwi") != umap.end();
    std::cout << "contains kiwi: " << (hasKiwi ? "true" : "false") << "\n";
    checksum += hasKiwi ? 1 : 0;

    return checksum;
}

uint64_t adapterSection() {
    std::cout << "\n-- std::stack / std::queue / std::priority_queue --\n";
    uint64_t checksum = 0;

    std::stack<int> stk;
    for (int i = 1; i <= 6; ++i) stk.push(i * 10);
    std::cout << "stack pop order: ";
    while (!stk.empty()) {
        std::cout << stk.top() << " ";
        checksum += static_cast<uint64_t>(stk.top());
        stk.pop();
    }
    std::cout << "\n";

    std::queue<int> q;
    for (int i = 1; i <= 6; ++i) q.push(i * 10);
    std::cout << "queue pop order: ";
    while (!q.empty()) {
        std::cout << q.front() << " ";
        checksum += static_cast<uint64_t>(q.front());
        q.pop();
    }
    std::cout << "\n";

    std::priority_queue<int> maxHeap;
    for (int v : {5, 1, 9, 3, 7, 2, 8}) maxHeap.push(v);
    std::cout << "max-heap pop order: ";
    while (!maxHeap.empty()) {
        std::cout << maxHeap.top() << " ";
        checksum += static_cast<uint64_t>(maxHeap.top());
        maxHeap.pop();
    }
    std::cout << "\n";

    std::priority_queue<int, std::vector<int>, std::greater<int>> minHeap;
    for (int v : {5, 1, 9, 3, 7, 2, 8}) minHeap.push(v);
    std::cout << "min-heap pop order: ";
    while (!minHeap.empty()) {
        std::cout << minHeap.top() << " ";
        checksum += static_cast<uint64_t>(minHeap.top());
        minHeap.pop();
    }
    std::cout << "\n";

    return checksum;
}

uint64_t pairTupleSection() {
    std::cout << "\n-- std::pair / std::tuple --\n";
    uint64_t checksum = 0;

    std::pair<std::string, int> p1{"width", 1920};
    std::pair<std::string, int> p2{"height", 1080};
    std::cout << p1.first << "=" << p1.second << ", " << p2.first << "=" << p2.second << "\n";
    checksum += static_cast<uint64_t>(p1.second + p2.second);

    std::vector<std::pair<std::string, int>> pairs = {
        {"charlie", 3}, {"alpha", 1}, {"bravo", 2}, {"delta", 1}
    };
    std::stable_sort(pairs.begin(), pairs.end(),
                      [](const auto& a, const auto& b) { return a.second < b.second; });
    for (const auto& p : pairs) {
        std::cout << "  " << p.first << " : " << p.second << "\n";
    }

    std::tuple<int, double, std::string> record{7, 3.5, "sample"};
    auto [id, value, label] = record;
    std::cout << "tuple -> id=" << id << " value=" << std::fixed << std::setprecision(2)
              << value << " label=" << label << "\n";
    checksum += static_cast<uint64_t>(id);

    bool pairsEqual = (p1 == std::pair<std::string, int>{"width", 1920});
    std::cout << "pair equality check: " << (pairsEqual ? "true" : "false") << "\n";
    checksum += pairsEqual ? 1 : 0;

    return checksum;
}

uint64_t bitsetSection() {
    std::cout << "\n-- std::bitset --\n";
    uint64_t checksum = 0;

    std::bitset<16> bits(0);
    bits.set(0);
    bits.set(4);
    bits.set(8);
    bits.set(15);
    std::cout << "bits: " << bits.to_string() << "\n";
    std::cout << "count: " << bits.count() << "\n";
    checksum += bits.count();

    bits.flip();
    std::cout << "flipped: " << bits.to_string() << "\n";

    bits.reset();
    std::cout << "reset: " << bits.to_string() << "\n";

    std::bitset<8> fromNumber(200);
    std::cout << "200 as bitset<8>: " << fromNumber.to_string() << "\n";
    checksum += fromNumber.to_ulong();

    return checksum;
}

uint64_t numericAlgorithmsSection() {
    std::cout << "\n-- Numeric algorithms --\n";
    uint64_t checksum = 0;

    std::vector<int> data = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5};

    int total = std::accumulate(data.begin(), data.end(), 0);
    std::cout << "accumulate: " << total << "\n";
    checksum += static_cast<uint64_t>(total);

    int product = std::accumulate(data.begin(), data.begin() + 5, 1, std::multiplies<int>());
    std::cout << "product of first 5: " << product << "\n";
    checksum += static_cast<uint64_t>(product);

    int innerProd = std::inner_product(data.begin(), data.begin() + 5, data.begin(), 0);
    std::cout << "inner_product (first 5, self): " << innerProd << "\n";
    checksum += static_cast<uint64_t>(innerProd);

    std::vector<int> partialSums(data.size());
    std::partial_sum(data.begin(), data.end(), partialSums.begin());
    printIntContainer("partial_sum", partialSums);

    std::vector<int> adjDiff(data.size());
    std::adjacent_difference(data.begin(), data.end(), adjDiff.begin());
    printIntContainer("adjacent_difference", adjDiff);

    std::vector<int> transformed(data.size());
    std::transform(data.begin(), data.end(), transformed.begin(), [](int x) { return x * x; });
    printIntContainer("transformed (squares)", transformed);

    return checksum;
}

uint64_t structAndNestedSection() {
    std::cout << "\n-- Struct sorting and nested containers --\n";
    uint64_t checksum = 0;

    std::vector<Person> people = {
        {"Alice", 30}, {"Bob", 25}, {"Carol", 25}, {"Dave", 40}, {"Eve", 30}
    };
    std::sort(people.begin(), people.end(), comparePersonByAgeThenName);
    for (const auto& p : people) {
        std::cout << "  " << p.name << " (" << p.age << ")\n";
        checksum += static_cast<uint64_t>(p.age);
    }

    auto foundIt = std::find_if(people.begin(), people.end(),
                                 [](const Person& p) { return p.name == "Carol"; });
    std::cout << "found Carol: " << (foundIt != people.end() ? "true" : "false") << "\n";

    std::map<std::string, std::vector<int>> groupedScores;
    groupedScores["team_a"] = {90, 85, 88};
    groupedScores["team_b"] = {70, 75};
    groupedScores["team_c"] = {100, 95, 92, 88};

    for (const auto& entry : groupedScores) {
        int groupSum = std::accumulate(entry.second.begin(), entry.second.end(), 0);
        std::cout << "  " << entry.first << " sum=" << groupSum << "\n";
        checksum += static_cast<uint64_t>(groupSum);
    }

    std::array<Person, 3> peopleArray = {
        Person{"Frank", 50}, Person{"Grace", 22}, Person{"Heidi", 33}
    };
    std::sort(peopleArray.begin(), peopleArray.end(), comparePersonByAgeThenName);
    for (const auto& p : peopleArray) {
        std::cout << "  array person: " << p.name << " (" << p.age << ")\n";
    }

    return checksum;
}

uint64_t generatedDataSection() {
    std::cout << "\n-- Deterministically generated dataset --\n";
    uint64_t checksum = 0;

    Lcg rng(123456789ULL);
    std::vector<int> generated;
    generated.reserve(30);
    for (int i = 0; i < 30; ++i) {
        generated.push_back(rng.nextInt(1, 1000));
    }
    printIntContainer("generated", generated);

    std::vector<int> sortedGenerated = generated;
    std::sort(sortedGenerated.begin(), sortedGenerated.end());
    printIntContainer("sorted generated", sortedGenerated);

    int genSum = std::accumulate(generated.begin(), generated.end(), 0);
    std::cout << "generated sum: " << genSum << "\n";
    checksum += static_cast<uint64_t>(genSum);

    std::set<int> uniqueValues(generated.begin(), generated.end());
    std::cout << "unique value count: " << uniqueValues.size() << "\n";
    checksum += uniqueValues.size();

    return checksum;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();

    std::cout << "=== Comprehensive std:: Containers Test ===\n\n";

    uint64_t checksum = 0;
    checksum += vectorSection();
    checksum += arraySection();
    checksum += dequeSection();
    checksum += listSection();
    checksum += setSection();
    checksum += mapSection();
    checksum += unorderedSection();
    checksum += adapterSection();
    checksum += pairTupleSection();
    checksum += bitsetSection();
    checksum += numericAlgorithmsSection();
    checksum += structAndNestedSection();
    checksum += generatedDataSection();

    std::cout << "\n=== Final checksum ===\n";
    std::cout << "TOTAL_CHECKSUM: " << checksum << "\n";

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << diff << "\n";

    return 0;
}
