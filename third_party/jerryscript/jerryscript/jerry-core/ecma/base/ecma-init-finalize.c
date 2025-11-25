/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ecma-builtins.h"
#include "ecma-gc.h"
#include "ecma-helpers.h"
#include "ecma-init-finalize.h"
#include "ecma-lcache.h"
#include "ecma-lex-env.h"
#include "ecma-literal-storage.h"
#include "jmem-allocator.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmainitfinalize Initialization and finalization of ECMA components
 * @{
 */

/**
 * Initialize ECMA components
 */
void
ecma_init (void)
{
  ecma_lcache_init ();
  ecma_init_global_lex_env ();

  jmem_register_free_unused_memory_callback (ecma_free_unused_memory);
} /* ecma_init */

/**
 * Finalize ECMA components
 */
void
ecma_finalize (void)
{
  jmem_unregister_free_unused_memory_callback (ecma_free_unused_memory);

  ecma_finalize_global_lex_env ();
  ecma_finalize_builtins ();
  ecma_gc_run (JMEM_FREE_UNUSED_MEMORY_SEVERITY_LOW);
  ecma_finalize_lit_storage ();
} /* ecma_finalize */

/**
 * @}
 * @}
 */
