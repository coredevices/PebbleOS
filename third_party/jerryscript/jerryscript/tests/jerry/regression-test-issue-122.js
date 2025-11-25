// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try
{
  print({toString: function() { throw new TypeError("foo"); }}, []);
  assert (false);
}
catch (e)
{
  assert (e instanceof TypeError);
  assert (e.message === "foo");
}
