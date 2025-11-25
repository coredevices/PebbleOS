// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function f_empty()
{
  return;
}

function f_42()
{
  return 42;
}

function f_expr()
{
  var a = 5;
  var b = 5;
  return a + b;
}

assert(f_expr() === 10);
assert(f_42() === 42);