/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

/**
 * Pool manager interface
 */
#ifndef JMEM_POOLMAN_H
#define JMEM_POOLMAN_H

#include "jrt.h"

/** \addtogroup mem Memory allocation
 * @{
 *
 * \addtogroup poolman Memory pool manager
 * @{
 */

extern void jmem_pools_finalize (void);
extern void *jmem_pools_alloc (size_t);
extern void jmem_pools_free (void *, size_t);
extern void jmem_pools_collect_empty (void);

#ifdef JMEM_STATS
/**
 * Pools' memory usage statistics
 */
typedef struct
{
  /** pools' count */
  size_t pools_count;

  /** peak pools' count */
  size_t peak_pools_count;

  /** non-resettable peak pools' count */
  size_t global_peak_pools_count;

  /** free chunks count */
  size_t free_chunks;

  /* Number of newly allocated pool chunks */
  size_t new_alloc_count;

  /* Number of reused pool chunks */
  size_t reused_count;
} jmem_pools_stats_t;

extern void jmem_pools_get_stats (jmem_pools_stats_t *);
extern void jmem_pools_stats_reset_peak (void);
extern void jmem_pools_stats_print (void);
#endif /* JMEM_STATS */

/**
 * @}
 * @}
 */

#endif /* !JMEM_POOLMAN_H */
