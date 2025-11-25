// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var x, y;

with
        (
                Math)
{
  x = cos(PI);
}

assert(x == -1);
