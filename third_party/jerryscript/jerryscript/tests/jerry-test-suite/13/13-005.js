// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var foo = 1; //override

assert (foo === 1);

function foo()
{
    return 1;
}
