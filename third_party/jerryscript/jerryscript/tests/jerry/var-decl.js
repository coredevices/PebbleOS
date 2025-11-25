// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert (x === undefined);
assert (y === undefined);
assert (z === undefined);
assert (i === undefined);
assert (j === undefined);
assert (k === undefined);
assert (q === undefined);
assert (v === undefined);

eval ('var n');
eval ('var m = 1');

try
{
  x = p;
  assert (false);
}
catch (e)
{
  assert (e instanceof ReferenceError);
}

{
  var y;
}
var x = y;

do var z; while (0);

for (var i, j = function () {var p;}; i === undefined; i = null)
{
}

for (var q in {})
{
}

{ var v = 1 }

try
{
  var k
  l
  assert (false)
}
catch (e)
{
  assert (e instanceof ReferenceError);
}
