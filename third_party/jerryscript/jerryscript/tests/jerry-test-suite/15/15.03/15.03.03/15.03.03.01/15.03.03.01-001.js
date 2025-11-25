// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var obj = Function.prototype;
Function.prototype = function () {
  return "shifted";
};

if (Function.prototype !== obj)
{
  assert(false);
}

try
{
  if (Function.prototype() !== undefined)
  {
    assert(false);
  }
}
catch (e)
{
  assert(false);
}