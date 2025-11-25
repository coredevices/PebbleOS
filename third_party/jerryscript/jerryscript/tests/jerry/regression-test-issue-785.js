// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try { RegExp.prototype.toString() } catch (err) {  }
try { isNaN.apply(unescape, JSON.stringify) } catch (err) {  }
try { Array.prototype.forEach(eval) } catch (err) {  }
try { RegExp.prototype.compile(RegExp.prototype) } catch (err) {  }
