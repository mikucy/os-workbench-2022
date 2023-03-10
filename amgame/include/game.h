#include <am.h>
#include <amdev.h>
#include <klib.h>
#include <klib-macros.h>

int w, h;
void splash();
int readkey();
void draw_ball();
static inline void puts(const char *s) {
  for (; *s; s++) putch(*s);
}
