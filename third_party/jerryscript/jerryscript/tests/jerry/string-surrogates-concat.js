// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var str0 = "𐐀";
var str1 = "\ud801\udc00";
var str2 = "\ud801";
var str3 = "\udc00";

var str_concat = str2 + str3;

assert(str0 == str_concat);
assert(str1 == str_concat);
