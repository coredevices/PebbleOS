// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var a = "foo", r;
switch(a) {
    case true ? "foo" : "bar":
        r = "OK";
        break;
}
assert(r === "OK");
