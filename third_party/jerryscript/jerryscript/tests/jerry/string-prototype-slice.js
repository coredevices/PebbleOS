// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var str = "universe";
var res;

res = str.slice();
assert (res === "universe");

res = str.slice(1, 6);
assert (res === "niver");

res = str.slice("a", "-4");
assert (res === "univ");

res = str.slice(-5);
assert (res === "verse");

res = str.slice(-12, undefined);
assert (res === "universe");

res = str.slice(undefined, -20);
assert (res === "");

res = str.slice(undefined, undefined);
assert (res === "universe");

res = str.slice(Infinity, NaN);
assert (res === "");

res = str.slice(-Infinity, Infinity);
assert (res === "universe");

res = str.slice(NaN, -Infinity);
assert (res === "");

res = str.slice(false, true);
assert (res === "u");

var x;
res = str.slice(x, x);
assert (res === "universe");

var obj = {y: "foo"};
var arr = [x, x];
res = str.slice(obj, arr);
assert (res === "");
