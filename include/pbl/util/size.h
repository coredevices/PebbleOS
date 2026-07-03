/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Calculate the length of an array, based on the size of the element type.
//! @param array The array to be evaluated.
//! @return The length of the array.
#define ARRAY_LENGTH(array) (sizeof((array))/sizeof((array)[0]))

//! Calculate the length of a literal array based on the size of the given type
//! This is usable in contexts that require compile time constants
//! @param type Type of the elements
//! @param array Literal definition of the array
//! @return Length of the array in bytes
#define STATIC_ARRAY_LENGTH(type, array) (sizeof((type[]) array) / sizeof(type))

#define MEMBER_SIZE(type, member) sizeof(((type *)0)->member)
