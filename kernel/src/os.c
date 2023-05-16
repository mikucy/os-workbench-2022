#include <common.h>
#include <klib-tests.h>

static void os_init() {
  pmm->init();
}

static void os_run() {
  klib_tests();
  for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
    putch(*s == '*' ? '0' + cpu_current() : *s);
  }
  while (1) ;
}

MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
};
