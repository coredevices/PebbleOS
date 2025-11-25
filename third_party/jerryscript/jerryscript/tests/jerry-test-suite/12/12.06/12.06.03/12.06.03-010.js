// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

for
        (
                var i = 0
                ;
                i < 10
                ;
                i++
                )
{
  i++;
}

assert(i == 10);
