// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

parseInt(NaN,RegExp("54"));

try {
    parseInt(ArrayBuffer());
} catch(err) {}

try {
    isNaN(__proto__);
} catch(err) {}

try {
    RangeError(parseInt(Infinity,readbuffer()));
} catch (err) {}
