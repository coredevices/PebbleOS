// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var func = new Function("a,b", "c", "return a+b+c")
assert(func(1, 2, 3) == 6);
