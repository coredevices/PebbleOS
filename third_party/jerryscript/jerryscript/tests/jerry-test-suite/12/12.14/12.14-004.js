// SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function test ()
{
  try {
    throw "error";
  } catch (e) {
    return false;
  } finally {
    return true;
  }

  return false;
}

assert (test ());
