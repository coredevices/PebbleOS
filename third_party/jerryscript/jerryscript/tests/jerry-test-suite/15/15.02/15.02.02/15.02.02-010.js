// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = new Object(undefined);
assert(typeof a === 'object' &&
        typeof (Object.getPrototypeOf(a)) === "object" &&
        Object.isExtensible(a));
       