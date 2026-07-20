/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "process_management/process_loader.h"

#include "drivers/flash.h"
#include "kernel/util/segment.h"
#include "process_management/pebble_process_md.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/process_management/app_storage.h"
#include <pbl/logging/logging.h>
#include "system/passert.h"
#include "util/legacy_checksum.h"

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

PBL_LOG_MODULE_DECLARE(service_process_management, CONFIG_SERVICE_PROCESS_MANAGEMENT_LOG_LEVEL);

//! This comes from the generated pebble.auto.c with all the exported functions in it.
extern const void* const g_pbl_system_tbl[];

// ----------------------------------------------------------------------------------------------
static bool prv_verify_checksum(const PebbleProcessInfo* app_info, const uint8_t* data) {
  const size_t header_size = sizeof(PebbleProcessInfo);

  const uint8_t *crc_data = data + header_size;
  const uint32_t app_size = app_info->load_size - header_size;
  uint32_t calculated_crc = legacy_defective_checksum_memory(crc_data,
                                                             app_size);

  if (app_info->crc != calculated_crc) {
    PBL_LOG_WRN("Calculated App CRC is 0x%"PRIx32", expected 0x%"PRIx32"!",
            calculated_crc, app_info->crc);
    return false;
  } else {
    return true;
  }
}

static void * prv_offset_to_address(MemorySegment *segment, size_t offset) {
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

static bool prv_validate_process_info(const PebbleProcessInfo *info,
                                      MemorySegment *destination,
                                      size_t *load_size_out) {
  if (strncmp("PBLAPP", info->header, sizeof(info->header)) != 0) {
    PBL_LOG_WRN("Invalid app header");
    return false;
  }

  const size_t segment_size = memory_segment_get_size(destination);
  const size_t header_size = sizeof(PebbleProcessInfo);
  const size_t image_size = info->load_size;
  const size_t virtual_size = info->virtual_size;

  size_t load_size;
  if (!app_storage_get_process_load_size(info, &load_size)) {
    return false;
  }

  if (image_size < header_size) {
    PBL_LOG_WRN("App image smaller than header: image=%zu header=%zu",
                image_size, header_size);
    return false;
  }

  if (image_size > virtual_size) {
    PBL_LOG_WRN("App image exceeds virtual size: image=%zu virtual=%zu",
                image_size, virtual_size);
    return false;
  }

  if (virtual_size > segment_size) {
    PBL_LOG_WRN("App virtual size exceeds segment: virtual=%zu segment=%zu",
                virtual_size, segment_size);
    return false;
  }

  if (load_size > segment_size) {
    PBL_LOG_ERR("App/Worker exceeds available program space: %"PRIu16" + (%"PRIu32" * 4) = %zu",
                info->load_size, info->num_reloc_entries, load_size);
    return false;
  }

  if (!prv_offset_is_halfword_aligned(info->offset) ||
      !prv_offset_range_fits(info->offset, sizeof(uint16_t), image_size) ||
      (info->offset < header_size)) {
    PBL_LOG_WRN("Invalid app entry point offset: 0x%"PRIx32,
                info->offset);
    return false;
  }

  if (!prv_offset_is_word_aligned(info->sym_table_addr) ||
      !prv_offset_range_fits(info->sym_table_addr, sizeof(uint32_t), virtual_size) ||
      (info->sym_table_addr < header_size)) {
    PBL_LOG_WRN("Invalid app jump table offset: 0x%"PRIx32,
                info->sym_table_addr);
    return false;
  }

  *load_size_out = load_size;
  return true;
}

static bool prv_process_info_matches_md(const PebbleProcessInfo *info,
                                        const PebbleProcessMd *app_md) {
  if (process_metadata_get_size_bytes(app_md) != info->virtual_size) {
    PBL_LOG_WRN("App metadata virtual size mismatch: md=%"PRIu32" info=%"PRIu16,
                process_metadata_get_size_bytes(app_md), info->virtual_size);
    return false;
  }

  if ((uint32_t)(uintptr_t)app_md->main_func != info->offset) {
    PBL_LOG_WRN("App metadata entry point mismatch: md=0x%"PRIx32" info=0x%"PRIx32,
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

static bool prv_apply_relocations(const PebbleProcessInfo *info,
                                  MemorySegment *destination) {
  // Relocation entries point to uint32_t slots in the loaded image
  // Slot values are still relative to the app image
  // Legacy SDK apps can have a non-word-aligned image size, which places the
  // table itself at an unaligned offset, so entries must be read bytewise
  uint8_t *reloc_table = prv_offset_to_address(destination, info->load_size);

  for (uint32_t i = 0; i < info->num_reloc_entries; ++i) {
    // A target has to land on a word boundary inside the image and past the header,
    // otherwise the write below is an out of bounds write in a privileged context
    uint32_t reloc_offset;
    memcpy(&reloc_offset, &reloc_table[i * sizeof(uint32_t)], sizeof(reloc_offset));
    if (!prv_offset_is_word_aligned(reloc_offset) ||
        !prv_offset_range_fits(reloc_offset, sizeof(uint32_t), info->load_size) ||
        (reloc_offset < sizeof(PebbleProcessInfo))) {
      PBL_LOG_WRN("Invalid app relocation target[%"PRIu32"]: 0x%"PRIx32,
                  i, reloc_offset);
      return false;
    }

    uint32_t *addr_to_change = prv_offset_to_address(destination, reloc_offset);
    const uint32_t app_relative_value = *addr_to_change;
    // One past the end of an object is a valid pointer in C, you just cannot deref it
    // A value of exactly virtual_size points there, which is safe here since we never do
    if (app_relative_value > info->virtual_size) {
      PBL_LOG_WRN("Invalid app relocation value[%"PRIu32"]: 0x%"PRIx32,
                  i, app_relative_value);
      return false;
    }

    *addr_to_change = (uint32_t)(uintptr_t)prv_offset_to_address(destination,
                                                                app_relative_value);
  }

  // The reloc table is loaded over the start of .bss, so we have to restore the zeros the app expects
  if (info->num_reloc_entries != 0) {
    memset(reloc_table, 0, info->num_reloc_entries * sizeof(uint32_t));
  }
  return true;
}

// ---------------------------------------------------------------------------------------------
static bool prv_intialize_sdk_process(PebbleTask task, const PebbleProcessInfo *info,
                                      MemorySegment *destination) {
  if (!prv_verify_loaded_header(info, destination->start)) {
    return false;
  }

  if (!prv_verify_checksum(info, destination->start)) {
    PBL_LOG_DBG("Calculated CRC does not match, aborting...");
    return false;
  }

  if (!prv_apply_relocations(info, destination)) {
    return false;
  }

  // Write this after relocations so app relocations can't corrupt the pointer to the SDK table
  uint32_t *pbl_jump_table_addr = prv_offset_to_address(destination, info->sym_table_addr);
  *pbl_jump_table_addr = (uint32_t)(uintptr_t)&g_pbl_system_tbl;

  return true;
}

// ----------------------------------------------------------------------------------------------
static bool prv_load_from_flash(const PebbleProcessMd *app_md, PebbleTask task,
                                MemorySegment *destination) {
  PebbleProcessInfo info;
  AppStorageGetAppInfoResult result;
  AppInstallId app_id = process_metadata_get_code_bank_num(app_md);

  result = app_storage_get_process_info(&info, NULL, app_id, task);

  if (result != GET_APP_INFO_SUCCESS) {
    // Failed to load the app out of flash, this function will have already printed an error.
    return false;
  }

  // We load the full binary (.text + .data) into ram as well as the relocation entries. These
  // relocation entries will overlap with the .bss section of the loaded app, but we'll fix that
  // up later.
  size_t load_size;
  if (!prv_validate_process_info(&info, destination, &load_size) ||
      !prv_process_info_matches_md(&info, app_md)) {
    return false;
  }

  // load the process from the pfs file appX or workerX
  char process_name[APP_FILENAME_MAX_LENGTH];
  int fd;
  app_storage_get_file_name(process_name, sizeof(process_name), app_id, task);

  if ((fd = pfs_open(process_name, OP_FLAG_READ, 0, 0)) < S_SUCCESS) {
    PBL_LOG_ERR("Process open failed for process %s, fd = %d", process_name, fd);
    return (false);
  }

  if (pfs_read(fd, destination->start, load_size) != (int)load_size) {
    PBL_LOG_ERR("Process read failed for process %s, fd = %d", process_name, fd);
    pfs_close(fd);
    return (false);
  }
  pfs_close(fd);

  return prv_intialize_sdk_process(task, &info, destination);
}

// ----------------------------------------------------------------------------------------------
static bool prv_load_from_resource(const PebbleProcessMdResource *app_md,
                                   PebbleTask task,
                                   MemorySegment *destination) {
  PebbleProcessInfo info;
  PBL_ASSERTN(resource_load_byte_range_system(SYSTEM_APP, app_md->bin_resource_id, 0,
        (uint8_t *)&info, sizeof(info)) == sizeof(info));

  // We load the full binary (.text + .data) into ram as well as the relocation entries. These
  // relocation entries will overlap with the .bss section of the loaded app, but we'll fix that
  // up later.
  size_t load_size;
  if (!prv_validate_process_info(&info, destination, &load_size) ||
      !prv_process_info_matches_md(&info, (const PebbleProcessMd *)app_md)) {
    return false;
  }

  // load the process from the resource
  PBL_ASSERTN(resource_load_byte_range_system(SYSTEM_APP, app_md->bin_resource_id, 0,
        destination->start, load_size) == load_size);

  // Process the relocation entries
  return prv_intialize_sdk_process(task, &info, destination);
}

void * process_loader_load(const PebbleProcessMd *app_md, PebbleTask task,
                           MemorySegment *destination) {

  if (app_md->process_storage == ProcessStorageFlash) {
    if (!prv_load_from_flash(app_md, task, destination)) {
      return NULL;
    }
  } else if (app_md->process_storage == ProcessStorageResource) {
    PebbleProcessMdResource *res_app_md = (PebbleProcessMdResource *)app_md;
    if (!prv_load_from_resource(res_app_md, task, destination)) {
      return NULL;
    }
  }

  // The final process image size may be smaller than the amount of
  // memory required to load it, (the relocation table needs to be
  // loaded into memory during load but is not needed after) so the
  // memory segment is split only after loading completes.
  size_t loaded_size = process_metadata_get_size_bytes(app_md);
  if (loaded_size) {
    void *main_func = prv_offset_to_address(
        destination, (uintptr_t)app_md->main_func);
    if (!memory_segment_split(destination, NULL, loaded_size)) {
      return NULL;
    }
    // Set the THUMB bit on the function pointer.
    return (void *)((uintptr_t)main_func | 1);
  } else {
    // No loaded size; must be builtin. The entry point address is
    // already a physical address.
    return app_md->main_func;
  }
}
