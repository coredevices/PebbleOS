// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

function check_syntax_error (code) {
  try {
    eval(code);
    assert (false);
  } catch (e) {
    assert (e instanceof SyntaxError);
  }
}

check_syntax_error ("function eval () {'use strict';}");
check_syntax_error ("function f (eval) {'use strict';}");
check_syntax_error ("function arguments () {'use strict';}");
check_syntax_error ("function f (arguments) {'use strict';}");

check_syntax_error ("(function eval () {'use strict';})");
check_syntax_error ("(function f (eval) {'use strict';})");
check_syntax_error ("(function arguments () {'use strict';})");
check_syntax_error ("(function f (arguments) {'use strict';})");
