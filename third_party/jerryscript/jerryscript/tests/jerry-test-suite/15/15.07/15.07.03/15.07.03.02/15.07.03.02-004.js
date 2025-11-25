// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var b = Number.MAX_VALUE;
Number.MAX_VALUE = 0;
assert(Number.MAX_VALUE === b);