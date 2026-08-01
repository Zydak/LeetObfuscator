#pragma once

#define LEET_STRING_ENCRYPTION_PROBABILITY(value) __attribute__((annotate("leet.StringEncryptionPass.probability=" #value)))

#define LEET_MBA_EXPANSION_COUNT(count) __attribute__((annotate("leet.MBAPass.expansionCount=" #count)))
#define LEET_MBA_INSTRUCTION_SET(values) __attribute__((annotate("leet.MBAPass.instructionSet=" values)))
#define LEET_MBA_PROBABILITY(value) __attribute__((annotate("leet.MBAPass.probability=" #value)))

#define LEET_BLOCK_SPLITTER_PROBABILITY(value) __attribute__((annotate("leet.BlockSplitterPass.probability=" #value)))

#define LEET_DISPATCHER_PROBABILITY(value) __attribute__((annotate("leet.DispatcherPass.probability=" #value)))

#define LEET_BOGUS_BLOCK_COUNT(value) __attribute__((annotate("leet.AntiAnalysisPass.bogusBlockCount=" #value)))
#define LEET_VALID_BOGUS_BLOCKS_PROBABILITY(value) __attribute__((annotate("leet.AntiAnalysisPass.validBogusBlocksProbability=" #value)))
#define LEET_INVALID_BOGUS_BLOCKS_PROBABILITY(value) __attribute__((annotate("leet.AntiAnalysisPass.invalidBogusBlocksProbability=" #value)))

#define LEET_AAMBA_PROBABILITY(value) __attribute__((annotate("leet.AAMBAPass.probability=" #value)))
#define LEET_AAMBA_TARGET_OPS(values) __attribute__((annotate("leet.AAMBAPass.targetOps=" values)))

#define LEET_PASS_LIST(X, ...) \
    X("StringEncryptionPass", __VA_ARGS__) \
    X("MBAPass", __VA_ARGS__) \
    X("BlockSplitterPass", __VA_ARGS__) \
    X("DispatcherPass", __VA_ARGS__) \
    X("AntiAnalysisPass", __VA_ARGS__) \
    X("AntiAliasingPass", __VA_ARGS__) \
    X("AAMBAPass", __VA_ARGS__)

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

#include <stdio.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <cstdint>
#include <sys/ucontext.h>
#include <cstring>
#include <cstdio>
#include <setjmp.h>

struct NanomiteEntry {
    uint32_t nanomiteId;
    void* functionAddress;
};

extern "C" {
    extern NanomiteEntry __nanomites_table[];
    extern uint32_t __nanomites_table_size;
}

static constexpr uint32_t ADDRESS_STACK_SIZE = 128;
static void* addressStack[ADDRESS_STACK_SIZE];
static uint32_t stackPointer = 0;

__attribute__((optnone))
extern "C" void __leet_exception_handler(int signum, siginfo_t *info, void *ucontext)
{
    static const char msg_stack_underflow[] = "STACK UNDERFLOW!\n";
    static const char msg_found_stack_ptr[] = "FOUND STACK PTR\n";
    static const char msg_stack_lookup_fail[] = "FUFUFUUFUFUFCK\n";
    static const char msg_stack_overflow[] = "STACK OVERFLOW!\n";
    static const char msg_invalid_id[] = "ERROR: Invalid nanomite ID!\n";
    static const char msg_pop_true[]  = "popFromStack=1\n";
    static const char msg_pop_false[] = "popFromStack=0\n";

    ucontext_t *uc = (ucontext_t *)ucontext;
    unsigned long long rip = uc->uc_mcontext.gregs[REG_RIP];
    uint32_t nanomiteId = *((uint32_t*)((uint8_t*)rip + 1));
    bool popFromStack = (nanomiteId == 0);

    void* target = nullptr;

    if (popFromStack)
    {
        if (stackPointer == 0)
        {
            write(STDOUT_FILENO, msg_stack_underflow, sizeof(msg_stack_underflow) - 1);
            _exit(1);
        }

        target = addressStack[stackPointer];
        stackPointer--;
    }
    else
    {
        for (uint32_t i = 0; i < __nanomites_table_size; i++)
        {
            if (__nanomites_table[i].nanomiteId == nanomiteId)
            {
                target = __nanomites_table[i].functionAddress;
                break;
            }
        }
    }

    if (target != nullptr)
    {
        if (!popFromStack)
        {
            stackPointer++;
            if (stackPointer >= ADDRESS_STACK_SIZE)
            {
                write(STDOUT_FILENO, msg_stack_overflow, sizeof(msg_stack_overflow) - 1);
                _exit(1);
            }

            addressStack[stackPointer] = ((uint8_t*)rip) + 6;
        }

        uc->uc_mcontext.gregs[REG_RIP] = (greg_t)target;
    }
    else
    {
        write(STDOUT_FILENO, msg_invalid_id, sizeof(msg_invalid_id) - 1);
        _exit(1);
    }
}

static constexpr size_t kAltStackSize = 8192 * 4;
static uint8_t g_altStack[kAltStackSize];

__attribute__((noinline))
extern "C" bool __leet_exception_handler_setup()
{
    stack_t ss;
    ss.ss_sp = g_altStack;
    ss.ss_size = sizeof(g_altStack);
    ss.ss_flags = 0;
    if (sigaltstack(&ss, nullptr) == -1) {
        perror("sigaltstack failed");
        return false;
    }

    struct sigaction sa;
    sa.sa_sigaction = __leet_exception_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;

    if (sigaction(SIGTRAP, &sa, NULL) == -1) {
        perror("sigaction failed");
        return false;
    }

    return true;
}

[[gnu::constructor]]
static void __leet_exception_handler_init()
{
    if (!__leet_exception_handler_setup())
        exit(1);
}