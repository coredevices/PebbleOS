/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Installing a .pbw handed over by the host, as a phone would: the app's
// binaries and resources go into the filesystem, its record into the app
// database, and then it launches.

#include "samsa_pbw_bottom.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/logging/logging.h>
#include <pbl/services/app_cache.h>
#include <pbl/services/blob_db/app_db.h>
#include <pbl/services/process_management/app_storage.h>
#include <pbl/services/system_task.h>
#include <pbl/util/size.h>

#include <kernel/event_loop.h>
#include <kernel/events.h>
#include <kernel/pbl_malloc.h>
#include <pbl_arch_posix.h>
#include <process_management/app_install_manager.h>
#include <process_management/app_manager.h>
#include <process_management/pebble_process_info.h>
#include <resource/resource_storage.h>
#include <shell/normal/watchface.h>
#include <uzlib.h>

PBL_LOG_MODULE_DECLARE(samsa, CONFIG_SAMSA_LOG_LEVEL);

// The platform directories of a .pbw to take an app from, best first.
static const char *const s_platforms[] = {
#if defined(CONFIG_PLATFORM_EMERY)
  "emery/",
  "basalt/",
  "",
#elif defined(CONFIG_PLATFORM_GABBRO)
  "gabbro/",
  "chalk/",
#elif defined(CONFIG_PLATFORM_FLINT)
  "flint/",
  "diorite/",
  "",
#else
  "",
#endif
};

typedef struct {
  const uint8_t *data;
  uint32_t compressed_size;
  uint32_t size;
  uint16_t method;
} ZipEntry;

// One install at a time: the host offers the next once this is done.
typedef struct {
  bool busy;
  const uint8_t *pbw;
  size_t len;
  ZipEntry app;
  ZipEntry worker;
  ZipEntry res;
  PebbleProcessInfo info;
  AppInstallId id;
} Install;

static Install s_install;

// ----------------------------------------------------------------------------
// Zip

static uint16_t prv_rd16(const uint8_t *p) {
  return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t prv_rd32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

//! Find a file in the zip's central directory.
static bool prv_zip_find(const uint8_t *zip, size_t len, const char *name, ZipEntry *out) {
  // The end of central directory record, before an optional comment.
  if (len < 22) {
    return false;
  }
  size_t eocd = len - 22;
  while (prv_rd32(zip + eocd) != 0x06054b50) {
    if (eocd == 0 || len - eocd > 22 + 0xffff) {
      return false;
    }
    eocd--;
  }
  const uint16_t count = prv_rd16(zip + eocd + 10);
  size_t at = prv_rd32(zip + eocd + 16);
  const size_t name_len = strlen(name);
  for (uint16_t i = 0; i < count; i++) {
    if (at + 46 > len || prv_rd32(zip + at) != 0x02014b50) {
      return false;
    }
    const uint16_t this_len = prv_rd16(zip + at + 28);
    const size_t next = at + 46 + this_len + prv_rd16(zip + at + 30) + prv_rd16(zip + at + 32);
    if (next > len) {
      return false;
    }
    if (this_len == name_len && memcmp(zip + at + 46, name, name_len) == 0) {
      const size_t local = prv_rd32(zip + at + 42);
      if (local + 30 > len || prv_rd32(zip + local) != 0x04034b50) {
        return false;
      }
      const size_t data = local + 30 + prv_rd16(zip + local + 26) + prv_rd16(zip + local + 28);
      *out = (ZipEntry){
        .data = zip + data,
        .compressed_size = prv_rd32(zip + at + 20),
        .size = prv_rd32(zip + at + 24),
        .method = prv_rd16(zip + at + 10),
      };
      return data + out->compressed_size <= len && (out->method == 0 || out->method == 8);
    }
    at = next;
  }
  return false;
}

//! Inflate or copy a zip entry into a new file, in chunks.
static bool prv_write_file(const char *name, const ZipEntry *entry) {
  pbl_fs_unlink(name);
  struct pbl_fs_file file;
  int ret = pbl_fs_open(&file, name, PBL_FS_O_RDWR | PBL_FS_O_CREATE);
  if (ret == 0) {
    ret = (entry->size > 0) ? pbl_fs_truncate(&file, entry->size) : -EINVAL;
    if (ret < 0) {
      pbl_fs_close(&file);
    }
  }
  if (ret < 0) {
    PBL_LOG_ERR("Could not create %s: %d", name, ret);
    return false;
  }
  bool ok = true;
  if (entry->method == 0) {
    ok = pbl_fs_write(&file, entry->data, entry->size) == (int)entry->size;
  } else {
    // Deflate refers back up to 32 KiB, which the dictionary ring keeps.
    const size_t dict_size = 32 * 1024, chunk_size = 1024;
    uint8_t *dict = kernel_malloc(dict_size);
    uint8_t *chunk = kernel_malloc(chunk_size);
    TINF_DATA *d = kernel_malloc(sizeof(*d));
    ok = dict && chunk && d;
    if (ok) {
      uzlib_init();
      uzlib_uncompress_init(d, dict, dict_size);
      d->source = entry->data;
      d->source_limit = entry->data + entry->compressed_size;
      d->source_read_cb = nullptr;
      size_t written = 0;
      for (;;) {
        d->dest_start = d->dest = chunk;
        d->dest_limit = chunk + chunk_size;
        const int res = uzlib_uncompress(d);
        const size_t n = d->dest - chunk;
        if (n && pbl_fs_write(&file, chunk, n) != (int)n) {
          ok = false;
          break;
        }
        written += n;
        if (res == TINF_DONE) {
          break;
        }
        if (res < 0 || written > entry->size) {
          ok = false;
          break;
        }
      }
      ok = ok && written == entry->size;
    }
    kernel_free(d);
    kernel_free(chunk);
    kernel_free(dict);
  }
  pbl_fs_close(&file);
  if (!ok) {
    PBL_LOG_ERR("Could not write %s", name);
    pbl_fs_unlink(name);
  }
  return ok;
}

// ----------------------------------------------------------------------------
// Installing

static void prv_finish(Install *install) {
  samsa_pbw_release((void *)install->pbw);
  *install = (Install){};
}

//! Write the files and launch, on KernelMain, after the app database's
//! callbacks: those of an upgrade delete the files an earlier version left.
static void prv_install_files(void *data) {
  Install *install = data;
  char name[APP_FILENAME_MAX_LENGTH];
  bool ok = true;
  app_storage_get_file_name(name, sizeof(name), install->id, PebbleTask_App);
  ok = ok && prv_write_file(name, &install->app);
  if (ok && install->worker.data) {
    app_storage_get_file_name(name, sizeof(name), install->id, PebbleTask_Worker);
    ok = prv_write_file(name, &install->worker);
  }
  if (ok && install->res.data) {
    resource_storage_get_file_name(name, sizeof(name), install->id);
    ok = prv_write_file(name, &install->res);
  }
  if (ok) {
    const uint32_t total = install->app.size + install->worker.size + install->res.size;
    ok = app_cache_add_entry(install->id, total) == S_SUCCESS;
  }
  if (ok) {
    PBL_LOG_INFO("Installed %s as app %" PRId32, install->info.name, install->id);
    if (install->info.flags & PROCESS_INFO_WATCH_FACE) {
      watchface_set_default_install_id(install->id);
    }
    app_manager_put_launch_app_event(&(AppLaunchEventConfig){.id = install->id});
  }
  prv_finish(install);
}

static void prv_install(void *data) {
  Install *install = data;
  const char *platform = nullptr;
  char name[48];
  for (unsigned i = 0; i < ARRAY_LENGTH(s_platforms) && !platform; i++) {
    snprintf(name, sizeof(name), "%spebble-app.bin", s_platforms[i]);
    if (prv_zip_find(install->pbw, install->len, name, &install->app)) {
      platform = s_platforms[i];
    }
  }
  if (!platform) {
    PBL_LOG_ERR("No app in the .pbw for this platform");
    prv_finish(install);
    return;
  }
  snprintf(name, sizeof(name), "%spebble-worker.bin", platform);
  prv_zip_find(install->pbw, install->len, name, &install->worker);
  snprintf(name, sizeof(name), "%sapp_resources.pbpack", platform);
  prv_zip_find(install->pbw, install->len, name, &install->res);

  // The app's header is all the app database takes, and it is stored first.
  uint8_t header[sizeof(PebbleProcessInfo)];
  const ZipEntry head = {
    .data = install->app.data,
    .compressed_size = install->app.compressed_size,
    .size = sizeof(header),
    .method = install->app.method,
  };
  bool ok = install->app.size >= sizeof(header);
  if (ok && head.method == 0) {
    memcpy(header, head.data, sizeof(header));
  } else if (ok) {
    uint8_t *dict = kernel_malloc(32 * 1024);
    TINF_DATA *d = kernel_malloc(sizeof(*d));
    ok = dict && d;
    if (ok) {
      uzlib_init();
      uzlib_uncompress_init(d, dict, 32 * 1024);
      d->source = head.data;
      d->source_limit = head.data + head.compressed_size;
      d->source_read_cb = nullptr;
      d->dest_start = d->dest = header;
      d->dest_limit = header + sizeof(header);
      ok = uzlib_uncompress(d) >= 0 && d->dest == d->dest_limit;
    }
    kernel_free(d);
    kernel_free(dict);
  }
  memcpy(&install->info, header, sizeof(install->info));
  if (!ok || strncmp(install->info.header, "PBLAPP", sizeof(install->info.header)) != 0) {
    PBL_LOG_ERR("Not a Pebble app");
    prv_finish(install);
    return;
  }

  AppDBEntry entry = {
    .info_flags = install->info.flags,
    .icon_resource_id = install->info.icon_resource_id,
    .app_version = install->info.process_version,
    .sdk_version = install->info.sdk_version,
  };
  memcpy(&entry.uuid, &install->info.uuid, sizeof(entry.uuid));
  strncpy(entry.name, install->info.name, MIN(sizeof(entry.name), sizeof(install->info.name)));
  const status_t status = app_db_insert((const uint8_t *)&entry.uuid, sizeof(entry.uuid),
                                        (const uint8_t *)&entry, sizeof(entry));
  if (status != S_SUCCESS) {
    PBL_LOG_ERR("Could not add %s to the app database: %" PRId32, install->info.name, status);
    prv_finish(install);
    return;
  }
  install->id = app_db_get_install_id_for_uuid(&entry.uuid);
  launcher_task_add_callback(prv_install_files, install);
}

//! On KernelMain, which takes events once the launcher runs, after the
//! services an install needs have started. The work itself goes to the
//! system task.
static void prv_install_when_ready(void *data) {
  system_task_add_callback(prv_install, data);
}

static void prv_install_isr(void *arg) {
  bool *accepted = arg;
  *accepted = !s_install.busy;
  if (*accepted) {
    s_install.busy = true;
    event_put_isr(&(PebbleEvent){
      .type = PEBBLE_CALLBACK_EVENT,
      .callback = {.callback = prv_install_when_ready, .data = &s_install},
    });
  }
}

bool samsa_pbw_install(const void *pbw, size_t len) {
  bool accepted = false;
  if (!s_install.busy) {
    s_install.pbw = pbw;
    s_install.len = len;
    pbl_posix_irq_run(prv_install_isr, &accepted);
  }
  return accepted;
}
