// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try { String.prototype(new (RegExp)())  } catch($){}
try { RegExp.prototype.compile() } catch($){}
try { isNaN(RegExp.prototype.compile(RegExp.prototype)) } catch($){}
