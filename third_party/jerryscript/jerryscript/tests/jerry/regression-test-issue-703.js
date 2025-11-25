// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

function f(a, b) {
  return {
    a: a,
    b: b
  };
}

var o = f('1', '2');

assert(o.a == '1');
assert(o.b == '2');
