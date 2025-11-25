// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var array = ["foo", [], Infinity, 4]

function f(arg1, arg2, arg3) {
  assert(arg1 === array[arg2]);
  assert(arg3 === array);
  return true;
}

var filtered = array.filter(f);
assert(filtered.length === array.length);
for (i = 0; i < filtered.length; i++) {
  assert(filtered[i] === array[i]);
}

var array = [1, 2, 3, 4, 5, 6, 7, 8];

function g (arg1, arg2, arg3) {
  if (arg2 % 2 === 0) {
    return true;
  } else {
    return false;
  }
}

filtered = array.filter(g)
assert(filtered.length === 4);
assert(filtered[0] === 1);
assert(filtered[1] === 3);
assert(filtered[2] === 5);
assert(filtered[3] === 7);

var arr = [1,2];
Array.prototype[0] = 3;
var newArr = arr.filter(function() { return true; });
delete Array.prototype[0];
assert(newArr.hasOwnProperty("0"));
assert(newArr[0] === 1);

// Checking behavior when unable to get length
var obj = {};
Object.defineProperty(obj, 'length', { 'get' : function () {throw new ReferenceError ("foo"); } });
obj.filter = Array.prototype.filter;

try {
  obj.filter(f);
  assert(false);
} catch (e) {
  assert(e.message === "foo");
  assert(e instanceof ReferenceError);
}

// Checking behavior when unable to get element
var obj = {}
obj.length = 1;
Object.defineProperty(obj, '0', { 'get' : function () {throw new ReferenceError ("foo"); } });
obj.filter = Array.prototype.filter

try {
  obj.filter(f);
  assert(false);
} catch (e) {
  assert(e.message === "foo");
  assert(e instanceof ReferenceError);
}
