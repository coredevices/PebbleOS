/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// ${name}
///////////////////////////////////////////

#if (${check_size} && ${min_sdk} <= 2 && ${size_2x} > 0)
_Static_assert(sizeof(${name}) <= ${size_2x}, "<${name}> is too large for 2.x");
#endif

#if (${check_size} && ${size_3x} > 0)
_Static_assert(sizeof(${name}) <= ${size_3x}, "<${name}> is too large for 3.x");
_Static_assert(sizeof(${name}) + ${total_3x_padding} == ${size_3x},
               "<${name}> is incorrectly padded for 3.x, "
               "total padding: ${total_3x_padding} total size: ${size_3x}");
#endif

void *_applib_type_malloc_${name}(void) {
#if defined(MALLOC_INSTRUMENTATION)
  register uintptr_t lr __asm("lr");
  const uintptr_t saved_lr = lr;
#else
  const uintptr_t saved_lr = 0;
#endif
  return prv_malloc(prv_find_size(ApplibType_${name}), saved_lr);
}

void *_applib_type_zalloc_${name}(void) {
#if defined(MALLOC_INSTRUMENTATION)
  register uintptr_t lr __asm("lr");
  const uintptr_t saved_lr = lr;
#else
  const uintptr_t saved_lr = 0;
#endif
  return prv_zalloc(prv_find_size(ApplibType_${name}), saved_lr);
}

size_t _applib_type_size_${name}(void) {
  return prv_find_size(ApplibType_${name});
}
