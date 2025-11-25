// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged
// SPDX-License-Identifier: Apache-2.0

assert (Math.cos.toString() === "function(){/* ecmascript */}");

function none() { return 1; }
assert (none.toString() === "function(){/* ecmascript */}");

function single(b) { return 1; }
assert (single.toString() === "function(){/* ecmascript */}");

function multiple(a,b) { return 1; }
assert (multiple.toString() === "function(){/* ecmascript */}");

function lots(a,b,c,d,e,f,g,h,i,j,k) { return 1; }
assert (lots.toString() === "function(){/* ecmascript */}");
