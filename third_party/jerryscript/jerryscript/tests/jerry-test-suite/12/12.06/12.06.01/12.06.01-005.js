// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var x = 1 / 3;
do
{
  x = 1;
}
while (x === 3 / 9);
assert (x === 1);
