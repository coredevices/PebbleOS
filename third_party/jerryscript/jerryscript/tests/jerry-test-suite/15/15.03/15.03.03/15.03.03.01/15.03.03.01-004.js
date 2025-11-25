// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function foo() {
}

Object.defineProperty(foo, 'prototype', {value: {}});
assert(foo.prototype ===
        Object.getOwnPropertyDescriptor(foo, 'prototype').value);
    