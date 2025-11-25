// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0


/*
 * Check that in case of 'primitive' base, 'this' argument of [[Call]] is the base value, not ToObject (base).
 */
function test_1 ()
{
 'use strict';

 Object.defineProperty (Number.prototype,
                        'getter',
                        { get : function () { return this; }, configurable : true });

 assert ((10).getter === 10);
 assert (typeof ((10).getter) === 'number');

 delete Number.prototype.getter;
}

test_1 ();
