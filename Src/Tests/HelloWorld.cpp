#include <iostream>
#include <random>
#include <time.h>

#define LEET_IMPLEMENTATION
#include "../Leet.h"

__attribute__((noinline))
int Foo(int x, int y)
{
    printf("HelloWorld\n");
    printf("orld\n");
    printf("ld\n");
    printf("\n");
    printf("1d\n");
    return (x^y) * 10;
}

int main()
{
    //srand(time(nullptr));
    std::cout << Foo(rand(), rand()) << std::endl;
}
