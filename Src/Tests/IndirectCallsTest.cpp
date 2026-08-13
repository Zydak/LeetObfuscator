#include <iostream>
#include <vector>
#include <functional>
#include <cstdint>
#include <utility>
#include <chrono>

#define LEET_IMPLEMENTATION
#include "../Leet.h"

struct Lcg {
    uint64_t state;
    explicit Lcg(uint64_t seed) : state(seed) {}
    uint64_t nextRaw() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    }
    uint64_t nextInt(uint64_t lo, uint64_t hi) {
        uint64_t range = hi - lo + 1ULL;
        return lo + (nextRaw() % range);
    }
};

uint64_t opAdd(uint64_t a, uint64_t b) { return a + b; }
uint64_t opSub(uint64_t a, uint64_t b) { return a >= b ? a - b : b - a; }
uint64_t opMul(uint64_t a, uint64_t b) { return a * b; }
uint64_t opMax(uint64_t a, uint64_t b) { return a > b ? a : b; }
uint64_t opMin(uint64_t a, uint64_t b) { return a < b ? a : b; }

typedef uint64_t (*BinaryOp)(uint64_t, uint64_t);

uint64_t rawFunctionPointersAndJumpTableSection() {
    std::cout << "-- Raw function pointers and jump table --\n";
    uint64_t checksum = 0;

    BinaryOp table[5] = { opAdd, opSub, opMul, opMax, opMin };
    const char* names[5] = { "add", "sub", "mul", "max", "min" };
    uint64_t a = 17, b = 5;

    for (int i = 0; i < 5; ++i) {
        BinaryOp fn = table[i];
        uint64_t r = fn(a, b);
        std::cout << "  " << names[i] << "(" << a << ", " << b << ") = " << r << "\n";
        checksum += r;
    }

    BinaryOp selected = table[2];
    uint64_t chained = selected(selected(a, b), selected(b, a));
    std::cout << "  chained mul(mul(a,b), mul(b,a)) = " << chained << "\n";
    checksum += chained;

    return checksum;
}

uint64_t globalCounterSeed = 0;

uint64_t incrementGlobal(uint64_t step) { globalCounterSeed += step; return globalCounterSeed; }
uint64_t decrementGlobal(uint64_t step) {
    globalCounterSeed = globalCounterSeed >= step ? globalCounterSeed - step : 0;
    return globalCounterSeed;
}

uint64_t (*g_activeOp)(uint64_t) = incrementGlobal;

uint64_t globalFunctionPointersSection() {
    std::cout << "\n-- Global function pointers --\n";
    uint64_t checksum = 0;

    globalCounterSeed = 100;
    g_activeOp = incrementGlobal;
    for (int i = 1; i <= 5; ++i) {
        uint64_t r = g_activeOp(static_cast<uint64_t>(i));
        std::cout << "  step " << i << " (inc) -> " << r << "\n";
        checksum += r;
    }

    g_activeOp = decrementGlobal;
    for (int i = 1; i <= 3; ++i) {
        uint64_t r = g_activeOp(static_cast<uint64_t>(i * 2));
        std::cout << "  step " << i << " (dec) -> " << r << "\n";
        checksum += r;
    }

    return checksum;
}

struct Instruction {
    uint64_t (*handler)(uint64_t, uint64_t);
    uint64_t lhs;
    uint64_t rhs;
};

uint64_t handlerAnd(uint64_t x, uint64_t y) { return x & y; }
uint64_t handlerOr(uint64_t x, uint64_t y) { return x | y; }
uint64_t handlerXor(uint64_t x, uint64_t y) { return x ^ y; }
uint64_t handlerShiftLeft(uint64_t x, uint64_t y) { return x << (y % 64ULL); }

uint64_t structFunctionPointerMembersSection() {
    std::cout << "\n-- Struct with function pointer members (opcode dispatch) --\n";
    uint64_t checksum = 0;

    Instruction program[4] = {
        { handlerAnd, 0xFFULL, 0x0FULL },
        { handlerOr, 0x10ULL, 0x01ULL },
        { handlerXor, 0xAAULL, 0x55ULL },
        { handlerShiftLeft, 1ULL, 4ULL }
    };

    for (int i = 0; i < 4; ++i) {
        uint64_t r = program[i].handler(program[i].lhs, program[i].rhs);
        std::cout << "  instruction[" << i << "] = " << r << "\n";
        checksum += r;
    }

    return checksum;
}

struct Calculator {
    int64_t base;
    explicit Calculator(int64_t b) : base(b) {}
    int64_t addTo(int64_t v) const { return base + v; }
    int64_t subFrom(int64_t v) const { return base - v; }
    int64_t mulBy(int64_t v) const { return base * v; }
};

typedef int64_t (Calculator::*CalcMethod)(int64_t) const;

uint64_t memberFunctionPointerSection() {
    std::cout << "\n-- Pointer-to-member-function and pointer-to-member-data --\n";
    uint64_t checksum = 0;

    Calculator calc(10);
    CalcMethod methods[3] = { &Calculator::addTo, &Calculator::subFrom, &Calculator::mulBy };
    const char* labels[3] = { "addTo", "subFrom", "mulBy" };
    int64_t values[3] = { 3, 4, 5 };

    for (int i = 0; i < 3; ++i) {
        int64_t r = (calc.*methods[i])(values[i]);
        std::cout << "  " << labels[i] << "(" << values[i] << ") = " << r << "\n";
        checksum += static_cast<uint64_t>(r < 0 ? -r : r);
    }

    Calculator* calcPtr = &calc;
    CalcMethod chosen = methods[2];
    int64_t viaPointer = (calcPtr->*chosen)(6);
    std::cout << "  via object pointer: " << viaPointer << "\n";
    checksum += static_cast<uint64_t>(viaPointer < 0 ? -viaPointer : viaPointer);

    int64_t Calculator::*dataMember = &Calculator::base;
    int64_t baseViaPtr = calc.*dataMember;
    std::cout << "  base via pointer-to-member-data: " << baseViaPtr << "\n";
    checksum += static_cast<uint64_t>(baseViaPtr);

    return checksum;
}

struct Multiplier {
    int64_t factor;
    explicit Multiplier(int64_t f) : factor(f) {}
    int64_t operator()(int64_t v) const { return v * factor; }
};

int64_t freeSquare(int64_t v) { return v * v; }

uint64_t stdFunctionAndFunctorSection() {
    std::cout << "\n-- std::function, functors, std::bind --\n";
    uint64_t checksum = 0;

    std::function<int64_t(int64_t)> fn1 = freeSquare;
    std::function<int64_t(int64_t)> fn2 = Multiplier(3);
    std::function<int64_t(int64_t)> fn3 = [](int64_t v) { return v + 100; };
    std::vector<std::function<int64_t(int64_t)>> pipeline = { fn1, fn2, fn3 };

    int64_t value = 4;
    for (size_t i = 0; i < pipeline.size(); ++i) {
        value = pipeline[i](value);
        std::cout << "  stage " << i << " -> " << value << "\n";
    }
    checksum += static_cast<uint64_t>(value);

    Multiplier byFive(5);
    std::function<int64_t(int64_t)> bound = std::bind(&Multiplier::operator(), &byFive, std::placeholders::_1);
    int64_t boundResult = bound(9);
    std::cout << "  bound multiplier(9) = " << boundResult << "\n";
    checksum += static_cast<uint64_t>(boundResult);

    return checksum;
}

uint64_t lambdaVarietySection() {
    std::cout << "\n-- Lambda variety --\n";
    uint64_t checksum = 0;

    auto nonCapturing = [](int64_t a, int64_t b) -> int64_t { return a + b; };
    int64_t (*asFnPtr)(int64_t, int64_t) = nonCapturing;
    int64_t r1 = asFnPtr(3, 4);
    std::cout << "  non-capturing via function pointer: " << r1 << "\n";
    checksum += static_cast<uint64_t>(r1);

    int64_t captureVal = 10;
    auto capturingByValue = [captureVal](int64_t x) { return captureVal + x; };
    int64_t r2 = capturingByValue(5);
    std::cout << "  capture by value: " << r2 << "\n";
    checksum += static_cast<uint64_t>(r2);

    int64_t mutableState = 0;
    auto capturingByRef = [&mutableState](int64_t x) { mutableState += x; return mutableState; };
    int64_t r3 = capturingByRef(7) + capturingByRef(3);
    std::cout << "  capture by reference accumulated: " << r3 << "\n";
    checksum += static_cast<uint64_t>(r3);

    auto makeAdder = [](int64_t base) {
        return [base](int64_t x) { return base + x; };
    };
    auto addTen = makeAdder(10);
    int64_t r4 = addTen(32);
    std::cout << "  nested lambda (lambda returning lambda): " << r4 << "\n";
    checksum += static_cast<uint64_t>(r4);

    auto genericLambda = [](auto x, auto y) { return x * y; };
    int64_t r5 = genericLambda(int64_t(6), int64_t(7));
    std::cout << "  generic lambda: " << r5 << "\n";
    checksum += static_cast<uint64_t>(r5);

    int64_t r6 = [](int64_t x) { return x * x; }(9);
    std::cout << "  immediately invoked lambda: " << r6 << "\n";
    checksum += static_cast<uint64_t>(r6);

    std::vector<std::function<int64_t(int64_t)>> lambdaVec;
    for (int64_t i = 1; i <= 4; ++i) {
        lambdaVec.push_back([i](int64_t x) { return x * i; });
    }
    int64_t r7 = 0;
    for (size_t i = 0; i < lambdaVec.size(); ++i) {
        r7 += lambdaVec[i](2);
    }
    std::cout << "  vector of capturing lambdas summed: " << r7 << "\n";
    checksum += static_cast<uint64_t>(r7);

    return checksum;
}

uint64_t factorialRecursive(uint64_t n) {
    if (n <= 1ULL) return 1ULL;
    return n * factorialRecursive(n - 1ULL);
}

uint64_t fibonacciRecursive(uint64_t n) {
    if (n <= 1ULL) return n;
    return fibonacciRecursive(n - 1ULL) + fibonacciRecursive(n - 2ULL);
}

uint64_t directRecursionSection() {
    std::cout << "\n-- Direct recursion --\n";
    uint64_t checksum = 0;

    uint64_t fact10 = factorialRecursive(10ULL);
    std::cout << "  factorial(10) = " << fact10 << "\n";
    checksum += fact10;

    uint64_t fib30 = fibonacciRecursive(30ULL);
    std::cout << "  fibonacci(30) = " << fib30 << "\n";
    checksum += fib30;

    return checksum;
}

bool isEvenMutual(uint64_t n);
bool isOddMutual(uint64_t n);

bool isEvenMutual(uint64_t n) {
    if (n == 0ULL) return true;
    return isOddMutual(n - 1ULL);
}

bool isOddMutual(uint64_t n) {
    if (n == 0ULL) return false;
    return isEvenMutual(n - 1ULL);
}

uint64_t g_pingCount = 0;
uint64_t g_pongCount = 0;

void pingFn(int n);
void pongFn(int n);

void pingFn(int n) {
    if (n <= 0) return;
    ++g_pingCount;
    pongFn(n - 1);
}

void pongFn(int n) {
    if (n <= 0) return;
    ++g_pongCount;
    pingFn(n - 1);
}

uint64_t mutualRecursionSection() {
    std::cout << "\n-- Mutual (indirect) recursion --\n";
    uint64_t checksum = 0;

    bool even200 = isEvenMutual(200ULL);
    std::cout << "  isEvenMutual(200) = " << (even200 ? "true" : "false") << "\n";
    checksum += even200 ? 1 : 0;

    bool odd201 = isOddMutual(201ULL);
    std::cout << "  isOddMutual(201) = " << (odd201 ? "true" : "false") << "\n";
    checksum += odd201 ? 1 : 0;

    g_pingCount = 0;
    g_pongCount = 0;
    pingFn(100);
    std::cout << "  ping/pong counts: " << g_pingCount << " / " << g_pongCount << "\n";
    checksum += g_pingCount + g_pongCount;

    return checksum;
}

uint64_t (*g_sumRecursor)(uint64_t, uint64_t) = nullptr;

uint64_t sumRecursiveImpl(uint64_t n, uint64_t acc) {
    if (n == 0ULL) return acc;
    return g_sumRecursor(n - 1ULL, acc + n);
}

uint64_t recursionViaFunctionPointerSection() {
    std::cout << "\n-- Recursion performed entirely through a function pointer --\n";
    uint64_t checksum = 0;

    g_sumRecursor = sumRecursiveImpl;
    uint64_t sum = g_sumRecursor(250ULL, 0ULL);
    std::cout << "  sum(1..250) via indirect self-recursion = " << sum << "\n";
    checksum += sum;

    return checksum;
}

uint64_t recursiveLambdaViaStdFunctionSection() {
    std::cout << "\n-- Recursive lambda via std::function --\n";
    uint64_t checksum = 0;

    std::function<uint64_t(uint64_t)> factorialLambda = [&factorialLambda](uint64_t n) -> uint64_t {
        if (n <= 1ULL) return 1ULL;
        return n * factorialLambda(n - 1ULL);
    };
    uint64_t r = factorialLambda(12ULL);
    std::cout << "  recursive lambda factorial(12) = " << r << "\n";
    checksum += r;

    std::function<uint64_t(uint64_t, uint64_t)> gcdLambda = [&gcdLambda](uint64_t x, uint64_t y) -> uint64_t {
        if (y == 0ULL) return x;
        return gcdLambda(y, x % y);
    };
    uint64_t g = gcdLambda(1071ULL, 462ULL);
    std::cout << "  recursive lambda gcd(1071, 462) = " << g << "\n";
    checksum += g;

    return checksum;
}

struct ShapeVisitor;

struct Shape2 {
    virtual ~Shape2() = default;
    virtual uint64_t id() const = 0;
    virtual uint64_t weight() const = 0;
    virtual uint64_t accept(const ShapeVisitor& v) const = 0;
};

struct Cube;
struct Sphere2;
struct Pyramid;

struct ShapeVisitor {
    virtual ~ShapeVisitor() = default;
    virtual uint64_t visitCube(const Cube&) const { return 100; }
    virtual uint64_t visitSphere(const Sphere2&) const { return 200; }
    virtual uint64_t visitPyramid(const Pyramid&) const { return 300; }
};

struct Cube : Shape2 {
    uint64_t side;
    explicit Cube(uint64_t s) : side(s) {}
    uint64_t id() const override { return 1; }
    uint64_t weight() const override { return side * side * side; }
    uint64_t accept(const ShapeVisitor& v) const override { return v.visitCube(*this); }
};

struct Sphere2 : Shape2 {
    uint64_t radius;
    explicit Sphere2(uint64_t r) : radius(r) {}
    uint64_t id() const override { return 2; }
    uint64_t weight() const override { return radius * radius * radius * 4ULL; }
    uint64_t accept(const ShapeVisitor& v) const override { return v.visitSphere(*this); }
};

struct Pyramid : Shape2 {
    uint64_t base, height;
    Pyramid(uint64_t b, uint64_t h) : base(b), height(h) {}
    uint64_t id() const override { return 3; }
    uint64_t weight() const override { return (base * base * height) / 3ULL; }
    uint64_t accept(const ShapeVisitor& v) const override { return v.visitPyramid(*this); }
};

struct HeavyVisitor : ShapeVisitor {
    uint64_t visitCube(const Cube& c) const override { return c.weight() * 2ULL; }
    uint64_t visitSphere(const Sphere2& s) const override { return s.weight() * 3ULL; }
    uint64_t visitPyramid(const Pyramid& p) const override { return p.weight() * 5ULL; }
};

typedef uint64_t (Shape2::*Shape2Method)() const;

uint64_t virtualDispatchIndirectSection() {
    std::cout << "\n-- Virtual dispatch combined with pointer-to-member and double dispatch --\n";
    uint64_t checksum = 0;

    Cube cube(4ULL);
    Sphere2 sphere(3ULL);
    Pyramid pyramid(6ULL, 5ULL);
    Shape2* shapes[3] = { &cube, &sphere, &pyramid };

    Shape2Method methods[2] = { &Shape2::id, &Shape2::weight };
    const char* methodNames[2] = { "id", "weight" };

    for (int i = 0; i < 3; ++i) {
        for (int m = 0; m < 2; ++m) {
            uint64_t r = (shapes[i]->*methods[m])();
            std::cout << "  shapes[" << i << "]->" << methodNames[m] << "() = " << r << "\n";
            checksum += r;
        }
    }

    HeavyVisitor visitor;
    ShapeVisitor* visitorPtr = &visitor;
    for (int i = 0; i < 3; ++i) {
        uint64_t r = shapes[i]->accept(*visitorPtr);
        std::cout << "  shapes[" << i << "]->accept(visitor) = " << r << "\n";
        checksum += r;
    }

    return checksum;
}

enum class State : int { Idle = 0, Running = 1, Paused = 2, Stopped = 3 };

State stateIdleHandler(uint64_t& acc) { acc += 1ULL; return State::Running; }
State stateRunningHandler(uint64_t& acc) { acc += 10ULL; return State::Paused; }
State statePausedHandler(uint64_t& acc) { acc += 100ULL; return State::Running; }
State stateStoppedHandler(uint64_t& acc) { acc += 1000ULL; return State::Stopped; }

typedef State (*StateHandler)(uint64_t&);
StateHandler stateTable[4] = { stateIdleHandler, stateRunningHandler, statePausedHandler, stateStoppedHandler };

uint64_t stateMachineJumpTableSection() {
    std::cout << "\n-- Function-pointer-table state machine --\n";
    uint64_t checksum = 0;

    uint64_t acc = 0;
    State current = State::Idle;

    for (int step = 0; step < 10; ++step) {
        int idx = static_cast<int>(current);
        StateHandler handler = stateTable[idx];
        State next = handler(acc);
        std::cout << "  step " << step << ": state " << idx << " -> " << static_cast<int>(next) << " acc=" << acc << "\n";
        current = next;
        if (step == 7) current = State::Stopped;
    }

    checksum += acc;
    return checksum;
}

uint64_t stageDouble(uint64_t x) { return x * 2ULL; }
uint64_t stageAddSeven(uint64_t x) { return x + 7ULL; }
uint64_t stageSquareModulo(uint64_t x) { return (x * x) % 1000003ULL; }

uint64_t applyPipeline(uint64_t input, const std::vector<uint64_t(*)(uint64_t)>& stages) {
    uint64_t v = input;
    for (size_t i = 0; i < stages.size(); ++i) v = stages[i](v);
    return v;
}

struct HandlerRegistry {
    std::vector<std::function<void(uint64_t)>> handlers;
    uint64_t accumulated = 0;
    void registerHandler(std::function<void(uint64_t)> h) { handlers.push_back(h); }
    void dispatch(uint64_t v) {
        for (size_t i = 0; i < handlers.size(); ++i) handlers[i](v);
    }
};

uint64_t callablePipelineAndHandlerRegistrySection() {
    std::cout << "\n-- Callable pipeline and handler registry --\n";
    uint64_t checksum = 0;

    std::vector<uint64_t(*)(uint64_t)> stages = { stageDouble, stageAddSeven, stageSquareModulo };
    uint64_t piped = applyPipeline(9ULL, stages);
    std::cout << "  pipeline(9) = " << piped << "\n";
    checksum += piped;

    HandlerRegistry registry;
    registry.registerHandler([&checksum](uint64_t v) { checksum += v; });
    registry.registerHandler([&registry](uint64_t v) { registry.accumulated += v * 2ULL; });

    uint64_t total = 0;
    auto reducer = [&total](uint64_t v) { total += v; };
    registry.registerHandler(reducer);

    registry.dispatch(15ULL);
    registry.dispatch(25ULL);

    std::cout << "  registry accumulated = " << registry.accumulated << ", total = " << total << "\n";
    checksum += registry.accumulated + total;

    return checksum;
}

uint64_t deterministicShuffleSection() {
    std::cout << "\n-- Function pointer array driven by deterministic data --\n";
    uint64_t checksum = 0;

    Lcg rng(987654321ULL);
    BinaryOp ops[5] = { opAdd, opSub, opMul, opMax, opMin };
    uint64_t accumulator = 1;

    for (int i = 0; i < 20; ++i) {
        uint64_t opIndex = rng.nextInt(0, 4);
        uint64_t operand = rng.nextInt(1, 9);
        BinaryOp chosen = ops[opIndex];
        accumulator = chosen(accumulator % 1000003ULL, operand);
        std::cout << "  step " << i << " opIndex=" << opIndex << " operand=" << operand
                  << " accumulator=" << accumulator << "\n";
    }

    checksum += accumulator;
    return checksum;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();

    std::cout << "=== Indirect Call Stress Test ===\n\n";

    uint64_t checksum = 0;
    checksum += rawFunctionPointersAndJumpTableSection();
    checksum += globalFunctionPointersSection();
    checksum += structFunctionPointerMembersSection();
    checksum += memberFunctionPointerSection();
    checksum += stdFunctionAndFunctorSection();
    checksum += lambdaVarietySection();
    checksum += directRecursionSection();
    checksum += mutualRecursionSection();
    checksum += recursionViaFunctionPointerSection();
    checksum += recursiveLambdaViaStdFunctionSection();
    checksum += virtualDispatchIndirectSection();
    checksum += stateMachineJumpTableSection();
    checksum += callablePipelineAndHandlerRegistrySection();
    checksum += deterministicShuffleSection();

    std::cout << "\n=== Final checksum ===\n";
    std::cout << "TOTAL_CHECKSUM: " << checksum << "\n";

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << diff << "\n";

    return 0;
}