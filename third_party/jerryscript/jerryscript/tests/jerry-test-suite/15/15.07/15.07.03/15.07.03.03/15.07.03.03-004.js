// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var b = Number.MIN_VALUE
Number.MIN_VALUE = 0
assert(Number.MIN_VALUE === b);
