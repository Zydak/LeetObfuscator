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
#define LEET_ANTI_ANALYSIS_RDTSC_PROBABILITY(value) __attribute__((annotate("leet.AntiAnalysisPass.rdtscProbability=" #value)))
#define LEET_ANTI_ANALYSIS_VALID_BOGUS_BLOCKS_PROBABILITY(value) __attribute__((annotate("leet.AntiAnalysisPass.validBogusBlocksProbability=" #value)))
#define LEET_ANTI_ANALYSIS_INVALID_BOGUS_BLOCKS_PROBABILITY(value) __attribute__((annotate("leet.AntiAnalysisPass.invalidBogusBlocksProbability=" #value)))

#define LEET_ANTI_ALIASING_PROBABILITY(value) __attribute__((annotate("leet.AntiAliasingPass.probability=" #value)))

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

#define LEET_SKIP_PASS(pass) __attribute__((annotate("leet." pass ".skip")))
#define LEET_FORCE_PASS(pass) __attribute__((annotate("leet." pass ".forcePass")))

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

#ifdef LEET_IMPLEMENTATION

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

extern "C" inline void* __leet_exception_resolve_address(uint32_t nanomiteId)
{
    uint32_t nanomiteIdXored = nanomiteId ^ 0xB16B00B5;
    for (TableChunk* c = __nanomite_chunk_head; c; c = c->next)
        for (uint32_t i = 0; i < c->count; i++)
            if (c->entries[i].nanomiteId == nanomiteIdXored)
                return c->entries[i].functionAddress;
    return nullptr;
}

static thread_local uintptr_t s_PointerStack[512];
static thread_local uint32_t s_StackPointer = 0;
extern "C" inline bool __leet_exception_handle_trap(leet_ctx_t ctx)
{
	// windows doesn't advance RIP immediately, linux does
	#if defined(_WIN32)
	uint32_t nanomiteIDOffset = 4;
    uint32_t garbageBytesOffset = 9;
	#else
	uint32_t nanomiteIDOffset = 3;
    uint32_t garbageBytesOffset = 8;
	#endif

    uintptr_t ip = __leet_exception_get_ip(ctx);
    uint32_t nanomiteId = *((uint32_t*)((uint8_t*)ip + nanomiteIDOffset));
    bool popFromStack = (nanomiteId == 0xB16B00B5);
    bool isTrampolineCall = *((bool*)((uint8_t*)ip + nanomiteIDOffset + 4));

    void* target = nullptr;

    if (!isTrampolineCall || (isTrampolineCall && !popFromStack))
    {
        target = __leet_exception_resolve_address(nanomiteId);
    }
    else
    {
        if (s_StackPointer == 0)
        {
            fputs("ERROR: Exception Handler Stack Underflow!\n", stderr);
        }
        s_StackPointer--;
        target = reinterpret_cast<void*>(s_PointerStack[s_StackPointer]);
    }

    if (target == nullptr)
        return false;

    if (isTrampolineCall && !popFromStack)
    {
        s_PointerStack[s_StackPointer] = ip + garbageBytesOffset;
        s_StackPointer++;

        if (s_StackPointer >= 512)
        {
            fputs("ERROR: Exception Handler Stack Overflow!\n", stderr);
        }
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

    fputs("ERROR: Invalid nanomite ID!\n", stderr);
    _exit(1);
}

static PVOID s_leetVehHandle = nullptr;

extern "C" bool __leet_exception_handler_setup()
{
    s_leetVehHandle = AddVectoredExceptionHandler(1, __leet_exception_veh_handler);
    if (!s_leetVehHandle)
    {
        fprintf(stderr, "AddVectoredExceptionHandler failed: %lu\n", GetLastError());
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
    static const char invalidIdMessage[] = "ERROR: Invalid nanomite ID!\n";

    ucontext_t *uc = (ucontext_t *)ucontext;

    if (!__leet_exception_handle_trap(uc))
    {
        write(STDOUT_FILENO, invalidIdMessage, sizeof(invalidIdMessage) - 1);
        _exit(1);
    }
}

static constexpr size_t kAltStackSize = 8192 * 4;
static uint8_t g_altStack[kAltStackSize];

extern "C" bool __leet_exception_handler_setup()
{
    stack_t ss;
    ss.ss_sp = g_altStack;
    ss.ss_size = sizeof(g_altStack);
    ss.ss_flags = 0;
    if (sigaltstack(&ss, nullptr) == -1)
    {
        perror("sigaltstack failed");
        return false;
    }

    struct sigaction sa;
    sa.sa_sigaction = __leet_exception_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;

    if (sigaction(SIGTRAP, &sa, NULL) == -1)
    {
        perror("sigaction failed");
        return false;
    }

    return true;
}

#endif // _WIN32 / Linux

[[gnu::constructor]]
static void __leet_exception_handler_init()
{
    if (!__leet_exception_handler_setup())
        exit(1);
}

#endif // LEET_IMPLEMENTATION