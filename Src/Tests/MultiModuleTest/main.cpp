#include <iostream>
#include "Foo.h"

#include "../../Leet.h"

__attribute__((noinline))
void Foo1(int x, int y)
{
    std::cout << "X; " << x << "X^Y: " << (x^y) << std::endl;
}

int main()
{
    srand(time(nullptr));
    int x = Foo(rand(), rand());
    Foo1(rand(), rand());
    std::cout << "Result: " << x << std::endl;
}