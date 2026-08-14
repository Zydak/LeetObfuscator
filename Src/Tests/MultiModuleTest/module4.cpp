#include "Common.h"
#include "../../Leet.h"
#include <iostream>

struct PackedFlags {
    uint32_t flagA : 1;
    uint32_t flagB : 1;
    uint32_t flagC : 3;
    uint32_t reserved : 11;
    uint32_t value : 16;
};

extern "C" {
uint64_t cLinkageAdd(uint64_t a, uint64_t b) { return a + b; }
uint64_t cLinkageMul(uint64_t a, uint64_t b) { return a * b; }
}

template uint64_t templateIdentity<uint64_t>(uint64_t);

[[nodiscard]] uint64_t computeImportantValue(uint64_t seed) {
    return seed * 31ULL + 7ULL;
}

uint64_t g_earlyTouchResult = 0;

struct EarlyToucher {
    EarlyToucher() { g_earlyTouchResult = sharedResource().touch(); }
};

static EarlyToucher g_earlyToucherInstance;

thread_local uint64_t t_localCounter = 700;

namespace {
uint64_t internalCombine(uint64_t a, uint64_t b) {
    return (a + b) * 3ULL;
}
}

uint64_t gammaBitfieldsSection() {
    std::cout << "\n-- [Gamma] Bitfields, alignas/alignof, extern \"C\" linkage --\n";
    uint64_t checksum = 0;

    PackedFlags flags{};
    flags.flagA = 1;
    flags.flagB = 0;
    flags.flagC = 5;
    flags.reserved = 0;
    flags.value = 4321;

    uint64_t readBack = static_cast<uint64_t>(flags.flagA)
                       + static_cast<uint64_t>(flags.flagB) * 2ULL
                       + static_cast<uint64_t>(flags.flagC) * 4ULL
                       + static_cast<uint64_t>(flags.value) * 100ULL;
    std::cout << "  flagA=" << flags.flagA << " flagB=" << flags.flagB
              << " flagC=" << flags.flagC << " value=" << flags.value << "\n";
    checksum += readBack;

    std::cout << "  sizeof(PackedFlags)=" << sizeof(PackedFlags)
              << " alignof(PackedFlags)=" << alignof(PackedFlags) << "\n";
    checksum += sizeof(PackedFlags) + alignof(PackedFlags);

    uint64_t cAdd = cLinkageAdd(111, 222);
    uint64_t cMul = cLinkageMul(6, 7);
    std::cout << "  extern \"C\" cLinkageAdd(111,222)=" << cAdd << " cLinkageMul(6,7)=" << cMul << "\n";
    checksum += cAdd + cMul;

    uint64_t combined = internalCombine(20, 30);
    std::cout << "  internal-linkage internalCombine(20,30) = " << combined << "\n";
    checksum += combined;

    std::cout << "  static-init-time sharedResource().touch() result = " << g_earlyTouchResult << "\n";
    checksum += g_earlyTouchResult;

    t_localCounter += 1;
    std::cout << "  thread_local t_localCounter after increment = " << t_localCounter << "\n";
    checksum += t_localCounter;

    return checksum;
}

uint64_t gammaAttributesAndLinkageSection() {
    std::cout << "\n-- [Gamma] Attributes, user-defined literals, extern template --\n";
    uint64_t checksum = 0;

    uint64_t twoKb = 2_kb;
    uint64_t fiveMb = 5_mb;
    std::cout << "  2_kb = " << twoKb << " 5_mb = " << fiveMb << "\n";
    checksum += twoKb + fiveMb;

    uint64_t identityResult = templateIdentity<uint64_t>(918273ULL);
    std::cout << "  templateIdentity<uint64_t>(918273) = " << identityResult << "\n";
    checksum += identityResult;

    uint64_t important = computeImportantValue(50ULL);
    std::cout << "  computeImportantValue(50) = " << important << "\n";
    checksum += important;

    [[maybe_unused]] uint64_t debugOnlyMarker = 4242;

    g_inlineSharedCounter += 1ULL;
    std::cout << "  g_inlineSharedCounter after increment = " << g_inlineSharedCounter << "\n";
    checksum += g_inlineSharedCounter;

    return checksum;
}