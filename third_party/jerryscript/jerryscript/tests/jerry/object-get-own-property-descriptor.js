// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var obj;
var desc;

obj = { bar: 42 };
desc = Object.getOwnPropertyDescriptor (obj, 'bar');
// desc is { configurable: true, enumerable: true, value: 42, writable: true }
assert (desc.value === 42);
assert (desc.configurable);
assert (desc.writable);
assert (desc.enumerable);

obj = { "foo": "bar" };
desc = Object.getOwnPropertyDescriptor (obj, 'foo');
assert (desc.value === "bar");

obj = {};
Object.defineProperty (obj, 'baz', { value: 8675309, writable: false, enumerable: false });
desc = Object.getOwnPropertyDescriptor (obj, 'baz');
// desc is { value: 8675309, writable: false, enumerable: false, configurable: false }
assert (desc.value === 8675309);
assert (!desc.configurable);
assert (!desc.writable);
assert (!desc.enumerable);

obj = { "0.00001": 1 };
desc = Object.getOwnPropertyDescriptor (obj, 1e-5);
assert ( desc.value === 1);

obj = { "123": 1 };
desc = Object.getOwnPropertyDescriptor (obj, 123);
assert (desc.value === 1);

obj = { "undefined": 1 };
var desc1 = Object.getOwnPropertyDescriptor (obj, undefined);
var desc2 = Object.getOwnPropertyDescriptor (obj, "undefined");
assert (desc1.value === 1 && desc2.value === 1);

obj = { "0": 1 };
desc = Object.getOwnPropertyDescriptor (obj, -0);
assert (desc.value === 1);

obj = { "Hellobj": 42 };
desc = Object.getOwnPropertyDescriptor (obj, new String ("Hellobj"));
assert (desc.value === 42);

obj = {};
desc = Object.getOwnPropertyDescriptor (obj, 'baz');
assert (desc === undefined);

obj = { get foo() { return obj.bar; }, set foo(a) { obj.bar = a; }, bar: 0 };
desc = Object.getOwnPropertyDescriptor(obj, 'foo');
// d is { configurable: true, enumerable: true, get: /*the getter function*/, set: /*the setter function*/ }
assert (typeof(desc.get) === 'function');
assert (typeof(desc.set) === 'function');
assert (desc.configurable);
assert (desc.enumerable);
assert (obj.foo === 0)

var array_desc = Object.getOwnPropertyDescriptor(Array, "prototype");
assert (array_desc.configurable === false);
assert (array_desc.writable === false);
assert (array_desc.enumerable === false);

var obj_undef;
try {
    Object.getOwnPropertyDescriptor (obj_undef, "fail");
    assert (false);
} catch (e) {
    assert (e instanceof TypeError);
}
