// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try
{
  // This should be failed in ECMA-262 v5.1
  eval("({ get 1() {}, 1:1 })");
  assert (false);
}
catch (e)
{
  assert (e instanceof SyntaxError);
}
