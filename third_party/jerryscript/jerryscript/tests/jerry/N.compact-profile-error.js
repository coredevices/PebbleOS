// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var catched = false;

function f1()
{
  var arguments = 1;
}

try
{
  f1();
} catch (e)
{
  assert (e === CompactProfileError);

  catched = true;
}

assert(catched);

catched = false;

function f2()
{
  var a = arguments;
}

try
{
  f2();
} catch (e)
{
  assert (e === CompactProfileError);

  catched = true;
}

assert(catched);

catched = false;

try
{
  eval('abc');
} catch (e)
{
  assert (e === CompactProfileError);

  catched = true;
}

assert(catched);

catched = false;

try
{
  Function('abc');
} catch (e)
{
  assert (e === CompactProfileError);

  catched = true;
}

assert(catched);

catched = false;

try
{
  new Function('abc');
} catch (e)
{
  assert (e === CompactProfileError);

  catched = true;
}

assert(catched);

catched = false;

try
{
  var a = Date.now();
} catch (e)
{
  assert (e === CompactProfileError);

  catched = true;
}

assert(catched);
