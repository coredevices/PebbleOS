// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = {}, b = 0;

while (a[b]) {
  assert (false);
}

for ( ; a[b]; ) {
  assert (false);
}

var flag = false;
do
{
  assert (!flag);
  flag = true;
} while (a[b]);

a = { };
a.b = { c : 1 };

with (a.b)
{
  assert (c === 1);
}
