// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var b = Number.NEGATIVE_INFINITY;
Number.NEGATIVE_INFINITY = 0;
assert(Number.NEGATIVE_INFINITY === b);
