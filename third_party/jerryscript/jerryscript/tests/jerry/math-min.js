// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert(isNaN (Math['min'] (1.0, NaN)));
assert(isNaN (Math['min'] (NaN, 1.0)));
assert(isNaN (Math['min'] (-Infinity, NaN)));
assert(isNaN (Math['min'] (NaN, -Infinity)));
assert(Math['min'] (1.0, 3.0, 0.0) === 0.0);
assert(Math['min'] (1.0, 3.0, Infinity) === 1.0);
assert(Math['min'] (1.0, 3.0, -Infinity) === -Infinity);
assert(Math['min'] (-Infinity, Infinity) === -Infinity);
assert(Math['min'] (Infinity, -Infinity) === -Infinity);
assert(Math['min'] (Infinity, Infinity) === Infinity);
assert(Math['min'] (-Infinity, -Infinity) === -Infinity);
assert(Math['min'] () === Infinity);

assert(Math['min'] (0.0, -0.0) === -0.0);
assert(Math['min'] (-0.0, 0.0) === -0.0);

assert(Math['min'] (2, -Infinity) === -Infinity);
assert(Math['min'] (-Infinity, 2) === -Infinity);
assert(Math['min'] (2, Infinity) === 2);
assert(Math['min'] (Infinity, 2) === 2);

assert(Math['min'] (-2, Infinity) === -2);
assert(Math['min'] (Infinity, -2) === -2);
assert(Math['min'] (-2, -Infinity) === -Infinity);
assert(Math['min'] (-Infinity, -2) === -Infinity);
