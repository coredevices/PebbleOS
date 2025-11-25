// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

// New objects are extensible.
var empty = {};
assert (Object.isExtensible(empty) === true);

// ...but that can be changed.
Object.preventExtensions(empty);
assert(Object.isExtensible(empty) === false);

// Call on undefined should throw TypeError.
try
{
    Object.isExtensible(undefined);
    assert (false);
} catch (e) {
    assert (e instanceof TypeError);
}

try
{
    Object.preventExtensions(undefined);
    assert (false);
} catch (e) {
    assert (e instanceof TypeError);
}

// Sealed objects are by definition non-extensible.
var sealed = Object.seal({});
assert (Object.isExtensible(sealed) === false);

// Frozen objects are also by definition non-extensible.
var frozen = Object.freeze({});
assert(Object.isExtensible(frozen) === false);
