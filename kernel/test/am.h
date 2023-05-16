#include <stddef.h>
#include <stdint.h>

int cpu_current();
int atomic_xchg (int *addr, int newval);