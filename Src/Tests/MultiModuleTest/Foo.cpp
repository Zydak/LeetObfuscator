#include "Foo.h"

#include <iostream>

__attribute__((noinline))
int Foo(int x, int y)
{
    std::cout << "Hello" << std::endl;
    return (x^y) * 10 / (x^x) + 5;
}