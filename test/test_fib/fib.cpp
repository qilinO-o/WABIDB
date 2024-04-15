#include <emscripten.h>

extern "C" {
    int fib(int x);
}

EMSCRIPTEN_KEEPALIVE
int fib(int x) {
    if (x == 1 || x == 2) return 1;
    int a1 = fib(x - 1);
    int a2 = fib(x - 2);
    return a1 + a2;
}