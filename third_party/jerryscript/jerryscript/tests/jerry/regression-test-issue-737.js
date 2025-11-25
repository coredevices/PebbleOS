// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function check_syntax_error (s) {
  try {
    eval (s);
    assert (false);
  } catch (e) {
    assert (e instanceof SyntaxError);
  }
}

/* Test case #1 */
check_syntax_error (
" new function f(f) {                                \
  return {className: 'xxx'};                         \
};                                                   \
x = 1;                                               \
function g(active) {                                 \
  for (i = 1; i <= 1000; i++) { if (i == active) {   \
  x = i;   if (f(\"\" + i) != null) { }              \
    } else {                                         \
  if (f(\"\" + i) != null) }                         \
  }                                                  \
}                                                    \
g(0)                                                 \
");

/* Test case #2 */
check_syntax_error (
" new function a(a) {;for (f in [1,2,3]) print(f);   \
}; 1;                                                \
function g(active) {                                 \
  for (i = 1; i <= 1000; i++) { if (i == active) {   \
  xI                                                 \
      if (f != null) { }                             \
    } else {                                         \
  if (f(\"\" + i) != null) }                         \
  }                                                  \
}                                                    \
g(0)                                                 \
");

/* Test case #3 */
check_syntax_error (
" new function f(f) {;for (f in [1,2,3]) pRint(f);   \
}; 1;                                                \
function g(active) {                                 \
  for (i = 1; i <= 1000; i++) { if (i == active) {   \
  x                                                  \
      if (f != null) { }                             \
    } else {                                         \
  if (f(\"\" + i) != null) }                         \
  }                                                  \
}                                                    \
g(0)                                                 \
");
