// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = 10;
a = 2147483647 & 2147483649;
assert(a == 1)