// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try {
    String("test");
    isNaN(__proto__);
} catch(err) {}

try {
    SyntaxError(RegExp("[/]"));
} catch(err) {}
