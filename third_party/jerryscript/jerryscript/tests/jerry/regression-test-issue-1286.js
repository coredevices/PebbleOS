// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var builtinEval = eval;
var eval = builtinEval.bind(undefined, "context");

var context = "global";
function checkIfDirectEval() {
  var context = "function";
  return (eval() == "function");
}

assert (!checkIfDirectEval());

