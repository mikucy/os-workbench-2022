#include <game.h>

#define FPS 60
#define R 16
#define BALL_COLOR 0x00ff00

bool game_started = false;

uint32_t uptime() {
  return io_read(AM_TIMER_UPTIME).us / 1000;
}

int vx = 0, vy = 0, x = 0, y = 0;

void startgame() {
  game_started = true;
  vx = FPS;
  vy = FPS;
}

void kbd_event(int key) {
  switch (key) {
    case AM_KEY_ESCAPE:
      halt(0);
      break;
    case AM_KEY_A:
      vx -= FPS;
      break;
    case AM_KEY_D:
      vx += FPS;
      break;
    case AM_KEY_W:
      vy -= FPS;
      break;
    case AM_KEY_S:
      vy += FPS;
      break;
    default: {
      if (!game_started) {
        startgame();
      }
      break;
    }
  }
}

void game_process() {
  if (vx == 0 && vy == 0) {
    return;
  }
  x += vx / FPS;
  y += vy / FPS;
  if (x <= 0 || x >= w - 2 * R) {
    vx = -vx;
  }
  if (y <= 0 || y >= h - 2 * R) {
    vy = -vy;
  }
}

void screen_update() {
  draw_ball(x, y, R, BALL_COLOR);
}

// Operating system is a C program!
int main(const char *args) {
  ioe_init();

  puts("Welcome to the ball game!\n");
  puts("Press any key to start.\n");

  uint32_t next_frame = 0;
  int key;

  splash();

  while (1) {
    while (uptime() < next_frame) ;
    while ((key = readkey()) != AM_KEY_NONE) {
      kbd_event(key);
    }
    game_process();
    screen_update();
    next_frame += 1000 / FPS;
  }
  return 0;
}
