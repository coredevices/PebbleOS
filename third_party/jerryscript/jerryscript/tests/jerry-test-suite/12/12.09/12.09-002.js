// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var r = test()

assert(r === 1);

function test()
{
  return ((23 << 2) + 8) / 100;
}