/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef ECMA_LCACHE_H
#define ECMA_LCACHE_H

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmalcache Property lookup cache
 * @{
 */

extern void ecma_lcache_init (void);
extern void ecma_lcache_insert (ecma_object_t *, ecma_string_t *, ecma_property_t *);
extern ecma_property_t *ecma_lcache_lookup (ecma_object_t *, const ecma_string_t *);
extern void ecma_lcache_invalidate (ecma_object_t *, ecma_string_t *, ecma_property_t *);

/**
 * @}
 * @}
 */

#endif /* !ECMA_LCACHE_H */
