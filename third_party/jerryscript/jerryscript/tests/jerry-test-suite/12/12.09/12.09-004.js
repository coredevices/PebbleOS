// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var r = test()

assert(r == undefined);

function test()
{
  var r = 1;
  return
  r;
}