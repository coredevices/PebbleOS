// SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function d () {
  throw "exception";
}
function c () {
  d ();
}
function b () {
  c ();
}
function a () {
  b ();
}

function test ()
{
  try {
    a ();
  } catch (e) {
    return true;
  }
  return false;
}

assert (test ());
