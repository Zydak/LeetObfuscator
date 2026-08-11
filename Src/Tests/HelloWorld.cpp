#include <iostream>
#include <random>
#include <time.h>

#define LEET_IMPLEMENTATION
#include "../Leet.h"

__attribute__((noinline))
int Foo(int x, int y)
{
    return (x^y) * 10;
}

int main()
{
    srand(time(nullptr));
    std::cout << Foo(rand(), rand()) << std::endl;
}
