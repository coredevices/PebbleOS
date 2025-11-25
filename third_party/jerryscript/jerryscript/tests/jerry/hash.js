// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

a = {};
a['12345']=1;
a['13345']=3;
a['sss45']=4;
a['1'] = 2;

assert (a[12345] === 1);
assert (a[1] === 2);
assert (a[13345] === 3);
assert (a['sss45'] === 4);
