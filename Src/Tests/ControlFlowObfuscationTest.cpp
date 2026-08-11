#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>

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
static inline uint64_t add_f(uint64_t h, float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    return mix64(h ^ u);
}

__attribute__((noinline))
static inline uint64_t add_d(uint64_t h, double d) {
    uint64_t u;
    std::memcpy(&u, &d, sizeof(u));
    return mix64(h ^ u);
}

__attribute__((noinline))
static uint64_t nestedLoopComplex(int maxA, int maxB, int maxC) {
    uint64_t h = 0xdeadbeefcafebabeULL;
    
    for (int a = 0; a < maxA; ++a) {
        for (int b = 0; b < maxB; ++b) {
            for (int c = 0; c < maxC; ++c) {
                int value = a * b * c;
                
                if (value % 7 == 0) {
                    h = mix64(h ^ (uint64_t)(value + 1000));
                    if (a > 2 && b > 2) {
                        h = mix64(h ^ (uint64_t)(value + 2000));
                        if (c > 3) {
                            h = mix64(h ^ (uint64_t)(value + 3000));
                            continue;
                        }
                    }
                } else if (value % 5 == 0) {
                    h = mix64(h ^ (uint64_t)(value + 4000));
                    if (a + b + c > 10) {
                        h = mix64(h ^ (uint64_t)(value + 5000));
                        break;
                    }
                } else {
                    h = mix64(h ^ (uint64_t)value);
                }
                
                if (a % 3 == 0 && b % 3 == 0 && c % 3 == 0) {
                    h = mix64(h ^ (uint64_t)(value + 6000));
                }
            }
            
            if (a * b > 20) {
                h = mix64(h ^ (uint64_t)(a * b + 7000));
            }
        }
        
        if (a % 4 == 0) {
            h = mix64(h ^ (uint64_t)(a + 8000));
        }
    }
    
    return h;
}

__attribute__((noinline))
static uint64_t complexStateMachine(const std::array<int, 20> &inputs, int initialState) {
    uint64_t h = 0xfeedfacebadc0ffeULL;
    int state = initialState;
    
    for (int input : inputs) {
        switch (state) {
        case 0:
            if (input % 2 == 0) {
                state = 1;
                h = mix64(h ^ (uint64_t)(input + 100));
            } else if (input % 3 == 0) {
                state = 2;
                h = mix64(h ^ (uint64_t)(input + 200));
            } else {
                state = 3;
                h = mix64(h ^ (uint64_t)(input + 300));
            }
            break;
        case 1:
            if (input > 10) {
                state = 4;
                h = mix64(h ^ (uint64_t)(input + 400));
            } else if (input < 5) {
                state = 0;
                h = mix64(h ^ (uint64_t)(input + 500));
            } else {
                state = 5;
                h = mix64(h ^ (uint64_t)(input + 600));
            }
            break;
        case 2:
            if (input % 5 == 0) {
                state = 6;
                h = mix64(h ^ (uint64_t)(input + 700));
            } else {
                state = 7;
                h = mix64(h ^ (uint64_t)(input + 800));
            }
            break;
        case 3:
            state = (input % 4);
            h = mix64(h ^ (uint64_t)(input + 900));
            break;
        case 4:
            state = 8;
            h = mix64(h ^ (uint64_t)(input + 1000));
            break;
        case 5:
            state = 9;
            h = mix64(h ^ (uint64_t)(input + 1100));
            break;
        case 6:
            state = 10;
            h = mix64(h ^ (uint64_t)(input + 1200));
            break;
        case 7:
            state = 11;
            h = mix64(h ^ (uint64_t)(input + 1300));
            break;
        case 8:
            state = 0;
            h = mix64(h ^ (uint64_t)(input + 1400));
            break;
        case 9:
            state = 1;
            h = mix64(h ^ (uint64_t)(input + 1500));
            break;
        case 10:
            state = 2;
            h = mix64(h ^ (uint64_t)(input + 1600));
            break;
        case 11:
            state = 3;
            h = mix64(h ^ (uint64_t)(input + 1700));
            break;
        default:
            state = 0;
            h = mix64(h ^ (uint64_t)(input + 1800));
            break;
        }
    }
    
    h = mix64(h ^ (uint64_t)state);
    return h;
}

__attribute__((noinline))
static uint64_t controlFlowFlattening(const std::array<int, 16> &data) {
    uint64_t h = 0x0badf00d1337c0deULL;
    int state = 0;
    int pc = 0;
    int result = 0;
    
    while (pc < 100) {
        switch (state) {
        case 0:
            result += data[pc % data.size()];
            state = 1;
            break;
        case 1:
            result *= 2;
            state = 2;
            break;
        case 2:
            if (result % 3 == 0) {
                state = 3;
            } else {
                state = 4;
            }
            break;
        case 3:
            result += 10;
            state = 5;
            break;
        case 4:
            result -= 5;
            state = 5;
            break;
        case 5:
            result ^= pc;
            state = 6;
            break;
        case 6:
            if (pc % 2 == 0) {
                state = 7;
            } else {
                state = 8;
            }
            break;
        case 7:
            result += data[(pc + 1) % data.size()];
            state = 0;
            break;
        case 8:
            result += data[(pc + 2) % data.size()];
            state = 0;
            break;
        default:
            state = 0;
            break;
        }
        
        h = mix64(h ^ (uint64_t)result);
        pc++;
        
        if (pc > 50) {
            state = pc % 9;
        }
    }
    
    return h;
}

__attribute__((noinline))
static uint64_t complexConditionalChain(int input, const std::array<int, 10> &thresholds) {
    uint64_t h = 0xcafebabedeadfaceULL;
    
    if (input < thresholds[0]) {
        h = mix64(h ^ (uint64_t)(input + 1));
        if (input < thresholds[0] / 2) {
            h = mix64(h ^ (uint64_t)(input + 2));
            if (input < thresholds[0] / 4) {
                h = mix64(h ^ (uint64_t)(input + 3));
            } else {
                h = mix64(h ^ (uint64_t)(input + 4));
            }
        } else {
            h = mix64(h ^ (uint64_t)(input + 5));
        }
    } else if (input < thresholds[1]) {
        h = mix64(h ^ (uint64_t)(input + 10));
        if (input > thresholds[0] + (thresholds[1] - thresholds[0]) / 2) {
            h = mix64(h ^ (uint64_t)(input + 11));
        } else {
            h = mix64(h ^ (uint64_t)(input + 12));
        }
    } else if (input < thresholds[2]) {
        h = mix64(h ^ (uint64_t)(input + 20));
        for (int i = 0; i < 3; ++i) {
            if (input < thresholds[2] - i * 10) {
                h = mix64(h ^ (uint64_t)(input + 30 + i));
            }
        }
    } else if (input < thresholds[3]) {
        h = mix64(h ^ (uint64_t)(input + 40));
        switch (input % 4) {
        case 0:
            h = mix64(h ^ (uint64_t)(input + 41));
            break;
        case 1:
            h = mix64(h ^ (uint64_t)(input + 42));
            break;
        case 2:
            h = mix64(h ^ (uint64_t)(input + 43));
            break;
        case 3:
            h = mix64(h ^ (uint64_t)(input + 44));
            break;
        }
    } else if (input < thresholds[4]) {
        h = mix64(h ^ (uint64_t)(input + 50));
    } else if (input < thresholds[5]) {
        h = mix64(h ^ (uint64_t)(input + 60));
    } else if (input < thresholds[6]) {
        h = mix64(h ^ (uint64_t)(input + 70));
    } else if (input < thresholds[7]) {
        h = mix64(h ^ (uint64_t)(input + 80));
    } else if (input < thresholds[8]) {
        h = mix64(h ^ (uint64_t)(input + 90));
    } else {
        h = mix64(h ^ (uint64_t)(input + 100));
    }
    
    return h;
}

__attribute__((noinline))
static uint64_t loopWithComplexConditions(const std::array<int, 20> &data) {
    uint64_t h = 0xfacefeed0ddba11eULL;
    int i = 0;
    int sum = 0;
    
    while (i < (int)data.size()) {
        int value = data[i];
        
        if (value % 2 == 0) {
            if (value % 4 == 0) {
                if (value % 8 == 0) {
                    sum += value * 3;
                } else {
                    sum += value * 2;
                }
            } else {
                sum += value;
            }
        } else {
            if (value % 3 == 0) {
                if (value % 9 == 0) {
                    sum += value * 4;
                } else {
                    sum += value * 3;
                }
            } else {
                sum += value * 2;
            }
        }
        
        h = mix64(h ^ (uint64_t)sum);
        
        if (sum > 1000) {
            sum = sum % 100;
        }
        
        if (i % 5 == 0) {
            i += 2;
        } else if (i % 3 == 0) {
            i += 1;
        } else {
            i += 3;
        }
        
        if (i >= (int)data.size()) {
            break;
        }
    }
    
    h = mix64(h ^ (uint64_t)sum);
    return h;
}

__attribute__((noinline))
static uint64_t complexSwitchFallthrough(int value, const std::array<int, 15> &data) {
    uint64_t h = 0xbaddcafebabe1337ULL;
    int result = 0;
    
    switch (value % 12) {
    case 0:
        result += data[0];
    case 1:
        result += data[1];
        if (result > 10) {
            result -= 5;
        }
    case 2:
        result += data[2];
        break;
    case 3:
        result += data[3];
    case 4:
        result += data[4];
    case 5:
        result += data[5];
        if (result < 20) {
            result += 10;
        }
        break;
    case 6:
        result += data[6];
    case 7:
        result += data[7];
    case 8:
        result += data[8];
    case 9:
        result += data[9];
        break;
    case 10:
        result += data[10];
    case 11:
        result += data[11];
        if (result % 2 == 0) {
            result /= 2;
        }
        break;
    default:
        result += data[12];
        break;
    }
    
    h = mix64(h ^ (uint64_t)result);
    return h;
}

__attribute__((noinline))
static uint64_t virtualDispatchSimulation(int operation, int operand) {
    uint64_t h = 0xc0dedeadbadf00dULL;
    int result = 0;
    
    switch (operation) {
    case 0:
        result = operand + 10;
        break;
    case 1:
        result = operand - 5;
        break;
    case 2:
        result = operand * 2;
        break;
    case 3:
        if (operand != 0) {
            result = 100 / operand;
        }
        break;
    case 4:
        if (operand != 0) {
            result = operand % 7;
        }
        break;
    case 5:
        result = operand * operand;
        break;
    case 6:
        result = (int)std::sqrt((double)operand);
        break;
    case 7:
        result = (operand < 0) ? -operand : operand;
        break;
    case 8:
        result = -operand;
        break;
    case 9:
        result = operand + 1;
        break;
    case 10:
        result = operand - 1;
        break;
    default:
        result = operand;
        break;
    }
    
    h = mix64(h ^ (uint64_t)result);
    h = mix64(h ^ (uint64_t)operation);
    return h;
}

__attribute__((noinline))
static uint64_t exceptionFlowSimulation(int input) {
    uint64_t h = 0xfeedfacedeadbeefULL;
    int result = 0;
    
    if (input < 0) {
        result = -input;
        h = mix64(h ^ (uint64_t)(result + 1000));
    } else if (input > 100) {
        result = input % 100;
        h = mix64(h ^ (uint64_t)(result + 2000));
    } else {
        result = input;
        h = mix64(h ^ (uint64_t)(result + 3000));
    }
    
    switch (result % 5) {
    case 0:
        result += 10;
        h = mix64(h ^ (uint64_t)(result + 4000));
        break;
    case 1:
        result += 20;
        h = mix64(h ^ (uint64_t)(result + 5000));
        break;
    case 2:
        result += 30;
        h = mix64(h ^ (uint64_t)(result + 6000));
        break;
    case 3:
        result += 40;
        h = mix64(h ^ (uint64_t)(result + 7000));
        break;
    case 4:
        result += 50;
        h = mix64(h ^ (uint64_t)(result + 8000));
        break;
    }
    
    h = mix64(h ^ (uint64_t)result);
    return h;
}

__attribute__((noinline))
static uint64_t recursiveControlFlow(int n, const std::array<int, 8> &values) {
    uint64_t h = 0xfaceface12345678ULL;
    
    if (n <= 0) {
        h = mix64(h ^ (uint64_t)n);
        return h;
    }
    
    if (n % 2 == 0) {
        h = mix64(h ^ recursiveControlFlow(n / 2, values));
        h = mix64(h ^ (uint64_t)values[n % values.size()]);
    } else {
        h = mix64(h ^ recursiveControlFlow(n - 1, values));
        h = mix64(h ^ (uint64_t)(values[n % values.size()] * 2));
    }
    
    if (n % 3 == 0) {
        h = mix64(h ^ recursiveControlFlow(n / 3, values));
    }
    
    return h;
}

__attribute__((noinline))
static uint64_t loopUnrollingSimulation(const std::array<int, 16> &data) {
    uint64_t h = 0xdead1234face5678ULL;
    int sum = 0;
    
    for (unsigned i = 0; i < data.size(); i += 4) {
        sum += data[i];
        h = mix64(h ^ (uint64_t)data[i]);
        
        if (i + 1 < data.size()) {
            sum += data[i + 1];
            h = mix64(h ^ (uint64_t)data[i + 1]);
        }
        
        if (i + 2 < data.size()) {
            sum += data[i + 2];
            h = mix64(h ^ (uint64_t)data[i + 2]);
        }
        
        if (i + 3 < data.size()) {
            sum += data[i + 3];
            h = mix64(h ^ (uint64_t)data[i + 3]);
        }
    }
    
    h = mix64(h ^ (uint64_t)sum);
    return h;
}

__attribute__((noinline))
static uint64_t indirectBranchingSimulation(const std::array<int, 10> &targets, int selector) {
    uint64_t h = 0xcafe1234dead5678ULL;
    int result = 0;
    
    int targetIndex = selector % targets.size();
    
    switch (targets[targetIndex]) {
    case 0:
        result = selector * 2;
        break;
    case 1:
        result = selector * 3;
        break;
    case 2:
        result = selector * 4;
        break;
    case 3:
        result = selector * 5;
        break;
    case 4:
        result = selector * 6;
        break;
    case 5:
        result = selector * 7;
        break;
    case 6:
        result = selector * 8;
        break;
    case 7:
        result = selector * 9;
        break;
    case 8:
        result = selector * 10;
        break;
    case 9:
        result = selector * 11;
        break;
    default:
        result = selector;
        break;
    }
    
    h = mix64(h ^ (uint64_t)result);
    h = mix64(h ^ (uint64_t)targets[targetIndex]);
    return h;
}

__attribute__((noinline))
static uint64_t tableBasedControlFlow(const std::array<int, 8> &table, int index) {
    uint64_t h = 0x1337deadbeef1337ULL;
    int result = 0;
    
    int jumpTarget = table[index % table.size()];
    
    switch (jumpTarget) {
    case 0:
        result = index + 1;
        break;
    case 1:
        result = index + 2;
        break;
    case 2:
        result = index + 3;
        break;
    case 3:
        result = index + 4;
        break;
    case 4:
        result = index + 5;
        break;
    case 5:
        result = index + 6;
        break;
    case 6:
        result = index + 7;
        break;
    case 7:
        result = index + 8;
        break;
    default:
        result = index;
        break;
    }
    
    h = mix64(h ^ (uint64_t)result);
    return h;
}

__attribute__((noinline))
static uint64_t complexLoopWithControl(const std::array<int, 25> &data) {
    uint64_t h = 0xfacedeadbeefcafeULL;
    int sum = 0;
    int count = 0;
    
    for (int i = 0; i < (int)data.size(); ++i) {
        int value = data[i];
        
        if (value == 0) {
            continue;
        }
        
        if (value < 0) {
            sum += -value;
            count++;
            
            if (count > 10) {
                break;
            }
            
            continue;
        }
        
        if (value % 10 == 0) {
            sum += value / 10;
        } else if (value % 5 == 0) {
            sum += value / 5;
        } else {
            sum += value;
        }
        
        count++;
        
        if (sum > 500) {
            sum = sum % 100;
        }
        
        h = mix64(h ^ (uint64_t)sum);
        
        if (i % 7 == 0 && i > 0) {
            i += 2;
        }
    }
    
    h = mix64(h ^ (uint64_t)count);
    return h;
}

__attribute__((noinline))
static uint64_t nestedSwitchStatements(int a, int b, int c) {
    uint64_t h = 0xdeadcafe12345678ULL;
    int result = 0;
    
    switch (a % 4) {
    case 0:
        switch (b % 3) {
        case 0:
            result = a + b + c;
            break;
        case 1:
            result = a * b + c;
            break;
        case 2:
            result = a + b * c;
            break;
        }
        break;
    case 1:
        switch (b % 3) {
        case 0:
            result = a - b + c;
            break;
        case 1:
            result = a - b - c;
            break;
        case 2:
            result = a * b * c;
            break;
        }
        break;
    case 2:
        switch (c % 3) {
        case 0:
            result = (a + b) * c;
            break;
        case 1:
            result = a * (b + c);
            break;
        case 2:
            result = (a + b + c) / 2;
            break;
        }
        break;
    case 3:
        switch (c % 4) {
        case 0:
            result = a ^ b ^ c;
            break;
        case 1:
            result = a | b | c;
            break;
        case 2:
            result = a & b & c;
            break;
        case 3:
            result = (a + b) ^ c;
            break;
        }
        break;
    }
    
    h = mix64(h ^ (uint64_t)result);
    return h;
}

__attribute__((noinline))
static uint64_t computedGotoSimulation(const std::array<int, 12> &labels, int start) {
    uint64_t h = 0xbeefdeadface1234ULL;
    int pc = start % labels.size();
    int accumulator = 0;
    int iterations = 0;
    
    while (iterations < 50) {
        int label = labels[pc];
        
        switch (label) {
        case 0:
            accumulator += 1;
            pc = (pc + 1) % labels.size();
            break;
        case 1:
            accumulator *= 2;
            pc = (pc + 2) % labels.size();
            break;
        case 2:
            accumulator += 10;
            pc = (pc + 3) % labels.size();
            break;
        case 3:
            accumulator -= 5;
            pc = (pc + 4) % labels.size();
            break;
        case 4:
            accumulator ^= pc;
            pc = (pc + 5) % labels.size();
            break;
        case 5:
            accumulator = (accumulator + 100) % 50;
            pc = (pc + 1) % labels.size();
            break;
        case 6:
            accumulator = accumulator * 3 + 1;
            pc = (pc + 2) % labels.size();
            break;
        case 7:
            accumulator = accumulator / 2;
            pc = (pc + 3) % labels.size();
            break;
        case 8:
            accumulator += accumulator;
            pc = (pc + 4) % labels.size();
            break;
        case 9:
            accumulator = -accumulator;
            pc = (pc + 5) % labels.size();
            break;
        case 10:
            accumulator = 0;
            pc = (pc + 1) % labels.size();
            break;
        case 11:
            accumulator = 42;
            pc = (pc + 2) % labels.size();
            break;
        default:
            pc = (pc + 1) % labels.size();
            break;
        }
        
        h = mix64(h ^ (uint64_t)accumulator);
        iterations++;
        
        if (accumulator > 1000) {
            accumulator = accumulator % 100;
        }
    }
    
    h = mix64(h ^ (uint64_t)iterations);
    return h;
}

__attribute__((noinline))
static uint64_t complexReturnPath(int input, const std::array<int, 6> &thresholds) {
    uint64_t h = 0xcafebeefdeadfaceULL;
    int result = 0;
    
    if (input < thresholds[0]) {
        if (input < thresholds[0] / 2) {
            if (input < thresholds[0] / 4) {
                result = input * 4;
                h = mix64(h ^ (uint64_t)result);
                return h;
            } else {
                result = input * 3;
            }
        } else {
            result = input * 2;
        }
    } else if (input < thresholds[1]) {
        if (input > thresholds[0] + (thresholds[1] - thresholds[0]) / 2) {
            result = input + 10;
            h = mix64(h ^ (uint64_t)result);
            return h;
        } else {
            result = input + 5;
        }
    } else if (input < thresholds[2]) {
        result = input - 3;
    } else if (input < thresholds[3]) {
        result = input / 2;
    } else if (input < thresholds[4]) {
        result = input % 7;
    } else {
        result = input ^ 0xFF;
    }
    
    h = mix64(h ^ (uint64_t)result);
    return h;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    std::array<int, 20> stateMachineInputs;
    for (unsigned i = 0; i < stateMachineInputs.size(); ++i)
        stateMachineInputs[i] = (int)(i * 7 + 13);

    std::array<int, 16> flattenData;
    for (unsigned i = 0; i < flattenData.size(); ++i)
        flattenData[i] = (int)(i * 5 + 3);

    std::array<int, 10> conditionalThresholds;
    for (unsigned i = 0; i < conditionalThresholds.size(); ++i)
        conditionalThresholds[i] = (int)(i * 15 + 20);

    std::array<int, 20> loopData;
    for (unsigned i = 0; i < loopData.size(); ++i)
        loopData[i] = (int)(i * 11 + 7);

    std::array<int, 15> switchData;
    for (unsigned i = 0; i < switchData.size(); ++i)
        switchData[i] = (int)(i * 9 + 5);

    std::array<int, 8> recursiveValues;
    for (unsigned i = 0; i < recursiveValues.size(); ++i)
        recursiveValues[i] = (int)(i * 3 + 1);

    std::array<int, 16> unrollData;
    for (unsigned i = 0; i < unrollData.size(); ++i)
        unrollData[i] = (int)(i * 2 + 1);

    std::array<int, 10> indirectTargets;
    for (unsigned i = 0; i < indirectTargets.size(); ++i)
        indirectTargets[i] = i;

    std::array<int, 8> jumpTable;
    for (unsigned i = 0; i < jumpTable.size(); ++i)
        jumpTable[i] = i % 8;

    std::array<int, 25> complexLoopData;
    for (unsigned i = 0; i < complexLoopData.size(); ++i)
        complexLoopData[i] = (int)((i - 12) * 3);

    std::array<int, 12> computedGotoLabels;
    for (unsigned i = 0; i < computedGotoLabels.size(); ++i)
        computedGotoLabels[i] = i % 12;

    std::array<int, 6> returnPathThresholds;
    for (unsigned i = 0; i < returnPathThresholds.size(); ++i)
        returnPathThresholds[i] = (int)(i * 25 + 30);

    uint64_t checksum = 0xdeadbeefdeadbeefULL;
    
    checksum ^= nestedLoopComplex(5, 5, 5);
    checksum ^= nestedLoopComplex(3, 7, 4);
    checksum ^= complexStateMachine(stateMachineInputs, 0);
    checksum ^= complexStateMachine(stateMachineInputs, 5);
    checksum ^= controlFlowFlattening(flattenData);
    checksum ^= complexConditionalChain(25, conditionalThresholds);
    checksum ^= complexConditionalChain(68, conditionalThresholds);
    checksum ^= loopWithComplexConditions(loopData);
    checksum ^= complexSwitchFallthrough(7, switchData);
    checksum ^= complexSwitchFallthrough(13, switchData);
    checksum ^= virtualDispatchSimulation(5, 42);
    checksum ^= virtualDispatchSimulation(8, 73);
    checksum ^= exceptionFlowSimulation(-15);
    checksum ^= exceptionFlowSimulation(150);
    checksum ^= recursiveControlFlow(20, recursiveValues);
    checksum ^= recursiveControlFlow(30, recursiveValues);
    checksum ^= loopUnrollingSimulation(unrollData);
    checksum ^= indirectBranchingSimulation(indirectTargets, 7);
    checksum ^= indirectBranchingSimulation(indirectTargets, 13);
    checksum ^= tableBasedControlFlow(jumpTable, 5);
    checksum ^= tableBasedControlFlow(jumpTable, 11);
    checksum ^= complexLoopWithControl(complexLoopData);
    checksum ^= nestedSwitchStatements(5, 7, 11);
    checksum ^= nestedSwitchStatements(13, 17, 19);
    checksum ^= computedGotoSimulation(computedGotoLabels, 3);
    checksum ^= computedGotoSimulation(computedGotoLabels, 8);
    checksum ^= complexReturnPath(15, returnPathThresholds);
    checksum ^= complexReturnPath(85, returnPathThresholds);
    
    checksum = mix64(checksum);

    std::printf("CHECKSUM: 0x%016llx\n", (unsigned long long)checksum);

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("%ld\n", diff);
    return 0;
}
