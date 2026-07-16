/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

// The firmware manages all heaps itself (see pbl_malloc.c, wired up through
// --wrap=malloc/free/...). A toolchain libc must therefore never grow a heap
// of its own: fail every _sbrk request so that a stray libc-internal
// allocation errors out cleanly instead of colliding with the statically
// partitioned RAM. This also overrides the unbounded _sbrk from nosys.specs.
void *_sbrk(intptr_t incr) {
  errno = ENOMEM;
  return (void *)-1;
}
