#include "Common.h"
#include "../../Leet.h"
#include <iostream>
#include <new>

uint64_t g_lifetimeTrackerDestructions = 0;

struct LifetimeTracker {
    uint64_t id;
    explicit LifetimeTracker(uint64_t i) : id(i) {}
    ~LifetimeTracker() { ++g_lifetimeTrackerDestructions; }
    uint64_t identify() const { return id; }
};

class OptionalFlag {
public:
    explicit OptionalFlag(bool v) : flag_(v) {}
    explicit operator bool() const { return flag_; }
private:
    bool flag_;
};

union ManualUnion {
    LifetimeTracker tracker;
    uint64_t plainValue;
    ManualUnion() : plainValue(0) {}
    ~ManualUnion() {}
};

uint64_t betaManualLifetimeSection() {
    std::cout << "\n-- [Beta] Manual object lifetime (placement new, explicit dtor, std::launder) --\n";
    uint64_t checksum = 0;

    alignas(LifetimeTracker) unsigned char storage[sizeof(LifetimeTracker)];
    void* rawSlot = static_cast<void*>(storage);

    LifetimeTracker* obj1 = ::new (rawSlot) LifetimeTracker(111);
    uint64_t id1 = obj1->identify();
    std::cout << "  obj1->identify() = " << id1 << "\n";
    checksum += id1;

    obj1->~LifetimeTracker();

    LifetimeTracker* obj2raw = ::new (rawSlot) LifetimeTracker(222);
    (void)obj2raw;
    LifetimeTracker* obj2 = std::launder(static_cast<LifetimeTracker*>(rawSlot));
    uint64_t id2 = obj2->identify();
    std::cout << "  obj2->identify() (via laundered reused-storage pointer) = " << id2 << "\n";
    checksum += id2;

    obj2->~LifetimeTracker();

    std::cout << "  total LifetimeTracker destructions so far = " << g_lifetimeTrackerDestructions << "\n";
    checksum += g_lifetimeTrackerDestructions * 1000ULL;

    OptionalFlag armedFlag(true);
    OptionalFlag disarmedFlag(false);
    uint64_t flagScore = 0;
    if (armedFlag) flagScore += 10ULL;
    if (disarmedFlag) flagScore += 20ULL;
    bool explicitBool = static_cast<bool>(armedFlag);
    std::cout << "  contextual explicit-operator-bool score = " << flagScore
              << " explicit static_cast<bool> = " << (explicitBool ? "true" : "false") << "\n";
    checksum += flagScore + (explicitBool ? 1ULL : 0ULL);

    return checksum;
}

uint64_t betaUnionLifetimeSection() {
    std::cout << "\n-- [Beta] Union with a non-trivial member (manual active-member lifetime) --\n";
    uint64_t checksum = 0;

    ManualUnion u;
    u.plainValue = 999;
    uint64_t plainRead = u.plainValue;
    std::cout << "  union as plainValue = " << plainRead << "\n";
    checksum += plainRead;

    ::new (static_cast<void*>(&u.tracker)) LifetimeTracker(777);
    uint64_t trackedId = u.tracker.identify();
    std::cout << "  union as LifetimeTracker (after placement new) = " << trackedId << "\n";
    checksum += trackedId;

    u.tracker.~LifetimeTracker();
    u.plainValue = 555;
    std::cout << "  union reset back to plainValue = " << u.plainValue << "\n";
    checksum += u.plainValue;

    std::cout << "  total LifetimeTracker destructions so far = " << g_lifetimeTrackerDestructions << "\n";
    checksum += g_lifetimeTrackerDestructions * 1000ULL;

    return checksum;
}