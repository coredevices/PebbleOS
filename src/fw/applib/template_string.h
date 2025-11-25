/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

//! Variables that change how a template string is evaluated
typedef struct TemplateStringVars {
  time_t current_time;
} TemplateStringVars;

//! Variables that are used for determining when the string must be re-evaluated
typedef struct TemplateStringEvalConditions {
  //! If true, string MUST be re-evaluated on eval_time,  regardless of other conditions.
  //! Otherwise, only re-evaluate if all the other conditions are also true.
  bool force_eval_on_time;
  //! Timestamp for next time to re-evaluate the string
  //! If this is 0, there is no need to re-evaluate based on time
  time_t eval_time;
} TemplateStringEvalConditions;

//! Bitfield describing errors in a template string
typedef enum TemplateStringErrorStatus {
  //! No error occurred. `index_in_string` is invalid.
  TemplateStringErrorStatus_Success = 0,
  //! Couldn't resolve the template string to a final string.
  TemplateStringErrorStatus_CantResolve,
  //! Closing curly-brace was missing.
  TemplateStringErrorStatus_MissingClosingBrace,
  //! Missing argument.
  TemplateStringErrorStatus_MissingArgument,
  //! No result was generated.
  TemplateStringErrorStatus_NoResultGenerated,
  //! Unknown filter used.
  TemplateStringErrorStatus_UnknownFilter,
  //! format() was not the last filter.
  TemplateStringErrorStatus_FormatBeforeLast,
  //! Time unit in predicate is invalid.
  TemplateStringErrorStatus_InvalidTimeUnit,
  //! Escape character at end of string.
  TemplateStringErrorStatus_InvalidEscapeCharacter,
  //! Opening parenthesis for filter was missing.
  TemplateStringErrorStatus_MissingOpeningParen,
  //! Closing parenthesis for filter was missing.
  TemplateStringErrorStatus_MissingClosingParen,
  //! Invalid conversion specifier for format.
  TemplateStringErrorStatus_InvalidConversionSpecifier,
  //! Invalid parameter.
  TemplateStringErrorStatus_InvalidParameter,
  //! Opening quote for filter was missing.
  TemplateStringErrorStatus_MissingOpeningQuote,
  //! Closing quote for filter was missing.
  TemplateStringErrorStatus_MissingClosingQuote,
  //! Invalid argument separator.
  TemplateStringErrorStatus_InvalidArgumentSeparator,

  TemplateStringErrorStatusCount,
} TemplateStringErrorStatus;

//! Contains information about a template string error
typedef struct TemplateStringError {
  //! 0-indexed position in the input string where the error occurred.
  size_t index_in_string;
  TemplateStringErrorStatus status;
} TemplateStringError;

//! @param input_template_string The template string to evaluate
//! @param output A pointer to a string buffer for which to write the evaluated string output upon
//!               success. May be NULL for no output
//! @param output_size The size of the output string buffer in bytes; no bytes beyond this size
//!                    will be written to the output string buffer. This should also include the
//!                    ending NUL terminator. May be 0 for no output
//! @param reeval_cond Pointer to a structure that will be filled with information on when to
//!                    re-evaluate the string. Maybe be NULL
//! @param vars Pointer to variables that affect how the template string will be evaluated
//! @param errors Pointer to a TemplateStringError that will be set to contain information about
//!               the first error detected in the template string upon failure
//! @return True if the template string is successfully evaluated and the output written to the
//!         provided output string buffer, false otherwise
bool template_string_evaluate(const char *input_template_string, char *output, size_t output_size,
                              TemplateStringEvalConditions *reeval_cond,
                              const TemplateStringVars *vars, TemplateStringError *error);
