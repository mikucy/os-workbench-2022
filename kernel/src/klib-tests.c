#include <klib.h>

static void printf_tests() {
    int a = 80;
    int b = -123;
    char c = 'c';
    assert(printf("test\n") == 5);
    assert(printf("test %d\n", a) == 8);
    assert(printf("test %d\n", b) == 10);
    assert(printf("test %c\n", c) == 7);
    assert(printf("test %%\n") == 7);
    assert(printf("test %s\n", "cy") == 8);
    printf("test %p\n", &a);
}

void klib_tests() {
    printf_tests();
}