// Deterministic branch-and-recursion test for the Leet obfuscator.
// Uses recursion, switch, and many scalar parameters to ensure stability.
// Expanded to stress test control flow obfuscation with complex branching patterns.

#include <array>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>

#include <chrono>
#include "../Leet.h"

static inline uint64_t mix64(uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

static inline uint64_t add_f(uint64_t h, float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    return mix64(h ^ u);
}

static inline uint64_t add_d(uint64_t h, double d) {
    uint64_t u;
    std::memcpy(&u, &d, sizeof(u));
    return mix64(h ^ u);
}

// Complex recursive branching function with multiple decision points
static uint64_t branchWorker(int depth,
                             int a0, int a1, int a2, int a3, int a4,
                             int a5, int a6, int a7, int a8, int a9,
                             bool chooseLeft,
                             const std::array<int, 16> &table,
                             double factor,
                             double offset) {
    uint64_t h = 0x0f0f0f0f0f0f0f0fULL;
    if (depth == 0) {
        for (unsigned i = 0; i < table.size(); ++i)
            h = mix64(h ^ (uint64_t)(table[i] + a0 + a1 + a2 + a3 + a4));
        h = add_d(h, factor + offset + a5 + a6 + a7 + a8 + a9);
        return h;
    }

    int decision = (a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + depth) & 3;
    switch (decision) {
    case 0:
        h = mix64(h ^ branchWorker(depth - 1, a1, a2, a3, a4, a5,
                                  a6, a7, a8, a9, a0, !chooseLeft,
                                  table, factor + 0.5, offset - 0.25));
        break;
    case 1:
        h = mix64(h ^ branchWorker(depth - 1, a9, a8, a7, a6, a5,
                                  a4, a3, a2, a1, a0, chooseLeft,
                                  table, factor - 0.5, offset + 0.75));
        break;
    case 2:
        h = mix64(h ^ branchWorker(depth - 1, a2, a4, a6, a8, a0,
                                  a1, a3, a5, a7, a9, !chooseLeft,
                                  table, factor * 1.25, offset * 0.5));
        break;
    default:
        h = mix64(h ^ branchWorker(depth - 1, a5, a4, a3, a2, a1,
                                  a0, a9, a8, a7, a6, chooseLeft,
                                  table, factor + 1.0, offset + 1.0));
        break;
    }

    if (chooseLeft) {
        h = add_d(h, factor - offset);
    } else {
        h = add_d(h, offset - factor);
    }
    h = mix64(h ^ (uint64_t)(a0 ^ a1 ^ a2 ^ a3 ^ a4 ^ a5 ^ a6 ^ a7 ^ a8 ^ a9));
    return h;
}

// Extended switch worker with more cases and complex logic
static uint64_t switchWorker(int code,
                             const std::array<int, 20> &values,
                             int extra0, int extra1,
                             int extra2, int extra3,
                             int extra4, int extra5) {
    uint64_t h = 0x1122334455667788ULL;
    switch (code) {
    case 0:
    case 1:
        for (unsigned i = 0; i < values.size(); ++i)
            h = mix64(h ^ (uint64_t)(values[i] + extra0 + i));
        break;
    case 2:
    case 3:
        for (unsigned i = 0; i < values.size(); ++i)
            h = add_d(h, (double)values[i] * 0.125 + extra1);
        break;
    case 4:
    case 5:
        for (unsigned i = 0; i < values.size(); i += 2)
            h = mix64(h ^ (uint64_t)(values[i] ^ extra2));
        break;
    case 6:
    case 7:
        for (unsigned i = 1; i < values.size(); i += 2)
            h = add_d(h, (double)(values[i] + extra3) * 0.25);
        break;
    case 8:
    case 9:
        for (unsigned i = 0; i < values.size(); ++i)
            h = mix64(h ^ (uint64_t)(values[i] * extra4));
        break;
    case 10:
    case 11:
        for (unsigned i = 0; i < values.size(); ++i)
            h = add_d(h, (double)(values[i] + extra5) * 0.5);
        break;
    default:
        for (unsigned i = 0; i < values.size(); ++i)
            h = mix64(h ^ (uint64_t)(values[i] + extra4 + extra5 * i));
        break;
    }
    h = mix64(h ^ (uint64_t)code);
    h = add_d(h, extra0 + extra1 + extra2 + extra3 + extra4 + extra5);
    return h;
}

// Nested branching with multiple conditions
static uint64_t nestedBranchWorker(int level,
                                   int value,
                                   const std::array<int, 12> &data,
                                   float multiplier,
                                   double divisor) {
    uint64_t h = 0xdeadbeefcafebabeULL;
    
    if (level > 5) {
        h = mix64(h ^ (uint64_t)value);
        return h;
    }
    
    int idx = value % data.size();
    h = mix64(h ^ (uint64_t)data[idx]);
    
    if (data[idx] % 3 == 0) {
        h = mix64(h ^ nestedBranchWorker(level + 1, data[idx] + 1, data, multiplier * 1.1f, divisor / 1.1));
    } else if (data[idx] % 3 == 1) {
        h = add_f(h, multiplier * (float)data[idx]);
        h = mix64(h ^ nestedBranchWorker(level + 1, data[idx] + 2, data, multiplier * 0.9f, divisor * 1.2));
    } else {
        h = add_d(h, (double)data[idx] / divisor);
        h = mix64(h ^ nestedBranchWorker(level + 1, data[idx] + 3, data, multiplier, divisor));
    }
    
    return h;
}

// Complex conditional chain
static uint64_t conditionalChainWorker(int input,
                                       const std::array<double, 8> &thresholds,
                                       const std::array<int, 8> &outputs) {
    uint64_t h = 0xabcdef0123456789ULL;
    double dinput = (double)input;
    
    if (dinput < thresholds[0]) {
        h = mix64(h ^ (uint64_t)outputs[0]);
    } else if (dinput < thresholds[1]) {
        h = mix64(h ^ (uint64_t)outputs[1]);
        h = add_d(h, thresholds[0]);
    } else if (dinput < thresholds[2]) {
        h = mix64(h ^ (uint64_t)outputs[2]);
        h = add_d(h, thresholds[1]);
    } else if (dinput < thresholds[3]) {
        h = mix64(h ^ (uint64_t)outputs[3]);
        h = add_d(h, thresholds[2]);
    } else if (dinput < thresholds[4]) {
        h = mix64(h ^ (uint64_t)outputs[4]);
        h = add_d(h, thresholds[3]);
    } else if (dinput < thresholds[5]) {
        h = mix64(h ^ (uint64_t)outputs[5]);
        h = add_d(h, thresholds[4]);
    } else if (dinput < thresholds[6]) {
        h = mix64(h ^ (uint64_t)outputs[6]);
        h = add_d(h, thresholds[5]);
    } else if (dinput < thresholds[7]) {
        h = mix64(h ^ (uint64_t)outputs[7]);
        h = add_d(h, thresholds[6]);
    } else {
        h = mix64(h ^ (uint64_t)(outputs[0] + outputs[7]));
        h = add_d(h, thresholds[7]);
    }
    
    return h;
}

// Multi-level recursion with branching
static uint64_t multiLevelRecursion(int depth, int maxDepth,
                                    int branchMask,
                                    const std::array<int, 10> &values,
                                    double weight) {
    uint64_t h = 0x13579bdf2468ace0ULL;
    
    if (depth >= maxDepth) {
        h = mix64(h ^ (uint64_t)depth);
        for (unsigned i = 0; i < values.size(); ++i) {
            h = mix64(h ^ (uint64_t)(values[i] + depth));
        }
        return h;
    }
    
    int branch = (depth + branchMask) & 7;
    switch (branch) {
    case 0:
        h = mix64(h ^ multiLevelRecursion(depth + 1, maxDepth, branchMask ^ 1, values, weight * 1.1));
        break;
    case 1:
        h = mix64(h ^ multiLevelRecursion(depth + 2, maxDepth, branchMask ^ 2, values, weight * 0.9));
        break;
    case 2:
        h = add_d(h, weight);
        h = mix64(h ^ multiLevelRecursion(depth + 1, maxDepth, branchMask ^ 4, values, weight * 1.05));
        break;
    case 3:
        h = add_d(h, weight * 2.0);
        h = mix64(h ^ multiLevelRecursion(depth + 3, maxDepth, branchMask ^ 8, values, weight * 0.95));
        break;
    case 4:
        for (unsigned i = 0; i < values.size(); ++i) {
            h = mix64(h ^ (uint64_t)values[i]);
        }
        h = mix64(h ^ multiLevelRecursion(depth + 1, maxDepth, branchMask ^ 16, values, weight));
        break;
    case 5:
        for (unsigned i = 0; i < values.size(); i += 2) {
            h = mix64(h ^ (uint64_t)(values[i] * values[i + 1]));
        }
        h = mix64(h ^ multiLevelRecursion(depth + 2, maxDepth, branchMask ^ 32, values, weight * 1.2));
        break;
    case 6:
        h = add_d(h, weight * 0.5);
        h = mix64(h ^ multiLevelRecursion(depth + 1, maxDepth, branchMask ^ 64, values, weight * 0.8));
        break;
    default:
        h = mix64(h ^ (uint64_t)branchMask);
        h = mix64(h ^ multiLevelRecursion(depth + 1, maxDepth, branchMask ^ 128, values, weight * 1.15));
        break;
    }
    
    return h;
}

// Complex switch with fallthrough patterns
static uint64_t switchFallthroughWorker(int pattern,
                                       const std::array<int, 15> &data,
                                       int modifier) {
    uint64_t h = 0xfedcba9876543210ULL;
    
    switch (pattern % 10) {
    case 0:
        h = mix64(h ^ (uint64_t)data[0]);
        // fallthrough
    case 1:
        h = mix64(h ^ (uint64_t)data[1]);
        modifier += 1;
        // fallthrough
    case 2:
        h = mix64(h ^ (uint64_t)data[2]);
        modifier += 2;
        break;
    case 3:
        h = mix64(h ^ (uint64_t)data[3]);
        modifier += 3;
        // fallthrough
    case 4:
        h = mix64(h ^ (uint64_t)data[4]);
        modifier += 4;
        // fallthrough
    case 5:
        h = mix64(h ^ (uint64_t)data[5]);
        modifier += 5;
        break;
    case 6:
        h = mix64(h ^ (uint64_t)data[6]);
        modifier += 6;
        // fallthrough
    case 7:
        h = mix64(h ^ (uint64_t)data[7]);
        modifier += 7;
        // fallthrough
    case 8:
        h = mix64(h ^ (uint64_t)data[8]);
        modifier += 8;
        break;
    case 9:
        h = mix64(h ^ (uint64_t)data[9]);
        modifier += 9;
        break;
    default:
        h = mix64(h ^ (uint64_t)data[10]);
        modifier += 10;
        break;
    }
    
    h = mix64(h ^ (uint64_t)modifier);
    for (unsigned i = 10; i < data.size(); ++i) {
        h = mix64(h ^ (uint64_t)(data[i] + modifier));
    }
    
    return h;
}

// Recursive tree traversal simulation
static uint64_t treeTraversalWorker(int nodeId,
                                   int depth,
                                   const std::array<int, 32> &treeData,
                                   bool goLeft) {
    uint64_t h = 0x1234567890abcdefULL;
    
    if (depth > 8 || nodeId >= (int)treeData.size()) {
        return h;
    }
    
    h = mix64(h ^ (uint64_t)treeData[nodeId]);
    
    int leftChild = (nodeId * 2 + 1) % treeData.size();
    int rightChild = (nodeId * 2 + 2) % treeData.size();
    
    if (goLeft) {
        h = mix64(h ^ treeTraversalWorker(leftChild, depth + 1, treeData, !goLeft));
        h = mix64(h ^ treeTraversalWorker(rightChild, depth + 1, treeData, goLeft));
    } else {
        h = mix64(h ^ treeTraversalWorker(rightChild, depth + 1, treeData, goLeft));
        h = mix64(h ^ treeTraversalWorker(leftChild, depth + 1, treeData, !goLeft));
    }
    
    return h;
}

// State machine simulation
static uint64_t stateMachineWorker(int initialState,
                                  const std::array<int, 16> &transitions,
                                  int steps) {
    uint64_t h = 0xaaaabbbbccccddddULL;
    int state = initialState;
    
    for (int i = 0; i < steps; ++i) {
        int input = transitions[i % transitions.size()];
        
        switch (state) {
        case 0:
            if (input % 2 == 0) state = 1;
            else state = 3;
            break;
        case 1:
            if (input % 3 == 0) state = 2;
            else state = 4;
            break;
        case 2:
            if (input % 5 == 0) state = 0;
            else state = 5;
            break;
        case 3:
            if (input % 7 == 0) state = 6;
            else state = 1;
            break;
        case 4:
            if (input % 2 == 0) state = 7;
            else state = 2;
            break;
        case 5:
            if (input % 3 == 0) state = 8;
            else state = 0;
            break;
        case 6:
            if (input % 5 == 0) state = 9;
            else state = 3;
            break;
        case 7:
            if (input % 7 == 0) state = 10;
            else state = 4;
            break;
        case 8:
            if (input % 2 == 0) state = 11;
            else state = 5;
            break;
        case 9:
            if (input % 3 == 0) state = 12;
            else state = 6;
            break;
        case 10:
            if (input % 5 == 0) state = 13;
            else state = 7;
            break;
        case 11:
            if (input % 7 == 0) state = 14;
            else state = 8;
            break;
        case 12:
            if (input % 2 == 0) state = 15;
            else state = 9;
            break;
        case 13:
            if (input % 3 == 0) state = 0;
            else state = 10;
            break;
        case 14:
            if (input % 5 == 0) state = 1;
            else state = 11;
            break;
        case 15:
            if (input % 7 == 0) state = 2;
            else state = 12;
            break;
        default:
            state = 0;
            break;
        }
        
        h = mix64(h ^ (uint64_t)(state + input));
    }
    
    return h;
}

// Complex branching with mathematical operations
static uint64_t mathBranchWorker(int operation,
                                 double x, double y,
                                 const std::array<double, 8> &constants) {
    uint64_t h = 0x1111222233334444ULL;
    
    switch (operation % 8) {
    case 0:
        h = add_d(h, x + y);
        h = add_d(h, constants[0]);
        break;
    case 1:
        h = add_d(h, x - y);
        h = add_d(h, constants[1]);
        break;
    case 2:
        h = add_d(h, x * y);
        h = add_d(h, constants[2]);
        break;
    case 3:
        if (y != 0.0) {
            h = add_d(h, x / y);
        }
        h = add_d(h, constants[3]);
        break;
    case 4:
        h = add_d(h, std::pow(x, y));
        h = add_d(h, constants[4]);
        break;
    case 5:
        h = add_d(h, std::fmod(x, y));
        h = add_d(h, constants[5]);
        break;
    case 6:
        h = add_d(h, std::sqrt(x * x + y * y));
        h = add_d(h, constants[6]);
        break;
    case 7:
        h = add_d(h, std::fabs(x - y));
        h = add_d(h, constants[7]);
        break;
    }
    
    return h;
}

// Recursive divide and conquer simulation
static uint64_t divideAndConquerWorker(int left, int right,
                                       const std::array<int, 64> &data,
                                       int depth) {
    uint64_t h = 0x5555666677778888ULL;
    
    if (depth > 6 || left >= right) {
        if (left < (int)data.size()) {
            h = mix64(h ^ (uint64_t)data[left]);
        }
        return h;
    }
    
    int mid = (left + right) / 2;
    
    if (depth % 2 == 0) {
        h = mix64(h ^ divideAndConquerWorker(left, mid, data, depth + 1));
        h = mix64(h ^ divideAndConquerWorker(mid + 1, right, data, depth + 1));
    } else {
        h = mix64(h ^ divideAndConquerWorker(mid + 1, right, data, depth + 1));
        h = mix64(h ^ divideAndConquerWorker(left, mid, data, depth + 1));
    }
    
    if (mid < (int)data.size()) {
        h = mix64(h ^ (uint64_t)data[mid]);
    }
    
    return h;
}

// Nested loop with branching
static uint64_t nestedLoopBranchWorker(int iterations,
                                      const std::array<int, 10> &multipliers,
                                      const std::array<float, 5> &floats) {
    uint64_t h = 0x9999aaaabbbbccccULL;
    
    for (int i = 0; i < iterations; ++i) {
        for (int j = 0; j < 5; ++j) {
            int val = i * multipliers[j % multipliers.size()];
            
            if (val % 2 == 0) {
                h = mix64(h ^ (uint64_t)val);
            } else if (val % 3 == 0) {
                h = add_f(h, floats[j % floats.size()] * (float)val);
            } else if (val % 5 == 0) {
                h = mix64(h ^ (uint64_t)(val * 2));
            } else {
                h = mix64(h ^ (uint64_t)(val + 1));
            }
            
            for (int k = 0; k < 3; ++k) {
                if ((i + j + k) % 4 == 0) {
                    h = mix64(h ^ (uint64_t)(i + j + k));
                } else {
                    h = mix64(h ^ (uint64_t)(i * j * k));
                }
            }
        }
    }
    
    return h;
}

// Complex pattern matching
static uint64_t patternMatchWorker(const std::array<int, 20> &pattern,
                                   const std::array<int, 20> &input,
                                   int threshold) {
    uint64_t h = 0xddddeeeeffff0000ULL;
    int matches = 0;
    
    for (unsigned i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == input[i]) {
            matches++;
            h = mix64(h ^ (uint64_t)i);
        } else if (std::abs(pattern[i] - input[i]) < threshold) {
            h = mix64(h ^ (uint64_t)(i + 1000));
        } else {
            h = mix64(h ^ (uint64_t)(i + 2000));
        }
        
        switch (matches % 5) {
        case 0:
            h = mix64(h ^ (uint64_t)pattern[i]);
            break;
        case 1:
            h = mix64(h ^ (uint64_t)input[i]);
            break;
        case 2:
            h = mix64(h ^ (uint64_t)(pattern[i] + input[i]));
            break;
        case 3:
            h = mix64(h ^ (uint64_t)(pattern[i] * input[i]));
            break;
        case 4:
            h = mix64(h ^ (uint64_t)(pattern[i] ^ input[i]));
            break;
        }
    }
    
    h = mix64(h ^ (uint64_t)matches);
    return h;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    std::array<int, 16> table;
    for (unsigned i = 0; i < table.size(); ++i)
        table[i] = (int)(i * 7 + 11);

    std::array<int, 20> values;
    for (unsigned i = 0; i < values.size(); ++i)
        values[i] = (int)(i * 13 + 5);

    std::array<int, 12> nestedData;
    for (unsigned i = 0; i < nestedData.size(); ++i)
        nestedData[i] = (int)(i * 17 + 23);

    std::array<double, 8> thresholds;
    for (unsigned i = 0; i < thresholds.size(); ++i)
        thresholds[i] = 10.0 + i * 15.5;

    std::array<int, 8> thresholdOutputs;
    for (unsigned i = 0; i < thresholdOutputs.size(); ++i)
        thresholdOutputs[i] = (int)(i * 31 + 47);

    std::array<int, 10> multiLevelValues;
    for (unsigned i = 0; i < multiLevelValues.size(); ++i)
        multiLevelValues[i] = (int)(i * 19 + 37);

    std::array<int, 15> switchData;
    for (unsigned i = 0; i < switchData.size(); ++i)
        switchData[i] = (int)(i * 23 + 41);

    std::array<int, 32> treeData;
    for (unsigned i = 0; i < treeData.size(); ++i)
        treeData[i] = (int)(i * 29 + 53);

    std::array<int, 16> transitions;
    for (unsigned i = 0; i < transitions.size(); ++i)
        transitions[i] = (int)(i * 11 + 19);

    std::array<double, 8> constants;
    for (unsigned i = 0; i < constants.size(); ++i)
        constants[i] = 1.0 + i * 0.7;

    std::array<int, 64> divideData;
    for (unsigned i = 0; i < divideData.size(); ++i)
        divideData[i] = (int)(i * 3 + 7);

    std::array<int, 10> multipliers;
    for (unsigned i = 0; i < multipliers.size(); ++i)
        multipliers[i] = (int)(i * 5 + 13);

    std::array<float, 5> floats;
    for (unsigned i = 0; i < floats.size(); ++i)
        floats[i] = 1.5f + i * 0.3f;

    std::array<int, 20> pattern;
    for (unsigned i = 0; i < pattern.size(); ++i)
        pattern[i] = (int)(i * 2 + 1);

    std::array<int, 20> inputPattern;
    for (unsigned i = 0; i < inputPattern.size(); ++i)
        inputPattern[i] = (int)(i * 2 + 1 + (i % 3));

    uint64_t checksum = 0x99aabbccddeeff00ULL;
    
    // Original tests
    checksum ^= branchWorker(5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, true, table, 1.25, 0.75);
    checksum ^= branchWorker(4, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, false, table, 2.5, 1.5);
    checksum ^= switchWorker(0, values, 1, 2, 3, 4, 5, 6);
    checksum ^= switchWorker(7, values, 10, 11, 12, 13, 14, 15);
    checksum ^= switchWorker(12, values, 20, 21, 22, 23, 24, 25);
    
    // New expanded tests
    checksum ^= nestedBranchWorker(0, 42, nestedData, 1.5f, 2.5);
    checksum ^= nestedBranchWorker(0, 73, nestedData, 2.3f, 1.7);
    checksum ^= conditionalChainWorker(25, thresholds, thresholdOutputs);
    checksum ^= conditionalChainWorker(68, thresholds, thresholdOutputs);
    checksum ^= conditionalChainWorker(113, thresholds, thresholdOutputs);
    checksum ^= multiLevelRecursion(0, 6, 0x55, multiLevelValues, 1.0);
    checksum ^= multiLevelRecursion(0, 8, 0xAA, multiLevelValues, 1.5);
    checksum ^= switchFallthroughWorker(0, switchData, 5);
    checksum ^= switchFallthroughWorker(3, switchData, 10);
    checksum ^= switchFallthroughWorker(7, switchData, 15);
    checksum ^= treeTraversalWorker(0, 0, treeData, true);
    checksum ^= treeTraversalWorker(1, 0, treeData, false);
    checksum ^= stateMachineWorker(0, transitions, 32);
    checksum ^= stateMachineWorker(5, transitions, 48);
    checksum ^= stateMachineWorker(10, transitions, 64);
    checksum ^= mathBranchWorker(0, 3.5, 2.7, constants);
    checksum ^= mathBranchWorker(4, 5.2, 3.1, constants);
    checksum ^= mathBranchWorker(7, 7.8, 4.5, constants);
    checksum ^= divideAndConquerWorker(0, 63, divideData, 0);
    checksum ^= divideAndConquerWorker(0, 31, divideData, 0);
    checksum ^= nestedLoopBranchWorker(8, multipliers, floats);
    checksum ^= nestedLoopBranchWorker(12, multipliers, floats);
    checksum ^= patternMatchWorker(pattern, inputPattern, 3);
    checksum ^= patternMatchWorker(pattern, pattern, 0);
    
    checksum = mix64(checksum);

    std::printf("CHECKSUM: 0x%016llx\n", (unsigned long long)checksum);

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("%ld\n", diff);
    return 0;
}
