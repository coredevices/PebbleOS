// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var nan = NaN;
var p_zero = 0.0;
var m_zero = -p_zero;
var p_inf = Infinity;
var m_inf = -p_inf;

assert (isNaN(Math['abs'](NaN)));
assert (Math['abs'](p_zero) === p_zero);
assert (Math['abs'](m_zero) === p_zero);
assert (Math['abs'](p_inf) === p_inf);
assert (Math['abs'](m_inf) === p_inf);

assert (Math['abs'](0.5) === 0.5);
assert (Math['abs'](-0.5) === 0.5);
assert (Math['abs'](1.2) === 1.2);
assert (Math['abs'](1.5) === 1.5);
assert (Math['abs'](0.7) === 0.7);
assert (Math['abs'](0.2) === 0.2);
assert (Math['abs'](-0.2) === 0.2);
assert (Math['abs'](-0.7) === 0.7);
assert (Math['abs'](-1.2) === 1.2);
assert (Math['abs'](-1.7) === 1.7);
assert (Math['abs'](-1.5) === 1.5);
