/* SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2015-2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "js-parser-internal.h"

/** \addtogroup parser Parser
 * @{
 *
 * \addtogroup jsparser JavaScript
 * @{
 *
 * \addtogroup jsparser_bytecode Bytecode
 * @{
 */

#define CBC_OPCODE(arg1, arg2, arg3, arg4) \
  ((arg2) | (((arg3) + CBC_STACK_ADJUST_BASE) << CBC_STACK_ADJUST_SHIFT)),

/**
 * Flags of the opcodes.
 */
const uint8_t cbc_flags[] JERRY_CONST_DATA =
{
  CBC_OPCODE_LIST
};

/**
 * Flags of the extended opcodes.
 */
const uint8_t cbc_ext_flags[] =
{
  CBC_EXT_OPCODE_LIST
};

#undef CBC_OPCODE

#ifdef PARSER_DUMP_BYTE_CODE

#define CBC_OPCODE(arg1, arg2, arg3, arg4) #arg1,

/**
 * Names of the opcodes.
 */
const char * const cbc_names[] =
{
  CBC_OPCODE_LIST
};

/**
 * Names of the extended opcodes.
 */
const char * const cbc_ext_names[] =
{
  CBC_EXT_OPCODE_LIST
};

#undef CBC_OPCODE

#endif /* PARSER_DUMP_BYTE_CODE */

/**
 * @}
 * @}
 * @}
 */
