// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function check_syntax_error (script)
{
  try
  {
    eval (script);
    assert (false);
  }
  catch (e)
  {
    assert (e instanceof SyntaxError);
  }
}

check_syntax_error ('{');
check_syntax_error ('}');
check_syntax_error ('[');
check_syntax_error (']');
check_syntax_error ('(');
check_syntax_error (')');

check_syntax_error ('function f (');
check_syntax_error ('function f ()');
check_syntax_error ('function f () {');
check_syntax_error ('function f () }');
check_syntax_error ('function f ({) }');
check_syntax_error ('function f { }');
check_syntax_error ('function f {');
check_syntax_error ('function f }');

check_syntax_error ('a = [[];');

check_syntax_error ('a = {;');
check_syntax_error ('a = };');
check_syntax_error ('a = {{};');

check_syntax_error ('a = {get q {} };');
check_syntax_error ('a = {get q ( {} };');
check_syntax_error ('a = {get q ) {} };');
check_syntax_error ('a = {get q () };');
check_syntax_error ('a = {get q () { };');
check_syntax_error ('a = {get q () };');
check_syntax_error ('a = {get q () { };');
