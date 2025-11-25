/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef JRT_BIT_FIELDS_H
#define JRT_BIT_FIELDS_H

/**
 * Extract a bit-field.
 *
 * @param type type of container
 * @param container container to extract bit-field from
 * @param lsb least significant bit of the value to be extracted
 * @param width width of the bit-field to be extracted
 * @return bit-field's value
 */
#define JRT_EXTRACT_BIT_FIELD(type, container, lsb, width) \
  (((container) >> lsb) & ((((type) 1) << (width)) - 1))

/**
 * Set a bit-field.
 *
 * @param type type of container
 * @param container container to insert bit-field to
 * @param new_bit_field_value value of bit-field to insert
 * @param lsb least significant bit of the value to be inserted
 * @param width width of the bit-field to be inserted
 * @return bit-field's value
 */
#define JRT_SET_BIT_FIELD_VALUE(type, container, new_bit_field_value, lsb, width) \
  (((container) & ~(((((type) 1) << (width)) - 1) << (lsb))) | (((type) new_bit_field_value) << (lsb)))

#endif /* !JRT_BIT_FIELDS_H */
