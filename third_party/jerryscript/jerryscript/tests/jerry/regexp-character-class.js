// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var r;

r = new RegExp ("[abc]*").exec("aaabbcccabcacbacabacbacab");
assert (r == "aaabbcccabcacbacabacbacab");

r = new RegExp ("[abc]*").exec("aaabbcccabdcacb");
assert (r == "aaabbcccab");

r = new RegExp ("[abc]*").exec("defghjklmnopqrstuvwxyz");
assert (r == "");

r = new RegExp ("[a-z]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "abcdefghjklmnopqrstuvwxyz");

r = new RegExp ("[A-Z]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "");

r = new RegExp ("[^a-z]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "");

r = new RegExp ("[^A-Z]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "abcdefghjklmnopqrstuvwxyz");

r = new RegExp ("\\d*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "");

r = new RegExp ("\\D*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "abcdefghjklmnopqrstuvwxyz");

r = new RegExp ("\\w*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "abcdefghjklmnopqrstuvwxyz");

r = new RegExp ("\\W*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "");

r = new RegExp ("\\s*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "");

r = new RegExp ("\\S*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "abcdefghjklmnopqrstuvwxyz");

r = new RegExp ("[\\d]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "");

r = new RegExp ("[\\D]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "abcdefghjklmnopqrstuvwxyz");

r = new RegExp ("[\\w]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "abcdefghjklmnopqrstuvwxyz");

r = new RegExp ("[\\W]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "");

r = new RegExp ("[\\s]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "");

r = new RegExp ("[\\S]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "abcdefghjklmnopqrstuvwxyz");

r = new RegExp ("[^\\d]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "abcdefghjklmnopqrstuvwxyz");

r = new RegExp ("[^\\D]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "");

r = new RegExp ("[^\\w]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "");

r = new RegExp ("[^\\W]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "abcdefghjklmnopqrstuvwxyz");

r = new RegExp ("[^\\s]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "abcdefghjklmnopqrstuvwxyz");

r = new RegExp ("[^\\S]*").exec("abcdefghjklmnopqrstuvwxyz");
assert (r == "");

r = new RegExp ("\\d*").exec("0123456789");
assert (r == "0123456789");

try
{
  r = new RegExp("[");
  assert (false);
}
catch (e)
{
  assert (e instanceof SyntaxError);
}

try
{
  r = new RegExp("[\\");
  assert (false);
}
catch (e)
{
  assert (e instanceof SyntaxError);
}
