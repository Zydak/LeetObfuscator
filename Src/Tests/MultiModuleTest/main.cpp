#include <iostream>
#include "Foo.h"
#include <chrono>

#define LEET_IMPLEMENTATION
#include "../../Leet.h"

__attribute__((noinline))
void Foo1(int x, int y)
{
    std::cout << "X; " << x << "X^Y: " << (x^y) << std::endl;
}

int main()
{
    auto start = std::chrono::high_resolution_clock::now();

    int x = Foo(rand(), rand());
    Foo1(rand(), rand());
    std::printf("CHECKSUM: 0x%016llx\n", (unsigned long long)x);

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("%ld\n", diff);
    return 0;
}