// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert(isNaN(Math['exp'] (NaN)));
assert(Math['exp'] (0.0) === 1.0);
assert(Math['exp'] (Infinity) === Infinity);
assert(Math['exp'] (-Infinity) === 0.0);

assert(Math['exp'] (1) >= 0.999999 * Math['E']);
assert(Math['exp'] (1) <= 1.000001 * Math['E']);

assert(Math['exp'] (-1) >= 0.999999 * (1 / Math['E']));
assert(Math['exp'] (-1) <= 1.000001 * (1 / Math['E']));

assert(Math['exp'] (0.5) >= 0.999999 * 1.6487212707);
assert(Math['exp'] (0.5) <= 1.000001 * 1.6487212707);

assert(Math['exp'] (30) >= 0.999999 * 1.06864745815e+13);
assert(Math['exp'] (30) <= 1.000001 * 1.06864745815e+13);
