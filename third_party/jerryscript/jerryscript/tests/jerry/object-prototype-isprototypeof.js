// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

function Function_A() { }
function Function_B() { }
Function_B.prototype = new Function_A();

function Function_C() { }
Function_C.prototype = new Function_B();

function Function_D() { }
Function_D.prototype = new Function_C();

var d_instance = new Function_D();

assert (Function_A.prototype.isPrototypeOf (d_instance) === true)
assert (Function_A.prototype.isPrototypeOf (Array) === false)


assert (Function_A.prototype.isPrototypeOf.call(0, 0) === false);
assert (Function_A.prototype.isPrototypeOf.call(Function_A, 0) === false);

assert (Function.prototype.isPrototypeOf (Object) === true)
