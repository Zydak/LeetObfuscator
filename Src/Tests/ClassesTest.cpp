#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <typeinfo>
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

uint64_t isqrt(uint64_t n) {
    if (n == 0) return 0;
    uint64_t lo = 1;
    uint64_t hi = n;
    uint64_t result = 0;
    while (lo <= hi) {
        uint64_t mid = lo + (hi - lo) / 2;
        if (mid <= n / mid) {
            result = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return result;
}

struct Animal {
    virtual ~Animal() = default;
    virtual uint64_t sound() const { return 1; }
};

struct Dog : Animal {
    uint64_t sound() const override { return 2; }
};

struct Puppy : Dog {
    uint64_t sound() const override { return Dog::sound() + 3; }
};

uint64_t smallHierarchySection() {
    std::cout << "-- Small hierarchy (Animal -> Dog -> Puppy) --\n";
    uint64_t checksum = 0;

    Animal baseAnimal;
    Dog dog;
    Puppy puppy;
    Animal* animals[3] = { &baseAnimal, &dog, &puppy };

    for (int i = 0; i < 3; ++i) {
        uint64_t s = animals[i]->sound();
        std::cout << "  animals[" << i << "]->sound() = " << s << "\n";
        checksum += s;
    }

    return checksum;
}

template <int N>
struct ChainLink : ChainLink<N - 1> {
    uint64_t compute() const override {
        return ChainLink<N - 1>::compute() + static_cast<uint64_t>(N);
    }
    uint64_t depth() const override {
        return ChainLink<N - 1>::depth() + 1ULL;
    }
};

template <>
struct ChainLink<0> {
    virtual ~ChainLink() = default;
    virtual uint64_t compute() const { return 0; }
    virtual uint64_t depth() const { return 0; }
};

constexpr int kChainDepth = 180;

uint64_t bigChainHierarchySection() {
    std::cout << "\n-- Big linear inheritance chain (depth " << kChainDepth << ") --\n";
    uint64_t checksum = 0;

    ChainLink<kChainDepth> link;
    ChainLink<0>* basePtr = &link;

    uint64_t result = basePtr->compute();
    uint64_t depth = basePtr->depth();
    std::cout << "  chain compute() via base pointer = " << result << "\n";
    std::cout << "  chain depth() via base pointer = " << depth << "\n";
    checksum += result;
    checksum += depth;

    uint64_t expected = static_cast<uint64_t>(kChainDepth) * static_cast<uint64_t>(kChainDepth + 1) / 2ULL;
    bool matches = (result == expected) && (depth == static_cast<uint64_t>(kChainDepth));
    std::cout << "  matches expected closed form: " << (matches ? "true" : "false") << "\n";
    checksum += matches ? 1000 : 0;

    return checksum;
}

struct Shape {
    virtual ~Shape() = default;
    virtual uint64_t area() const = 0;
    virtual uint64_t perimeter() const = 0;
};

struct ShapeSquare : Shape {
    uint64_t side;
    explicit ShapeSquare(uint64_t s) : side(s) {}
    uint64_t area() const override { return side * side; }
    uint64_t perimeter() const override { return side * 4ULL; }
};

struct ShapeRectangle : Shape {
    uint64_t width, height;
    ShapeRectangle(uint64_t w, uint64_t h) : width(w), height(h) {}
    uint64_t area() const override { return width * height; }
    uint64_t perimeter() const override { return (width + height) * 2ULL; }
};

struct ShapeRightTriangle : Shape {
    uint64_t base, height;
    ShapeRightTriangle(uint64_t b, uint64_t h) : base(b), height(h) {}
    uint64_t area() const override { return (base * height) / 2ULL; }
    uint64_t perimeter() const override {
        return base + height + isqrt(base * base + height * height);
    }
};

struct ShapeRegularPentagon : Shape {
    uint64_t side;
    explicit ShapeRegularPentagon(uint64_t s) : side(s) {}
    uint64_t area() const override { return (side * side * 172ULL) / 100ULL; }
    uint64_t perimeter() const override { return side * 5ULL; }
};

struct ShapeRegularHexagon : Shape {
    uint64_t side;
    explicit ShapeRegularHexagon(uint64_t s) : side(s) {}
    uint64_t area() const override { return (side * side * 259ULL) / 100ULL; }
    uint64_t perimeter() const override { return side * 6ULL; }
};

struct ShapeRegularHeptagon : Shape {
    uint64_t side;
    explicit ShapeRegularHeptagon(uint64_t s) : side(s) {}
    uint64_t area() const override { return (side * side * 337ULL) / 100ULL; }
    uint64_t perimeter() const override { return side * 7ULL; }
};

struct ShapeRegularOctagon : Shape {
    uint64_t side;
    explicit ShapeRegularOctagon(uint64_t s) : side(s) {}
    uint64_t area() const override { return (side * side * 483ULL) / 100ULL; }
    uint64_t perimeter() const override { return side * 8ULL; }
};

struct ShapeRhombus : Shape {
    uint64_t diagA, diagB;
    ShapeRhombus(uint64_t a, uint64_t b) : diagA(a), diagB(b) {}
    uint64_t area() const override { return (diagA * diagB) / 2ULL; }
    uint64_t perimeter() const override {
        return 4ULL * isqrt((diagA / 2ULL) * (diagA / 2ULL) + (diagB / 2ULL) * (diagB / 2ULL));
    }
};

struct ShapeParallelogram : Shape {
    uint64_t base, height, side;
    ShapeParallelogram(uint64_t b, uint64_t h, uint64_t s) : base(b), height(h), side(s) {}
    uint64_t area() const override { return base * height; }
    uint64_t perimeter() const override { return (base + side) * 2ULL; }
};

struct ShapeTrapezoid : Shape {
    uint64_t topSide, bottomSide, height;
    ShapeTrapezoid(uint64_t t, uint64_t b, uint64_t h) : topSide(t), bottomSide(b), height(h) {}
    uint64_t area() const override { return ((topSide + bottomSide) * height) / 2ULL; }
    uint64_t perimeter() const override { return topSide + bottomSide + height * 2ULL; }
};

struct ShapeCircleApprox : Shape {
    uint64_t radius;
    explicit ShapeCircleApprox(uint64_t r) : radius(r) {}
    uint64_t area() const override { return (radius * radius * 22ULL) / 7ULL; }
    uint64_t perimeter() const override { return (radius * 44ULL) / 7ULL; }
};

struct ShapeSemiCircleApprox : Shape {
    uint64_t radius;
    explicit ShapeSemiCircleApprox(uint64_t r) : radius(r) {}
    uint64_t area() const override { return (radius * radius * 11ULL) / 7ULL; }
    uint64_t perimeter() const override { return (radius * 22ULL) / 7ULL + radius * 2ULL; }
};

struct ShapeEllipseApprox : Shape {
    uint64_t semiMajor, semiMinor;
    ShapeEllipseApprox(uint64_t a, uint64_t b) : semiMajor(a), semiMinor(b) {}
    uint64_t area() const override { return (semiMajor * semiMinor * 22ULL) / 7ULL; }
    uint64_t perimeter() const override { return ((semiMajor + semiMinor) * 22ULL) / 7ULL; }
};

struct ShapeAnnulusApprox : Shape {
    uint64_t outerRadius, innerRadius;
    ShapeAnnulusApprox(uint64_t o, uint64_t i) : outerRadius(o), innerRadius(i) {}
    uint64_t area() const override {
        return ((outerRadius * outerRadius) - (innerRadius * innerRadius)) * 22ULL / 7ULL;
    }
    uint64_t perimeter() const override { return (outerRadius + innerRadius) * 44ULL / 7ULL; }
};

struct ShapeKite : Shape {
    uint64_t diagA, diagB, sideA, sideB;
    ShapeKite(uint64_t da, uint64_t db, uint64_t sa, uint64_t sb) : diagA(da), diagB(db), sideA(sa), sideB(sb) {}
    uint64_t area() const override { return (diagA * diagB) / 2ULL; }
    uint64_t perimeter() const override { return 2ULL * (sideA + sideB); }
};

uint64_t wideHierarchySection() {
    std::cout << "\n-- Wide hierarchy (Shape and 15 direct derived classes) --\n";
    uint64_t checksum = 0;

    Lcg rng(0xC0FFEEULL);
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<ShapeSquare>(rng.nextInt(3, 30)));
    shapes.push_back(std::make_unique<ShapeRectangle>(rng.nextInt(3, 30), rng.nextInt(3, 30)));
    shapes.push_back(std::make_unique<ShapeRightTriangle>(rng.nextInt(3, 30), rng.nextInt(3, 30)));
    shapes.push_back(std::make_unique<ShapeRegularPentagon>(rng.nextInt(3, 30)));
    shapes.push_back(std::make_unique<ShapeRegularHexagon>(rng.nextInt(3, 30)));
    shapes.push_back(std::make_unique<ShapeRegularHeptagon>(rng.nextInt(3, 30)));
    shapes.push_back(std::make_unique<ShapeRegularOctagon>(rng.nextInt(3, 30)));
    shapes.push_back(std::make_unique<ShapeRhombus>(rng.nextInt(4, 40), rng.nextInt(4, 40)));
    shapes.push_back(std::make_unique<ShapeParallelogram>(rng.nextInt(3, 30), rng.nextInt(3, 30), rng.nextInt(3, 30)));
    shapes.push_back(std::make_unique<ShapeTrapezoid>(rng.nextInt(3, 30), rng.nextInt(3, 30), rng.nextInt(3, 30)));
    shapes.push_back(std::make_unique<ShapeCircleApprox>(rng.nextInt(3, 30)));
    shapes.push_back(std::make_unique<ShapeSemiCircleApprox>(rng.nextInt(3, 30)));
    shapes.push_back(std::make_unique<ShapeEllipseApprox>(rng.nextInt(3, 30), rng.nextInt(3, 30)));
    shapes.push_back(std::make_unique<ShapeAnnulusApprox>(rng.nextInt(20, 40), rng.nextInt(3, 19)));
    shapes.push_back(std::make_unique<ShapeKite>(rng.nextInt(4, 40), rng.nextInt(4, 40), rng.nextInt(3, 20), rng.nextInt(3, 20)));

    for (size_t i = 0; i < shapes.size(); ++i) {
        uint64_t a = shapes[i]->area();
        uint64_t p = shapes[i]->perimeter();
        std::cout << "  shape[" << i << "] area=" << a << " perimeter=" << p << "\n";
        checksum += a + p;
    }

    std::cout << "  total shapes: " << shapes.size() << "\n";
    checksum += shapes.size();

    return checksum;
}

struct Flyer {
    virtual ~Flyer() = default;
    virtual uint64_t flySpeed() const { return 10; }
};

struct Swimmer {
    virtual ~Swimmer() = default;
    virtual uint64_t swimSpeed() const { return 5; }
};

struct Runner {
    virtual ~Runner() = default;
    virtual uint64_t runSpeed() const { return 8; }
};

struct Duck : Flyer, Swimmer {
    uint64_t flySpeed() const override { return Flyer::flySpeed() + 2ULL; }
    uint64_t swimSpeed() const override { return Swimmer::swimSpeed() + 1ULL; }
};

struct Platypus : Swimmer, Runner {
    uint64_t swimSpeed() const override { return Swimmer::swimSpeed() + 3ULL; }
    uint64_t runSpeed() const override { return Runner::runSpeed() + 1ULL; }
};

struct Griffin : Flyer, Swimmer, Runner {
    uint64_t flySpeed() const override { return Flyer::flySpeed() + 20ULL; }
    uint64_t swimSpeed() const override { return Swimmer::swimSpeed() + 4ULL; }
    uint64_t runSpeed() const override { return Runner::runSpeed() + 15ULL; }
};

uint64_t multipleInheritanceSection() {
    std::cout << "\n-- Multiple inheritance (unrelated mixin bases) --\n";
    uint64_t checksum = 0;

    Duck duck;
    Platypus platypus;
    Griffin griffin;

    Flyer* asFlyerDuck = &duck;
    Swimmer* asSwimmerDuck = &duck;
    std::cout << "  duck fly=" << asFlyerDuck->flySpeed() << " swim=" << asSwimmerDuck->swimSpeed() << "\n";
    checksum += asFlyerDuck->flySpeed() + asSwimmerDuck->swimSpeed();

    Swimmer* asSwimmerPlat = &platypus;
    Runner* asRunnerPlat = &platypus;
    std::cout << "  platypus swim=" << asSwimmerPlat->swimSpeed() << " run=" << asRunnerPlat->runSpeed() << "\n";
    checksum += asSwimmerPlat->swimSpeed() + asRunnerPlat->runSpeed();

    Flyer* asFlyerGriffin = &griffin;
    Swimmer* asSwimmerGriffin = &griffin;
    Runner* asRunnerGriffin = &griffin;
    std::cout << "  griffin fly=" << asFlyerGriffin->flySpeed()
              << " swim=" << asSwimmerGriffin->swimSpeed()
              << " run=" << asRunnerGriffin->runSpeed() << "\n";
    checksum += asFlyerGriffin->flySpeed() + asSwimmerGriffin->swimSpeed() + asRunnerGriffin->runSpeed();

    return checksum;
}

struct VBase {
    uint64_t tag = 7;
    virtual ~VBase() = default;
    virtual uint64_t identify() const { return tag; }
};

struct VLeft : virtual VBase {
    uint64_t identify() const override { return VBase::identify() + 100ULL; }
};

struct VRight : virtual VBase {
    uint64_t identify() const override { return VBase::identify() + 200ULL; }
};

struct VDiamond : VLeft, VRight {
    uint64_t identify() const override { return VLeft::identify() + VRight::identify() + tag; }
};

struct NBase {
    uint64_t value = 3;
};

struct NLeft : NBase {
    uint64_t leftValue() const { return NBase::value + 1ULL; }
};

struct NRight : NBase {
    uint64_t rightValue() const { return NBase::value + 2ULL; }
};

struct NDiamond : NLeft, NRight {
    uint64_t combined() const {
        return NLeft::leftValue() + NRight::rightValue() + NLeft::value + NRight::value;
    }
};

uint64_t diamondInheritanceSection() {
    std::cout << "\n-- Diamond inheritance (virtual and non-virtual) --\n";
    uint64_t checksum = 0;

    VDiamond vd;
    VBase* asVBase = &vd;
    uint64_t vResult = asVBase->identify();
    std::cout << "  virtual-inheritance diamond identify() = " << vResult << "\n";
    checksum += vResult;

    NDiamond nd;
    uint64_t nResult = nd.combined();
    std::cout << "  non-virtual diamond combined() = " << nResult << "\n";
    checksum += nResult;

    VBase* viaLeft = static_cast<VLeft*>(&vd);
    VBase* viaRight = static_cast<VRight*>(&vd);
    bool sharedBase = (viaLeft == viaRight);
    std::cout << "  virtual diamond shares one VBase subobject: " << (sharedBase ? "true" : "false") << "\n";
    checksum += sharedBase ? 1000 : 0;

    return checksum;
}

struct IDrawable {
    virtual ~IDrawable() = default;
    virtual uint64_t draw() const = 0;
};

struct ISerializable {
    virtual ~ISerializable() = default;
    virtual uint64_t serialize() const = 0;
};

struct IComparableTag {
    virtual ~IComparableTag() = default;
    virtual uint64_t tagValue() const = 0;
};

struct MultiInterfaceWidget : IDrawable, ISerializable, IComparableTag {
    uint64_t id;
    explicit MultiInterfaceWidget(uint64_t i) : id(i) {}
    uint64_t draw() const override { return id * 2ULL; }
    uint64_t serialize() const override { return id + 1000ULL; }
    uint64_t tagValue() const override { return id % 7ULL; }
};

uint64_t interfaceSection() {
    std::cout << "\n-- Pure virtual interfaces (multiple inheritance of interfaces) --\n";
    uint64_t checksum = 0;

    std::vector<std::unique_ptr<MultiInterfaceWidget>> widgets;
    for (uint64_t i = 1; i <= 6; ++i) {
        widgets.push_back(std::make_unique<MultiInterfaceWidget>(i * 11ULL));
    }

    std::vector<IDrawable*> drawables;
    std::vector<ISerializable*> serializables;
    std::vector<IComparableTag*> taggables;
    for (auto& w : widgets) {
        drawables.push_back(w.get());
        serializables.push_back(w.get());
        taggables.push_back(w.get());
    }

    for (size_t i = 0; i < drawables.size(); ++i) {
        uint64_t d = drawables[i]->draw();
        uint64_t s = serializables[i]->serialize();
        uint64_t t = taggables[i]->tagValue();
        std::cout << "  widget[" << i << "] draw=" << d << " serialize=" << s << " tag=" << t << "\n";
        checksum += d + s + t;
    }

    return checksum;
}

struct Vector3D {
    int64_t x, y, z;
    Vector3D(int64_t xi = 0, int64_t yi = 0, int64_t zi = 0) : x(xi), y(yi), z(zi) {}
    Vector3D operator+(const Vector3D& o) const { return Vector3D(x + o.x, y + o.y, z + o.z); }
    Vector3D operator-(const Vector3D& o) const { return Vector3D(x - o.x, y - o.y, z - o.z); }
    Vector3D operator-() const { return Vector3D(-x, -y, -z); }
    Vector3D& operator+=(const Vector3D& o) { x += o.x; y += o.y; z += o.z; return *this; }
    bool operator==(const Vector3D& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Vector3D& o) const { return !(*this == o); }
    bool operator<(const Vector3D& o) const {
        int64_t magA = x * x + y * y + z * z;
        int64_t magB = o.x * o.x + o.y * o.y + o.z * o.z;
        return magA < magB;
    }
    int64_t dot(const Vector3D& o) const { return x * o.x + y * o.y + z * o.z; }
};

std::ostream& operator<<(std::ostream& os, const Vector3D& v) {
    os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return os;
}

struct ComplexInt {
    int64_t re, im;
    ComplexInt(int64_t r = 0, int64_t i = 0) : re(r), im(i) {}
    ComplexInt operator+(const ComplexInt& o) const { return ComplexInt(re + o.re, im + o.im); }
    ComplexInt operator*(const ComplexInt& o) const {
        return ComplexInt(re * o.re - im * o.im, re * o.im + im * o.re);
    }
    bool operator==(const ComplexInt& o) const { return re == o.re && im == o.im; }
};

std::ostream& operator<<(std::ostream& os, const ComplexInt& c) {
    os << c.re << (c.im >= 0 ? "+" : "") << c.im << "i";
    return os;
}

uint64_t operatorOverloadSection() {
    std::cout << "\n-- Operator overloading (Vector3D, ComplexInt) --\n";
    uint64_t checksum = 0;

    std::vector<Vector3D> vecs = {
        Vector3D(3, -4, 0), Vector3D(1, 1, 1), Vector3D(-10, 2, 2), Vector3D(5, 5, 5)
    };
    std::sort(vecs.begin(), vecs.end());
    for (size_t i = 0; i < vecs.size(); ++i) {
        std::cout << "  sorted[" << i << "] = " << vecs[i] << "\n";
    }

    Vector3D sum(0, 0, 0);
    for (size_t i = 0; i < vecs.size(); ++i) sum += vecs[i];
    std::cout << "  sum = " << sum << "\n";
    checksum += static_cast<uint64_t>(sum.x < 0 ? -sum.x : sum.x);
    checksum += static_cast<uint64_t>(sum.y < 0 ? -sum.y : sum.y);
    checksum += static_cast<uint64_t>(sum.z < 0 ? -sum.z : sum.z);

    Vector3D negated = -vecs[0];
    bool eqCheck = (negated == Vector3D(-vecs[0].x, -vecs[0].y, -vecs[0].z));
    std::cout << "  negation equality check: " << (eqCheck ? "true" : "false") << "\n";
    checksum += eqCheck ? 1 : 0;

    int64_t dotProduct = vecs[0].dot(vecs[1]);
    std::cout << "  dot(vecs[0], vecs[1]) = " << dotProduct << "\n";
    checksum += static_cast<uint64_t>(dotProduct < 0 ? -dotProduct : dotProduct);

    ComplexInt a(3, 4);
    ComplexInt b(-2, 5);
    ComplexInt productAB = a * b;
    ComplexInt sumAB = a + b;
    std::cout << "  a=" << a << " b=" << b << " a*b=" << productAB << " a+b=" << sumAB << "\n";
    checksum += static_cast<uint64_t>(productAB.re < 0 ? -productAB.re : productAB.re);
    checksum += static_cast<uint64_t>(productAB.im < 0 ? -productAB.im : productAB.im);

    return checksum;
}

struct Tracked {
    static uint64_t defaultCtorCount;
    static uint64_t copyCtorCount;
    static uint64_t moveCtorCount;
    static uint64_t copyAssignCount;
    static uint64_t moveAssignCount;
    static uint64_t dtorCount;
    int64_t payload;
    explicit Tracked(int64_t p = 0) : payload(p) { ++defaultCtorCount; }
    Tracked(const Tracked& o) : payload(o.payload) { ++copyCtorCount; }
    Tracked(Tracked&& o) noexcept : payload(o.payload) { o.payload = 0; ++moveCtorCount; }
    Tracked& operator=(const Tracked& o) { payload = o.payload; ++copyAssignCount; return *this; }
    Tracked& operator=(Tracked&& o) noexcept { payload = o.payload; o.payload = 0; ++moveAssignCount; return *this; }
    ~Tracked() { ++dtorCount; }
};

uint64_t Tracked::defaultCtorCount = 0;
uint64_t Tracked::copyCtorCount = 0;
uint64_t Tracked::moveCtorCount = 0;
uint64_t Tracked::copyAssignCount = 0;
uint64_t Tracked::moveAssignCount = 0;
uint64_t Tracked::dtorCount = 0;

uint64_t ruleOfFiveSection() {
    std::cout << "\n-- Rule of five (copy/move construction and assignment) --\n";
    uint64_t checksum = 0;

    std::vector<Tracked> pool;
    pool.reserve(8);
    for (int64_t i = 0; i < 5; ++i) pool.emplace_back(i * 3 + 1);

    Tracked a(42);
    Tracked b = a;
    Tracked c = std::move(a);
    Tracked d;
    d = b;
    Tracked e;
    e = std::move(b);
    pool.push_back(d);
    pool.push_back(std::move(e));

    std::cout << "  defaultCtor=" << Tracked::defaultCtorCount
              << " copyCtor=" << Tracked::copyCtorCount
              << " moveCtor=" << Tracked::moveCtorCount
              << " copyAssign=" << Tracked::copyAssignCount
              << " moveAssign=" << Tracked::moveAssignCount << "\n";

    checksum += Tracked::defaultCtorCount;
    checksum += Tracked::copyCtorCount * 2ULL;
    checksum += Tracked::moveCtorCount * 3ULL;
    checksum += Tracked::copyAssignCount * 5ULL;
    checksum += Tracked::moveAssignCount * 7ULL;

    uint64_t poolSum = 0;
    for (size_t i = 0; i < pool.size(); ++i) {
        int64_t v = pool[i].payload;
        poolSum += static_cast<uint64_t>(v < 0 ? -v : v);
    }
    std::cout << "  pool payload sum = " << poolSum << "\n";
    checksum += poolSum;

    return checksum;
}

template <typename Derived>
struct CrtpBase {
    uint64_t compute() const {
        return static_cast<const Derived*>(this)->computeImpl();
    }
};

struct CrtpAdd : CrtpBase<CrtpAdd> {
    uint64_t a, b;
    CrtpAdd(uint64_t x, uint64_t y) : a(x), b(y) {}
    uint64_t computeImpl() const { return a + b; }
};

struct CrtpMul : CrtpBase<CrtpMul> {
    uint64_t a, b;
    CrtpMul(uint64_t x, uint64_t y) : a(x), b(y) {}
    uint64_t computeImpl() const { return a * b; }
};

struct CrtpMax : CrtpBase<CrtpMax> {
    uint64_t a, b;
    CrtpMax(uint64_t x, uint64_t y) : a(x), b(y) {}
    uint64_t computeImpl() const { return a > b ? a : b; }
};

template <typename T>
uint64_t invokeCrtp(const CrtpBase<T>& obj) { return obj.compute(); }

uint64_t crtpStaticPolymorphismSection() {
    std::cout << "\n-- CRTP static polymorphism --\n";
    uint64_t checksum = 0;

    CrtpAdd addObj(11, 22);
    CrtpMul mulObj(6, 7);
    CrtpMax maxObj(50, 19);

    uint64_t r1 = invokeCrtp(addObj);
    uint64_t r2 = invokeCrtp(mulObj);
    uint64_t r3 = invokeCrtp(maxObj);

    std::cout << "  add=" << r1 << " mul=" << r2 << " max=" << r3 << "\n";
    checksum += r1 + r2 + r3;

    return checksum;
}

template <typename T>
struct Box {
    T value;
    explicit Box(T v) : value(v) {}
    virtual ~Box() = default;
    virtual uint64_t weigh() const { return static_cast<uint64_t>(sizeof(T)); }
};

struct HeavyIntBox : Box<int64_t> {
    explicit HeavyIntBox(int64_t v) : Box<int64_t>(v) {}
    uint64_t weigh() const override {
        int64_t absValue = value < 0 ? -value : value;
        return Box<int64_t>::weigh() * 10ULL + static_cast<uint64_t>(absValue % 1000);
    }
};

struct LightCharBox : Box<char> {
    explicit LightCharBox(char v) : Box<char>(v) {}
    uint64_t weigh() const override {
        return Box<char>::weigh() + static_cast<uint64_t>(static_cast<unsigned char>(value));
    }
};

uint64_t templatesInheritanceSection() {
    std::cout << "\n-- Template class inheritance --\n";
    uint64_t checksum = 0;

    HeavyIntBox heavy(123456);
    LightCharBox light('Q');

    Box<int64_t>* asHeavyBase = &heavy;
    Box<char>* asLightBase = &light;

    uint64_t hw = asHeavyBase->weigh();
    uint64_t lw = asLightBase->weigh();
    std::cout << "  heavy box weigh() = " << hw << "\n";
    std::cout << "  light box weigh() = " << lw << "\n";
    checksum += hw + lw;

    return checksum;
}

class Outer {
public:
    class Inner {
    public:
        uint64_t compute(const Outer& o) const;
    };
    explicit Outer(uint64_t s) : secret(s) {}
private:
    uint64_t secret;
    friend class OuterFriend;
};

uint64_t Outer::Inner::compute(const Outer& o) const {
    return o.secret * 2ULL;
}

class OuterFriend {
public:
    static uint64_t peek(const Outer& o) { return o.secret + 1ULL; }
};

uint64_t nestedClassesFriendSection() {
    std::cout << "\n-- Nested classes and friend access --\n";
    uint64_t checksum = 0;

    Outer outer(21);
    Outer::Inner inner;

    uint64_t viaInner = inner.compute(outer);
    std::cout << "  inner.compute(outer) = " << viaInner << "\n";
    checksum += viaInner;

    uint64_t viaFriend = OuterFriend::peek(outer);
    std::cout << "  OuterFriend::peek(outer) = " << viaFriend << "\n";
    checksum += viaFriend;

    return checksum;
}

struct Base {
    static uint64_t destructionCount;
    virtual ~Base() { ++destructionCount; }
    virtual uint64_t kind() const = 0;
};

uint64_t Base::destructionCount = 0;

struct DerivedA : Base { uint64_t kind() const override { return 1; } };
struct DerivedB : Base { uint64_t kind() const override { return 2; } };
struct DerivedC : DerivedA { uint64_t kind() const override { return 3; } };

struct CloneBase {
    virtual ~CloneBase() = default;
    virtual CloneBase* clone() const = 0;
    virtual uint64_t value() const = 0;
};

struct CloneDerived : CloneBase {
    uint64_t v;
    explicit CloneDerived(uint64_t val) : v(val) {}
    CloneDerived* clone() const override { return new CloneDerived(v + 1ULL); }
    uint64_t value() const override { return v; }
};

uint64_t abstractPolymorphicDestructionSection() {
    std::cout << "\n-- Abstract base destruction, dynamic_cast, typeid, covariant clone --\n";
    uint64_t checksum = 0;

    std::vector<std::unique_ptr<Base>> objs;
    objs.push_back(std::make_unique<DerivedA>());
    objs.push_back(std::make_unique<DerivedB>());
    objs.push_back(std::make_unique<DerivedC>());

    for (const auto& o : objs) {
        checksum += o->kind();
    }

    DerivedC* asDerivedC = dynamic_cast<DerivedC*>(objs[2].get());
    bool castOk = (asDerivedC != nullptr);
    std::cout << "  dynamic_cast to DerivedC succeeded: " << (castOk ? "true" : "false") << "\n";
    checksum += castOk ? 10 : 0;

    DerivedB* wrongCast = dynamic_cast<DerivedB*>(objs[0].get());
    bool castShouldFail = (wrongCast == nullptr);
    std::cout << "  dynamic_cast to unrelated type failed as expected: " << (castShouldFail ? "true" : "false") << "\n";
    checksum += castShouldFail ? 20 : 0;

    bool sameType = (typeid(*objs[0]) == typeid(DerivedA));
    std::cout << "  typeid(*objs[0]) == typeid(DerivedA): " << (sameType ? "true" : "false") << "\n";
    checksum += sameType ? 30 : 0;

    objs.clear();
    std::cout << "  destructionCount after clear = " << Base::destructionCount << "\n";
    checksum += Base::destructionCount * 100ULL;

    std::unique_ptr<CloneBase> original(new CloneDerived(500));
    std::unique_ptr<CloneBase> cloned(original->clone());
    std::cout << "  original.value() = " << original->value() << " cloned.value() = " << cloned->value() << "\n";
    checksum += original->value() + cloned->value();

    return checksum;
}

class Counter {
public:
    static Counter& instance() {
        static Counter inst;
        return inst;
    }
    uint64_t next() { return ++value; }
    uint64_t current() const { return value; }
private:
    Counter() : value(0) {}
    uint64_t value;
};

uint64_t singletonStaticMembersSection() {
    std::cout << "\n-- Singleton via function-local static --\n";
    uint64_t checksum = 0;

    for (int i = 0; i < 5; ++i) {
        uint64_t v = Counter::instance().next();
        std::cout << "  Counter::instance().next() = " << v << "\n";
        checksum += v;
    }

    std::cout << "  Counter::instance().current() = " << Counter::instance().current() << "\n";
    checksum += Counter::instance().current();

    return checksum;
}

struct Growable {
    virtual ~Growable() = default;
    virtual uint64_t grow() const { return 1; }
};

struct Sapling : Growable {
    uint64_t grow() const override { return Growable::grow() + 5ULL; }
};

struct Tree final : Sapling {
    uint64_t grow() const override final { return Sapling::grow() + 50ULL; }
};

uint64_t finalOverrideSection() {
    std::cout << "\n-- final classes and final virtual methods --\n";
    uint64_t checksum = 0;

    Growable plain;
    Sapling sapling;
    Tree tree;
    Growable* forest[3] = { &plain, &sapling, &tree };

    for (int i = 0; i < 3; ++i) {
        uint64_t g = forest[i]->grow();
        std::cout << "  forest[" << i << "]->grow() = " << g << "\n";
        checksum += g;
    }

    return checksum;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();

    std::cout << "=== Class / Inheritance / Polymorphism Stress Test ===\n\n";

    uint64_t checksum = 0;
    checksum += smallHierarchySection();
    checksum += bigChainHierarchySection();
    checksum += wideHierarchySection();
    checksum += multipleInheritanceSection();
    checksum += diamondInheritanceSection();
    checksum += interfaceSection();
    checksum += operatorOverloadSection();
    checksum += ruleOfFiveSection();
    checksum += crtpStaticPolymorphismSection();
    checksum += templatesInheritanceSection();
    checksum += nestedClassesFriendSection();
    checksum += abstractPolymorphicDestructionSection();
    checksum += singletonStaticMembersSection();
    checksum += finalOverrideSection();

    std::cout << "\n=== Final checksum ===\n";
    std::cout << "TOTAL_CHECKSUM: " << checksum << "\n";

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << diff << "\n";

    return 0;
}