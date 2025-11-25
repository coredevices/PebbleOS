// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a, b, c, res;

res = (a = 39, b = null, c = 12.5);

assert(a == 39 && b == null && c == 12.5 && res == 12.5)
