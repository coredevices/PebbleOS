// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try
{
  f1(function(a1){});
  assert (false);
}
catch (e)
{
  assert (e instanceof ReferenceError);
}

var x = {
foo: function () { throw new TypeError("bar"); }
};
