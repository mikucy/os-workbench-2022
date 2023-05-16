#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>


#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

const char digits[] = "0123456789abcdef";

static int print_number(int number, int sign, int radix, char* out) {
  char buf[32];
  int pos = 0;
  if (sign) {
    number = -number;
  }

  do {
      buf[pos++] = digits[number % radix];
      number /= radix;
  } while (number > 0);
  if (sign) {
    buf[pos++] = '-';
  }
  for (int i = 0; i < pos; ++i) {
    out[i] = buf[pos - i - 1];
  }
  return pos;
}

int printf(const char *fmt, ...) {
  assert(fmt);
  int res = 0;
  va_list arg_list;
  va_start(arg_list, fmt);

  while (*fmt) {
    if (*fmt != '%') {
      putch(*fmt);
      ++res;
    } else {
      if (fmt + 1 == NULL) {
        panic("Invalid string!");
        return -1;
      }
      ++fmt;
      switch (*fmt) {
        case 'd': {
          int d = va_arg(arg_list, int);
          char s[32];
          int l = print_number(d, d > 0 ? 0 : 1, 10, s);
          for (int i = 0; i < l; ++i) {
            putch(s[i]);
          }
          res += l;
          break;
        }
        case 'c': {
          char c = va_arg(arg_list, int);
          putch(c);
          ++res;
          break;
        }
        case 's': {
          const char* s = va_arg(arg_list, const char*);
          for (const char *p = s; *p; p++) {
            putch(*p);
            ++res;
          }
          break;
        }
        case 'p': {
          putch('0');
          putch('x');
          res += 2;
          int p = va_arg(arg_list, int);
          char s[32];
          int l = print_number(p, 0, 16, s);
          for (int i = 0; i < l; ++i) {
            putch(s[i]);
          }
          res += l;
          break;
        }
        case '%': {
          putch('%');
          ++res;
          break;
        }
        default:
          break;
      }
    }
    ++fmt;
  }
  va_end(arg_list);
  return res;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  panic("Not implemented");
}

int sprintf(char *out, const char *fmt, ...) {
  panic("Not implemented");
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
