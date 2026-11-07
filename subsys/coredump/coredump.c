/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "coredump_priv.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

// itoa is a libc extension hidden under strict-ANSI (-std=c23).
extern char *itoa(int value, char *str, int base);

#include <pbl/coredump/arch.h>
#include <pbl/coredump/backend.h>
#include <pbl/coredump/coredump.h>
#include <pbl/coredump/format.h>
#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/time.h>
#include <pbl/util/math.h>

#ifdef CONFIG_CONSOLE
#include <pbl/console/console.h>
#endif
#include <logging/logging_private.h>
#include <pebbleos/chip_id.h>

static_assert(PBL_COREDUMP_NUM_REGISTERS == PBL_THREAD_REG_COUNT, "register layout mismatch");

// Feeding the watchdog this often keeps it quiet however slow the backends are.
#define REGION_WRITE_SIZE 0x10000

#ifndef PBL_NO_LINKER_SCRIPT
extern const struct pbl_coredump_backend __pbl_coredump_backends_start[];
extern const struct pbl_coredump_backend __pbl_coredump_backends_end[];
extern const struct pbl_coredump_region __pbl_coredump_regions_start[];
extern const struct pbl_coredump_region __pbl_coredump_regions_end[];
#else
extern const struct pbl_coredump_backend __pbl_coredump_backends_start[] PBL_UNSORTED_SECTION_START(
    pbl_cd_be);
extern const struct pbl_coredump_backend __pbl_coredump_backends_end[] PBL_UNSORTED_SECTION_END(
    pbl_cd_be);
extern const struct pbl_coredump_region __pbl_coredump_regions_start[] PBL_UNSORTED_SECTION_START(
    pbl_cd_regs);
extern const struct pbl_coredump_region __pbl_coredump_regions_end[] PBL_UNSORTED_SECTION_END(
    pbl_cd_regs);
#endif

#define PRV_FOREACH_BACKEND(_be)                                               \
  for (const struct pbl_coredump_backend *_be = __pbl_coredump_backends_start; \
       _be < __pbl_coredump_backends_end; _be++)

// Backends still receiving the dump, by position.
static uint32_t s_active;
static bool s_initiated;
static bool s_forced;

#ifdef CONFIG_COREDUMP_TEST_FAULT
static enum pbl_coredump_test_fault s_test_fault;

void pbl_coredump_test_fault_set(enum pbl_coredump_test_fault fault) {
  s_test_fault = fault;
}

static void prv_test_fault(void) {
  if (s_test_fault == PBL_COREDUMP_TEST_FAULT_BUS) {
    void (*kaboom)(void) = nullptr;
    kaboom();
  } else if (s_test_fault == PBL_COREDUMP_TEST_FAULT_LOOP) {
    while (true) {
    }
  } else if (s_test_fault == PBL_COREDUMP_TEST_FAULT_ASSERT) {
    PBL_ASSERTN(false);
  }
}
#else
#define prv_test_fault()
#endif

#ifdef UNITTEST
void pbl_coredump__test_reset(void) {
  s_active = 0;
  s_initiated = false;
  s_forced = false;
}
#endif

// vsniprintf and cohorts are avoided to keep the stack small.
void pbl_coredump__log(const char *msg) {
  kernel_pbl_log_from_fault_handler(__FILE_NAME__, 0, msg);
}

void pbl_coredump__log_int(const char *msg, uint32_t value, int base) {
  char buffer[64];

  strncpy(buffer, msg, sizeof(buffer) - 12);
  buffer[sizeof(buffer) - 12] = '\0';
  itoa(value, &buffer[strlen(buffer)], base);
  pbl_coredump__log(buffer);
}

[[noreturn]] static void prv_reset(void) {
#ifdef CONFIG_CONSOLE
  pbl_console_flush();
#endif
  pbl_reboot_emergency();
}

static uint32_t prv_backend_bit(const struct pbl_coredump_backend *backend) {
  return 1U << (backend - __pbl_coredump_backends_start);
}

static void prv_backend_end(const struct pbl_coredump_backend *backend) {
  if (backend->api->end != nullptr) {
    backend->api->end(backend);
  }
  s_active &= ~prv_backend_bit(backend);
}

static void prv_output(const void *data, size_t len) {
  PRV_FOREACH_BACKEND(backend) {
    if ((s_active & prv_backend_bit(backend)) == 0) {
      continue;
    }
    int ret = backend->api->write(backend, data, len);
    if (ret < 0) {
      pbl_coredump__log_int("CD: write failed ", -ret, 10);
      prv_backend_end(backend);
    }
  }

  if (s_active == 0) {
    pbl_coredump__log("CD: no backends left");
    prv_reset();
  }
}

static void prv_terminator_write(const struct pbl_coredump_backend *backend) {
  const struct pbl_coredump_chunk_hdr hdr = {.key = PBL_COREDUMP_CHUNK_TERMINATOR};

  (void)backend->api->write(backend, &hdr, sizeof(hdr));
}

// Starts a chunk of @p size bytes. A backend without room for it gets its image ended here instead,
// so that what it stores stays parseable.
static void prv_chunk_begin(uint32_t key, uint32_t size) {
  const struct pbl_coredump_chunk_hdr hdr = {.key = key, .size = size};
  const size_t needed = sizeof(hdr) + size + sizeof(hdr);

  PRV_FOREACH_BACKEND(backend) {
    if ((s_active & prv_backend_bit(backend)) == 0 || backend->api->space == nullptr) {
      continue;
    }
    if (backend->api->space(backend) < needed) {
      pbl_coredump__log("CD: backend full");
      prv_terminator_write(backend);
      prv_backend_end(backend);
    }
  }

  prv_output(&hdr, sizeof(hdr));
}

static void prv_regions_write(bool late) {
  for (const struct pbl_coredump_region *region = __pbl_coredump_regions_start;
       region < __pbl_coredump_regions_end; region++) {
    if (region->late != late) {
      continue;
    }
    if (region->prepare != nullptr) {
      region->prepare();
    }

    const struct pbl_coredump_memory_hdr hdr = {.start = (uint32_t)(uintptr_t)region->start};

    prv_chunk_begin(PBL_COREDUMP_CHUNK_MEMORY, sizeof(hdr) + region->size);
    prv_output(&hdr, sizeof(hdr));

    for (size_t offset = 0; offset < region->size; offset += REGION_WRITE_SIZE) {
      prv_output((const uint8_t *)region->start + offset,
                 MIN(region->size - offset, REGION_WRITE_SIZE));
      watchdog_feed();
    }
  }
}

static void prv_thread_write(const struct pbl_thread_info *info, void *ctx) {
  struct pbl_coredump_thread thread = {
    .id = (uint32_t)info->id,
    .running = info->current,
  };

  prv_test_fault();

  strncpy((char *)thread.name, info->name, sizeof(thread.name));
  if (info->current) {
    uint32_t regs[PBL_THREAD_REG_COUNT];

    arch_coredump_thread_regs(regs);
    memcpy(thread.registers, regs, sizeof(thread.registers));
  } else {
    memcpy(thread.registers, info->regs, sizeof(thread.registers));
  }

  prv_chunk_begin(PBL_COREDUMP_CHUNK_THREAD, sizeof(thread));
  prv_output(&thread, sizeof(thread));
}

static void prv_backends_init(void) {
  PRV_FOREACH_BACKEND(backend) {
    int ret = (backend->api->init != nullptr) ? backend->api->init(backend) : 0;

    if (ret == 0) {
      s_active |= prv_backend_bit(backend);
    } else {
      pbl_coredump__log_int("CD: init failed ", -ret, 10);
    }
  }
}

static void prv_backends_start(uint32_t timestamp) {
  PRV_FOREACH_BACKEND(backend) {
    if ((s_active & prv_backend_bit(backend)) == 0) {
      continue;
    }

    int ret = backend->api->start(backend, s_forced, timestamp);

    if (ret != 0) {
      if (ret == -EEXIST) {
        pbl_coredump__log("CD: keeping the stored dump");
      } else {
        pbl_coredump__log_int("CD: start failed ", -ret, 10);
      }
      s_active &= ~prv_backend_bit(backend);
    }
  }
}

void pbl_coredump_trigger(bool forced) {
  // Re-entering means the dump itself crashed.
  if (s_initiated) {
    pbl_coredump__log("CD: re-entered");
    prv_reset();
  }
  s_initiated = true;
  s_forced = forced;

  arch_coredump_trigger();
}

void pbl_coredump_arch_handler(void) {
  struct pbl_coredump_metadata metadata = {};

  pbl_coredump__log("CD: starting");
  watchdog_feed();

  prv_backends_init();
  pbl_coredump_platform_prepare(&metadata);

  const uint32_t timestamp = pbl_realtime();

  prv_backends_start(timestamp);
  if (s_active == 0) {
    prv_reset();
  }

  struct pbl_coredump_image_hdr image_hdr = {
    .magic = PBL_COREDUMP_MAGIC,
    .core_number = CORE_ID_MAIN_MCU,
    .version = PBL_COREDUMP_VERSION,
    .time_stamp = timestamp,
  };
  strncpy((char *)image_hdr.serial_number, metadata.serial_number,
          sizeof(image_hdr.serial_number) - 1);
  strncpy((char *)image_hdr.build_id, metadata.build_id, sizeof(image_hdr.build_id) - 1);
  prv_output(&image_hdr, sizeof(image_hdr));

  prv_regions_write(false);

  const struct pbl_coredump_extra_regs *extra_regs = arch_coredump_extra_regs();
  prv_chunk_begin(PBL_COREDUMP_CHUNK_EXTRA_REG, sizeof(*extra_regs));
  prv_output(extra_regs, sizeof(*extra_regs));

  // Threads come after the essentials, in case the walk hits corrupted kernel structures. An
  // image without a terminator is still parseable from storage that reads as erased.
  pbl_thread_foreach(prv_thread_write, nullptr);

  struct pbl_thread_info exception = {
    .name = "ISR",
    .id = 1,
  };
  if (arch_coredump_exception_regs(exception.regs)) {
    prv_thread_write(&exception, nullptr);
  }

  prv_regions_write(true);

  PRV_FOREACH_BACKEND(backend) {
    if ((s_active & prv_backend_bit(backend)) != 0) {
      prv_terminator_write(backend);
      prv_backend_end(backend);
    }
  }

  pbl_coredump__log("CD: completed");
  prv_reset();
}

int pbl_coredump_stored_find(bool unread_only, struct pbl_coredump_stored *stored) {
  PRV_FOREACH_BACKEND(backend) {
    struct pbl_coredump_image_hdr hdr;
    uint32_t id;

    if (backend->api->stored_find == nullptr ||
        backend->api->stored_find(backend, unread_only, &id) != 0 ||
        backend->api->stored_read(backend, id, 0, &hdr, sizeof(hdr)) != 0 ||
        hdr.magic != PBL_COREDUMP_MAGIC) {
      continue;
    }

    *stored = (struct pbl_coredump_stored){.backend = backend, .id = id};
    return 0;
  }

  return -ENOENT;
}

int pbl_coredump_stored_size(const struct pbl_coredump_stored *stored, uint32_t *size) {
  uint32_t offset = sizeof(struct pbl_coredump_image_hdr);

  while (true) {
    struct pbl_coredump_chunk_hdr hdr;
    int ret = pbl_coredump_stored_read(stored, offset, &hdr, sizeof(hdr));

    if (ret < 0) {
      return -EBADMSG;
    }
    offset += sizeof(hdr);

    if (hdr.key == PBL_COREDUMP_CHUNK_TERMINATOR) {
      *size = offset;
      return 0;
    }

    if ((hdr.key != PBL_COREDUMP_CHUNK_RAM && hdr.key != PBL_COREDUMP_CHUNK_THREAD &&
         hdr.key != PBL_COREDUMP_CHUNK_EXTRA_REG && hdr.key != PBL_COREDUMP_CHUNK_MEMORY) ||
        hdr.size > UINT32_MAX - offset) {
      return -EBADMSG;
    }
    offset += hdr.size;
  }
}

int pbl_coredump_stored_read(const struct pbl_coredump_stored *stored, uint32_t offset, void *buf,
                             size_t len) {
  return stored->backend->api->stored_read(stored->backend, stored->id, offset, buf, len);
}

int pbl_coredump_stored_mark_read(const struct pbl_coredump_stored *stored) {
  return stored->backend->api->stored_mark_read(stored->backend, stored->id);
}
