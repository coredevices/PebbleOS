// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var cond = 12;
var res;

res = (cond < 13)
        ?
        1
        :
        2;

assert(res == 1)