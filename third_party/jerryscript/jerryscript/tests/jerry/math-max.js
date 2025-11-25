// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert(isNaN (Math['max'] (1.0, NaN)));
assert(isNaN (Math['max'] (NaN, 1.0)));
assert(isNaN (Math['max'] (Infinity, NaN)));
assert(isNaN (Math['max'] (NaN, Infinity)));
assert(Math['max'] (1.0, 3.0, 0.0) === 3.0);
assert(Math['max'] (1.0, 3.0, Infinity) === Infinity);
assert(Math['max'] (1.0, 3.0, -Infinity) === 3.0);
assert(Math['max'] (-Infinity, Infinity) === Infinity);
assert(Math['max'] (Infinity, -Infinity) === Infinity);
assert(Math['max'] (Infinity, Infinity) === Infinity);
assert(Math['max'] (-Infinity, -Infinity) === -Infinity);
assert(Math['max'] () === -Infinity);

assert(Math['max'] (0.0, -0.0) === 0.0);
assert(Math['max'] (-0.0, 0.0) === 0.0);

assert(Math['max'] (2, Infinity) === Infinity);
assert(Math['max'] (Infinity, 2) === Infinity);
assert(Math['max'] (2, -Infinity) === 2);
assert(Math['max'] (-Infinity, 2) === 2);

assert(Math['max'] (-2, Infinity) === Infinity);
assert(Math['max'] (Infinity, -2) === Infinity);
assert(Math['max'] (-2, -Infinity) === -2);
assert(Math['max'] (-Infinity, -2) === -2);
