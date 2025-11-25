// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var count = 0;
[].length = { valueOf: function() { count++; return 1; } };
assert(count == 2);

