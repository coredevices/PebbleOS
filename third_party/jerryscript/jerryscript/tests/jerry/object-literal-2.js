// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = {get a(){return undefined}, set a(b){}}

var b = {if:0, else:1, try:2, catch:3, finally:4, let:5}

assert (b.if + b.else + b.try + b.catch + b.finally + b.let === 15)

function c() {
  "use strict"
  var b = {let:15, enum:10}
  assert (b.let + b.enum === 25)
}
c();

function d () {
  "use strict";

  try {
    /* 'let' is a FutureReservedWord in strict mode code */
    eval ('var a = { get prop () { let = 1; } }');
    assert (false);
  } catch (e) {
    assert (e instanceof SyntaxError);
  }
}
d ();
