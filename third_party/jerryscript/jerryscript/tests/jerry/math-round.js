// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var nan = NaN;
var p_zero = 0.0;
var m_zero = -p_zero;
var p_inf = Infinity;
var m_inf = -p_inf;

assert (isNaN(Math['round'](NaN)));
assert (Math['round'](p_zero) === p_zero);
assert (Math['round'](m_zero) === m_zero);
assert (Math['round'](p_inf) === p_inf);
assert (Math['round'](m_inf) === m_inf);

assert (Math['round'](0.5) === 1.0);
assert (Math['round'](-0.5) === -0.0);
assert (Math['round'](1.2) === 1.0);
assert (Math['round'](1.5) === 2.0);
assert (Math['round'](0.7) === 1.0);
assert (Math['round'](0.2) === 0.0);
assert (Math['round'](-0.2) === -0.0);
assert (Math['round'](-0.7) === -1.0);
assert (Math['round'](-1.2) === -1.0);
assert (Math['round'](-1.7) === -2.0);
assert (Math['round'](-1.5) === -1.0);
