#include <iostream>
#include <vector>
#include <utility>
#include <type_traits>
#include <cstdint>
#include <cstddef>
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

template <typename T>
T templateMax(T a, T b) { return a > b ? a : b; }

template <typename T>
T templateMax(T a, T b, T c) { return templateMax(templateMax(a, b), c); }

template <typename T>
T templateMin(T a, T b) { return a < b ? a : b; }

template <typename T, size_t N>
T sumArray(const T (&arr)[N]) {
    T total = T(0);
    for (size_t i = 0; i < N; ++i) total += arr[i];
    return total;
}

uint64_t functionTemplateBasicsSection() {
    std::cout << "-- Function template basics (overloading, deduction, non-type params) --\n";
    uint64_t checksum = 0;

    uint64_t m1 = templateMax<uint64_t>(42, 17);
    int64_t m2 = templateMax<int64_t>(-5, -2);
    uint64_t m3 = templateMax(10ULL, 20ULL, 15ULL);
    std::cout << "  templateMax(42,17)=" << m1 << " templateMax(-5,-2)=" << m2
              << " templateMax(10,20,15)=" << m3 << "\n";
    checksum += m1 + static_cast<uint64_t>(m2 < 0 ? -m2 : m2) + m3;

    uint64_t arr[6] = { 1, 2, 3, 4, 5, 6 };
    uint64_t arrSum = sumArray(arr);
    std::cout << "  sumArray = " << arrSum << "\n";
    checksum += arrSum;

    uint64_t mn = templateMin(uint64_t(9), uint64_t(4));
    std::cout << "  templateMin(9,4) = " << mn << "\n";
    checksum += mn;

    return checksum;
}

template <typename T>
struct Describe {
    static uint64_t code() { return 0; }
};

template <>
struct Describe<int32_t> {
    static uint64_t code() { return 1; }
};

template <>
struct Describe<uint64_t> {
    static uint64_t code() { return 2; }
};

template <typename T>
struct Describe<T*> {
    static uint64_t code() { return 100ULL + Describe<T>::code(); }
};

template <typename T, size_t N>
struct Describe<T[N]> {
    static uint64_t code() { return 200ULL + Describe<T>::code() + static_cast<uint64_t>(N); }
};

uint64_t templateSpecializationSection() {
    std::cout << "\n-- Template specialization (primary, full, partial) --\n";
    uint64_t checksum = 0;

    uint64_t c1 = Describe<int32_t>::code();
    uint64_t c2 = Describe<uint64_t>::code();
    uint64_t c3 = Describe<double>::code();
    uint64_t c4 = Describe<int32_t*>::code();
    uint64_t c5 = Describe<uint64_t[5]>::code();

    std::cout << "  Describe<int32_t>::code() = " << c1 << "\n";
    std::cout << "  Describe<uint64_t>::code() = " << c2 << "\n";
    std::cout << "  Describe<double>::code() (primary fallback) = " << c3 << "\n";
    std::cout << "  Describe<int32_t*>::code() (partial, pointer) = " << c4 << "\n";
    std::cout << "  Describe<uint64_t[5]>::code() (partial, array) = " << c5 << "\n";

    checksum += c1 + c2 + c3 + c4 + c5;
    return checksum;
}

template <typename... Args>
constexpr auto sumAll(Args... args) {
    return (args + ...);
}

template <typename First, typename... Rest>
constexpr auto maxAll(First first, Rest... rest) {
    if constexpr (sizeof...(Rest) == 0) {
        return first;
    } else {
        auto restMax = maxAll(rest...);
        return first > restMax ? first : restMax;
    }
}

template <typename... Args>
void printAll(std::ostream& os, Args&&... args) {
    ((os << args << " "), ...);
}

uint64_t variadicTemplatesSection() {
    std::cout << "\n-- Variadic templates and fold expressions --\n";
    uint64_t checksum = 0;

    uint64_t s = sumAll(1ULL, 2ULL, 3ULL, 4ULL, 5ULL);
    std::cout << "  sumAll(1,2,3,4,5) = " << s << "\n";
    checksum += s;

    uint64_t mx = maxAll(3ULL, 9ULL, 1ULL, 7ULL, 2ULL);
    std::cout << "  maxAll(3,9,1,7,2) = " << mx << "\n";
    checksum += mx;

    std::cout << "  printAll: ";
    printAll(std::cout, 10, 20, 30, "done");
    std::cout << "\n";

    return checksum;
}

template <typename T>
struct SimpleBox {
    T value;
    explicit SimpleBox(T v) : value(v) {}
};

template <typename T>
struct DoubledBox {
    T value;
    explicit DoubledBox(T v) : value(static_cast<T>(v * 2)) {}
};

template <template <typename> class Holder, typename T>
struct HolderPrinter {
    static uint64_t extract(const Holder<T>& h) { return static_cast<uint64_t>(h.value); }
};

uint64_t templateTemplateParameterSection() {
    std::cout << "\n-- Template template parameters --\n";
    uint64_t checksum = 0;

    SimpleBox<uint64_t> box(55);
    DoubledBox<uint64_t> dbox(55);

    uint64_t v1 = HolderPrinter<SimpleBox, uint64_t>::extract(box);
    uint64_t v2 = HolderPrinter<DoubledBox, uint64_t>::extract(dbox);

    std::cout << "  HolderPrinter<SimpleBox, uint64_t>::extract = " << v1 << "\n";
    std::cout << "  HolderPrinter<DoubledBox, uint64_t>::extract = " << v2 << "\n";

    checksum += v1 + v2;
    return checksum;
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value, uint64_t>::type
classify(T v) {
    return static_cast<uint64_t>(v) % 2ULL == 0ULL ? 1000ULL : 2000ULL;
}

template <typename T>
typename std::enable_if<!std::is_integral<T>::value, uint64_t>::type
classify(T v) {
    (void)v;
    return 3000ULL;
}

uint64_t sfinaeAndTypeTraitsSection() {
    std::cout << "\n-- SFINAE and type traits --\n";
    uint64_t checksum = 0;

    uint64_t r1 = classify(42);
    uint64_t r2 = classify(SimpleBox<uint64_t>(9));
    std::cout << "  classify(42) [integral] = " << r1 << "\n";
    std::cout << "  classify(SimpleBox) [non-integral] = " << r2 << "\n";
    checksum += r1 + r2;

    static_assert(std::is_same<std::decay<const int&>::type, int>::value, "decay mismatch");
    static_assert(std::is_integral<uint64_t>::value, "expected integral");
    static_assert(!std::is_integral<SimpleBox<uint64_t>>::value, "expected non-integral");
    static_assert(std::is_same<std::conditional<true, int32_t, int64_t>::type, int32_t>::value, "conditional mismatch");

    checksum += 1;
    return checksum;
}

constexpr uint64_t constexprFactorial(uint64_t n) {
    return n <= 1ULL ? 1ULL : n * constexprFactorial(n - 1ULL);
}

template <uint64_t N>
struct FibValue {
    static constexpr uint64_t value = FibValue<N - 1>::value + FibValue<N - 2>::value;
};

template <>
struct FibValue<0> {
    static constexpr uint64_t value = 0;
};

template <>
struct FibValue<1> {
    static constexpr uint64_t value = 1;
};

constexpr uint64_t constexprGcd(uint64_t a, uint64_t b) {
    return b == 0ULL ? a : constexprGcd(b, a % b);
}

uint64_t constexprMetaprogrammingSection() {
    std::cout << "\n-- constexpr and compile-time template metaprogramming --\n";
    uint64_t checksum = 0;

    static_assert(constexprFactorial(10ULL) == 3628800ULL, "factorial mismatch");
    static_assert(FibValue<20>::value == 6765ULL, "fibonacci mismatch");
    static_assert(constexprGcd(1071ULL, 462ULL) == 21ULL, "gcd mismatch");

    constexpr uint64_t fact10 = constexprFactorial(10ULL);
    constexpr uint64_t fib20 = FibValue<20>::value;
    constexpr uint64_t gcdVal = constexprGcd(1071ULL, 462ULL);

    std::cout << "  constexprFactorial(10) = " << fact10 << "\n";
    std::cout << "  FibValue<20>::value = " << fib20 << "\n";
    std::cout << "  constexprGcd(1071, 462) = " << gcdVal << "\n";

    checksum += fact10 + fib20 + gcdVal;
    return checksum;
}

template <typename T, size_t Rows, size_t Cols>
struct Matrix {
    T data[Rows][Cols];
    Matrix() {
        for (size_t r = 0; r < Rows; ++r)
            for (size_t c = 0; c < Cols; ++c)
                data[r][c] = T(0);
    }
    T& operator()(size_t r, size_t c) { return data[r][c]; }
    const T& operator()(size_t r, size_t c) const { return data[r][c]; }
    Matrix operator+(const Matrix& o) const {
        Matrix result;
        for (size_t r = 0; r < Rows; ++r)
            for (size_t c = 0; c < Cols; ++c)
                result.data[r][c] = data[r][c] + o.data[r][c];
        return result;
    }
    bool operator==(const Matrix& o) const {
        for (size_t r = 0; r < Rows; ++r)
            for (size_t c = 0; c < Cols; ++c)
                if (data[r][c] != o.data[r][c]) return false;
        return true;
    }
};

template <typename T, size_t A, size_t B, size_t C>
Matrix<T, A, C> matMul(const Matrix<T, A, B>& lhs, const Matrix<T, B, C>& rhs) {
    Matrix<T, A, C> result;
    for (size_t i = 0; i < A; ++i) {
        for (size_t j = 0; j < C; ++j) {
            T sum = T(0);
            for (size_t k = 0; k < B; ++k) sum += lhs(i, k) * rhs(k, j);
            result(i, j) = sum;
        }
    }
    return result;
}

template <typename T, size_t Rows, size_t Cols>
std::ostream& operator<<(std::ostream& os, const Matrix<T, Rows, Cols>& m) {
    for (size_t r = 0; r < Rows; ++r) {
        os << "[";
        for (size_t c = 0; c < Cols; ++c) {
            os << m(r, c);
            if (c + 1 < Cols) os << ", ";
        }
        os << "]";
        if (r + 1 < Rows) os << " ";
    }
    return os;
}

uint64_t matrixOperatorOverloadSection() {
    std::cout << "\n-- Matrix<T, Rows, Cols> class template with operator overloading --\n";
    uint64_t checksum = 0;

    Lcg rng(0xABCDEFULL);

    Matrix<int64_t, 2, 2> a;
    a(0, 0) = static_cast<int64_t>(rng.nextInt(1, 9));
    a(0, 1) = static_cast<int64_t>(rng.nextInt(1, 9));
    a(1, 0) = static_cast<int64_t>(rng.nextInt(1, 9));
    a(1, 1) = static_cast<int64_t>(rng.nextInt(1, 9));

    Matrix<int64_t, 2, 2> b;
    b(0, 0) = static_cast<int64_t>(rng.nextInt(1, 9));
    b(0, 1) = static_cast<int64_t>(rng.nextInt(1, 9));
    b(1, 0) = static_cast<int64_t>(rng.nextInt(1, 9));
    b(1, 1) = static_cast<int64_t>(rng.nextInt(1, 9));

    Matrix<int64_t, 2, 2> sum = a + b;
    std::cout << "  a + b = " << sum << "\n";

    Matrix<int64_t, 2, 3> c;
    c(0, 0) = static_cast<int64_t>(rng.nextInt(1, 9));
    c(0, 1) = static_cast<int64_t>(rng.nextInt(1, 9));
    c(0, 2) = static_cast<int64_t>(rng.nextInt(1, 9));
    c(1, 0) = static_cast<int64_t>(rng.nextInt(1, 9));
    c(1, 1) = static_cast<int64_t>(rng.nextInt(1, 9));
    c(1, 2) = static_cast<int64_t>(rng.nextInt(1, 9));

    Matrix<int64_t, 3, 2> d;
    d(0, 0) = static_cast<int64_t>(rng.nextInt(1, 9));
    d(0, 1) = static_cast<int64_t>(rng.nextInt(1, 9));
    d(1, 0) = static_cast<int64_t>(rng.nextInt(1, 9));
    d(1, 1) = static_cast<int64_t>(rng.nextInt(1, 9));
    d(2, 0) = static_cast<int64_t>(rng.nextInt(1, 9));
    d(2, 1) = static_cast<int64_t>(rng.nextInt(1, 9));

    Matrix<int64_t, 2, 2> product = matMul(c, d);
    std::cout << "  c * d = " << product << "\n";

    bool selfEqual = (sum == sum);
    std::cout << "  sum == sum: " << (selfEqual ? "true" : "false") << "\n";

    for (size_t r = 0; r < 2; ++r) {
        for (size_t col = 0; col < 2; ++col) {
            int64_t v = sum(r, col);
            checksum += static_cast<uint64_t>(v < 0 ? -v : v);
        }
    }

    for (size_t r = 0; r < 2; ++r) {
        for (size_t col = 0; col < 2; ++col) {
            int64_t v = product(r, col);
            checksum += static_cast<uint64_t>(v < 0 ? -v : v);
        }
    }

    checksum += selfEqual ? 100 : 0;

    return checksum;
}

template <typename T>
struct Wrapper {
    T value;
    explicit Wrapper(T v) : value(v) {}
};

template <typename T, typename U>
auto operator+(const Wrapper<T>& a, const Wrapper<U>& b) -> Wrapper<decltype(a.value + b.value)> {
    return Wrapper<decltype(a.value + b.value)>(a.value + b.value);
}

template <typename T, typename U>
auto operator*(const Wrapper<T>& a, const Wrapper<U>& b) -> Wrapper<decltype(a.value * b.value)> {
    return Wrapper<decltype(a.value * b.value)>(a.value * b.value);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const Wrapper<T>& w) {
    os << w.value;
    return os;
}

uint64_t genericWrapperArithmeticSection() {
    std::cout << "\n-- Generic Wrapper<T> arithmetic via templated operators --\n";
    uint64_t checksum = 0;

    Wrapper<int32_t> w1(100);
    Wrapper<int64_t> w2(200000000000LL);

    auto sumW = w1 + w2;
    auto prodW = w1 * w1;

    std::cout << "  w1 + w2 = " << sumW << "\n";
    std::cout << "  w1 * w1 = " << prodW << "\n";

    int64_t sv = sumW.value;
    int64_t pv = static_cast<int64_t>(prodW.value);
    checksum += static_cast<uint64_t>(sv < 0 ? -sv : sv);
    checksum += static_cast<uint64_t>(pv < 0 ? -pv : pv);

    return checksum;
}

struct LoudConstruct {
    uint64_t id;
    explicit LoudConstruct(uint64_t i) : id(i) {}
    LoudConstruct(uint64_t i, uint64_t j) : id(i + j) {}
};

template <typename T, typename... Args>
T makeForwarded(Args&&... args) {
    return T(std::forward<Args>(args)...);
}

template <typename Func, typename... Args>
auto invokeForwarded(Func&& f, Args&&... args) -> decltype(f(std::forward<Args>(args)...)) {
    return f(std::forward<Args>(args)...);
}

uint64_t perfectForwardingSection() {
    std::cout << "\n-- Perfect forwarding with variadic templates --\n";
    uint64_t checksum = 0;

    LoudConstruct single = makeForwarded<LoudConstruct>(uint64_t(5));
    LoudConstruct combined = makeForwarded<LoudConstruct>(uint64_t(5), uint64_t(7));

    std::cout << "  single.id = " << single.id << " combined.id = " << combined.id << "\n";
    checksum += single.id + combined.id;

    auto adder = [](int64_t x, int64_t y, int64_t z) { return x + y + z; };
    int64_t forwardedResult = invokeForwarded(adder, int64_t(3), int64_t(4), int64_t(5));
    std::cout << "  invokeForwarded(adder, 3, 4, 5) = " << forwardedResult << "\n";
    checksum += static_cast<uint64_t>(forwardedResult);

    return checksum;
}

template <typename... Ts>
struct Tuple;

template <>
struct Tuple<> {};

template <typename Head, typename... Tail>
struct Tuple<Head, Tail...> : Tuple<Tail...> {
    Head head;
    Tuple(Head h, Tail... t) : Tuple<Tail...>(t...), head(h) {}
};

template <size_t I, typename... Ts>
struct TupleGetter;

template <typename Head, typename... Tail>
struct TupleGetter<0, Head, Tail...> {
    static Head& get(Tuple<Head, Tail...>& t) { return t.head; }
};

template <size_t I, typename Head, typename... Tail>
struct TupleGetter<I, Head, Tail...> {
    static auto get(Tuple<Head, Tail...>& t) -> decltype(TupleGetter<I - 1, Tail...>::get(t)) {
        return TupleGetter<I - 1, Tail...>::get(t);
    }
};

template <size_t I, typename... Ts>
auto tupleGet(Tuple<Ts...>& t) -> decltype(TupleGetter<I, Ts...>::get(t)) {
    return TupleGetter<I, Ts...>::get(t);
}

template <typename... Ts>
struct TupleEq;

template <>
struct TupleEq<> {
    static bool equals(Tuple<>&, Tuple<>&) { return true; }
};

template <typename Head, typename... Tail>
struct TupleEq<Head, Tail...> {
    static bool equals(Tuple<Head, Tail...>& a, Tuple<Head, Tail...>& b) {
        bool headEq = (a.head == b.head);
        Tuple<Tail...>& aTail = a;
        Tuple<Tail...>& bTail = b;
        return headEq && TupleEq<Tail...>::equals(aTail, bTail);
    }
};

template <typename... Ts>
bool tupleEquals(Tuple<Ts...>& a, Tuple<Ts...>& b) {
    return TupleEq<Ts...>::equals(a, b);
}

uint64_t customVariadicTupleSection() {
    std::cout << "\n-- Hand-rolled variadic Tuple via recursive template inheritance --\n";
    uint64_t checksum = 0;

    Tuple<uint64_t, int64_t, uint64_t> myTuple(10, -5, 20);

    uint64_t t0 = tupleGet<0>(myTuple);
    int64_t t1 = tupleGet<1>(myTuple);
    uint64_t t2 = tupleGet<2>(myTuple);

    std::cout << "  tupleGet<0> = " << t0 << " tupleGet<1> = " << t1 << " tupleGet<2> = " << t2 << "\n";
    checksum += t0 + static_cast<uint64_t>(t1 < 0 ? -t1 : t1) + t2;

    Tuple<uint64_t, int64_t, uint64_t> otherTuple(10, -5, 20);
    bool equalTuples = tupleEquals(myTuple, otherTuple);
    std::cout << "  tupleEquals(myTuple, otherTuple) = " << (equalTuples ? "true" : "false") << "\n";
    checksum += equalTuples ? 1000ULL : 0ULL;

    return checksum;
}

template <typename T>
using Vec = std::vector<T>;

template <typename T>
using Pair2 = std::pair<T, T>;

template <typename A, typename B>
auto addPair(A a, B b) -> decltype(a + b) {
    return a + b;
}

auto lambdaAutoReturn = [](int64_t x) { return x * x; };

uint64_t templateAliasAndDecltypeSection() {
    std::cout << "\n-- Template aliases, decltype, auto return deduction --\n";
    uint64_t checksum = 0;

    Vec<uint64_t> values = { 4, 8, 15, 16, 23, 42 };
    uint64_t total = 0;
    for (size_t i = 0; i < values.size(); ++i) total += values[i];
    std::cout << "  Vec<uint64_t> total = " << total << "\n";
    checksum += total;

    Pair2<int64_t> pr(11, 22);
    auto pairSum = addPair(pr.first, pr.second);
    std::cout << "  addPair(pr.first, pr.second) = " << pairSum << "\n";
    checksum += static_cast<uint64_t>(pairSum);

    auto squared = lambdaAutoReturn(9);
    std::cout << "  lambdaAutoReturn(9) = " << squared << "\n";
    checksum += static_cast<uint64_t>(squared);

    return checksum;
}

template <typename T>
uint64_t describeType(T value) {
    if constexpr (std::is_pointer<T>::value) {
        return value != nullptr ? 1ULL : 0ULL;
    } else if constexpr (std::is_integral<T>::value) {
        return static_cast<uint64_t>(value) % 100ULL;
    } else {
        return 999ULL;
    }
}

uint64_t ifConstexprBranchingSection() {
    std::cout << "\n-- if constexpr compile-time branching --\n";
    uint64_t checksum = 0;

    int64_t plainValue = 142;
    int64_t* pointerValue = &plainValue;
    SimpleBox<uint64_t> boxValue(9);

    uint64_t r1 = describeType(plainValue);
    uint64_t r2 = describeType(pointerValue);
    uint64_t r3 = describeType(boxValue);

    std::cout << "  describeType(int64_t) = " << r1 << "\n";
    std::cout << "  describeType(int64_t*) = " << r2 << "\n";
    std::cout << "  describeType(SimpleBox) = " << r3 << "\n";

    checksum += r1 + r2 + r3;
    return checksum;
}

struct MixinLog {
    uint64_t logValue() const { return 7; }
};

struct MixinCount {
    uint64_t countValue() const { return 13; }
};

struct MixinFlag {
    uint64_t flagValue() const { return 21; }
};

template <typename... Mixins>
struct Composed : Mixins... {
    uint64_t sumAllMixins() const {
        return sumMixinsImpl<Mixins...>();
    }

private:
    template <typename First, typename... Rest>
    uint64_t sumMixinsImpl() const {
        if constexpr (sizeof...(Rest) == 0) {
            return valueFrom<First>();
        } else {
            return valueFrom<First>() + sumMixinsImpl<Rest...>();
        }
    }

    template <typename M>
    uint64_t valueFrom() const {
        if constexpr (std::is_same<M, MixinLog>::value) {
            return static_cast<const MixinLog*>(this)->logValue();
        } else if constexpr (std::is_same<M, MixinCount>::value) {
            return static_cast<const MixinCount*>(this)->countValue();
        } else if constexpr (std::is_same<M, MixinFlag>::value) {
            return static_cast<const MixinFlag*>(this)->flagValue();
        } else {
            return 0ULL;
        }
    }
};

uint64_t variadicMixinInheritanceSection() {
    std::cout << "\n-- Variadic template mixin inheritance --\n";
    uint64_t checksum = 0;

    Composed<MixinLog, MixinCount, MixinFlag> composedObj;
    uint64_t total = composedObj.sumAllMixins();
    std::cout << "  Composed<Log,Count,Flag>.sumAllMixins() = " << total << "\n";
    checksum += total;

    Composed<MixinCount, MixinFlag> partial;
    uint64_t partialTotal = partial.sumAllMixins();
    std::cout << "  Composed<Count,Flag>.sumAllMixins() = " << partialTotal << "\n";
    checksum += partialTotal;

    return checksum;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();

    std::cout << "=== Template Metaprogramming Stress Test ===\n\n";

    uint64_t checksum = 0;
    checksum += functionTemplateBasicsSection();
    checksum += templateSpecializationSection();
    checksum += variadicTemplatesSection();
    checksum += templateTemplateParameterSection();
    checksum += sfinaeAndTypeTraitsSection();
    checksum += constexprMetaprogrammingSection();
    checksum += matrixOperatorOverloadSection();
    checksum += genericWrapperArithmeticSection();
    checksum += perfectForwardingSection();
    checksum += customVariadicTupleSection();
    checksum += templateAliasAndDecltypeSection();
    checksum += ifConstexprBranchingSection();
    checksum += variadicMixinInheritanceSection();

    std::cout << "\n=== Final checksum ===\n";
    std::cout << "TOTAL_CHECKSUM: " << checksum << "\n";

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << diff << "\n";

    return 0;
}