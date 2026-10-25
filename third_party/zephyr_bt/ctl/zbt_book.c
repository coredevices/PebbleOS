/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The phone's books on the watch: one file each, written whole by KernelBG as the host reads
// the book, then made current. Two files a book, so a reading that fails leaves the last one.

#include "zbt_link.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/irq.h>
#include <pbl/kernel/mutex.h>
#include <pbl/services/filesystem/pfs.h>
#include <pbl/services/settings/settings_file.h>
#include <pbl/services/system_task.h>

#include <kernel/pbl_malloc.h>
#include <system/status_codes.h>

#define BOOK_MAGIC 0x6b6f6f62 // "book"

// Which of a book's two files is current.
#define CURRENT_FILE      "bt_classic_books"
#define CURRENT_FILE_SIZE 256

static const uint16_t BOOK_MAX[ZBT_BOOK_COUNT] = {
  [ZBT_BOOK_CONTACTS] = 1000,
  [ZBT_BOOK_HISTORY] = 50,
  [ZBT_BOOK_FAVORITES] = 50,
};

struct book_header {
  uint32_t magic;
  uint16_t count;
  uint16_t entry_size;
};

// Entries the host thread hands over, saved by KernelBG; the host asks the phone for more only
// once they are, so the two never touch them at once. With the host's reading state, borrowed
// from the kernel heap while it reads.
#define PENDING_MAX 24
struct reading_buffers {
  struct zbt_book_entry pending[PENDING_MAX];
  uint8_t pbap[ZBT_PBAP_MEM_SIZE] __attribute__((aligned(8)));
};
static struct reading_buffers *s_bufs;
//! The host's reading holds the buffers (s_bufs is not given back while it does).
static bool s_bufs_in_use;
static int s_pending_count;
static enum zbt_book s_book;
static bool s_new;
static bool s_end;
static bool s_ok;

// KernelBG only: the file being written.
static int s_fd = -1;
static int s_open_err;
static bool s_write_failed;
static uint8_t s_slot;
static uint16_t s_count;
static uint16_t s_max;

// Readers and KernelBG's switch of the current file.
static PBL_MUTEX_DEFINE(s_lock);

void *zbt_ctl_pbap_acquire(void) {
  pbl_irq_lock();
  s_bufs_in_use = true;
  struct reading_buffers *bufs = s_bufs;
  pbl_irq_unlock();
  if (bufs == NULL) {
    bufs = kernel_zalloc(sizeof(*bufs));
    pbl_irq_lock();
    s_bufs = bufs;
    s_bufs_in_use = bufs != NULL;
    pbl_irq_unlock();
  }
  return bufs != NULL ? bufs->pbap : NULL;
}

// On KernelBG, after the reading's last entries are saved (the queue keeps the order).
static void prv_free_bg(void *data) {
  pbl_irq_lock();
  struct reading_buffers *bufs = s_bufs_in_use ? NULL : s_bufs;
  if (bufs != NULL) {
    s_bufs = NULL;
  }
  pbl_irq_unlock();
  kernel_free(bufs);
}

void zbt_ctl_pbap_release(void) {
  pbl_irq_lock();
  s_bufs_in_use = false;
  pbl_irq_unlock();
  // If it cannot be queued, the next reading takes the buffers again.
  (void)system_task_add_callback(prv_free_bg, NULL);
}

void zbt_book_host_stopped(void) {
  pbl_irq_lock();
  struct reading_buffers *bufs = s_bufs;
  s_bufs = NULL;
  s_bufs_in_use = false;
  pbl_irq_unlock();
  kernel_free(bufs);
}

static void prv_file_name(char *out, size_t size, enum zbt_book book, uint8_t slot) {
  snprintf(out, size, "bt_classic_book%u%c", (unsigned)book, slot ? 'b' : 'a');
}

static const char *prv_current_key(enum zbt_book book) {
  static const char *const KEYS[ZBT_BOOK_COUNT] = {"CONTACTS", "HISTORY", "FAVORITES"};
  return KEYS[book];
}

// Under s_lock: the current file's slot, or -1 when the book has none.
static int prv_current_slot(enum zbt_book book) {
  SettingsFile file;
  uint8_t slot = 0xff;
  if (settings_file_open(&file, CURRENT_FILE, CURRENT_FILE_SIZE) == S_SUCCESS) {
    const char *key = prv_current_key(book);
    if (settings_file_get(&file, key, strlen(key), &slot, sizeof(slot)) != S_SUCCESS) {
      slot = 0xff;
    }
    settings_file_close(&file);
  }
  return slot <= 1 ? slot : -1;
}

static void prv_set_current_slot(enum zbt_book book, uint8_t slot) {
  SettingsFile file;
  if (settings_file_open(&file, CURRENT_FILE, CURRENT_FILE_SIZE) == S_SUCCESS) {
    const char *key = prv_current_key(book);
    (void)settings_file_set(&file, key, strlen(key), &slot, sizeof(slot));
    settings_file_close(&file);
  }
}

static void prv_open_new(enum zbt_book book) {
  s_open_err = 0;
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const int current = prv_current_slot(book);
  pbl_mutex_unlock(&s_lock);
  s_slot = current == 0 ? 1 : 0;
  s_count = 0;

  char name[24];
  prv_file_name(name, sizeof(name), book, s_slot);
  (void)pfs_remove(name);
  // As many entries as the file system has room for, down to an eighth.
  for (s_max = BOOK_MAX[book]; s_max >= BOOK_MAX[book] / 8; s_max /= 2) {
    const size_t size = sizeof(struct book_header) + s_max * sizeof(struct zbt_book_entry);
    s_fd = pfs_open(name, OP_FLAG_WRITE | OP_FLAG_READ, FILE_TYPE_STATIC, size);
    if (s_fd != E_OUT_OF_STORAGE) {
      break;
    }
  }
  if (s_fd < 0) {
    s_open_err = s_fd;
    return;
  }
  // The header is written last, once: flash takes each byte once.
  if (pfs_seek(s_fd, sizeof(struct book_header), FSeekSet) != sizeof(struct book_header)) {
    pfs_close_and_remove(s_fd);
    s_fd = -1;
  }
}

static void prv_finish(enum zbt_book book, bool ok) {
  struct zbt_status *st = zbt_ctl_status();
  if (s_fd < 0) {
    if (ok) {
      // Read in full, but no file to keep it in.
      st->pbap_result[book] = (int16_t)(s_open_err == E_OUT_OF_STORAGE ? -ENOSPC : -EIO);
    }
    return;
  }
  if (ok) {
    const struct book_header header = {
      .magic = BOOK_MAGIC,
      .count = s_count,
      .entry_size = sizeof(struct zbt_book_entry),
    };
    ok = pfs_seek(s_fd, 0, FSeekSet) == 0 &&
         pfs_write(s_fd, &header, sizeof(header)) == sizeof(header);
  }
  if (!ok) {
    if (st->pbap_result[book] == 0) {
      st->pbap_result[book] = -EIO;
    }
    pfs_close_and_remove(s_fd);
    s_fd = -1;
    return;
  }
  pfs_close(s_fd);
  s_fd = -1;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  prv_set_current_slot(book, s_slot);
  char name[24];
  prv_file_name(name, sizeof(name), book, s_slot ? 0 : 1);
  (void)pfs_remove(name);
  pbl_mutex_unlock(&s_lock);
  zbt_ctl_status()->pbap_entries[book] = s_count;
}

static void prv_save_bg(void *data) {
  if (s_new) {
    s_new = false;
    s_write_failed = false;
    prv_open_new(s_book);
  }
  for (int i = 0; i < s_pending_count && s_fd >= 0 && s_bufs != NULL; i++) {
    if (s_count >= s_max) {
      break;
    }
    if (pfs_write(s_fd, &s_bufs->pending[i], sizeof(s_bufs->pending[i])) !=
        sizeof(s_bufs->pending[i])) {
      s_write_failed = true;
      zbt_ctl_status()->pbap_result[s_book] = -EIO;
      break;
    }
    s_count++;
  }
  s_pending_count = 0;

  if (!s_end) {
    zbt_host_pbap_next();
    return;
  }
  prv_finish(s_book, s_ok && !s_write_failed);
  zbt_host_pbap_next();
}

void zbt_ctl_book_start(enum zbt_book book) {
  s_book = book;
  s_new = true;
  s_pending_count = 0;
}

bool zbt_ctl_book_entry(const struct zbt_book_entry *entry) {
  if (s_bufs == NULL || s_pending_count >= PENDING_MAX) {
    return false;
  }
  s_bufs->pending[s_pending_count++] = *entry;
  return true;
}

void zbt_ctl_book_hand_over(bool end, bool ok) {
  s_end = end;
  s_ok = ok;
  if (!system_task_add_callback(prv_save_bg, NULL)) {
    // Nothing will ask for more; the host's reading times out.
    s_pending_count = 0;
  }
}

// Under s_lock: opens the current file of @p book for reading and reads its header.
static int prv_open_current(enum zbt_book book, struct book_header *header) {
  const int slot = prv_current_slot(book);
  if (slot < 0) {
    return -ENOENT;
  }
  char name[24];
  prv_file_name(name, sizeof(name), book, (uint8_t)slot);
  const int fd = pfs_open(name, OP_FLAG_READ, 0, 0);
  if (fd < 0) {
    return fd;
  }
  if (pfs_read(fd, header, sizeof(*header)) != sizeof(*header) || header->magic != BOOK_MAGIC ||
      header->entry_size != sizeof(struct zbt_book_entry)) {
    pfs_close(fd);
    return -EINVAL;
  }
  return fd;
}

// The last @p n digits of @p number, or fewer.
static const char *prv_digits_tail(const char *number, size_t n) {
  size_t len = strlen(number);
  return len > n ? number + len - n : number;
}

bool zbt_book_find_name(const char *number, char *name, size_t size) {
  enum {
    CHUNK = 8,
    MATCH_DIGITS = 9
  };
  if (number == NULL || strlen(number) < 4) {
    return false;
  }
  const char *want = prv_digits_tail(number, MATCH_DIGITS);
  struct zbt_book_entry *chunk = kernel_malloc(CHUNK * sizeof(*chunk));
  if (chunk == NULL) {
    return false;
  }
  bool found = false;
  for (int start = 0; !found; start += CHUNK) {
    const int n = zbt_book_read(ZBT_BOOK_CONTACTS, start, chunk, CHUNK);
    for (int i = 0; i < n && !found; i++) {
      if (chunk[i].name[0] != '\0' &&
          strcmp(prv_digits_tail(chunk[i].number, MATCH_DIGITS), want) == 0) {
        strncpy(name, chunk[i].name, size - 1);
        name[size - 1] = '\0';
        found = true;
      }
    }
    if (n < CHUNK) {
      break;
    }
  }
  kernel_free(chunk);
  return found;
}

int zbt_book_count(enum zbt_book book) {
  if (book >= ZBT_BOOK_COUNT) {
    return 0;
  }
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  struct book_header header;
  const int fd = prv_open_current(book, &header);
  int count = 0;
  if (fd >= 0) {
    count = header.count;
    pfs_close(fd);
  }
  pbl_mutex_unlock(&s_lock);
  return count;
}

int zbt_book_read(enum zbt_book book, int start, struct zbt_book_entry *out, int max) {
  if (book >= ZBT_BOOK_COUNT || start < 0 || max <= 0) {
    return 0;
  }
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  struct book_header header;
  const int fd = prv_open_current(book, &header);
  int n = 0;
  if (fd >= 0) {
    if (start < header.count) {
      n = header.count - start < max ? header.count - start : max;
      const int offset = sizeof(header) + start * sizeof(*out);
      if (pfs_seek(fd, offset, FSeekSet) != offset ||
          pfs_read(fd, out, n * sizeof(*out)) != (int)(n * sizeof(*out))) {
        n = 0;
      }
    }
    pfs_close(fd);
  }
  pbl_mutex_unlock(&s_lock);
  return n;
}
