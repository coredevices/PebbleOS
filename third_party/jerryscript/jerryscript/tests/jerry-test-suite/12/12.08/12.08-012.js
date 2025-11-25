// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var i = 10;
var cnt = 0;

do
{
  var j = 0;
  do
  {
    if (j % 2 === 0)
      break;
    cnt++;
  }
  while (j++ < 20)

  if (i % 2)
    break;
}
while (i-- > 0);

assert(cnt === 0);
