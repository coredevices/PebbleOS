// SPDX-FileCopyrightText: 2015 University of Szeged
// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

// Conversion

assert ("0123456789abcdefghijklmnopqrstuvwxzyABCDEFGHIJKLMNOPQRSTUVWXYZ".toLowerCase()
        == "0123456789abcdefghijklmnopqrstuvwxzyabcdefghijklmnopqrstuvwxyz");
assert ("0123456789abcdefghijklmnopqrstuvwxzyABCDEFGHIJKLMNOPQRSTUVWXYZ".toUpperCase()
        == "0123456789ABCDEFGHIJKLMNOPQRSTUVWXZYABCDEFGHIJKLMNOPQRSTUVWXYZ");

// Although codepoint 0x10400 and 0x10428 are an upper-lowercase pair,
// we must not do their conversion in JavaScript. We must also ignore
// stray surrogates.

assert ("\ud801\ud801\udc00\udc00".toLowerCase() == "\ud801\ud801\udc00\udc00");
assert ("\ud801\ud801\udc28\udc28".toUpperCase() == "\ud801\ud801\udc28\udc28");

// Conversion of non-string objects.

assert (String.prototype.toUpperCase.call(true) == "TRUE");
assert (String.prototype.toLowerCase.call(-23) == "-23");

var object = { toString : function() { return "<sTr>"; } };
assert (String.prototype.toUpperCase.call(object) == "<STR>");
assert (String.prototype.toLowerCase.call(object) == "<str>");

try
{
  String.prototype.toUpperCase.call(null);
  assert(false);
}
catch (e)
{
  assert (e instanceof TypeError);
}
