// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = 0x1fffffffe; // % 0x100000000 >= 0x10000000
assert(~a === ~(0xfffffffe))