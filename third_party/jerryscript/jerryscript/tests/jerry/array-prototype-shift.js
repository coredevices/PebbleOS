// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var array = ["foo", [], Infinity, 4]

assert(array.length === 4);

assert(array.shift() === "foo");
assert(array.length === 3);

var a = array.shift();
assert(a instanceof Array);
assert(array.length === 2);

assert(array.shift() === Infinity);
assert(array.length === 1);

assert(array.shift() === 4);
assert(array.length === 0);

assert(array.shift() === undefined);
assert(array.length === 0);

var referenceErrorThrower = function () {
  throw new ReferenceError ("foo");
}

// Checking behavior when unable to get length
var obj = { shift : Array.prototype.shift };
Object.defineProperty(obj, 'length', { 'get' : referenceErrorThrower });

try {
  obj.shift();
  assert(false);
} catch (e) {
  assert(e.message === "foo");
  assert(e instanceof ReferenceError);
}

// Checking behavior when unable to set length
var obj = { shift : Array.prototype.shift };
Object.defineProperty(obj, 'length', { 'set' : referenceErrorThrower });

try {
  obj.shift();
  assert(false);
} catch (e) {
  assert(e.message === "foo");
  assert(e instanceof ReferenceError);
}

// Checking behavior when no length property defined
var obj = { shift : Array.prototype.shift };
assert (obj.length === undefined)
assert (obj.shift() === undefined)
assert (obj.length === 0)

// Checking behavior when unable to get element
var obj = { shift : Array.prototype.shift, length : 1 };
Object.defineProperty(obj, '0', { 'get' : referenceErrorThrower });

try {
  obj.shift();
  assert(false);
} catch (e) {
  assert(e.message === "foo");
  assert(e instanceof ReferenceError);
}
