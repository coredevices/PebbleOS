// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

v_0 = /(?!(?!l{666,}))/;

try
{
  v_0.exec("llllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllll");
}
catch (e)
{
  assert (e instanceof RangeError);
  assert (e.message === "RegExp executor steps limit is exceeded.");
}
