// SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015-2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try
{
  v_0 = new RegExp("N(?![^6](?:.)|(?!C[^k-o]*|p){0,}|H)|\\\\xDF\\db{0,}|i\\\\0?)");
  assert (false);
}
catch (e)
{
  assert (e instanceof SyntaxError);
}
