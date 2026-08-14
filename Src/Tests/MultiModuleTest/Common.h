#pragma once
#include <cstdint>
#include <memory>

struct Lcg {
    uint64_t state;
    explicit Lcg(uint64_t seed) : state(seed) {}
    uint64_t nextRaw() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    }
    uint64_t nextInt(uint64_t lo, uint64_t hi) {
        uint64_t range = hi - lo + 1ULL;
        return lo + (nextRaw() % range);
    }
};

class GuardedResource {
public:
    GuardedResource() : data(0) {}
    uint64_t touch() { return ++data; }
    uint64_t value() const { return data; }
private:
    uint64_t data;
};

extern uint64_t g_guardedResourceRefCount;
extern GuardedResource* g_guardedResourcePtr;

inline GuardedResource& sharedResource() {
    return *g_guardedResourcePtr;
}

namespace detail {

class GuardedResourceInitializer {
public:
    GuardedResourceInitializer() {
        if (g_guardedResourceRefCount == 0ULL) {
            g_guardedResourcePtr = new GuardedResource();
        }
        ++g_guardedResourceRefCount;
    }
    ~GuardedResourceInitializer() {
        --g_guardedResourceRefCount;
        if (g_guardedResourceRefCount == 0ULL) {
            delete g_guardedResourcePtr;
            g_guardedResourcePtr = nullptr;
        }
    }
};

}

namespace {
detail::GuardedResourceInitializer g_niftyCounterGuard;
}

class LazySingleton {
public:
    static LazySingleton& instance() {
        static LazySingleton inst;
        return inst;
    }
    uint64_t next() { return ++counter; }
    uint64_t current() const { return counter; }
private:
    LazySingleton() : counter(2000) {}
    uint64_t counter;
};

class RegistryCounter {
public:
    static uint64_t count;
    static uint64_t bump();
};

class Reporter {
public:
    virtual ~Reporter() = default;
    virtual uint64_t report() const = 0;
};

std::unique_ptr<Reporter> makeDeltaReporter(uint64_t seed);

extern "C" {
uint64_t cLinkageAdd(uint64_t a, uint64_t b);
uint64_t cLinkageMul(uint64_t a, uint64_t b);
}

constexpr uint64_t operator"" _kb(unsigned long long v) {
    return static_cast<uint64_t>(v) * 1024ULL;
}

constexpr uint64_t operator"" _mb(unsigned long long v) {
    return static_cast<uint64_t>(v) * 1024ULL * 1024ULL;
}

inline uint64_t g_inlineSharedCounter = 500;

template <typename T>
T templateIdentity(T v) { return v; }

extern template uint64_t templateIdentity<uint64_t>(uint64_t);

[[nodiscard]] uint64_t computeImportantValue(uint64_t seed);

extern uint64_t g_earlyTouchResult;

inline uint64_t g_declvalTarget = 888;

inline decltype(auto) getDeclTypeAutoRef() {
    return (g_declvalTarget);
}

uint64_t alphaStaticInitSection();
uint64_t alphaConstructOnFirstUseSection();
uint64_t betaManualLifetimeSection();
uint64_t betaUnionLifetimeSection();
uint64_t gammaBitfieldsSection();
uint64_t gammaAttributesAndLinkageSection();
uint64_t deltaNamespaceAdlSection();
uint64_t deltaPolymorphismAndStaticMembersSection();