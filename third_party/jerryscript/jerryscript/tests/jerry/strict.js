// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

'use strict';

var temp;

try
{
  a = 1;

  assert (false);
} catch (e)
{
  assert (e instanceof ReferenceError);
}

try
{
  NaN = 1;

  assert (false);
} catch (e)
{
  assert (e instanceof TypeError);
}

function f()
{
  assert(this === undefined);
}

f();

Object.function_prop = function ()
{
  assert (this === Object);
}

Object.function_prop ();

try
{
  var temp = f.caller;

  assert (false);
} catch (e)
{
  assert (e instanceof TypeError);
}

try
{
  delete this.NaN;

  assert (false);
} catch (e)
{
  assert (e instanceof TypeError);
}

(function (a) {
  (function (a) {
  });
});
