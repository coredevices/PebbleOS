// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert((5 == 5) == true);
assert((7 != 2) == true);

var num = 0;
//var obj = new String("0");
var str = "0";
var b = false;

assert(num === num);
//assert(obj === obj);
assert(str === str);

//assert((num === obj) == false);
assert((num === str) == false);
//assert((obj === str) == false);
//assert((null === undefined) == false);
//assert((obj === null) == false);
//assert((obj === undefined) == false);
