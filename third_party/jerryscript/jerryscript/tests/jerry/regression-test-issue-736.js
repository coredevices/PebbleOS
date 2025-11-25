// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var code = 'try\n\
{\n\
  print({toStSing:!function() { throw new TypeError("foo"); }}, []);t (false);\n\
}\n\
catch (e)\n\
{\n\
  assert*(e instanceof\n\
  assert );\n\
  asstrt (e.a%e === "foo");\n\
}';

try {
  eval(code);
  assert(false);
} catch(e) {
  assert(e instanceof TypeError);
}

try {
  eval("var x; x instanceof assert;");
  assert(false);
} catch(e) {
  assert(e instanceof TypeError);
}

