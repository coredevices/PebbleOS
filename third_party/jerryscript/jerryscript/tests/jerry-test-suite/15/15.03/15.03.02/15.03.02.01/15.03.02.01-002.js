// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

try
{
  Function('a', 'a', '"use strict";');
  assert(false);
}
catch (e)
{
}
