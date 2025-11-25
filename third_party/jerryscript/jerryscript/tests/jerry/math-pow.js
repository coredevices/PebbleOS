// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert ( isNaN (Math.pow (0.0 /* any number */, NaN)) );
assert ( Math.pow (NaN, 0.0) === 1.0 );
// assert ( Math.pow (NaN, -0.0) === 1.0 );
assert ( isNaN (Math.pow (NaN, 1.0 /* any non-zero number */)) );
assert ( Math.pow (2.0, Infinity) === Infinity );
assert ( Math.pow (2.0, -Infinity) === 0.0 );
assert ( isNaN (Math.pow (1.0, Infinity)) );
assert ( isNaN (Math.pow (1.0, -Infinity)) );
assert ( Math.pow (0.5, Infinity) === 0.0 );
assert ( Math.pow (0.5, -Infinity) === Infinity );
assert ( Math.pow (Infinity, 1.0) === Infinity );
assert ( Math.pow (Infinity, -1.0) === 0 );
assert ( Math.pow (-Infinity, 3.0) === -Infinity );
assert ( Math.pow (-Infinity, 2.0) === Infinity );
assert ( Math.pow (-Infinity, 2.5) === Infinity );
// assert ( Math.pow (-Infinity, -3.0) === -0.0 );
assert ( Math.pow (-Infinity, -2.0) === 0.0 );
assert ( Math.pow (-Infinity, -2.5) === 0.0 );
assert ( Math.pow (0.0, 1.2) === 0.0 );
assert ( Math.pow (0.0, -1.2) === Infinity );
// assert ( Math.pow (-0.0, 3.0) === -0.0 );
// assert ( Math.pow (-0.0, 2.0) === 0.0 );
// assert ( Math.pow (-0.0, 2.5) === 0.0 );
// assert ( Math.pow (-0.0, -3.0) === -Infinity );
// assert ( Math.pow (-0.0, -2.0) === Infinity );
// assert ( Math.pow (-0.0, -2.5) === Infinity );
assert ( isNaN (Math.pow (-3, 2.5)) );

assert(Math.pow (-2, 2) === 4);
assert(Math.pow (2, 2) === 4);

assert(Math.pow (2, 3) === 8);
assert(Math.pow (-2, 3) === -8);

assert(Math.pow (5, 3) === 125);
assert(Math.pow (-5, 3) === -125);
