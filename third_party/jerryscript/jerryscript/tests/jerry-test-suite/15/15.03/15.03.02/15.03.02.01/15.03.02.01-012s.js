// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

"use strict";

try
{
  Function('a', 'a', 'return;');
}
catch (e)
{
  assert(false);
}
