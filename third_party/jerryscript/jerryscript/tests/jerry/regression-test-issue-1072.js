// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try { new (this.$)(new (this.RegExp)().ignoreCase).$ ()  } catch($){}
try { new (this.String)() .constructor.prototype.match()  } catch($){}
try { this.RegExp().compile() } catch($){}
try { this.$(this.RegExp.prototype .compile (this.RegExp.prototype))  } catch($){}
