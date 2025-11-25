// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function fact(n)
{
  return n < 2 ? 1 : n * fact(n - 1);
}

switch (fact(5)) {
  case 5 * fact(4):
    break;
  default:
    assert(false);
}