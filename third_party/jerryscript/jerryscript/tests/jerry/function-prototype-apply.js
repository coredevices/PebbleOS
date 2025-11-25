// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

// See a general usage: number addition.
function addNum ()
{
  var sum = 0;
  for(var i = 0; i < arguments.length; i++)
  {
    sum += arguments[i];
  }
  return sum;
}

var array = [6720, 4, 42];
var obj;

obj = addNum.apply(obj, array);
assert (obj === 6766);

// If the arguments are missing.
obj = addNum.apply();
assert (obj === 0);

obj = addNum.apply(obj);
assert (obj === 0);

// If the function is a builtin.
assert (Math.min.apply(null, array) === 4);
assert (Math.max.apply(null, array) === 6720);

// If the function can't be used as caller.
try {
  obj = new Function.prototype.apply();
  assert (false);
} catch (e) {
  assert (e instanceof TypeError);
}

// If the called function throws an error.
function throwError ()
{
  throw new ReferenceError ("foo");
}

try {
  obj = throwError.apply(obj, array);
  assert (false);
} catch (e) {
  assert (e.message === "foo");
  assert (e instanceof ReferenceError);
}

// If the array access throws an error.
Object.defineProperty(array, '0', { 'get' : function () { throw new ReferenceError ("foo"); } });

try {
  obj = addNum.apply(obj, array);
  assert (false);
} catch (e) {
  assert (e.message === "foo");
  assert (e instanceof ReferenceError);
}
