// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function f_arg() {
}


var x = function () {
  throw "x";
};
var y = function () {
  throw "y";
};
try
{
  f_arg(x(), y());
  assert(false);
}
catch (e)
{
  if (e === "y")
  {
    assert(false);
  } else {
    if (e !== "x")
    {
      assert(false);
    }
  }
}
