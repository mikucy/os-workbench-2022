#define _GNU_SOURCE
#include <common.h>
#include <stdio.h>
#include <sched.h>
#include <stdatomic.h>
#include <thread.h>

int single;

int cpu_current() {
    if (single == 1) {
        return 0;
    }
    return sched_getcpu() % 8;
}

int atomic_xchg (int *addr, int newval) {
    return atomic_exchange((int *)addr, newval);
}

void test_single_alloc_and_free() {
    void* p = pmm->alloc(16 << 20);
    assert(p == NULL);

    p = pmm->alloc(32);
    assert(p && (size_t)p % 32 == 0);
    void* p_next = pmm->alloc(32);
    assert(p && (size_t)p_next - (size_t)p == 32);
    pmm->free(p);
    pmm->free(p_next);
    p = pmm->alloc(33);
    assert(p && (size_t)p % 64 == 0);
    pmm->free(p);
    p = pmm->alloc(4096);
    assert(p && (size_t)p % 4096 == 0);
    pmm->free(p);
    printf("Single alloc passed\n");
}

void test_alloc_full_page() {
    int page_size = 64 * 1024;
    for (int i = 0; i < 9; ++i) {
        int block_size = 32 << i;
        int count = page_size / block_size;
        void* p, *p_first, *p_last, *p_next_page;
        for (int j = 0; j < count; ++j) {
            p = pmm->alloc(block_size);
            if (j == 0) {
                p_first = p;
            }
            if (j == count - 2) {
                p_last = p;
            }
            if (j == count - 1) {
                p_next_page = p;
            }
        }
        assert(p_next_page - p_first == page_size * 8 * 9);
        pmm->free(p_first);
        void* p_new = pmm->alloc(block_size);
        assert(p_new == p_first);
        pmm->free(p_last);
        p_new = pmm->alloc(block_size);
        assert(p_new == p_last);
    }
    printf("Full page passed\n");
}

void test_alloc_huge_memory_and_free() {
    void* p1 = pmm->alloc(8193);
    assert(p1 && (size_t)p1 % 16384 == 0);
    void* p2 = pmm->alloc(8193);
    assert(p2 && (size_t)p2 == (size_t)p1 + 16384);
    void* p3 = pmm->alloc(16384);
    assert(p3 && (size_t)p3 == (size_t)p2 + 16384);
    pmm->free(p2);
    void* p4 = pmm->alloc(8193);
    assert(p4);
    assert(p4 == p2);
}

void test_single_thread() {
    single = 1;
    test_single_alloc_and_free();
    test_alloc_full_page();
    test_alloc_huge_memory_and_free();
    printf("Single thread passed\n");
}

void alloc_task() {
    void* p = pmm->alloc(32);
    assert(p);
    pmm->free(p);
}

void alloc_huge_task() {
    void* p = pmm->alloc(8193);
    assert(p);
    pmm->free(p);
}

void test_multi_alloc_and_free() {
    for (int i = 0; i < 8; ++i) {
        create(alloc_task);
    }
}

void test_multi_alloc_huge_memory_and_free() {
   for (int i = 0; i < 8; ++i) {
        create(alloc_huge_task);
    } 
}

void test_multi_thread() {
    single = 0;
    test_multi_alloc_and_free();
    test_multi_alloc_huge_memory_and_free();
    printf("Multi thread passed\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) exit(1);
    pmm->init();
    printf("Make test start with %s\n", argv[1]);
    switch (atoi(argv[1])) {
        case 0:
            test_single_thread();
            break;
        case 1:
            test_multi_thread();
            break;
    }
}