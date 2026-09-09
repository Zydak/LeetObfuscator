#pragma once
#include <cstdint>
#include <cstddef>
#include <array>

namespace LeetObfuscator
{

struct PayloadShuffle
{
    uint8_t order[4];
};

struct TrapTemplate {
    uint8_t decoyBytesBeforePayload; // bytes between the selector and the payload
    uint8_t decoyBytesAfterPayload;  // trailing decoy bytes
    uint8_t sibRelativeOffset;
    PayloadShuffle shuffle;
    const uint8_t* opcodeCandidates;
    uint8_t opcodeCandidateCount;
};

// byte order permutations, one per template so there's no single global shuffle
inline constexpr PayloadShuffle kShuffle0 = {{1,3,2,0}};
inline constexpr PayloadShuffle kShuffle1 = {{2,0,3,1}};
inline constexpr PayloadShuffle kShuffle2 = {{3,1,0,2}};
inline constexpr PayloadShuffle kShuffle3 = {{0,2,1,3}};
inline constexpr PayloadShuffle kShuffle4 = {{2,3,0,1}};
inline constexpr PayloadShuffle kShuffle5 = {{1,0,3,2}};

// opcode pools per template
inline constexpr uint8_t kOpcodesT0[] = {0x81,0x81,0x81,0x81,0x81,0x81};
inline constexpr uint8_t kOpcodesT1[] = {0xC7,0xC7,0xC7,0x39};
inline constexpr uint8_t kOpcodesT2[] = {0x69,0x69,0x8D};
inline constexpr uint8_t kOpcodesT3[] = {0xF7,0xF7,0xF7,0x3B};
inline constexpr uint8_t kOpcodesT4[] = {0x85,0x21,0x23};
inline constexpr uint8_t kOpcodesT5[] = {0x09,0x0B,0x31,0x33};

inline constexpr size_t kTemplateCount = 6;

inline constexpr TrapTemplate gTrapTemplates[kTemplateCount] = {
    {1, 0, 0, kShuffle0, kOpcodesT0, (uint8_t)std::size(kOpcodesT0)},
    {1, 2, 0, kShuffle1, kOpcodesT1, (uint8_t)std::size(kOpcodesT1)},
    {2, 0, 1, kShuffle2, kOpcodesT2, (uint8_t)std::size(kOpcodesT2)},
    {1, 3, 0, kShuffle3, kOpcodesT3, (uint8_t)std::size(kOpcodesT3)},
    {2, 1, 0, kShuffle4, kOpcodesT4, (uint8_t)std::size(kOpcodesT4)},
    {1, 1, 0, kShuffle5, kOpcodesT5, (uint8_t)std::size(kOpcodesT5)},
};

inline constexpr uint32_t gTrapKeyTable[16] = {
    0x3F1A7C29, 0x9E44B801, 0x552C6FDA, 0x0B8E19F3,
    0xC77A2E56, 0x184FD3A0, 0x6BE05C97, 0xA23F81DE,
    0xF056E1B4, 0x2D9A0C63, 0x7C1BE8F2, 0x9481D005,
    0x3EB27A1C, 0x60F5C8D9, 0xD12A3B47, 0x877C90EE,
};

struct OpcodeToTemplateTable
{
    uint8_t data[256]{};
};

constexpr OpcodeToTemplateTable BuildOpcodeToTemplateTable()
{
    OpcodeToTemplateTable table{};

    for (auto& b : table.data)
        b = 0xFF;

    for (uint8_t t = 0; t < kTemplateCount; t++)
    {
        const TrapTemplate& trapTemplate = gTrapTemplates[t];

        for (uint8_t i = 0; i < trapTemplate.opcodeCandidateCount; i++)
            table.data[trapTemplate.opcodeCandidates[i]] = t;
    }

    return table;
}

inline constexpr OpcodeToTemplateTable gPrimaryOpcodeToTemplate = BuildOpcodeToTemplateTable();


} // namespace LeetObfuscator