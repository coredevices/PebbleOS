// SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function test ()
{
    try {
        throw 1;
    } catch (e) {
        return (e === 1);
    }

    return false;
}

assert (test ());
