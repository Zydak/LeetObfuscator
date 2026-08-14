#include "Common.h"

#define LEET_IMPLEMENTATION
#include "../../Leet.h"
#include <iostream>
#include <chrono>

uint64_t mainIntegrationSection() {
    std::cout << "\n-- [Main] Final cross-module integration --\n";
    uint64_t checksum = 0;

    uint64_t touched = sharedResource().touch();
    std::cout << "  sharedResource().touch() from Main = " << touched << "\n";
    checksum += touched;

    uint64_t singletonNow = LazySingleton::instance().current();
    std::cout << "  LazySingleton::instance().current() = " << singletonNow << "\n";
    checksum += singletonNow;

    uint64_t bumped = RegistryCounter::bump();
    std::cout << "  RegistryCounter::bump() from Main = " << bumped << "\n";
    checksum += bumped;

    uint64_t identityResult = templateIdentity<uint64_t>(555ULL);
    std::cout << "  templateIdentity<uint64_t>(555) from Main = " << identityResult << "\n";
    checksum += identityResult;

    uint64_t important = computeImportantValue(3ULL);
    std::cout << "  computeImportantValue(3) from Main = " << important << "\n";
    checksum += important;

    uint64_t cResult = cLinkageAdd(cLinkageMul(3, 4), 5);
    std::cout << "  cLinkageAdd(cLinkageMul(3,4), 5) from Main = " << cResult << "\n";
    checksum += cResult;

    std::unique_ptr<Reporter> reporter = makeDeltaReporter(21ULL);
    uint64_t reportValue = reporter->report();
    std::cout << "  makeDeltaReporter(21)->report() from Main = " << reportValue << "\n";
    checksum += reportValue;

    std::cout << "  final g_guardedResourceRefCount = " << g_guardedResourceRefCount << "\n";
    checksum += g_guardedResourceRefCount;

    std::cout << "  final g_inlineSharedCounter = " << g_inlineSharedCounter << "\n";
    checksum += g_inlineSharedCounter;

    return checksum;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();

    std::cout << "=== Multi-Module Interdependent Stress Test ===\n\n";

    uint64_t checksum = 0;
    checksum += alphaStaticInitSection();
    checksum += alphaConstructOnFirstUseSection();
    checksum += betaManualLifetimeSection();
    checksum += betaUnionLifetimeSection();
    checksum += gammaBitfieldsSection();
    checksum += gammaAttributesAndLinkageSection();
    checksum += deltaNamespaceAdlSection();
    checksum += deltaPolymorphismAndStaticMembersSection();
    checksum += mainIntegrationSection();

    std::cout << "\n=== Final checksum ===\n";
    std::cout << "TOTAL_CHECKSUM: " << checksum << "\n";

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << diff << "\n";

    return 0;
}