#pragma once

#define LEET_STRING_ENCRYPTION_PROBABILITY(value) __attribute__((annotate("leet.StringEncryptionPass.probability=" #value)))

#define LEET_MBA_EXPANSION_COUNT(count) __attribute__((annotate("leet.MBAPass.expansionCount=" #count)))
#define LEET_MBA_INSTRUCTION_SET(values) __attribute__((annotate("leet.MBAPass.instructionSet=" values)))
#define LEET_MBA_PROBABILITY(value) __attribute__((annotate("leet.MBAPass.probability=" #value)))

#define LEET_BLOCK_SPLITTER_PROBABILITY(value) __attribute__((annotate("leet.BlockSplitterPass.probability=" #value)))
#define LEET_BLOCK_SPLITTER_SPLIT_SIZE(value) __attribute__((annotate("leet.BlockSplitterPass.blockSplitSize=" #value)))

#define LEET_DISPATCHER_PROBABILITY(value) __attribute__((annotate("leet.DispatcherPass.probability=" #value)))

#define LEET_ANTI_ANALYSIS_PROBABILITY(value) __attribute__((annotate("leet.AntiAnalysisPass.probability=" #value)))
#define LEET_ANTI_ANALYSIS_BOGUS_INSERT_POSITION(value) __attribute__((annotate("leet.AntiAnalysisPass.bogusInsertPosition=" value)))
#define LEET_ANTI_ANALYSIS_RDTSC_RATIO(value) __attribute__((annotate("leet.AntiAnalysisPass.rdtscRatio=" #value)))
#define LEET_ANTI_ANALYSIS_OPAQUE_RATIO(value) __attribute__((annotate("leet.AntiAnalysisPass.opaqueRatio=" #value)))
#define LEET_ANTI_ANALYSIS_PID_RATIO(value) __attribute__((annotate("leet.AntiAnalysisPass.pidRatio=" #value)))
#define LEET_ANTI_ANALYSIS_BLACK_LIST_RATIO(value) __attribute__((annotate("leet.AntiAnalysisPass.blackListRatio=" #value)))

#define LEET_ANTI_ALIASING_PROBABILITY(value) __attribute__((annotate("leet.AntiAliasingPass.probability=" #value)))
#define LEET_ANTI_ALIASING_OPAQUE_PROBABILITY(value) __attribute__((annotate("leet.AntiAliasingPass.opaqueProbability=" #value)))
#define LEET_ANTI_ALIASING_REUSE_PROBABILITY(value) __attribute__((annotate("leet.AntiAliasingPass.reuseProbability=" #value)))

#define LEET_AAMBA_PROBABILITY(value) __attribute__((annotate("leet.AAMBAPass.probability=" #value)))
#define LEET_AAMBA_TARGET_OPS(values) __attribute__((annotate("leet.AAMBAPass.targetOps=" values)))

#define LEET_NANOMITES_CALLS_PROBABILITY(value) __attribute__((annotate("leet.NanomitesPass.callsProbability=" #value)))

#define LEET_VARIABLE_SPLITTING_PROBABILITY(value) __attribute__((annotate("leet.VariableSplittingPass.probability=" #value)))
#define LEET_VARIABLE_SPLITTING_SPLIT_COUNT(value) __attribute__((annotate("leet.VariableSplittingPass.splitCount=" #value)))

__attribute__((noinline))
__attribute__((optnone))
extern "C" void __leet_nanomite_marker();

// Macro to mark individual call sites for nanomite obfuscation
// Usage: LEET_NANOMITE_CALL(targetFunction(args))
// This adds a marker call that the NanomitesPass will detect and apply nanomite protection to the following call
#define LEET_NANOMITE_CALL(func) ({ \
    __leet_nanomite_marker(); \
    func; \
})

#define LEET_PASS_LIST(X, ...) \
    X("StringEncryptionPass", __VA_ARGS__) \
    X("MBAPass", __VA_ARGS__) \
    X("BlockSplitterPass", __VA_ARGS__) \
    X("DispatcherPass", __VA_ARGS__) \
    X("AntiAnalysisPass", __VA_ARGS__) \
    X("AntiAliasingPass", __VA_ARGS__) \
    X("AAMBAPass", __VA_ARGS__) \
    X("NanomitesPass", __VA_ARGS__)

#define LEET_SKIP_PASS(pass) __attribute__((annotate("leet." pass ".skip=true")))
#define LEET_FORCE_PASS(pass) __attribute__((annotate("leet." pass ".forcePass=true")))

#define LEET_RUNTIME_SEED_PASS(pass, seed) __attribute__((annotate("leet." pass ".runtimeSeed=" #seed)))
#define LEET_MIN_FUNCTION_SIZE(pass, size) __attribute__((annotate("leet." pass ".minFunctionSize=" #size)))
#define LEET_MAX_FUNCTION_SIZE(pass, size) __attribute__((annotate("leet." pass ".maxFunctionSize=" #size)))
#define LEET_MIN_BLOCK_SIZE(pass, size) __attribute__((annotate("leet." pass ".minBlockSize=" #size)))
#define LEET_MAX_BLOCK_SIZE(pass, size) __attribute__((annotate("leet." pass ".maxBlockSize=" #size)))

#define LEET_SKIP_EXPAND(pass, ...) LEET_SKIP_PASS(pass)
#define LEET_FORCE_EXPAND(pass, ...) LEET_FORCE_PASS(pass)

#define LEET_RUNTIME_SEED_EXPAND(pass, seed) LEET_RUNTIME_SEED_PASS(pass, seed)
#define LEET_MIN_FUNCTION_SIZE_EXPAND(pass, size) LEET_MIN_FUNCTION_SIZE(pass, size)
#define LEET_MAX_FUNCTION_SIZE_EXPAND(pass, size) LEET_MAX_FUNCTION_SIZE(pass, size)
#define LEET_MIN_BLOCK_SIZE_EXPAND(pass, size) LEET_MIN_BLOCK_SIZE(pass, size)
#define LEET_MAX_BLOCK_SIZE_EXPAND(pass, size) LEET_MAX_BLOCK_SIZE(pass, size)

// _ALL variants
#define LEET_SKIP_ALL LEET_PASS_LIST(LEET_SKIP_EXPAND)
#define LEET_FORCE_ALL LEET_PASS_LIST(LEET_FORCE_EXPAND)
#define LEET_RUNTIME_SEED_ALL(seed) LEET_PASS_LIST(LEET_RUNTIME_SEED_EXPAND, seed)
#define LEET_MIN_FUNCTION_SIZE_ALL(size) LEET_PASS_LIST(LEET_MIN_FUNCTION_SIZE_EXPAND, size)
#define LEET_MAX_FUNCTION_SIZE_ALL(size) LEET_PASS_LIST(LEET_MAX_FUNCTION_SIZE_EXPAND, size)
#define LEET_MIN_BLOCK_SIZE_ALL(size) LEET_PASS_LIST(LEET_MIN_BLOCK_SIZE_EXPAND, size)
#define LEET_MAX_BLOCK_SIZE_ALL(size) LEET_PASS_LIST(LEET_MAX_BLOCK_SIZE_EXPAND, size)


//#define LEET_IMPLEMENTATION
#ifdef LEET_IMPLEMENTATION

#include "NanomiteTraps.h"

__attribute__((noinline))
__attribute__((optnone))
extern "C" void __leet_nanomite_marker()
{
    
}

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
	#include <windows.h>
#else
	#include <signal.h>
	#include <ucontext.h>
	#include <sys/ucontext.h>
	#include <unistd.h>
#endif

struct NanomiteEntry { uint32_t nanomiteId; void* functionAddress; };
struct TableChunk { const NanomiteEntry* entries; uint32_t count; TableChunk* next; };

extern "C" TableChunk* __nanomite_chunk_head = nullptr;
extern "C" uint64_t __leet_poison_ring[8] = {
    0x1020304050607080ULL,
    0x2030405060708090ULL,
    0x30405060708090A0ULL,
    0x405060708090A0B0ULL,
    0x5060708090A0B0C0ULL,
    0x60708090A0B0C0D0ULL,
    0x708090A0B0C0D0E0ULL,
    0x1020304050607080ULL ^ 0x2030405060708090ULL ^ 0x30405060708090A0ULL ^ 0x405060708090A0B0ULL ^ 0x5060708090A0B0C0ULL ^ 0x60708090A0B0C0D0ULL ^ 0x708090A0B0C0D0E0ULL
};

#if defined(_WIN32)
using leet_ctx_t = PCONTEXT;
#else
using leet_ctx_t = ucontext_t*;
#endif

extern "C" inline uintptr_t __leet_exception_get_ip(leet_ctx_t ctx)
{
#if defined(_WIN32)
	#if defined(_WIN64)
		return (uintptr_t)ctx->Rip;
	#else
		return (uintptr_t)ctx->Eip;
	#endif
#else
	#if defined(__x86_64__)
		return (uintptr_t)ctx->uc_mcontext.gregs[REG_RIP];
	#else
		return (uintptr_t)ctx->uc_mcontext.gregs[REG_EIP];
  	#endif
#endif
}

extern "C" inline void __leet_exception_set_ip(leet_ctx_t ctx, uintptr_t v)
{
#if defined(_WIN32)
	#if defined(_WIN64)
		ctx->Rip = (DWORD64)v;
	#else
		ctx->Eip = (DWORD)v;
	#endif
#else
	#if defined(__x86_64__)
		ctx->uc_mcontext.gregs[REG_RIP] = (greg_t)v;
	#else
		ctx->uc_mcontext.gregs[REG_EIP] = (greg_t)v;
	#endif
#endif
}

LEET_VARIABLE_SPLITTING_SPLIT_COUNT(1)
LEET_ANTI_ANALYSIS_BLACK_LIST_RATIO(0)
LEET_ANTI_ANALYSIS_PID_RATIO(0)
extern "C" inline void* __leet_exception_resolve_address(uint32_t nanomiteId)
{
    using namespace LeetObfuscator;
    const uint64_t ringSum = __leet_poison_ring[0] ^ __leet_poison_ring[1] ^ __leet_poison_ring[2] ^ __leet_poison_ring[3] ^
                             __leet_poison_ring[4] ^ __leet_poison_ring[5] ^ __leet_poison_ring[6] ^ __leet_poison_ring[7];
    const uint32_t searchKey = nanomiteId ^ (kNanomiteTableMask ^ (uint32_t)ringSum);

    for (TableChunk* c = __nanomite_chunk_head; c; c = c->next)
    {
        if (c->count == 0)
            continue;

        // quick boundary check
        if (searchKey < c->entries[0].nanomiteId || searchKey > c->entries[c->count - 1].nanomiteId)
            continue;

        // Binary search
        int32_t low = 0;
        int32_t high = int32_t(c->count) - 1;
        while (low <= high)
        {
            int32_t mid = low + ((high - low) >> 1);
            uint32_t midVal = c->entries[mid].nanomiteId;

            if (midVal == searchKey)
            {
                return (void*)((uintptr_t)c + (uintptr_t)c->entries[mid].functionAddress);
            }

            if (midVal < searchKey)
            {
                low = mid + 1;
            }
            else
            {
                high = mid - 1;
            }
        }
    }
    return nullptr;
}

static thread_local uintptr_t s_PointerStack[512];
static thread_local uint32_t s_StackPointer = 0;
static uint64_t stackHash = __rdtsc();

extern "C"
//LEET_MBA_PROBABILITY(10)
LEET_VARIABLE_SPLITTING_SPLIT_COUNT(1)
LEET_ANTI_ANALYSIS_BLACK_LIST_RATIO(0)
LEET_ANTI_ANALYSIS_PID_RATIO(0)
inline bool __leet_exception_handle_trap(leet_ctx_t ctx)
{
    using namespace LeetObfuscator;

#if defined(_WIN32)
    constexpr uintptr_t kCCAdjust = 1; // linux advances the ip right away, windows doesn't
#else
    constexpr uintptr_t kCCAdjust = 0;
#endif

    uintptr_t ip = __leet_exception_get_ip(ctx);
    const uint8_t* base = (const uint8_t*)(ip + kCCAdjust); // base[0] == selector/primary opcode

    uint8_t primary = base[0];
    uint8_t templateIndex = gPrimaryOpcodeToTemplate.data[primary];
    if (templateIndex == 0xFF)
        return false; // not leet trap

    const TrapTemplate& trapTemplate = gTrapTemplates[templateIndex];

    uint8_t keyIndex = base[1 + trapTemplate.sibRelativeOffset] & 0x0F;
    uint32_t key = gTrapKeyTable[keyIndex];

    const uint8_t* rawPayload = base + 1 + trapTemplate.decoyBytesBeforePayload;
    uint8_t orderedBytes[4];
    for (int i = 0; i < 4; i++)
        orderedBytes[trapTemplate.shuffle.order[i]] = rawPayload[i];

    uint32_t encoded = (uint32_t(orderedBytes[0]) << 24) | (uint32_t(orderedBytes[1]) << 16)
        | (uint32_t(orderedBytes[2]) << 8) | uint32_t(orderedBytes[3]);
    
    uint32_t payload = encoded ^ key;

    bool isTrampolineCall = (payload >> 31) & 1u;
    uint32_t nanomiteId = payload & 0x7FFFFFFFu;
    bool popFromStack = (nanomiteId == 0);

    uint32_t totalLen = 1u + trapTemplate.decoyBytesBeforePayload + 4u + trapTemplate.decoyBytesAfterPayload;
    uintptr_t afterTrap = (uintptr_t)base + totalLen;

    void* target = nullptr;

    if (!isTrampolineCall || (isTrampolineCall && !popFromStack))
    {
        target = __leet_exception_resolve_address(nanomiteId);
    }
    else
    {
        #ifdef LEET_NANOMITE_ASSERTS
        if (s_StackPointer == 0)
            fputs("ERROR: Exception Handler Stack Underflow!\n", stderr);
        #endif
        s_StackPointer--;
        target = (void*)(s_PointerStack[s_StackPointer] ^ stackHash);
    }

    if (target == nullptr)
        return false;

    if (isTrampolineCall && !popFromStack)
    {
        s_PointerStack[s_StackPointer] = afterTrap ^ stackHash;
        s_StackPointer++;
        #ifdef LEET_NANOMITE_ASSERTS
        if (s_StackPointer >= 512)
            fputs("ERROR: Exception Handler Stack Overflow!\n", stderr);
        #endif
    }

    __leet_exception_set_ip(ctx, (uintptr_t)target);
    return true;
}

#if defined(_WIN32)

extern "C" LONG CALLBACK __leet_exception_veh_handler(PEXCEPTION_POINTERS ExceptionInfo)
{
    if (ExceptionInfo->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT)
        return EXCEPTION_CONTINUE_SEARCH;

    if (__leet_exception_handle_trap(ExceptionInfo->ContextRecord))
        return EXCEPTION_CONTINUE_EXECUTION;

    #ifdef LEET_NANOMITE_ASSERTS
    fputs("ERROR: Invalid nanomite ID!\n", stderr);
    _exit(1);
    #else
    return EXCEPTION_CONTINUE_EXECUTION;
    #endif
}

static PVOID s_leetVehHandle = nullptr;

extern "C" [[gnu::always_inline]] bool __leet_exception_handler_setup()
{
    s_leetVehHandle = AddVectoredExceptionHandler(1, __leet_exception_veh_handler);
    if (!s_leetVehHandle)
    {
        #ifdef LEET_NANOMITE_ASSERTS
        fprintf(stderr, "AddVectoredExceptionHandler failed: %lu\n", GetLastError());
        #endif

        return false;
    }
    return true;
}

extern "C" void __leet_exception_handler_teardown()
{
    if (s_leetVehHandle)
    {
        RemoveVectoredExceptionHandler(s_leetVehHandle);
        s_leetVehHandle = nullptr;
    }
}

#else // Linux

extern "C" void __leet_exception_handler(int signum, siginfo_t *info, void *ucontext)
{
    ucontext_t *uc = (ucontext_t *)ucontext;

    if (!__leet_exception_handle_trap(uc))
    {
        #ifdef LEET_NANOMITE_ASSERTS
        static const char invalidIdMessage[] = "ERROR: Invalid nanomite ID!\n";
        write(STDOUT_FILENO, invalidIdMessage, sizeof(invalidIdMessage) - 1);
        _exit(1);
        #endif
    }
}

static constexpr size_t kAltStackSize = 8192 * 4;
static uint8_t g_altStack[kAltStackSize];

extern "C" [[gnu::always_inline]] bool __leet_exception_handler_setup()
{
    stack_t ss;
    ss.ss_sp = g_altStack;
    ss.ss_size = sizeof(g_altStack);
    ss.ss_flags = 0;
    if (sigaltstack(&ss, nullptr) == -1)
    {
        #ifdef LEET_NANOMITE_ASSERTS
        perror("sigaltstack failed");
        #endif
        return false;
    }

    struct sigaction sa;
    sa.sa_sigaction = __leet_exception_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;

    if (sigaction(SIGTRAP, &sa, NULL) == -1)
    {
        #ifdef LEET_NANOMITE_ASSERTS
        perror("sigaction failed");
        #endif
        return false;
    }

    return true;
}

#endif // _WIN32 / Linux

[[gnu::constructor(101)]]
static void __leet_exception_handler_init()
{
    if (!__leet_exception_handler_setup())
    {
        #ifdef LEET_NANOMITE_ASSERTS
        exit(1);
        #endif
    }
}

#endif // LEET_IMPLEMENTATION