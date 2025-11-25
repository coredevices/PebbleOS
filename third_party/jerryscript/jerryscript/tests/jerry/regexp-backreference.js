// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var r;

r = new RegExp ("(a)b\\1").exec("aba");
assert (r[0] == "aba");
assert (r[1] == "a");

r = new RegExp ("(a)b\\1").exec("b");
assert (r == undefined);

r = new RegExp ("(a)*b\\1").exec("b");
assert (r[0] == "b");
assert (r[1] == undefined);
