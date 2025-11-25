// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

if (Function.propertyIsEnumerable('prototype'))
{
  assert(false);
}

var count = 0;

for (p in Function)
{
  if (p === "prototype")
    count++;
}

if (count !== 0)
{
  assert(false);
}
