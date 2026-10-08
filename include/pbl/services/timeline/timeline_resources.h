/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/uuid.h>

#include <applib/graphics/gdraw_command_image.h>
#include <applib/graphics/gtypes.h>

/**
 * @defgroup services_timeline_timeline_resources Timeline resources
 * @ingroup services_timeline
 * @brief Resolves timeline resource ids to image resources.
 *
 * Timeline icons are referred to by a TimelineResourceId with a size. Ids with
 * @ref SYSTEM_RESOURCE_FLAG set are system icons, looked up in a table generated with the system
 * resources. Other ids are published by an app: its resource pack holds a lookup table
 * (resource @ref TLUT_RESOURCE_ID, starting with the @ref TLUT_SIGNATURE) of TimelineLutEntry
 * rows indexed by the id.
 * @{
 */

#if !defined(CONFIG_RECOVERY_FW)
#include <resource/timeline_resource_ids.auto.h>
#else
/** @brief Timeline resource id; generated with the resources in normal firmware. */
typedef uint32_t TimelineResourceId;
#endif

/** @brief Flag marking a TimelineResourceId as a system resource. */
#define SYSTEM_RESOURCE_FLAG 0x80000000

/** @brief Major version of the first SDK supporting timeline icons in PBWs. */
#define TIMELINE_RESOURCE_PBW_SUPPORT_FIRST_SDK_VERSION_MAJOR (0x5)
/** @brief Minor version of the first SDK supporting timeline icons in PBWs. */
#define TIMELINE_RESOURCE_PBW_SUPPORT_FIRST_SDK_VERSION_MINOR (0x3d)

/** @brief Timeline resource to resolve. */
typedef struct {
  /** Timeline resource id. */
  TimelineResourceId res_id;
  /** App that published @ref res_id. */
  const Uuid *app_id;
  /** System resource used if @ref res_id cannot be resolved. */
  TimelineResourceId fallback_id;
} TimelineResourceInfo;

/** @brief Resolved image resource. */
typedef struct {
  /** Resource id within the bank. */
  uint32_t res_id;
  /** Resource bank: the system or an app. */
  ResAppNum res_app_num;
} AppResourceInfo;

/** @brief Sizes of timeline icons. */
typedef enum {
  /** 25x25 pixels. */
  TimelineResourceSizeTiny,
  /** 50x50 pixels. */
  TimelineResourceSizeSmall,
  /** 80x80 pixels. */
  TimelineResourceSizeLarge,
  /** Number of sizes. */
  TimelineResourceSizeCount
} TimelineResourceSize;

/** @brief Resource in a resource bank. */
typedef struct {
  /** Resource bank. */
  ResAppNum res_app_num;
  /** Resource id within the bank. */
  uint32_t resource_id;
} AppResourceId;

/** @brief Row of an app's timeline resource lookup table. */
typedef struct {
  /** Resource id of the tiny icon. */
  uint32_t tiny;
  /** Resource id of the small icon. */
  uint32_t small;
  /** Resource id of the large icon. */
  uint32_t large;
} TimelineLutEntry;

/** @brief Signature at the start of an app's timeline resource lookup table. */
#define TLUT_SIGNATURE PBL_FOURCC('T', 'L', 'U', 'T')
/** @brief Offset of the first TimelineLutEntry in the lookup table. */
#define TLUT_DATA_OFFSET sizeof(TLUT_SIGNATURE)
/** @brief Resource id of the lookup table in an app's resource pack. */
#define TLUT_RESOURCE_ID 1

/** @brief Size of tiny timeline icons. */
#define TIMELINE_TINY_RESOURCE_SIZE (GSize(25, 25))
/** @brief Size of small timeline icons. */
#define TIMELINE_SMALL_RESOURCE_SIZE (GSize(50, 50))
/** @brief Size of large timeline icons. */
#define TIMELINE_LARGE_RESOURCE_SIZE (GSize(80, 80))

/**
 * @brief Get the dimensions of an icon size.
 *
 * @param size Icon size.
 * @return Dimensions, GSizeZero for an invalid size.
 */
static inline GSize timeline_resources_get_gsize(TimelineResourceSize size) {
  switch (size) {
    case TimelineResourceSizeTiny:
      return TIMELINE_TINY_RESOURCE_SIZE;
    case TimelineResourceSizeSmall:
      return TIMELINE_SMALL_RESOURCE_SIZE;
    case TimelineResourceSizeLarge:
      return TIMELINE_LARGE_RESOURCE_SIZE;
    default:
      return GSizeZero;
  }
}

/**
 * @brief Check whether a timeline resource id is a system resource.
 *
 * @param timeline_id Timeline resource id.
 * @return true if @ref SYSTEM_RESOURCE_FLAG is set.
 */
bool timeline_resources_is_system(TimelineResourceId timeline_id);

/**
 * @brief Resolve a timeline resource id in a given app resource bank.
 *
 * Does not check that the app was built with an SDK supporting published timeline resources; use
 * timeline_resources_get_id() unless that is already known. App icons must be bitmaps or PDC
 * images of at most 700 bytes.
 *
 * @param timeline_id Timeline resource id.
 * @param size Icon size.
 * @param res_app_num App resource bank, used if @p timeline_id is not a system resource.
 * @param[out] res_info_out Resolved resource, set only on success; may be NULL.
 * @return true if @p timeline_id is valid for @p size and @p res_app_num.
 */
bool timeline_resources_get_id_system(TimelineResourceId timeline_id, TimelineResourceSize size,
                                      ResAppNum res_app_num, AppResourceInfo *res_info_out);

/**
 * @brief Resolve a timeline resource, falling back to its fallback system resource.
 *
 * @param timeline_res Timeline resource and the app that published it.
 * @param size Icon size.
 * @param[out] res_info_out Resolved resource; both fields 0 if neither the resource nor the
 *                          fallback could be resolved.
 */
void timeline_resources_get_id(const TimelineResourceInfo *timeline_res, TimelineResourceSize size,
                               AppResourceInfo *res_info_out);

/**
 * @brief Syscall version of timeline_resources_get_id().
 *
 * Validates the buffers of unprivileged callers.
 *
 * @param timeline_res Timeline resource and the app that published it.
 * @param size Icon size.
 * @param[out] res_info_out Resolved resource; zeroed on invalid arguments.
 */
void sys_timeline_resources_get_id(const TimelineResourceInfo *timeline_res,
                                   TimelineResourceSize size, AppResourceInfo *res_info_out);

/** @} */
