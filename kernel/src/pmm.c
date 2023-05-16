#include <common.h>
#ifdef TEST
#include <stdio.h>
#include <stdlib.h>
int HEAP_SIZE = 125 << 20;
#endif

#define CPU_NUM_MAX 8
#define MIN_BLOCK_SIZE 32
#define PAGES_PER_CPU 9 // 32 ~ 8192
#define MAX_BLOCK_SIZE 8192
#define PAGES_SIZE (64 * 1024)
#define MAX_KALLOC_SIZE (16 << 20)
#define MAGIC 132543
#define LOCKED 1
#define UNLOCKED 0

/*
Layout Overview

========== Address A ========== assert(A == heap.start)
Space for |pages_total|.
8 pointers to every CPU's page header array. 8 * 4 = 32Byte.
========== Address B ========== assert(B == *A)
Space for the PageInfo array |pages_total| points to.
For every CPU:
9 pointers to specific CPU's page headers of different block sizes.
16, 32, 64, 128, 256, 512, 1024, 2048, 4096
0,  1,  2,  3,   4,   5,   6,    7,    8
9 * 8 * 4 = 288Byte.
========== Address C ========== assert(C == Address A + PAGE_SIZE)
Space for initial pages. One page is reserved for single CPU's single size.
For every CPU:
9 pages of different block sizes. Every page has a PageHeader.
9 * 8 * 64KB = 4608KB.
For alignment simplicity, the pages are aligned by PAGE_SIZE.
========== Address D ========== assert(D == C + PAGE_SIZE * 9 * 8) 
Space for allocated expanded pages. When a page of some block size is full and that size
is requested again, a new page is allocated and marked as current page's next.
========== Address E ========== assert(E == pages_bottom && E - heap.start <= heapsize / 2)
Free space for pages to expand. For simplicity this space shouldn't exceed the middle of heap.
========== Address F ========== 
assert(F == nonpages_top)
Space for large memory allocations. Use NodeHeader to track all free memory blocks.
*/

// The list node struct in every page.
typedef struct Block {
  struct Block* next;
} Block;

// The header struct at beginning of every page.
typedef struct PageHeader {
  // The pointer to next page of the same block size.
  struct PageHeader* next;
  // The pointer to **the first** empty block in current page.
  // Initially it is the second block in every page, since the header occupies the first block.
  // When the page is full it is marked as NULL.
  Block* current_block;
  int lock;
} PageHeader;

// A helper struct to store all the pages headers pointer.
typedef struct PageInfo {
  PageHeader* headers[PAGES_PER_CPU];
} PageInfo;

// Pointer to PageInfo array.
PageInfo** pages_total;

// Struct for free node in huge allocation.
typedef struct NodeHeader {
  size_t size;
  struct NodeHeader* next;
} NodeHeader;

// Struct for allocated node in huge allocation.
typedef struct NodeInfo {
  size_t size;
  int magic;
  struct NodeHeader* header;
} NodeInfo;

// A global address which marks the bottom of pages. When a new page has to be allocated, it increases
// by PAGE_SIZE (But won't decrease when a page is empty).
uintptr_t pages_bottom;

// A global address which marks the top of nonpages.
NodeHeader* header_start;

int non_page_lock = UNLOCKED;

static void init_pages_total() {
  pages_total = (PageInfo**)heap.start;
  pages_bottom = (uintptr_t)heap.start + CPU_NUM_MAX * sizeof(PageInfo*);
#ifdef VERBOSE
  printf("Init pages_total, pages_total is %p, pages_bottom is %p\n", pages_total, (char*)pages_bottom);
#endif
}

static void init_pages_info() {
  for (int i = 0; i < CPU_NUM_MAX; ++i) {
    pages_total[i] = (PageInfo*)pages_bottom;
    pages_bottom += sizeof(PageInfo);
#ifdef VERBOSE
    printf("Set pages_total[%d] to %p\n", i, pages_total[i]);
#endif
  while (pages_bottom % PAGES_SIZE != 0) {
    ++pages_bottom;
  }
#ifdef VERBOSE
  printf("Align pages_bottom to %p\n", (char*)pages_bottom);
#endif
  }
}

static void init_nonpages_top() {
  header_start = (NodeHeader*)((uintptr_t)heap.start + ((uintptr_t)heap.end - (uintptr_t)heap.start) / 2);
  header_start->next = NULL;
  header_start->size = ((uintptr_t)heap.end - (uintptr_t)heap.start) / 2 - sizeof(NodeHeader);
}

// Alloc a new page starting from |pages_bottom| for block size of 32 << |index|.
static PageHeader* alloc_new_page_for_index(int index) {
  PageHeader* header = (PageHeader*)pages_bottom;
  header->next = NULL;
  header->lock = UNLOCKED;
  size_t block_size = MIN_BLOCK_SIZE << index;
  header->current_block = (Block*)(pages_bottom + block_size);
  Block* p = header->current_block; 
  for (uintptr_t b = (uintptr_t)p; b < pages_bottom + PAGES_SIZE - block_size; b += block_size) {
    p->next = (Block*)(b + block_size);
    p = p->next;
  }
  p->next = NULL;
  pages_bottom += PAGES_SIZE;
  return header;
}

static void init_pages() {
  for (int i = 0; i < CPU_NUM_MAX; ++i) {
    #ifdef VERBOSE
    printf("pages_total[%d]'s address is %p\n", i, &pages_total[i]);
    printf("pages_total[%d]'s header's address is %p\n", i, &pages_total[i]->headers);
    #endif
    for (int j = 0; j < PAGES_PER_CPU; ++j) {
      pages_total[i]->headers[j] = alloc_new_page_for_index(j);
    }
  }
}

static void *kalloc(size_t size) {
  if (size >= MAX_KALLOC_SIZE) {
    return NULL;
  }
  if (size <= MAX_BLOCK_SIZE) {
    // Fast path.
    int cpu = cpu_current();
    assert(cpu < CPU_NUM_MAX);
    PageInfo* info = pages_total[cpu];
    int index = 0;
    while ((MIN_BLOCK_SIZE << index) < size) {
      ++index;
    }
    assert(index < PAGES_PER_CPU);
    PageHeader* header = info->headers[index], *prev = NULL;
    assert(header);
    // Find first header whose current block is not NULL. i.e. It has space to allocate.
    while (header && header->current_block == NULL) {
      prev = header;
      header = header->next;
    }
    if (!header) {
      // Header is NULL, all pages are full, allocate a new page.
      assert(prev);
      header = alloc_new_page_for_index(index);
      prev->next = header;
    }

    while (atomic_xchg(&(header->lock), 1)) {
      // Spin here.
    }
    // Some exsiting page has spare space.
    Block* block = header->current_block;
    header->current_block = block->next;
    atomic_xchg(&(header->lock), 0);
    return block;
  }
  // Slow path. First find the minimum alloc_size to align.
  int alloc_size = 2 * MAX_BLOCK_SIZE;
  while (alloc_size < size) {
    alloc_size *= 2;
  }
  // Iterate the list, find the first available node.
  NodeHeader* cur = header_start, *prev = NULL;
  assert(header_start->size > 0);
  while (atomic_xchg(&non_page_lock, 1)) {
    // Spin here.
  }
  while (cur) {
    // Current block's size is bigger than info size plus requested size.
    if (cur->size > size + sizeof(NodeInfo)) {
      // Starting from header size to align the address with |alloc_size|.
      uintptr_t ptr = (uintptr_t)cur + sizeof(NodeInfo);
      while (ptr % alloc_size != 0) {
        ++ptr;
      }
      // Now |ptr| points to an address which meets the alignment, check if the remaining size
      // is enough to hold the requested size.
      if (cur->size - (ptr - (uintptr_t)cur) >= size) {
        // Size is enough, allocate the memory and update the list.
        // Save the current node.
        NodeHeader* tmp = cur;
        // Set NodeInfo for kfree.
        NodeInfo* info = (NodeInfo*)ptr - 1;
        info->size = size;
        info->magic = MAGIC;
        info->header = tmp;
        // Move |cur| to the bottom of allocated memory.
        cur = (NodeHeader*)(ptr + size);
        // Insert the new node after the allocated memory.
        cur->next = tmp->next;
        cur->size = tmp->size - ((uintptr_t)cur - (uintptr_t)tmp);
        // Update previous node if it exists. Otherwise move |header_start|.
        if (prev) {
          prev->next = cur;
        } else {
          header_start = cur;
        }
        atomic_xchg(&non_page_lock, 0);
        return (void*)ptr;
      } else {
        prev = cur;
        cur = cur->next;
      }
    } else {
      prev = cur;
      cur = cur->next;
    }
  }
  atomic_xchg(&non_page_lock, 0);
  return NULL;
}

// Find which page the |ptr| belongs to.
static PageHeader* find_page_header_for_ptr(void *ptr) {
 for (int i = 0; i < CPU_NUM_MAX; ++i) {
    for (int j = 0; j < PAGES_PER_CPU; ++j) {
      PageHeader* header = pages_total[i]->headers[j];
      while (header) {
       if ((uintptr_t)header < (uintptr_t)ptr && (uintptr_t)(header) + PAGES_SIZE > (uintptr_t)ptr) {
        return header;
       }
       header = header->next;
      }
    }
  }
  return NULL;
}

static void kfree(void *ptr) {
  // First, find whether it is fast or slow path.
  if ((uintptr_t)ptr < pages_bottom) {
    // Fast path. Then find which page it belongs to.
    PageHeader* header = find_page_header_for_ptr(ptr);
    assert(header);
    // Free the block it points to.
    Block* block = (Block*)ptr;
    if (block < header->current_block || header->current_block == NULL) {
      // Free a block before current_block, we mark the freed block as new current_block.
      block->next = header->current_block;
      header->current_block = block;
    } else {
      // Free a block after the current block, we insert the freed block into some empty blocks or at last.
      Block* current = header->current_block, *next = current->next;
      while (next && next < block) {
        current = next;
        next = current->next;
      }
      if (!next) {
        // Last empty block is before the freed block.
        current->next = block;
      } else {
        // Found an empty block after the freed block.
        current->next = block;
        block->next = next;
      }
    }
  } else {
    // Slow path.
    NodeInfo* info = (NodeInfo*)ptr - 1;
    assert(info->magic == MAGIC);
    NodeHeader* header = info->header;
    assert(header);
    // Find the previous and next node.
    NodeHeader* prev = NULL, *next = header_start;
    if (header < header_start) {
      next = header_start;
      header_start = header;
    } else {
      while (next && next < header) {
        prev = next;
        next = next->next;
      }
    }
    if (next) {
      if ((uintptr_t)next == (uintptr_t)ptr + info->size) {
        // Merge next node.
        if (prev) {
          prev->next = header;
        }
        header->next = next->next;
        header->size = header->size + sizeof(NodeHeader) + next->size;
      } else {
        if (prev) {
          prev->next = header;
        }
        header->next = next;
      }

      if (prev && ((uintptr_t)prev + sizeof(NodeHeader) + prev->size == (uintptr_t)header)) {
        // Merge previous node.
        prev->size = prev->size + sizeof(NodeHeader) + header->size;
        prev->next = header->next;
      }
    } else {
      // Last node.
      if (prev) {
        prev->next = header;
      }
      header->next = next;

      if (prev && ((uintptr_t)prev + sizeof(NodeHeader) + prev->size == (uintptr_t)header)) {
        // Merge previous node.
        prev->size = prev->size + sizeof(NodeHeader) + header->size;
        prev->next = header->next;
      }
    }
  }
}

#ifndef TEST
static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);
  init_pages_total();
  init_pages_info();
  init_nonpages_top();
  init_pages();
}
#else
static void pmm_init() {
  char *ptr  = malloc(HEAP_SIZE);
  heap.start = ptr;
  heap.end   = ptr + HEAP_SIZE;
  printf("Got %d MiB heap: [%p, %p)\n", HEAP_SIZE >> 20, heap.start, heap.end);
  init_pages_total();
  init_pages_info();
  init_nonpages_top();
  init_pages();
}
#endif

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};
