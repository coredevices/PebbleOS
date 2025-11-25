// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var i = 9;
var cnt = 0;

while (i-- > 0)
{
  if (i % 2)
    break;

  var j = 0;
  while (j++ < 20)
  {
    if (j % 2 === 0)
      break;
    cnt++;
  }

}

assert(cnt === 1);