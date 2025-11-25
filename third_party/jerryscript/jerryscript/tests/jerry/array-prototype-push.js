// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var len;
var d = [];
assert (d.length === 0);
len = d.push();
assert (d.length === 0);
assert (d.length === len);
len = d.push(1);
assert (d.length === 1);
assert (d.length === len);
len = d.push(2);
assert (d.length === 2);
assert (d.length === len);
len = d.push('a');
assert (d.length === 3);
assert (d.length === len);
len = d.push('b', 'c', 3);
assert (d.length == 6);
assert (d.length === len);
assert (d[0] === 1);
assert (d[1] === 2);
assert (d[2] === 'a');
assert (d[3] === 'b');
assert (d[4] === 'c');
assert (d[5] === 3);

var a = [];
a.length = 4294967294;
assert(a.push("x") === 4294967295);
assert(a.length === 4294967295);
assert(a[4294967294] === "x");

try {
  a.push("y");
  assert(false);
} catch (e) {
  assert (e instanceof RangeError);
}
assert(a.length === 4294967295)


var o = { length : 4294967294, push : Array.prototype.push };
assert(o.push("x") === 4294967295);
assert(o.length === 4294967295);
assert(o[4294967294] === "x");

try {
  assert(o.push("y") === 4294967296);
} catch (e) {
  assert(false);
}
assert(o.length === 4294967296);
assert(o[4294967295] === "y");

try {
  assert(o.push("z") === 1);
} catch (e) {
  assert(false);
}
assert(o.length === 1);
assert(o[0] === "z");
