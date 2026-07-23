#pragma once

#define LEET_SKIP_FUNCTION __attribute__((annotate("leet.skip")))
#define LEET_PARSE_FUNCTION __attribute__((annotate("leet.parse")))
#define LEET_MAX_BLOCK_SIZE(size) __attribute__((annotate("leet.maxBlockSize=" #size)))
#define LEET_MBA_EXPANSION_COUNT(count) __attribute__((annotate("leet.MBAexpansionCount=" #count)))