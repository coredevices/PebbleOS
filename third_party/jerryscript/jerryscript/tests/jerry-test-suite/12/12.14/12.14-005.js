// SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function test ()
{
    try {
        throw "error";
    } catch (e) {
        return true;
    } finally {
    }

    return false;
}

assert (test ());
