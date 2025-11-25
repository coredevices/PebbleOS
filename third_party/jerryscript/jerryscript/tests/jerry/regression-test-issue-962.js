// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

function re_test (pattern, string, expected)
{
  assert ((new RegExp(pattern)).exec(string) == expected);
}

re_test("[\\u0020]", "u", null);
re_test("[\\u0020]", " ", " ");
re_test("[\\u0020]", "x", null);

re_test("[\\x20]", "u", null);
re_test("[\\x20]", " ", " ");
re_test("[\\x20]", "x", null);
