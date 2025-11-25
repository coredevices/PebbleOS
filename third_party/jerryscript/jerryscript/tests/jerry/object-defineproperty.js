// SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015-2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var obj = [];

Object.defineProperty (obj, "prop", {
    value: 2010,
    writable: true,
    enumerable: true,
    configurable: false
});

assert (obj.hasOwnProperty ("prop"));
function getFunc() {
    return 20;
}

try {
    Object.defineProperty (obj, "prop", {
        get: getFunc
    });
    assert (false);
} catch (e) {
    assert (e instanceof TypeError);
    var desc = Object.getOwnPropertyDescriptor (obj, "prop");
    assert (desc.value === 2010);
    assert (typeof (desc.get) === 'undefined');
}

obj = {};
var setter = function () {};

Object.defineProperty(obj, "prop", {
    set: setter,
    configurable: true
});

var desc1 = Object.getOwnPropertyDescriptor(obj, "prop");

Object.defineProperty(obj, "prop", {
    set: undefined
});

var desc2 = Object.getOwnPropertyDescriptor(obj, "prop");
assert (desc1.set === setter && desc2.set === undefined);

obj = {};

/* This error is thrown even in non-strict mode. */
Object.defineProperty(obj, 'f', {
  set: function(value) { throw 234; },
});

try {
  obj.f = 5;
  assert (false);
} catch (err) {
  assert (err === 234);
}
