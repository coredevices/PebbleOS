// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function main()
{
  var sum = 0;
  for (var i = 0; i < 10; i++)
    for (var j = 0; j < 20; j++)
    {
      if (j === 10)
        break;

      sum += 1;
    }

  assert(sum === 100);
}

main ();
