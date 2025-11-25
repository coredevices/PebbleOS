// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try {
  var v;
  Object.getPrototypeOf(v);
  assert (false);
} catch (e) {
  assert (e instanceof TypeError);
}

try {
  Object.getPrototypeOf("foo");
  assert (false);
} catch (e) {
  assert (e instanceof TypeError);
}

try {
  Object.getPrototypeOf(60);
  assert (false);
} catch (e) {
  assert (e instanceof TypeError);
}

try {
  var y = Object.getPrototypeOf(null);
  assert (false);
} catch (e) {
  assert (e instanceof TypeError);
}

var obj = { x : "foo" };
assert (Object.getPrototypeOf(obj) === Object.prototype);

var constructor = function () {};
constructor.prototype = obj;

var d_obj = new constructor();
assert (Object.getPrototypeOf(d_obj) === obj);

obj = Object.create(null);
assert (Object.getPrototypeOf(obj) === null);
