/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <pbl/crc/crc.h>
#include <pbl/drivers/flash.h>
#include <pbl/fs/fs.h>
#include <pbl/loader/loader.h>
#include <pbl/logging/logging.h>
#include <pbl/services/process_management/app_storage.h>

#include <kernel/util/segment.h>
#include <process_management/pebble_process_md.h>
#include <process_management/process_loader.h>
#include <process_management/process_runtime.h>

PBL_LOG_MODULE_DECLARE(service_process_management, CONFIG_SERVICE_PROCESS_MANAGEMENT_LOG_LEVEL);

// ----------------------------------------------------------------------------------------------
static bool prv_verify_checksum(const PebbleProcessInfo *app_info, const uint8_t *data) {
  const uint8_t *crc_data = data + PROCESS_INFO_CRC_START_OFFSET;
  const uint32_t app_size = process_info_get_load_size(app_info) - PROCESS_INFO_CRC_START_OFFSET;
  uint32_t calculated_crc = pbl_crc32_legacy(crc_data, app_size);

  if (app_info->crc != calculated_crc) {
    PBL_LOG_WRN("Calculated App CRC is 0x%" PRIx32 ", expected 0x%" PRIx32 "!", calculated_crc,
                app_info->crc);
    return false;
  } else {
    return true;
  }
}

static void *prv_offset_to_address(MemorySegment *segment, size_t offset) {
  return (char *)segment->start + offset;
}

static bool prv_offset_range_fits(size_t offset, size_t size, size_t limit) {
  return (offset <= limit) && (size <= (limit - offset));
}

static bool prv_offset_is_word_aligned(size_t offset) {
  return ((offset & (sizeof(uint32_t) - 1)) == 0);
}

static bool prv_offset_is_halfword_aligned(size_t offset) {
  return ((offset & (sizeof(uint16_t) - 1)) == 0);
}

static size_t prv_header_size(const PebbleProcessInfo *info) {
  const Version first_wide = {
    .major = PROCESS_INFO_FIRST_WIDE_SIZE_STRUCT_VERSION_MAJOR,
    .minor = PROCESS_INFO_FIRST_WIDE_SIZE_STRUCT_VERSION_MINOR,
  };
  return version_compare(info->struct_version, first_wide) < 0 ? 0x82 : sizeof(*info);
}

static bool prv_validate_process_info(const PebbleProcessInfo *info) {
  if (strncmp("PBLAPP", info->header, sizeof(info->header)) != 0) {
    PBL_LOG_WRN("Invalid app header");
    return false;
  }

  const size_t header_size = prv_header_size(info);
  const size_t image_size = process_info_get_load_size(info);
  const size_t virtual_size = process_info_get_virtual_size(info);

  if (!prv_offset_is_halfword_aligned(info->offset) ||
      !prv_offset_range_fits(info->offset, sizeof(uint16_t), image_size) ||
      (info->offset < header_size)) {
    PBL_LOG_WRN("Invalid app entry point offset: 0x%" PRIx32, info->offset);
    return false;
  }

  if (!prv_offset_is_word_aligned(info->sym_table_addr) ||
      !prv_offset_range_fits(info->sym_table_addr, sizeof(uint32_t), virtual_size) ||
      (info->sym_table_addr < header_size)) {
    PBL_LOG_WRN("Invalid app jump table offset: 0x%" PRIx32, info->sym_table_addr);
    return false;
  }

  return true;
}

static bool prv_process_info_matches_md(const PebbleProcessInfo *info,
                                        const PebbleProcessMd *app_md) {
  const uint32_t virtual_size = process_info_get_virtual_size(info);
  if (process_metadata_get_size_bytes(app_md) != virtual_size) {
    PBL_LOG_WRN("App metadata virtual size mismatch: md=%" PRIu32 " info=%" PRIu32,
                process_metadata_get_size_bytes(app_md), virtual_size);
    return false;
  }

  if ((uint32_t)(uintptr_t)app_md->main_func != info->offset) {
    PBL_LOG_WRN("App metadata entry point mismatch: md=0x%" PRIx32 " info=0x%" PRIx32,
                (uint32_t)(uintptr_t)app_md->main_func, info->offset);
    return false;
  }

  return true;
}

static bool prv_verify_loaded_header(const PebbleProcessInfo *info, const uint8_t *data) {
  if (memcmp(info, data, sizeof(PebbleProcessInfo)) == 0) {
    return true;
  }

  PBL_LOG_WRN("App header changed while loading");
  return false;
}

static uint32_t prv_runtime_address(void *ctx, const void *addr) {
  return process_runtime_address(*(const PebbleTask *)ctx, addr);
}

// ---------------------------------------------------------------------------------------------
static bool prv_load_sdk_process(const PebbleProcessInfo *info, const PebbleProcessMd *app_md,
                                 PebbleTask task, const struct pbl_loader_source *src,
                                 MemorySegment *destination) {
  if (!prv_validate_process_info(info) || !prv_process_info_matches_md(info, app_md)) {
    return false;
  }

  // The relocation table follows the image, and is read over the start of .bss
  const struct pbl_loader_image img = {
    .file_size = process_info_get_load_size(info),
    .mem_size = process_info_get_virtual_size(info),
    .header_size = prv_header_size(info),
    .reloc_offset = process_info_get_load_size(info),
    .reloc_count = info->num_reloc_entries,
  };
  const size_t segment_size = memory_segment_get_size(destination);

  if (pbl_loader_copy(&img, src, destination->start, segment_size) < 0) {
    return false;
  }

  if (!prv_verify_loaded_header(info, destination->start)) {
    return false;
  }

  if (!prv_verify_checksum(info, destination->start)) {
    PBL_LOG_DBG("Calculated CRC does not match, aborting...");
    return false;
  }

  const int ret =
      pbl_loader_relocate(&img, src, destination->start, segment_size, prv_runtime_address, &task);
  if (ret < 0) {
    return false;
  }

  // Write this after relocations so app relocations can't corrupt the pointer to the SDK table
  uint32_t *pbl_jump_table_addr = prv_offset_to_address(destination, info->sym_table_addr);
  *pbl_jump_table_addr = process_runtime_sdk_table(task);

  return true;
}

// ----------------------------------------------------------------------------------------------
static int prv_file_read(void *ctx, size_t offset, void *buf, size_t len) {
  struct pbl_fs_file *file = ctx;
  const off_t pos = pbl_fs_seek(file, (off_t)offset, PBL_FS_SEEK_SET);
  if (pos < 0) {
    return (int)pos;
  }
  if ((size_t)pos != offset) {
    return -EIO;
  }

  const ssize_t ret = pbl_fs_read(file, buf, len);
  if (ret < 0) {
    return (int)ret;
  }
  if ((size_t)ret != len) {
    return -EIO;
  }

  return 0;
}

static bool prv_load_from_flash(const PebbleProcessMd *app_md, PebbleTask task,
                                MemorySegment *destination) {
  PebbleProcessInfo info;
  AppStorageGetAppInfoResult result;
  AppInstallId app_id = process_metadata_get_code_bank_num(app_md);

  result = app_storage_get_process_info(&info, nullptr, app_id, task);

  if (result != GET_APP_INFO_SUCCESS) {
    // Failed to load the app out of flash, this function will have already printed an error.
    return false;
  }

  // load the process from the pfs file appX or workerX
  char process_name[APP_FILENAME_MAX_LENGTH];
  struct pbl_fs_file file;
  app_storage_get_file_name(process_name, sizeof(process_name), app_id, task);

  int ret = pbl_fs_open(&file, process_name, PBL_FS_O_READ);
  if (ret < 0) {
    PBL_LOG_ERR("Process open failed for process %s, ret = %d", process_name, ret);
    return false;
  }

  const struct pbl_loader_source src = {.read = prv_file_read, .ctx = &file};
  const bool loaded = prv_load_sdk_process(&info, app_md, task, &src, destination);
  pbl_fs_close(&file);

  return loaded;
}

// ----------------------------------------------------------------------------------------------
static int prv_resource_read(void *ctx, size_t offset, void *buf, size_t len) {
  const uint32_t *resource_id = ctx;
  if (resource_load_byte_range_system(SYSTEM_APP, *resource_id, offset, buf, len) != len) {
    return -EIO;
  }

  return 0;
}

static bool prv_load_from_resource(const PebbleProcessMdResource *app_md, PebbleTask task,
                                   MemorySegment *destination) {
  uint32_t resource_id = app_md->bin_resource_id;
  const struct pbl_loader_source src = {.read = prv_resource_read, .ctx = &resource_id};

  PebbleProcessInfo info;
  if (prv_resource_read(&resource_id, 0, &info, sizeof(info)) < 0) {
    PBL_LOG_WRN("App resource read failed");
    return false;
  }

  return prv_load_sdk_process(&info, (const PebbleProcessMd *)app_md, task, &src, destination);
}

void *process_loader_load(const PebbleProcessMd *app_md, PebbleTask task,
                          MemorySegment *destination) {
  if (app_md->process_storage == ProcessStorageFlash) {
    if (!prv_load_from_flash(app_md, task, destination)) {
      return nullptr;
    }
  } else if (app_md->process_storage == ProcessStorageResource) {
    PebbleProcessMdResource *res_app_md = (PebbleProcessMdResource *)app_md;
    if (!prv_load_from_resource(res_app_md, task, destination)) {
      return nullptr;
    }
  }

  // The final process image size may be smaller than the amount of
  // memory required to load it, (the relocation table needs to be
  // loaded into memory during load but is not needed after) so the
  // memory segment is split only after loading completes.
  size_t loaded_size = process_metadata_get_size_bytes(app_md);
  if (loaded_size) {
    void *main_func = prv_offset_to_address(destination, (uintptr_t)app_md->main_func);
    if (!memory_segment_split(destination, nullptr, loaded_size)) {
      return nullptr;
    }
    // Set the THUMB bit on the function pointer.
    return (void *)((uintptr_t)main_func | 1);
  } else {
    // No loaded size; must be builtin. The entry point address is
    // already a physical address.
    return app_md->main_func;
  }
}
