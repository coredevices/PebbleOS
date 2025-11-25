// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try
{
  String.prototype.search(RegExp.prototype);
  assert (false);
}
catch (e)
{
  assert (e instanceof TypeError);
}
