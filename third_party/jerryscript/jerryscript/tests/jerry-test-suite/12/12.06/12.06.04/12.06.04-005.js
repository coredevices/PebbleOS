// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0


var b = {basep: "base"};

function dConstr()
{
  this.derivedp = "derived";
}
dConstr.prototype = b;

var d = new dConstr();

for (var p in d)
{
  d[p] += "A";
}

assert(d.basep === "baseA" && d.derivedp === "derivedA");
