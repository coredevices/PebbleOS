// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var obj = new function foo()
{
  this.prop = 1;
};

assert(obj.prop === 1);
