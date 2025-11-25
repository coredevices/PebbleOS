// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var t;

t = /\//.exec("/");
assert (t == "/");

t = /[/]/.exec("/");
assert ("a"+/x/+"b" == "a/x/b");

t = /\/\[[\]/]/.exec("/[/");
assert (t == "/[/");

t = /\u0000/.exec("\u0000");
assert (t == "\u0000");

try {
  eval("/" + String.fromCharCode("0x0000") + "/");
} catch (e) {
  assert (false);
}

try {
  eval("var x = 5\n\n/foo/");
  assert(false);
} catch (e) {
  assert(e instanceof SyntaxError);
}

try {
  eval("var x = 5;\n\n/foo/");
} catch (e) {
  assert(false);
}

try {
  eval("for (;false;/abc/.exec(\"abc\")) {5}");
} catch (e) {
  assert(false);
}

try {
  eval("var a = [] /foo/");
  assert(false);
} catch (e) {
  assert(e instanceof SyntaxError);
}

try {
  eval("/");
  assert(false);
} catch (e) {
  assert(e instanceof SyntaxError);
}

try {
  eval("var x = /aaa/");
} catch (e) {
  assert (false);
}

try {
  eval("{}/a/g");
} catch (e) {
  assert (false);
}

try {
  eval("var a, g; +{}/a/g");
} catch (e) {
  assert (false);
}
