// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var obj1 = {};
obj1.toString = function () { return "mystring"; }

assert (obj1.toLocaleString() === "mystring");

var obj2 = {a: 3};
assert (obj2.toLocaleString() === "[object Object]");


var obj3 = {toLocaleString: function() { throw ReferenceError ("foo"); }};
try {
  obj3.toLocaleString();

  assert (false);
} catch (e) {
  assert (e.message === "foo");
  assert (e instanceof ReferenceError);
}

// Test invalid toString call
var obj4 = {toString: 2};
try {
  obj4.toLocaleString();

  assert (false);
} catch (e) {
  assert (e instanceof TypeError);
}

// Test undefined toString call
var obj5 = {};
var obj6;
obj5.toString = obj6
try {
  obj5.toLocaleString();

  assert (false);
} catch (e) {
  assert (e instanceof TypeError);
}
