// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function f1(x)
{
  var c1 = (x >= 1);
  var c2 = (x <= 10);

  if (c1 === true)
  {
    if (c2 === true)
    {
      assert(t);

      return;
    }
  }

  assert(t === false);
}

var i;
var t = true;

for(i = 1; i <= 10; i++)
{
  f1(i);
}

t = false;

for(i = 11; i <= 20; i++)
{
  f1(i);
}

function g (p, p) {
  assert (p === arguments[1]);
  assert (p === 'second');
}

g ('first', 'second');

try {
  f1 ({});
  f1 (undefined_variable);
  assert (false);
}
catch (e) {
  assert (e instanceof ReferenceError);
}

function f2 ()
{
  return this;
}

with ({})
{
  assert (f2 () === this);
}
