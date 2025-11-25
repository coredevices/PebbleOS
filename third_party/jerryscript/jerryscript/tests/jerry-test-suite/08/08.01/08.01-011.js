// SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert (test ());

function test (arg)
{
  if (typeof (arg) === "undefined")
    return true;
  else
    return false;
}
