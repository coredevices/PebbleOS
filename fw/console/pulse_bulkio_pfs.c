/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pulse_bulkio_domain_handler.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/fs/pfs.h>
#include <pbl/kernel/compiler.h>
#include <pbl/util/math.h>

#include <kernel/pbl_malloc.h>
#include <system/status_codes.h>
#include <system/status_errno.h>

typedef struct PBL_PACKED PFSStatResp {
  uint8_t flags;
  uint32_t size;
} PFSStatResp;

// Open flags of the protocol
#define PFS_OPEN_READ               (1 << 0)
#define PFS_OPEN_WRITE              (1 << 1)
#define PFS_OPEN_OVERWRITE          (1 << 2)
#define PFS_OPEN_SKIP_HDR_CRC_CHECK (1 << 3)
#define PFS_OPEN_USE_PAGE_CACHE     (1 << 4)

typedef struct PBL_PACKED PFSOpenOptions {
  uint8_t op_flags;
  uint8_t filetype;
  uint32_t start_size;
  char filename[0];
} PFSOpenOptions;

#if !defined(CONFIG_RECOVERY_FW)
static size_t prv_make_path(char *path, const char *name, size_t name_length) {
  name_length = MIN(PBL_FS_NAME_MAX, name_length);
  strcpy(path, "/");
  strncat(path, name, name_length);
  return strlen(path);
}

static int prv_open_file(struct pbl_fs_file *file, void *packet_data, size_t length) {
  PFSOpenOptions *options = packet_data;
  if (length < sizeof(*options)) {
    return -EINVAL;
  }

  char path[sizeof("/") + PBL_FS_NAME_MAX];
  prv_make_path(path, options->filename, length - sizeof(*options));

  uint32_t flags = 0;
  if ((options->op_flags & PFS_OPEN_READ) != 0) {
    flags |= PBL_FS_O_READ;
  }
  if ((options->op_flags & PFS_OPEN_WRITE) != 0) {
    flags |= PBL_FS_O_WRITE | PBL_FS_O_CREATE;
  }
  if ((options->op_flags & PFS_OPEN_OVERWRITE) != 0) {
    flags |= PBL_FS_O_REPLACE;
  }
  if ((options->op_flags & PFS_OPEN_SKIP_HDR_CRC_CHECK) != 0) {
    flags |= PBL_FS_O_PFS_SKIP_HDR_CRC_CHECK;
  }
  if ((options->op_flags & PFS_OPEN_USE_PAGE_CACHE) != 0) {
    flags |= PBL_FS_O_PFS_USE_PAGE_CACHE;
  }

  int ret = pbl_fs_open(file, path, flags);
  if ((ret == 0) && ((flags & (PBL_FS_O_WRITE | PBL_FS_O_REPLACE)) != 0) &&
      (pbl_fs_size(file) == 0)) {
    ret = (options->start_size > 0) ? pbl_fs_truncate(file, options->start_size) : -EINVAL;
    if (ret < 0) {
      pbl_fs_close(file);
    }
  }

  return ret;
}

static int pfs_domain_read(uint8_t *buf, uint32_t address, uint32_t length, void *context) {
  int ret = pbl_fs_seek(context, address, PBL_FS_SEEK_SET);
  if (ret >= 0) {
    ret = pbl_fs_read(context, buf, length);
  }
  return (ret < 0) ? status_from_errno(ret) : ret;
}

static int pfs_domain_write(uint8_t *buf, uint32_t address, uint32_t length, void *context) {
  int ret = pbl_fs_seek(context, address, PBL_FS_SEEK_SET);
  if (ret >= 0) {
    ret = pbl_fs_write(context, buf, length);
  }
  return (ret < 0) ? status_from_errno(ret) : ret;
}

static int pfs_domain_stat(uint8_t *resp, size_t resp_max_len, void *context) {
  const off_t size = pbl_fs_size(context);

  PFSStatResp *stat_resp = (PFSStatResp *)resp;
  *stat_resp = (PFSStatResp){.flags = 0, .size = (size > 0) ? (uint32_t)size : 0};

  return sizeof(PFSStatResp);
}

static status_t pfs_domain_erase(uint8_t *packet_data, size_t length, uint8_t cookie) {
  char path[sizeof("/") + PBL_FS_NAME_MAX];
  prv_make_path(path, (char *)packet_data, length);

  return status_from_errno(pbl_fs_unlink(path));
}

static status_t pfs_domain_open(uint8_t *packet_data, size_t length, void **resp) {
  struct pbl_fs_file *file = kernel_malloc(sizeof(*file));
  if (file == nullptr) {
    return E_OUT_OF_MEMORY;
  }

  int ret = prv_open_file(file, packet_data, length);
  if (ret < 0) {
    kernel_free(file);
    return status_from_errno(ret);
  }

  *resp = file;
  return S_SUCCESS;
}

static status_t pfs_domain_close(void *context) {
  int ret = pbl_fs_close(context);
  kernel_free(context);
  return status_from_errno(ret);
}

PulseBulkIODomainHandler pulse_bulkio_domain_pfs = {
  .id = PulseBulkIODomainType_PFS,
  .open_proc = pfs_domain_open,
  .read_proc = pfs_domain_read,
  .write_proc = pfs_domain_write,
  .close_proc = pfs_domain_close,
  .stat_proc = pfs_domain_stat,
  .erase_proc = pfs_domain_erase
};
#endif
