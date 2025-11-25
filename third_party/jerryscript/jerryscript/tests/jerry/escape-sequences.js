// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert ('abcd\
efgh' === 'abcdefgh');

assert ('\'' === "'");
assert ("\'" === "'");
assert ('\"' === '"');
assert ("\"" === '"');

//
// TODO
//   Extend the test by verifying character codes after String.charCodeAt would be implemented
//
assert ((new String ('\\')).length === 1);
assert ((new String ('\b')).length === 1);
assert ((new String ('\f')).length === 1);
assert ((new String ('\n')).length === 1);
assert ((new String ('\r')).length === 1);
assert ((new String ('\t')).length === 1);
assert ((new String ('\v')).length === 1);

// 'p' is not SingleEscapeCharacter
assert ('\p' === 'p');

var v\u0061riable = 'valu\u0065';
assert (variable === 'value');
