// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function f (arg1, arg2, arg3)
{
  this.string = arg1;
  this.number = arg2;
  this.boolean = arg3;
}

var this_arg = {};

f.call (this_arg, 's', 1, true, null);

assert (this_arg.string  === 's');
assert (this_arg.number  === 1);
assert (this_arg.boolean === true);
