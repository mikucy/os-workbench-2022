#include <game.h>

#define SIDE 16
#define WHITE 0xffffff
extern int w, h;
static int cur_x = 0, cur_y = 0;

static void init() {
  AM_GPU_CONFIG_T info = {0};
  ioe_read(AM_GPU_CONFIG, &info);
  w = info.width;
  h = info.height;
}

int min(int a, int b) {
  return a < b ? a : b;
}

int max(int a, int b) {
  return a < b ? b : a;
}

// static void draw_tile(int x, int y, int w, int h, uint32_t color) {
//   uint32_t pixels[w * h]; // WARNING: large stack-allocated memory
//   AM_GPU_FBDRAW_T event = {
//     .x = x, .y = y, .w = w, .h = h, .sync = 1,
//     .pixels = pixels,
//   };
//   for (int i = 0; i < w * h; i++) {
//     pixels[i] = color;
//   }
//   ioe_write(AM_GPU_FBDRAW, &event);
// }

static uint32_t sqrt(uint32_t n) {
  uint32_t i = 0;
  while (i * i <= n) {
    ++i;
  }
  return i - 1; 
}

void draw_ball(int x, int y, int r, uint32_t color) {
  int draw_x = min(x, cur_x), draw_y = min(y, cur_y);
  int draw_w = max(x, cur_x) + 2 * r - draw_x;
  int draw_h = max(y, cur_y) + 2 * r - draw_y;

  uint32_t pixels[draw_w * draw_h];
  AM_GPU_FBDRAW_T event = {
    .x = draw_x, .y = draw_y, .w = draw_w, .h = draw_h, .sync = 1,
    .pixels = pixels,
  };
  for (int i = 0; i < draw_w * draw_h; ++i) pixels[i] = 0;
  int base_y = y - draw_y, base_x = x - draw_x;
  for (int i = base_y; i < base_y + 2 * r; ++i) {
    int j1 = base_x + r - sqrt(2 * (i - base_y) * r - (i - base_y) * (i - base_y));
    int j2 = base_x + r + sqrt(2 * (i - base_y) * r - (i - base_y) * (i - base_y));
    for (int j = j1 + 1; j < j2 - 1; ++j) {
      pixels[i * draw_w + j] = color;
    }
  }
  cur_x = x;
  cur_y = y;
  ioe_write(AM_GPU_FBDRAW, &event);
}

void splash() {
  init();
  // for (int x = 0; x * SIDE <= w; x ++) {
  //   for (int y = 0; y * SIDE <= h; y++) {
  //     if ((x & 1) ^ (y & 1)) {
  //       draw_tile(x * SIDE, y * SIDE, SIDE, SIDE, 0xffffff); // white
  //     }
  //   }
  // }
}
