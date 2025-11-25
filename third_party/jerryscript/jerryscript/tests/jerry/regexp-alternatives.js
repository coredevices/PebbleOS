// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var r;

r = new RegExp ("a|b");
assert (r.exec("a") == "a");

r = new RegExp ("a|b");
assert (r.exec("b") == "b");

r = new RegExp ("a|b|c");
assert (r.exec("b") == "b");

r = new RegExp ("a|b|c");
assert (r.exec("c") == "c");

r = new RegExp ("a|b|c|d");
assert (r.exec("") == undefined);

r = new RegExp ("a|b|c|d");
assert (r.exec("a") == "a");

r = new RegExp ("a|b|c|d");
assert (r.exec("b") == "b");

r = new RegExp ("a|b|c|d");
assert (r.exec("c") == "c");

r = new RegExp ("a|b|c|d");
assert (r.exec("d") == "d");

r = new RegExp ("a|bb|c|d");
assert (r.exec("e") == undefined);

r = new RegExp ("a|bb|c|d");
assert (r.exec("bb") == "bb");

r = new RegExp ("a|bb|c|d");
assert (r.exec("bba") == "bb");

r = new RegExp ("a|bb|c|d");
assert (r.exec("bbbb") == "bb");

r = new RegExp ("a|bb|c|d");
assert (r.exec("a") == "a");

r = new RegExp ("a|bb|c|d");
assert (r.exec("b") == undefined);
