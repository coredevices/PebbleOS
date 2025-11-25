// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var obj = {
  prop: function() {},
  foo: 'bar'
};

// New properties may be added, existing properties may be changed or removed
obj.foo = 'baz';
obj.lumpy = 'woof';
delete obj.prop;

var o = Object.freeze(obj);

assert(Object.isFrozen(obj) === true);

// Now any changes will fail
obj.foo = 'quux'; // silently does nothing
assert (obj.foo === 'baz');

obj.quaxxor = 'the friendly duck'; // silently doesn't add the property
assert (obj.quaxxor === undefined);

// ...and in strict mode such attempts will throw TypeErrors
function fail(){
  'use strict';

  try {
    obj.foo = 'sparky'; // throws a TypeError
    assert (false);
  } catch (e) {
    assert (e instanceof TypeError);
  }

  try {
    delete obj.foo; // throws a TypeError
    assert (false);
  } catch (e) {
    assert (e instanceof TypeError);
  }

  try {
    obj.sparky = 'arf'; // throws a TypeError
    assert (false);
  } catch (e) {
    assert (e instanceof TypeError);
  }
}

fail();

// Attempted changes through Object.defineProperty will also throw

try {
  Object.defineProperty(obj, 'ohai', { value: 17 }); // throws a TypeError
  assert (false);
} catch (e) {
  assert (e instanceof TypeError);
}

try {
  Object.defineProperty(obj, 'foo', { value: 'eit' }); // throws a TypeError
  assert (false);
} catch (e) {
  assert (e instanceof TypeError);
}
