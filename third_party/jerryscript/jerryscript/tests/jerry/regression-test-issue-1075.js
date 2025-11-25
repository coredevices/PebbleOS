// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try { new (RegExp)().$()  } catch($){}
try { RegExp.prototype.compile()  } catch($){}
try { new (this.$)(1, RegExp.prototype.compile(RegExp.prototype))  } catch($){}
