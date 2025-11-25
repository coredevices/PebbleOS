// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var cnt = 0;
do
{
  cnt++;
}
while (!(cnt & 0x8000));

assert(cnt == 32768);
