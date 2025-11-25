// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var i = 10;
var cnt = 0;

do
{
  if (i % 2)
    continue;

  var j = 0;
  do
  {
    if (j % 2 === 0)
      continue;
    cnt++;
  }
  while (j++ < 20)
}
while (i-- > 0);

assert(cnt === 60);