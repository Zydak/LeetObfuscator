#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <iostream>

#define LEET_IMPLEMENTATION
#include "../Leet.h"

__attribute__((noinline))
static inline uint64_t mix64(uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

__attribute__((noinline))
static uint64_t basicBitwiseOperations(uint32_t a, uint32_t b) {
    uint64_t h = 0xdeadbeefcafebabeULL;
    
    h = mix64(h ^ (uint64_t)(a & b));
    h = mix64(h ^ (uint64_t)(a | b));
    h = mix64(h ^ (uint64_t)(a ^ b));
    h = mix64(h ^ (uint64_t)(~a));
    h = mix64(h ^ (uint64_t)(~b));
    
    return h;
}

__attribute__((noinline))
static uint64_t bitShiftingOperations(uint32_t value) {
    uint64_t h = 0xfeedfacebadc0ffeULL;
    
    for (int shift = 0; shift < 32; ++shift) {
        h = mix64(h ^ (uint64_t)(value << shift));
        h = mix64(h ^ (uint64_t)(value >> shift));
        
        h = mix64(h ^ (uint64_t)((uint32_t)((int32_t)value >> shift)));
    }
    
    return h;
}

__attribute__((noinline))
static uint64_t bitMaskingOperations(uint32_t value) {
    uint64_t h = 0x0badf00d1337c0deULL;
    
    for (int bit = 0; bit < 32; ++bit) {
        uint32_t mask = 1u << bit;
        h = mix64(h ^ (uint64_t)((value & mask) ? 1 : 0));
    }
    
    for (int nibble = 0; nibble < 8; ++nibble) {
        uint32_t mask = 0xFu << (nibble * 4);
        h = mix64(h ^ (uint64_t)((value & mask) >> (nibble * 4)));
    }
    
    for (int byte = 0; byte < 4; ++byte) {
        uint32_t mask = 0xFFu << (byte * 8);
        h = mix64(h ^ (uint64_t)((value & mask) >> (byte * 8)));
    }
    
    return h;
}

__attribute__((noinline))
static uint64_t bitSetClearOperations(uint32_t value) {
    uint64_t h = 0xcafebabedeadfaceULL;
    
    for (int bit = 0; bit < 32; ++bit) {
        uint32_t setBit = value | (1u << bit);
        uint32_t clearBit = value & ~(1u << bit);
        uint32_t toggleBit = value ^ (1u << bit);
        
        h = mix64(h ^ (uint64_t)setBit);
        h = mix64(h ^ (uint64_t)clearBit);
        h = mix64(h ^ (uint64_t)toggleBit);
    }
    
    return h;
}

__attribute__((noinline))
static uint64_t bitCountingOperations(uint32_t value) {
    uint64_t h = 0xfacefeed0ddba11eULL;
    
    uint32_t count = 0;
    uint32_t temp = value;
    while (temp) {
        count += temp & 1;
        temp >>= 1;
    }
    h = mix64(h ^ (uint64_t)count);
    
    uint32_t leadingZeros = 0;
    if (value == 0) {
        leadingZeros = 32;
    } else {
        while ((value & (1u << 31)) == 0) {
            leadingZeros++;
            value <<= 1;
        }
    }
    h = mix64(h ^ (uint64_t)leadingZeros);
    
    uint32_t trailingZeros = 0;
    temp = value;
    if (temp == 0) {
        trailingZeros = 32;
    } else {
        while ((temp & 1) == 0) {
            trailingZeros++;
            temp >>= 1;
        }
    }
    h = mix64(h ^ (uint64_t)trailingZeros);
    
    return h;
}

__attribute__((noinline))
static uint64_t bitReversal(uint32_t value) {
    uint64_t h = 0xbaddcafebabe1337ULL;
    
    uint32_t reversed = 0;
    for (int i = 0; i < 32; ++i) {
        reversed = (reversed << 1) | (value & 1);
        value >>= 1;
    }
    
    h = mix64(h ^ (uint64_t)reversed);
    
    return h;
}

__attribute__((noinline))
static uint64_t byteSwapping(uint32_t value) {
    uint64_t h = 0xc0dedeadbadf00dULL;
    
    uint32_t swapped = ((value & 0xFF000000) >> 24) |
                      ((value & 0x00FF0000) >> 8) |
                      ((value & 0x0000FF00) << 8) |
                      ((value & 0x000000FF) << 24);
    
    h = mix64(h ^ (uint64_t)swapped);
    
    return h;
}

__attribute__((noinline))
static uint64_t rotateOperations(uint32_t value) {
    uint64_t h = 0xfeedfacedeadbeefULL;
    
    for (int shift = 1; shift < 32; ++shift) {
        uint32_t rotLeft = (value << shift) | (value >> (32 - shift));
        h = mix64(h ^ (uint64_t)rotLeft);
        
        uint32_t rotRight = (value >> shift) | (value << (32 - shift));
        h = mix64(h ^ (uint64_t)rotRight);
    }
    
    return h;
}

__attribute__((noinline))
static uint64_t bitFieldExtraction(uint64_t value) {
    uint64_t h = 0xfaceface12345678ULL;
    
    for (int start = 0; start < 64; start += 8) {
        for (int length = 1; length <= 8 && start + length <= 64; ++length) {
            uint64_t mask = ((1ULL << length) - 1) << start;
            uint64_t field = (value & mask) >> start;
            h = mix64(h ^ field);
        }
    }
    
    return h;
}

__attribute__((noinline))
static uint64_t signExtension(int32_t value, int bits) {
    uint64_t h = 0xdead1234face5678ULL;
    
    if (bits < 32) {
        int32_t mask = 1 << (bits - 1);
        int32_t extended = (value ^ mask) - mask;
        h = mix64(h ^ (uint64_t)extended);
    }
    
    return h;
}

__attribute__((noinline))
static uint64_t bitTwiddlingHacks(uint32_t value) {
    uint64_t h = 0xcafe1234dead5678ULL;
    
    bool isPowerOf2 = (value != 0) && ((value & (value - 1)) == 0);
    h = mix64(h ^ (uint64_t)isPowerOf2);
    
    uint32_t nextPowerOf2 = value - 1;
    nextPowerOf2 |= nextPowerOf2 >> 1;
    nextPowerOf2 |= nextPowerOf2 >> 2;
    nextPowerOf2 |= nextPowerOf2 >> 4;
    nextPowerOf2 |= nextPowerOf2 >> 8;
    nextPowerOf2 |= nextPowerOf2 >> 16;
    nextPowerOf2 += 1;
    h = mix64(h ^ (uint64_t)nextPowerOf2);
    
    int32_t intValue = (int32_t)value;
    int32_t mask = intValue >> 31;
    int32_t absValue = (intValue + mask) ^ mask;
    h = mix64(h ^ (uint64_t)absValue);
    
    uint32_t min = value ^ ((value ^ 42) & -(value < 42));
    h = mix64(h ^ (uint64_t)min);
    
    uint32_t max = value ^ ((value ^ 73) & -(value < 73));
    h = mix64(h ^ (uint64_t)max);
    
    return h;
}

__attribute__((noinline))
static uint64_t grayCodeConversion(uint32_t value) {
    uint64_t h = 0x1337deadbeef1337ULL;
    
    uint32_t gray = value ^ (value >> 1);
    h = mix64(h ^ (uint64_t)gray);
    
    uint32_t binary = gray;
    for (uint32_t mask = gray >> 1; mask != 0; mask >>= 1) {
        binary ^= mask;
    }
    h = mix64(h ^ (uint64_t)binary);
    
    return h;
}

__attribute__((noinline))
static uint64_t hammingDistance(uint32_t a, uint32_t b) {
    uint64_t h = 0xfacedeadbeefcafeULL;
    
    uint32_t xor1 = a ^ b;
    uint32_t distance = 0;
    while (xor1) {
        distance += xor1 & 1;
        xor1 >>= 1;
    }
    
    h = mix64(h ^ (uint64_t)distance);
    
    return h;
}

__attribute__((noinline))
static uint64_t parityCalculation(uint32_t value) {
    uint64_t h = 0xdeadcafe12345678ULL;
    
    uint32_t parity = 0;
    while (value) {
        parity ^= (value & 1);
        value >>= 1;
    }
    
    h = mix64(h ^ (uint64_t)parity);
    
    return h;
}

__attribute__((noinline))
static uint64_t bitArrayOperations(const std::array<uint32_t, 16> &data) {
    uint64_t h = 0xbeefdeadface1234ULL;
    
    uint32_t xorAll = 0;
    for (uint32_t value : data) {
        xorAll ^= value;
    }
    h = mix64(h ^ (uint64_t)xorAll);
    
    uint32_t andAll = 0xFFFFFFFF;
    for (uint32_t value : data) {
        andAll &= value;
    }
    h = mix64(h ^ (uint64_t)andAll);
    
    uint32_t orAll = 0;
    for (uint32_t value : data) {
        orAll |= value;
    }
    h = mix64(h ^ (uint64_t)orAll);
    
    return h;
}

__attribute__((noinline))
static uint64_t bitPatternMatching(uint32_t value, uint32_t pattern, uint32_t mask) {
    uint64_t h = 0xcafebeefdeadfaceULL;
    
    bool matches = ((value & mask) == (pattern & mask));
    h = mix64(h ^ (uint64_t)matches);
    
    return h;
}

__attribute__((noinline))
static uint64_t endiannessConversion(uint32_t value) {
    uint64_t h = 0xfeeddeadcafebeefULL;
    
    uint32_t bigEndian = ((value & 0xFF) << 24) |
                        ((value & 0xFF00) << 8) |
                        ((value & 0xFF0000) >> 8) |
                        ((value & 0xFF000000) >> 24);
    
    h = mix64(h ^ (uint64_t)bigEndian);
    
    uint32_t littleEndian = ((bigEndian & 0xFF) << 24) |
                           ((bigEndian & 0xFF00) << 8) |
                           ((bigEndian & 0xFF0000) >> 8) |
                           ((bigEndian & 0xFF000000) >> 24);
    
    h = mix64(h ^ (uint64_t)littleEndian);
    
    return h;
}

__attribute__((noinline))
static uint64_t bitInterleaving(uint16_t a, uint16_t b) {
    uint64_t h = 0x0badf00d12345678ULL;
    
    uint32_t interleaved = 0;
    for (int i = 0; i < 16; ++i) {
        interleaved |= ((a >> i) & 1) << (2 * i);
        interleaved |= ((b >> i) & 1) << (2 * i + 1);
    }
    
    h = mix64(h ^ (uint64_t)interleaved);
    
    return h;
}

__attribute__((noinline))
static uint64_t bitDeinterleaving(uint32_t interleaved) {
    uint64_t h = 0xfacefacecafe1234ULL;
    
    uint16_t a = 0, b = 0;
    for (int i = 0; i < 16; ++i) {
        a |= ((interleaved >> (2 * i)) & 1) << i;
        b |= ((interleaved >> (2 * i + 1)) & 1) << i;
    }
    
    h = mix64(h ^ (uint64_t)a);
    h = mix64(h ^ (uint64_t)b);
    
    return h;
}

__attribute__((noinline))
static uint64_t bitSpreading(uint16_t value) {
    uint64_t h = 0xdeadcafeface1234ULL;
    
    uint32_t spread = 0;
    for (int i = 0; i < 16; ++i) {
        spread |= ((value >> i) & 1) << (2 * i);
    }
    
    h = mix64(h ^ (uint64_t)spread);
    
    return h;
}

__attribute__((noinline))
static uint64_t bitCompression(uint32_t spread) {
    uint64_t h = 0xbeefdeadcafe5678ULL;
    
    uint16_t compressed = 0;
    for (int i = 0; i < 16; ++i) {
        compressed |= ((spread >> (2 * i)) & 1) << i;
    }
    
    h = mix64(h ^ (uint64_t)compressed);
    
    return h;
}

__attribute__((noinline))
static uint64_t mergeBits(uint32_t a, uint32_t b, const std::array<bool, 32> &selectA) {
    uint64_t h = 0xfacefeeddead5678ULL;
    
    uint32_t merged = 0;
    for (int i = 0; i < 32; ++i) {
        if (selectA[i]) {
            merged |= ((a >> i) & 1) << i;
        } else {
            merged |= ((b >> i) & 1) << i;
        }
    }
    
    h = mix64(h ^ (uint64_t)merged);
    
    return h;
}

__attribute__((noinline))
static uint64_t selectiveBitNegation(uint32_t value, uint32_t mask) {
    uint64_t h = 0x13371337deadfaceULL;
    
    uint32_t negated = value ^ mask;
    h = mix64(h ^ (uint64_t)negated);
    
    return h;
}

__attribute__((noinline))
static uint64_t bitRangeOperations(uint32_t value, int start, int end) {
    uint64_t h = 0xc0dec0dedeadbeefULL;
    
    if (start >= 0 && end <= 32 && start < end) {
        uint32_t mask = ((1u << (end - start)) - 1) << start;
        uint32_t range = (value & mask) >> start;
        h = mix64(h ^ (uint64_t)range);
        
        uint32_t cleared = value & ~mask;
        h = mix64(h ^ (uint64_t)cleared);
        
        uint32_t setRange = cleared | (0xFFFFFFFFu & mask);
        h = mix64(h ^ (uint64_t)setRange);
    }
    
    return h;
}

__attribute__((noinline))
static uint64_t circularBufferBits(uint32_t value, int positions) {
    uint64_t h = 0xdeadbeeffacefaceULL;
    
    uint32_t circularLeft = (value << positions) | (value >> (32 - positions));
    h = mix64(h ^ (uint64_t)circularLeft);
    
    uint32_t circularRight = (value >> positions) | (value << (32 - positions));
    h = mix64(h ^ (uint64_t)circularRight);
    
    return h;
}

__attribute__((noinline))
static uint64_t bitMatrixOperations(const std::array<std::array<uint8_t, 8>, 8> &matrix) {
    uint64_t h = 0xcafebabedead1234ULL;
    
    for (const auto &row : matrix) {
        uint8_t rowXor = 0;
        for (uint8_t value : row) {
            rowXor ^= value;
        }
        h = mix64(h ^ (uint64_t)rowXor);
    }
    
    for (int col = 0; col < 8; ++col) {
        uint8_t colXor = 0;
        for (int row = 0; row < 8; ++row) {
            colXor ^= matrix[row][col];
        }
        h = mix64(h ^ (uint64_t)colXor);
    }
    
    return h;
}

__attribute__((noinline))
static uint64_t bitSequenceSearch(uint32_t data, uint32_t pattern, int patternLength) {
    uint64_t h = 0xfeedfacecafe5678ULL;
    
    uint32_t mask = (1u << patternLength) - 1;
    int foundPosition = -1;
    
    for (int i = 0; i <= 32 - patternLength; ++i) {
        if (((data >> i) & mask) == pattern) {
            foundPosition = i;
            break;
        }
    }
    
    h = mix64(h ^ (uint64_t)foundPosition);
    
    return h;
}

__attribute__((noinline))
static uint64_t carrylessMultiplication(uint32_t a, uint32_t b) {
    uint64_t h = 0xdeadcafeface1234ULL;
    
    uint32_t result = 0;
    while (b) {
        if (b & 1) {
            result ^= a;
        }
        a <<= 1;
        b >>= 1;
    }
    
    h = mix64(h ^ (uint64_t)result);
    
    return h;
}

__attribute__((noinline))
static uint64_t crcComputation(const std::array<uint8_t, 16> &data, uint32_t polynomial) {
    uint64_t h = 0xbeefdeadcafe5678ULL;
    
    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
    }
    
    h = mix64(h ^ (uint64_t)~crc);
    
    return h;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    std::array<uint32_t, 16> bitArrayData;
    for (unsigned i = 0; i < bitArrayData.size(); ++i)
        bitArrayData[i] = (uint32_t)(i * 0x9e3779b9);

    std::array<bool, 32> selectBits;
    for (unsigned i = 0; i < selectBits.size(); ++i)
        selectBits[i] = (i % 2) == 0;

    std::array<std::array<uint8_t, 8>, 8> bitMatrix;
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            bitMatrix[i][j] = (uint8_t)((i * 8 + j) % 256);
        }
    }

    std::array<uint8_t, 16> crcData;
    for (unsigned i = 0; i < crcData.size(); ++i)
        crcData[i] = (uint8_t)(i * 7 + 13);

    uint64_t checksum = 0xdeadbeefdeadbeefULL;
    
    checksum ^= basicBitwiseOperations(0x12345678, 0x9ABCDEF0);
    checksum ^= basicBitwiseOperations(0xDEADBEEF, 0xCAFEBABE);
    checksum ^= bitShiftingOperations(0x12345678);
    checksum ^= bitShiftingOperations(0x9ABCDEF0);
    checksum ^= bitMaskingOperations(0x12345678);
    checksum ^= bitMaskingOperations(0xDEADBEEF);
    checksum ^= bitSetClearOperations(0x12345678);
    checksum ^= bitSetClearOperations(0x9ABCDEF0);
    checksum ^= bitCountingOperations(0x12345678);
    checksum ^= bitCountingOperations(0xFFFFFFFF);
    checksum ^= bitReversal(0x12345678);
    checksum ^= bitReversal(0x9ABCDEF0);
    checksum ^= byteSwapping(0x12345678);
    checksum ^= byteSwapping(0xDEADBEEF);
    checksum ^= rotateOperations(0x12345678);
    checksum ^= rotateOperations(0xCAFEBABE);
    checksum ^= bitFieldExtraction(0x123456789ABCDEF0ULL);
    checksum ^= bitFieldExtraction(0x9ABCDEF012345678ULL);
    checksum ^= signExtension(0x12345678, 16);
    checksum ^= signExtension(0xFFFF1234, 16);
    checksum ^= bitTwiddlingHacks(0x12345678);
    checksum ^= bitTwiddlingHacks(42);
    checksum ^= grayCodeConversion(0x12345678);
    checksum ^= grayCodeConversion(0x9ABCDEF0);
    checksum ^= hammingDistance(0x12345678, 0x9ABCDEF0);
    checksum ^= hammingDistance(0xDEADBEEF, 0xCAFEBABE);
    checksum ^= parityCalculation(0x12345678);
    checksum ^= parityCalculation(0xFFFFFFFF);
    checksum ^= bitArrayOperations(bitArrayData);
    checksum ^= bitPatternMatching(0x12345678, 0x12340000, 0xFFFF0000);
    checksum ^= bitPatternMatching(0x9ABCDEF0, 0x9ABC0000, 0xFFFF0000);
    checksum ^= endiannessConversion(0x12345678);
    checksum ^= endiannessConversion(0xDEADBEEF);
    checksum ^= bitInterleaving(0x1234, 0x5678);
    checksum ^= bitInterleaving(0x9ABC, 0xDEF0);
    checksum ^= bitDeinterleaving(0x55555555);
    checksum ^= bitDeinterleaving(0xAAAAAAAA);
    checksum ^= bitSpreading(0x1234);
    checksum ^= bitSpreading(0x5678);
    checksum ^= bitCompression(0x55555555);
    checksum ^= bitCompression(0xAAAAAAAA);
    checksum ^= mergeBits(0x12345678, 0x9ABCDEF0, selectBits);
    checksum ^= selectiveBitNegation(0x12345678, 0xFF00FF00);
    checksum ^= selectiveBitNegation(0x9ABCDEF0, 0x00FF00FF);
    checksum ^= bitRangeOperations(0x12345678, 8, 16);
    checksum ^= bitRangeOperations(0x9ABCDEF0, 16, 24);
    checksum ^= circularBufferBits(0x12345678, 4);
    checksum ^= circularBufferBits(0x9ABCDEF0, 8);
    checksum ^= bitMatrixOperations(bitMatrix);
    checksum ^= bitSequenceSearch(0x12345678, 0x345, 12);
    checksum ^= bitSequenceSearch(0x9ABCDEF0, 0xABC, 12);
    checksum ^= carrylessMultiplication(0x12345678, 0x9ABCDEF0);
    checksum ^= carrylessMultiplication(0xDEADBEEF, 0xCAFEBABE);
    checksum ^= crcComputation(crcData, 0x04C11DB7);
    checksum ^= crcComputation(crcData, 0x1EDC6F41);
    
    checksum = mix64(checksum);

    std::printf("CHECKSUM: 0x%016llx\n", (unsigned long long)checksum);

    auto end = std::chrono::high_resolution_clock::now();
    uint64_t diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << diff << std::endl;
    return 0;
}
