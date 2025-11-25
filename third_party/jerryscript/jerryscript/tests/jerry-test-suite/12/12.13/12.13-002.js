// SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function test ()
{
  try {
    while (true) {
      throw "error";
    }
  } catch (e) {
    return true;
  }
  return false;
}

assert (test ());
