#include "Common.h"
#include "../../Leet.h"
#include <iostream>

uint64_t g_guardedResourceRefCount = 0;
GuardedResource* g_guardedResourcePtr = nullptr;

struct ChainNodeA {
    uint64_t value;
    ChainNodeA() : value(11) {}
};

ChainNodeA g_chainA;

struct ChainNodeB {
    uint64_t value;
    ChainNodeB() : value(g_chainA.value + 22ULL) {}
};

ChainNodeB g_chainB;

struct ChainNodeC {
    uint64_t value;
    ChainNodeC() : value(g_chainB.value + 33ULL) {}
};

ChainNodeC g_chainC;

uint64_t alphaStaticInitSection() {
    std::cout << "-- [Alpha] Static-storage-duration constructors chained within one module --\n";
    uint64_t checksum = 0;

    std::cout << "  g_chainA.value=" << g_chainA.value
              << " g_chainB.value=" << g_chainB.value
              << " g_chainC.value=" << g_chainC.value << "\n";
    checksum += g_chainA.value + g_chainB.value + g_chainC.value;

    uint64_t touched = sharedResource().touch();
    std::cout << "  sharedResource().touch() from Alpha = " << touched << "\n";
    checksum += touched;

    std::cout << "  g_guardedResourceRefCount (one per module that included Common.h) = "
              << g_guardedResourceRefCount << "\n";
    checksum += g_guardedResourceRefCount;

    return checksum;
}

uint64_t alphaConstructOnFirstUseSection() {
    std::cout << "\n-- [Alpha] Construct-on-first-use singleton (function-local static in an inline function) --\n";
    uint64_t checksum = 0;

    uint64_t n1 = LazySingleton::instance().next();
    uint64_t n2 = LazySingleton::instance().next();
    uint64_t n3 = LazySingleton::instance().next();
    std::cout << "  LazySingleton next() sequence: " << n1 << ", " << n2 << ", " << n3 << "\n";
    checksum += n1 + n2 + n3;

    return checksum;
}