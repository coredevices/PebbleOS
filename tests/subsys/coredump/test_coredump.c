/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <pbl/coredump/arch.h>
#include <pbl/coredump/backend.h>
#include <pbl/coredump/coredump.h>
#include <pbl/coredump/format.h>
#include <pbl/drivers/flash.h>

#include <clar.h>
#include <coredump/coredump_priv.h>
#include <fake_spi_flash.h>

#define DAY_S     (24 * 60 * 60)
#define OUT_SIZE  8192
#define REGION_SZ 64

static jmp_buf s_reset;
static int s_reset_count;
static uint32_t s_time;
static bool s_in_exception;
static int s_prepare_calls;
static int s_late_prepare_calls;

static const uint8_t s_early[REGION_SZ] = {[0] = 0x11, [REGION_SZ - 1] = 0x22};
static const uint8_t s_late[REGION_SZ] = {[0] = 0x33, [REGION_SZ - 1] = 0x44};

static void prv_late_prepare(void) {
  s_late_prepare_calls++;
}

PBL_COREDUMP_REGION_DEFINE(early, s_early, sizeof(s_early));
PBL_COREDUMP_LATE_REGION_DEFINE(late, s_late, sizeof(s_late), prv_late_prepare);

// Kernel, arch and platform
//////////////////////////////////////////////////////////

char *itoa(int value, char *str, int base) {
  snprintf(str, 12, base == 16 ? "%x" : "%d", value);
  return str;
}

void kernel_pbl_log_from_fault_handler(const char *src_filename, uint16_t src_line_number,
                                       const char *message) {
}

time_t pbl_realtime(void) {
  return s_time;
}

[[noreturn]] void pbl_reboot_emergency(void) {
  s_reset_count++;
  longjmp(s_reset, 1);
}

int pbl_flash_coredump_init(const struct pbl_flash *flash) {
  return 0;
}

void pbl_coredump_platform_prepare(struct pbl_coredump_metadata *metadata) {
  s_prepare_calls++;
  strcpy(metadata->serial_number, "SERIAL");
  strcpy(metadata->build_id, "BUILD");
}

void pbl_thread_foreach(pbl_thread_info_fn fn, void *ctx) {
  struct pbl_thread_info running = {.name = "running", .id = 100, .current = true};
  struct pbl_thread_info waiting = {.name = "waiting", .id = 200};

  for (int i = 0; i < PBL_THREAD_REG_COUNT; i++) {
    waiting.regs[i] = 2000 + i;
  }
  fn(&running, ctx);
  fn(&waiting, ctx);
}

void arch_coredump_trigger(void) {
  pbl_coredump_arch_handler();
}

void arch_coredump_thread_regs(uint32_t regs[PBL_THREAD_REG_COUNT]) {
  for (int i = 0; i < PBL_THREAD_REG_COUNT; i++) {
    regs[i] = 1000 + i;
  }
}

bool arch_coredump_exception_regs(uint32_t regs[PBL_THREAD_REG_COUNT]) {
  for (int i = 0; i < PBL_THREAD_REG_COUNT; i++) {
    regs[i] = 3000 + i;
  }
  return s_in_exception;
}

const struct pbl_coredump_extra_regs *arch_coredump_extra_regs(void) {
  static const struct pbl_coredump_extra_regs regs = {.msp = 0x1234, .psp = 0x5678};
  return &regs;
}

// A second backend, that only outputs
//////////////////////////////////////////////////////////

static uint8_t s_out[OUT_SIZE];
static size_t s_out_len;
static int s_out_start_ret;
static int s_out_write_ret;
static size_t s_out_space;
static int s_out_end_calls;
static int s_out_init_ret;
static int s_out_start_calls;
static int s_prepare_calls_at_init;
static int s_prepare_calls_at_start;

static int prv_out_init(const struct pbl_coredump_backend *backend) {
  s_prepare_calls_at_init = s_prepare_calls;
  return s_out_init_ret;
}

static int prv_out_start(const struct pbl_coredump_backend *backend, bool forced,
                         uint32_t timestamp) {
  s_out_start_calls++;
  s_prepare_calls_at_start = s_prepare_calls;
  return s_out_start_ret;
}

static int prv_out_write(const struct pbl_coredump_backend *backend, const void *data, size_t len) {
  if (s_out_write_ret < 0) {
    return s_out_write_ret;
  }
  cl_assert(s_out_len + len <= OUT_SIZE);
  memcpy(&s_out[s_out_len], data, len);
  s_out_len += len;
  return 0;
}

static size_t prv_out_space(const struct pbl_coredump_backend *backend) {
  return s_out_space - s_out_len;
}

static void prv_out_end(const struct pbl_coredump_backend *backend) {
  s_out_end_calls++;
}

static const struct pbl_coredump_backend_api s_out_api = {
  .init = prv_out_init,
  .start = prv_out_start,
  .write = prv_out_write,
  .space = prv_out_space,
  .end = prv_out_end,
};

PBL_COREDUMP_BACKEND_DEFINE(out, s_out_api);

// Helpers
//////////////////////////////////////////////////////////

static void prv_dump(bool forced) {
  pbl_coredump__test_reset();
  if (setjmp(s_reset) == 0) {
    pbl_coredump_trigger(forced);
    cl_fail("a dump must reset");
  }
}

static int prv_stored_time(bool unread_only, uint32_t *time_stamp) {
  struct pbl_coredump_stored stored;
  struct pbl_coredump_image_hdr hdr;

  int ret = pbl_coredump_stored_find(unread_only, &stored);
  if (ret != 0) {
    return ret;
  }
  cl_assert_equal_i(pbl_coredump_stored_read(&stored, 0, &hdr, sizeof(hdr)), 0);
  *time_stamp = hdr.time_stamp;
  return 0;
}

static void prv_mark_read(void) {
  struct pbl_coredump_stored stored;

  cl_assert_equal_i(pbl_coredump_stored_find(false, &stored), 0);
  cl_assert_equal_i(pbl_coredump_stored_mark_read(&stored), 0);
}

static void prv_check_image(const uint8_t *image, size_t len, bool has_exception) {
  const struct pbl_coredump_image_hdr *hdr = (const void *)image;
  size_t offset = sizeof(*hdr);
  int threads = 0;
  int extra = 0;
  bool early = false;
  bool late = false;

  cl_assert_equal_i(hdr->magic, PBL_COREDUMP_MAGIC);
  cl_assert_equal_i(hdr->version, PBL_COREDUMP_VERSION);
  cl_assert_equal_i(hdr->time_stamp, s_time);
  cl_assert_equal_s((const char *)hdr->serial_number, "SERIAL");
  cl_assert_equal_s((const char *)hdr->build_id, "BUILD");

  while (true) {
    const struct pbl_coredump_chunk_hdr *chunk = (const void *)&image[offset];

    offset += sizeof(*chunk);
    cl_assert(offset <= len);
    if (chunk->key == PBL_COREDUMP_CHUNK_TERMINATOR) {
      break;
    }

    const uint8_t *payload = &image[offset];

    if (chunk->key == PBL_COREDUMP_CHUNK_MEMORY) {
      const struct pbl_coredump_memory_hdr *memory = (const void *)payload;
      const bool is_early = memory->start == (uint32_t)(uintptr_t)s_early;

      cl_assert_equal_i(chunk->size, sizeof(*memory) + REGION_SZ);
      cl_assert_equal_m(payload + sizeof(*memory), is_early ? s_early : s_late, REGION_SZ);
      if (is_early) {
        early = true;
        cl_assert_equal_i(threads, 0);
      } else {
        late = true;
        cl_assert(threads > 0);
      }
    } else if (chunk->key == PBL_COREDUMP_CHUNK_EXTRA_REG) {
      const struct pbl_coredump_extra_regs *regs = (const void *)payload;

      cl_assert_equal_i(regs->msp, 0x1234);
      cl_assert_equal_i(regs->psp, 0x5678);
      extra++;
    } else {
      const struct pbl_coredump_thread *thread = (const void *)payload;

      cl_assert_equal_i(chunk->key, PBL_COREDUMP_CHUNK_THREAD);
      if (threads == 0) {
        cl_assert_equal_s((const char *)thread->name, "running");
        cl_assert_equal_i(thread->running, 1);
        cl_assert_equal_i(thread->registers[3], 1003);
      } else if (threads == 1) {
        cl_assert_equal_s((const char *)thread->name, "waiting");
        cl_assert_equal_i(thread->running, 0);
        cl_assert_equal_i(thread->registers[3], 2003);
      } else {
        cl_assert_equal_s((const char *)thread->name, "ISR");
        cl_assert_equal_i(thread->id, 1);
        cl_assert_equal_i(thread->registers[3], 3003);
      }
      threads++;
    }
    offset += chunk->size;
  }

  cl_assert(early);
  cl_assert(late);
  cl_assert_equal_i(extra, 1);
  cl_assert_equal_i(threads, has_exception ? 3 : 2);
  cl_assert_equal_i(offset, len);
}

// Tests
//////////////////////////////////////////////////////////

void test_coredump__initialize(void) {
  fake_spi_flash_init(0, 32U * 1024U * 1024U);
  pbl_coredump__test_reset();
  s_reset_count = 0;
  s_time = 10 * DAY_S;
  s_in_exception = false;
  s_prepare_calls = 0;
  s_late_prepare_calls = 0;
  s_out_len = 0;
  s_out_start_ret = 0;
  s_out_write_ret = 0;
  s_out_space = OUT_SIZE;
  s_out_end_calls = 0;
  s_out_init_ret = 0;
  s_out_start_calls = 0;
  s_prepare_calls_at_init = -1;
  s_prepare_calls_at_start = -1;
}

void test_coredump__cleanup(void) {
  fake_spi_flash_cleanup();
}

void test_coredump__no_dump_stored_at_first(void) {
  struct pbl_coredump_stored stored;

  cl_assert_equal_i(pbl_coredump_stored_find(false, &stored), -ENOENT);
}

void test_coredump__every_backend_gets_the_same_image(void) {
  struct pbl_coredump_stored stored;
  uint32_t size;
  uint8_t image[OUT_SIZE];

  prv_dump(false);

  cl_assert_equal_i(s_prepare_calls, 1);
  cl_assert_equal_i(s_late_prepare_calls, 1);
  cl_assert_equal_i(s_out_end_calls, 1);
  cl_assert_equal_i(s_reset_count, 1);
  prv_check_image(s_out, s_out_len, false);

  cl_assert_equal_i(pbl_coredump_stored_find(true, &stored), 0);
  cl_assert_equal_i(pbl_coredump_stored_size(&stored, &size), 0);
  cl_assert_equal_i(size, s_out_len);
  cl_assert_equal_i(pbl_coredump_stored_read(&stored, 0, image, size), 0);
  cl_assert_equal_m(image, s_out, size);
}

// The platform can use the drivers of the backends, and clean up before stored data is touched.
void test_coredump__platform_prepares_between_init_and_start(void) {
  prv_dump(false);

  cl_assert_equal_i(s_prepare_calls_at_init, 0);
  cl_assert_equal_i(s_prepare_calls_at_start, 1);
}

void test_coredump__backend_that_does_not_init_is_skipped(void) {
  uint32_t time_stamp;

  s_out_init_ret = -EIO;

  prv_dump(false);

  cl_assert_equal_i(s_out_start_calls, 0);
  cl_assert_equal_i(s_out_len, 0);
  cl_assert_equal_i(prv_stored_time(true, &time_stamp), 0);
}

void test_coredump__exception_adds_a_thread(void) {
  s_in_exception = true;

  prv_dump(false);

  prv_check_image(s_out, s_out_len, true);
}

void test_coredump__failing_backend_does_not_affect_others(void) {
  struct pbl_coredump_stored stored;
  uint32_t size;
  uint8_t image[OUT_SIZE];

  s_out_write_ret = -EIO;

  prv_dump(false);

  cl_assert_equal_i(s_out_end_calls, 1);
  cl_assert_equal_i(pbl_coredump_stored_find(true, &stored), 0);
  cl_assert_equal_i(pbl_coredump_stored_size(&stored, &size), 0);
  cl_assert_equal_i(pbl_coredump_stored_read(&stored, 0, image, size), 0);
  prv_check_image(image, size, false);
}

void test_coredump__backend_that_does_not_start_is_skipped(void) {
  uint32_t time_stamp;

  s_out_start_ret = -EIO;

  prv_dump(false);

  cl_assert_equal_i(s_out_len, 0);
  cl_assert_equal_i(prv_stored_time(true, &time_stamp), 0);
}

void test_coredump__full_backend_ends_the_image_between_chunks(void) {
  struct pbl_coredump_image_hdr *hdr = (void *)s_out;
  size_t offset = sizeof(*hdr);

  s_out_space = sizeof(*hdr) + 100;

  prv_dump(false);

  cl_assert_equal_i(s_out_end_calls, 1);
  cl_assert(s_out_len <= s_out_space);

  while (true) {
    const struct pbl_coredump_chunk_hdr *chunk = (const void *)&s_out[offset];

    offset += sizeof(*chunk);
    if (chunk->key == PBL_COREDUMP_CHUNK_TERMINATOR) {
      break;
    }
    offset += chunk->size;
    cl_assert(offset < s_out_len);
  }
  cl_assert_equal_i(offset, s_out_len);
}

void test_coredump__recent_unread_dump_is_kept(void) {
  uint32_t first = s_time;
  uint32_t time_stamp;

  prv_dump(false);
  s_time += DAY_S - 1;
  prv_dump(false);

  cl_assert_equal_i(prv_stored_time(false, &time_stamp), 0);
  cl_assert_equal_i(time_stamp, first);
}

void test_coredump__old_unread_dump_is_replaced(void) {
  uint32_t time_stamp;

  prv_dump(false);
  s_time += DAY_S;
  prv_dump(false);

  cl_assert_equal_i(prv_stored_time(false, &time_stamp), 0);
  cl_assert_equal_i(time_stamp, s_time);
}

void test_coredump__forced_dump_replaces_a_recent_one(void) {
  uint32_t time_stamp;

  prv_dump(false);
  s_time += 1;
  prv_dump(true);

  cl_assert_equal_i(prv_stored_time(false, &time_stamp), 0);
  cl_assert_equal_i(time_stamp, s_time);
}

void test_coredump__read_dump_is_replaced(void) {
  uint32_t time_stamp;

  prv_dump(false);
  prv_mark_read();
  cl_assert_equal_i(prv_stored_time(true, &time_stamp), -ENOENT);
  cl_assert_equal_i(prv_stored_time(false, &time_stamp), 0);

  s_time += 1;
  prv_dump(false);

  cl_assert_equal_i(prv_stored_time(true, &time_stamp), 0);
  cl_assert_equal_i(time_stamp, s_time);
}

static const uint8_t *s_stored_image;
static size_t s_stored_image_len;

static int prv_image_read(const struct pbl_coredump_backend *backend, uint32_t id, uint32_t offset,
                          void *buf, size_t len) {
  if (offset > s_stored_image_len || len > s_stored_image_len - offset) {
    return -EINVAL;
  }
  memcpy(buf, &s_stored_image[offset], len);
  return 0;
}

static const struct pbl_coredump_backend_api s_image_api = {.stored_read = prv_image_read};
static const struct pbl_coredump_backend s_image_backend = {.api = &s_image_api};

static int prv_image_size(const struct pbl_coredump_chunk_hdr *chunk) {
  static uint8_t image[sizeof(struct pbl_coredump_image_hdr) + sizeof(*chunk)];
  const struct pbl_coredump_stored stored = {.backend = &s_image_backend};
  uint32_t size;

  memcpy(&image[sizeof(struct pbl_coredump_image_hdr)], chunk, sizeof(*chunk));
  s_stored_image = image;
  s_stored_image_len = sizeof(image);

  int ret = pbl_coredump_stored_size(&stored, &size);
  return (ret == 0) ? (int)size : ret;
}

void test_coredump__corrupted_dump_is_reported(void) {
  const struct pbl_coredump_chunk_hdr terminator = {.key = PBL_COREDUMP_CHUNK_TERMINATOR};
  const struct pbl_coredump_chunk_hdr unknown = {.key = 7};
  const struct pbl_coredump_chunk_hdr past_the_end = {.key = PBL_COREDUMP_CHUNK_THREAD, .size = 64};
  const struct pbl_coredump_chunk_hdr overflow = {
    .key = PBL_COREDUMP_CHUNK_MEMORY,
    .size = UINT32_MAX
  };

  cl_assert_equal_i(prv_image_size(&terminator),
                    sizeof(struct pbl_coredump_image_hdr) + sizeof(terminator));
  cl_assert_equal_i(prv_image_size(&unknown), -EBADMSG);
  cl_assert_equal_i(prv_image_size(&past_the_end), -EBADMSG);
  cl_assert_equal_i(prv_image_size(&overflow), -EBADMSG);
}

void test_coredump__reentering_resets_without_a_dump(void) {
  prv_dump(false);

  const size_t len = s_out_len;

  if (setjmp(s_reset) == 0) {
    pbl_coredump_trigger(false);
    cl_fail("re-entering must reset");
  }

  cl_assert_equal_i(s_out_len, len);
  cl_assert_equal_i(s_reset_count, 2);
}
