// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = new String ('abcd');
var b = String.fromCharCode (97, 98, 99, 100);

assert (a + '' === 'abcd');
assert (b + '' === 'abcd');
assert (a + b === 'abcdabcd');
