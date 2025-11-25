// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

assert ([].join() === "");
assert ([1].join() === "1");
assert ([1, 2].join() === "1,2");


assert ([].join('--') === "");
assert ([1].join("--") === "1");
assert ([1, 2].join('--') === "1--2");

assert ([1,2,3].join({toString: function() { return "--"; }}) === "1--2--3");


// Join should use 'length' to as the number of elements int the array.
var lst = [1,2,3,4];
lst.length = 3;
assert (lst.join() === [1,2,3].join());

// Checking behavior when unable to get length.
var obj = {};
Object.defineProperty(obj, 'length', { 'get' : function () {throw new ReferenceError ("foo"); } });
obj.join = Array.prototype.join;

try {
  obj.join();
  // Should not be reached.
  assert (false);
} catch (e) {
  assert (e.message === "foo");
  assert (e instanceof ReferenceError);
}

// Check join argument fail.
try {
  [1,2,3].join({toString: function() { throw new ReferenceError ("foo"); }});
  // Should not be reached.
  assert (false);
} catch (e) {
  assert (e.message === "foo");
  assert (e instanceof ReferenceError);
}

// Check single join element fail.
try {
  [1, 2, {toString: function() { throw new ReferenceError ("foo"); }}, 4].join();
  // Should not be reached.
  assert (false);
} catch (e) {
  assert (e.message === "foo");
  assert (e instanceof ReferenceError);
}

// Check join on different object.
var obj_2 = {};
obj_2.length = 3;
obj_2[0] = 1;
obj_2[1] = 2;
obj_2[2] = 3;
obj_2[3] = 4;

obj_2.join = Array.prototype.join;

assert (obj_2.join() === "1,2,3");
