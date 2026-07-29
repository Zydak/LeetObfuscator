#pragma once

#define LEET_STRING_ENCRYPTION_PROBABILITY(value) __attribute__((annotate("leet.StringEncryptionPass.probability=" #value)))

#define LEET_MBA_EXPANSION_COUNT(count) __attribute__((annotate("leet.MBAPass.expansionCount=" #count)))
#define LEET_MBA_INSTRUCTION_SET(values) __attribute__((annotate("leet.MBAPass.instructionSet=" values)))
#define LEET_MBA_PROBABILITY(value) __attribute__((annotate("leet.MBAPass.probability=" #value)))

#define LEET_MAX_BLOCK_SIZE(size) __attribute__((annotate("leet.BlockSplitterPass.maxBlockSize=" #size)))
#define LEET_MIN_BLOCK_SIZE(size) __attribute__((annotate("leet.BlockSplitterPass.minBlockSize=" #size)))
#define LEET_BLOCK_SPLITTER_PROBABILITY(value) __attribute__((annotate("leet.BlockSplitterPass.probability=" #value)))

#define LEET_DISPATCHER_PROBABILITY(value) __attribute__((annotate("leet.DispatcherPass.probability=" #value)))

#define LEET_BOGUS_BLOCK_COUNT(value) __attribute__((annotate("leet.AntiAnalysisPass.bogusBlockCount=" #value)))
#define LEET_VALID_BOGUS_BLOCKS_PROBABILITY(value) __attribute__((annotate("leet.AntiAnalysisPass.validBogusBlocksProbability=" #value)))
#define LEET_INVALID_BOGUS_BLOCKS_PROBABILITY(value) __attribute__((annotate("leet.AntiAnalysisPass.invalidBogusBlocksProbability=" #value)))

#define LEET_AAMBA_PROBABILITY(value) __attribute__((annotate("leet.AAMBAPass.probability=" #value)))
#define LEET_AAMBA_TARGET_OPS(values) __attribute__((annotate("leet.AAMBAPass.targetOps=" values)))

#define LEET_PASS_LIST(X) \
    X("StringEncryptionPass") \
    X("MBAPass") \
    X("BlockSplitterPass") \
    X("DispatcherPass") \
    X("AntiAnalysisPass") \
    X("AntiAliasingPass") \
    X("AAMBAPass")

#define LEET_SKIP_PASS(pass) \
    __attribute__((annotate("leet." pass ".skip")))

#define LEET_FORCE_PASS(pass) \
    __attribute__((annotate("leet." pass ".forcePass")))

#define LEET_RUNTIME_SEED_PASS(pass, seed) \
    __attribute__((annotate("leet." pass ".runtimeSeed=" #seed)))

#define LEET_MIN_FUNCTION_SIZE(pass, size) \
    __attribute__((annotate("leet." pass ".minFunctionSize=" #size)))

#define LEET_MAX_FUNCTION_SIZE(pass, size) \
    __attribute__((annotate("leet." pass ".maxFunctionSize=" #size)))

#define LEET_SKIP_EXPAND(pass) LEET_SKIP_PASS(pass)
#define LEET_FORCE_EXPAND(pass) LEET_FORCE_PASS(pass)
#define LEET_RUNTIME_SEED_EXPAND(pass) LEET_RUNTIME_SEED_PASS(pass, seed)
#define LEET_MIN_FUNCTION_SIZE_EXPAND(pass) LEET_MIN_FUNCTION_SIZE(pass, size)
#define LEET_MAX_FUNCTION_SIZE_EXPAND(pass) LEET_MAX_FUNCTION_SIZE(pass, size)

// _ALL variants
#define LEET_SKIP_ALL \
    LEET_PASS_LIST(LEET_SKIP_EXPAND)

#define LEET_FORCE_ALL \
    LEET_PASS_LIST(LEET_FORCE_EXPAND)

#define LEET_RUNTIME_SEED_ALL(seed) \
    LEET_PASS_LIST(LEET_RUNTIME_SEED_EXPAND)

#define LEET_MIN_FUNCTION_SIZE_ALL(size) \
    LEET_PASS_LIST(LEET_MIN_FUNCTION_SIZE_EXPAND)

#define LEET_MAX_FUNCTION_SIZE_ALL(size) \
    LEET_PASS_LIST(LEET_MAX_FUNCTION_SIZE_EXPAND)
