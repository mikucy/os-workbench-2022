#include "co.h"
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdint.h>
#include <assert.h>

#define STACK_SIZE 32 / sizeof(uint8_t) * 1024
#define CO_SIZE 128

enum co_status {
  CO_NEW = 1, // 新创建，还未执行过
  CO_RUNNING, // 已经执行过
  CO_WAITING, // 在 co_wait 上等待
  CO_DEAD,    // 已经结束，但还未释放资源
};

struct co {
  char name[30];
  void (*func)(void *); // co_start 指定的入口地址和参数
  void *arg;

  enum co_status status;  // 协程的状态
  struct co *    waiter;  // 是否有其他协程在等待当前协程
  jmp_buf        context; // 寄存器现场 (setjmp.h)
  uint8_t        stack[STACK_SIZE]__attribute__((aligned(16))); // 协程的堆栈
};

struct co *current; // 当前运行的协程
struct co *co_list[CO_SIZE]; // 所有协程的数组
int co_num; // 当前协程数

// 随机挑选出一个协程
struct co *get_next_co() {
  int count = 0;
  for (int i = 0; i < co_num; ++i) {
    assert(co_list[i]);
    if (co_list[i]->status == CO_NEW || co_list[i]->status == CO_RUNNING) {
      ++count;
    }
  }

  int id = rand() % count, i = 0;
  for (i = 0; i < co_num; ++i) {
    if (co_list[i]->status == CO_NEW || co_list[i]->status == CO_RUNNING) {
      if (id == 0) {
        break;
      }
      --id;
    }
  }
  return co_list[i];
}

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
  struct co* res = (struct co*)malloc(sizeof(struct co));
  strcpy(res->name, name);
  res->func = func;
  res->arg = arg;
  res->status = CO_NEW;
  res->waiter = NULL;
  assert(co_num < CO_SIZE);
  co_list[co_num++] = res;

  return res;
}

void co_wait(struct co *co) {
  assert(co != NULL);
  co->waiter = current;
  current->status = CO_WAITING;
  while (co->status != CO_DEAD) {
    co_yield();
  }
  free(co);
  int id = 0;
  for (id = 0; id < co_num; ++id) {
    if (co_list[id] == co) {
      break;
    }
  }
  while (id < co_num - 1) {
    co_list[id] = co_list[id+1];
    ++id;
  }
  --co_num;
  co_list[co_num] = NULL;
}

void co_yield() {
  int val = setjmp(current->context);
  if (val == 0) {
    struct co *next = get_next_co();
    current = next;
    if (next->status == CO_NEW) {
      next->status = CO_RUNNING;
      asm volatile(
      #if __x86_64__
                "movq %%rdi, (%0); movq %0, %%rsp; movq %2, %%rdi; call *%1"
                :
                : "b"((uintptr_t)(next->stack + sizeof(next->stack))), "d"(next->func), "a"((uintptr_t)(next->arg))
                : "memory"
      #else
                "movl %%esp, 0x8(%0); movl %%ecx, 0x4(%0); movl %0, %%esp; movl %2, (%0); call *%1"
                :
                : "b"((uintptr_t)(next->stack + sizeof(next->stack) - 8)), "d"(next->func), "a"((uintptr_t)(next->arg))
                : "memory" 
      #endif
      );

      asm volatile(
      #if __x86_64__
                "movq (%0), %%rdi"
                :
                : "b"((uintptr_t)(next->stack + sizeof(next->stack)))
                : "memory"
      #else
                "movl 0x8(%0), %%esp; movl 0x4(%0), %%ecx"
                :
                : "b"((uintptr_t)(next->stack + sizeof(next->stack) - 8))
                : "memory"
      #endif
      );

      next->status = CO_DEAD;

      if (current->waiter) {
        current = current->waiter;
        longjmp(current->context, 1);
      }
      co_yield();
    } else if (next->status == CO_RUNNING) {
      longjmp(next->context, 1);
    } else {
      assert(0);
    }
  } else {
    // longjmp返回，不处理
  }
}

__attribute__((constructor)) void init() {
  struct co* main = (struct co*)malloc(sizeof(struct co));
  strcpy(main->name, "main");
  main->status = CO_RUNNING;
  main->waiter = NULL;
  current = main;
  co_num = 1;
  memset(co_list, 0, sizeof(co_list));
  co_list[0] = main;
}
