/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/heap.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/util/size.h>

#define CALLER     ((uintptr_t)PBL_RETURN_ADDRESS(0))
#define BLOCK_SIZE 100U

/* A heap of its own bounds what one user can take, here a 2 KiB cache. */
static uint8_t s_region[2048];
static struct pbl_heap s_heap;
static PBL_MUTEX_DEFINE(s_heap_mutex);

static void prv_lock(void *ctx) {
  pbl_mutex_lock(ctx, PBL_FOREVER);
}

static void prv_unlock(void *ctx) {
  pbl_mutex_unlock(ctx);
}

static void prv_log_usage(const char *when) {
  unsigned int used;
  unsigned int free;
  unsigned int max_free;

  pbl_heap_stats(&s_heap, &used, &free, &max_free);
  PBL_LOG_INFO("heap: %s: %u used, %u free, largest free block %u", when, used, free, max_free);
}

int main(void) {
  void *blocks[32];
  size_t num_blocks = 0;
  char *name;
  void *big;

  pbl_heap_init(&s_heap, s_region, s_region + sizeof(s_region), false);
  /* Heaps do not lock on their own: one used from several threads is given a lock. */
  pbl_heap_set_lock(&s_heap, (struct pbl_heap_lock){
                               .lock = prv_lock,
                               .unlock = prv_unlock,
                               .ctx = &s_heap_mutex,
                             });
  prv_log_usage("empty");

  name = pbl_heap_strdup(&s_heap, "a string on the heap", CALLER);
  if (name == NULL) {
    PBL_LOG_ERR("heap: out of memory");
    return -ENOMEM;
  }
  PBL_LOG_INFO("heap: block is in the heap: %s, allocated: %s",
               pbl_heap_contains(&s_heap, name) ? "yes" : "no",
               pbl_heap_is_allocated(&s_heap, name) ? "yes" : "no");
  pbl_heap_free(&s_heap, name, CALLER);

  /* Allocation fails with NULL, which the caller handles. */
  while (num_blocks < ARRAY_LENGTH(blocks)) {
    blocks[num_blocks] = pbl_heap_alloc(&s_heap, BLOCK_SIZE, CALLER);
    if (blocks[num_blocks] == NULL) {
      break;
    }
    num_blocks++;
  }
  PBL_LOG_INFO("heap: full after %u blocks of %u bytes", (unsigned int)num_blocks, BLOCK_SIZE);
  prv_log_usage("full");

  /* Every other block freed: plenty of memory in total, but in pieces. */
  for (size_t i = 0; i < num_blocks; i += 2) {
    pbl_heap_free(&s_heap, blocks[i], CALLER);
  }
  prv_log_usage("every other block freed");
  big = pbl_heap_alloc(&s_heap, 2 * BLOCK_SIZE, CALLER);
  PBL_LOG_INFO("heap: %u bytes from a fragmented heap: %s", 2 * BLOCK_SIZE,
               big == NULL ? "refused" : "allocated");

  /* Freed neighbors merge, so the pieces become one block again. */
  for (size_t i = 1; i < num_blocks; i += 2) {
    pbl_heap_free(&s_heap, blocks[i], CALLER);
  }
  prv_log_usage("all freed");

  PBL_LOG_INFO("heap: least memory it ever had left: %u of %u bytes",
               (unsigned int)pbl_heap_min_headroom(&s_heap), (unsigned int)pbl_heap_size(&s_heap));

  PBL_LOG_INFO("Sample done");

  return 0;
}
