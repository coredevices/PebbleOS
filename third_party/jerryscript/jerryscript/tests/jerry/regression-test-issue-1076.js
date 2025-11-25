// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try { Array.prototype.unshift(1)  } catch($){}
try { Object.freeze(this.Array.prototype)()  } catch($){}
try { new String(1).split("")  } catch($){}
