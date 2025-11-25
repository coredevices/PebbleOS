// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = 'abcd';
var b = 'dfgh';

c = a + b;

assert(c == a + b);
assert(c === a + b);
assert(c != a);
assert(c != b);
assert(c !== a);
assert(c !== b);
assert(c != b + a);
assert(c !== b + a);
