// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = 21;
var b = 10;
var c;

c = a + b;
assert(c == 31);

c = a - b;
assert(c == 11);

c = a * b;
assert(c == 210);

c = a / b;
assert(c >= 2.1 - 0.000001 && c <= 2.1 + 0.000001);

c = a % b;
assert(c == 1);

c = a++;
assert(c == 21);

c = a--;
assert(c == 22);

var o = { p : 1 };

assert (++o.p === 2);
assert (o.p === 2);
assert (--o.p === 1);
assert (o.p === 1);

try {
  eval ('++ ++ a');
  assert (false);
}
catch (e) {
  assert (e instanceof ReferenceError);
}
