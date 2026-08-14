#include "Common.h"
#include "../../Leet.h"
#include <iostream>

namespace outerns::innerns {

struct Point {
    int64_t x, y;
    Point(int64_t xi, int64_t yi) : x(xi), y(yi) {}
};

Point operator+(const Point& a, const Point& b) {
    return Point(a.x + b.x, a.y + b.y);
}

std::ostream& operator<<(std::ostream& os, const Point& p) {
    os << "(" << p.x << ", " << p.y << ")";
    return os;
}

}

uint64_t RegistryCounter::count = 0;

uint64_t RegistryCounter::bump() {
    return ++count;
}

class DeltaReporter : public Reporter {
public:
    explicit DeltaReporter(uint64_t seed) : seed_(seed) {}
    uint64_t report() const override { return seed_ * 17ULL + 3ULL; }
private:
    uint64_t seed_;
};

std::unique_ptr<Reporter> makeDeltaReporter(uint64_t seed) {
    return std::make_unique<DeltaReporter>(seed);
}

namespace {
uint64_t internalCombine(uint64_t a, uint64_t b) {
    return (a ^ b) + 0x1111ULL;
}
}

uint64_t deltaNamespaceAdlSection() {
    std::cout << "\n-- [Delta] Nested namespaces and argument-dependent lookup (ADL) --\n";
    uint64_t checksum = 0;

    outerns::innerns::Point p1(3, 4);
    outerns::innerns::Point p2(10, -2);
    auto p3 = p1 + p2;
    std::cout << "  p1 + p2 = " << p3 << "\n";
    checksum += static_cast<uint64_t>(p3.x < 0 ? -p3.x : p3.x) + static_cast<uint64_t>(p3.y < 0 ? -p3.y : p3.y);

    uint64_t combined = internalCombine(77, 55);
    std::cout << "  internal-linkage internalCombine(77,55) [Delta's own copy] = " << combined << "\n";
    checksum += combined;

    decltype(auto) refTarget = getDeclTypeAutoRef();
    uint64_t beforeMutation = refTarget;
    getDeclTypeAutoRef() += 111ULL;
    uint64_t afterMutation = g_declvalTarget;
    std::cout << "  decltype(auto) reference-returning mutation: " << beforeMutation
              << " -> " << afterMutation << "\n";
    checksum += (afterMutation - beforeMutation);

    return checksum;
}

uint64_t deltaPolymorphismAndStaticMembersSection() {
    std::cout << "\n-- [Delta] Cross-module polymorphism and out-of-line static members --\n";
    uint64_t checksum = 0;

    uint64_t b1 = RegistryCounter::bump();
    uint64_t b2 = RegistryCounter::bump();
    std::cout << "  RegistryCounter::bump() = " << b1 << ", " << b2 << "\n";
    checksum += b1 + b2;

    std::unique_ptr<Reporter> reporter = makeDeltaReporter(9ULL);
    uint64_t reportValue = reporter->report();
    std::cout << "  reporter->report() (concrete type hidden from callers) = " << reportValue << "\n";
    checksum += reportValue;

    return checksum;
}