// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function f() {
    return 'foo';
}
assert ((function() {
    if (1 === 0) {
        function f() {
            return 'bar';
        }
    }
    return f();
})() === 'bar');

function check_syntax_error (s) {
  try {
    eval (s);
    assert (false);
  }
  catch (e) {
    assert (e instanceof SyntaxError);
  }
}

check_syntax_error ("'use strict'; function arguments () {}");
check_syntax_error ("'use strict'; var l = function arguments () {}");

check_syntax_error ("function f__strict_mode_duplicate_parameters (p, p) { 'use strict'; }");

function test_strict_mode_propagation_in_func_expr_and_getters_setters () {
  var p = function () {
    'use strict';

    return true;
  }

  var o = { get prop () { 'use strict'; return true; }, set prop (v) { 'use strict'; } };

  function test () {
    tmp_eval = eval;
    eval = tmp_eval;
  }
}
