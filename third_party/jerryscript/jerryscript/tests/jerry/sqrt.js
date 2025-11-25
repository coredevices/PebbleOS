// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert(isNaN(Math['sqrt'] (NaN)));
assert(isNaN(Math['sqrt'] (-1.0)));
assert(isNaN(Math['sqrt'] (-Infinity)));
assert(Math['sqrt'] (0.0) === 0.0);
assert(Math['sqrt'] (Infinity) === Infinity);

assert(Math['sqrt'] (1.0) === 1.0);
assert(Math['sqrt'] (2.0) >= Math['SQRT2'] * 0.999999);
assert(Math['sqrt'] (2.0) <= Math['SQRT2'] * 1.000001);
assert(Math['sqrt'] (0.5) >= Math['SQRT1_2'] * 0.999999);
assert(Math['sqrt'] (0.5) <= Math['SQRT1_2'] * 1.000001);

var sqrt_1e38 = Math['sqrt'] (1.0e+38);
assert(sqrt_1e38 > 0.999999 * 1.0e+19);
assert(sqrt_1e38 < 1.000001 * 1.0e+19);

var sqrt_1e38 = Math['sqrt'] (1.0e-38);
assert(sqrt_1e38 > 0.999999 * 1.0e-19);
assert(sqrt_1e38 < 1.000001 * 1.0e-19);
