// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = 5;
var b = a << -1;
assert(b == -2147483648) // -2^31