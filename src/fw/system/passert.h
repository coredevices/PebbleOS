/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "logging.h"

#include <util/attributes.h>
#include <util/likely.h>


#ifdef PBL_LOGS_HASHED
  #include <logging/log_hashing.h>

NORETURN passert_failed_hashed(uint32_t packed_loghash, ...);

NORETURN passert_failed_hashed_with_lr(uint32_t lr,
                                       uint32_t packed_loghash, ...);

NORETURN passert_failed_hashed_no_message(void);

NORETURN passert_failed_hashed_no_message_with_lr(uint32_t lr);

  #define PBL_ASSERT(expr, msg, ...) \
    do { \
      if (UNLIKELY(!(expr))) { \
        NEW_LOG_HASH(passert_failed_hashed, LOG_LEVEL_ALWAYS, LOG_COLOR_RED, \
            "*** ASSERTION FAILED: " msg, \
            ## __VA_ARGS__); \
      } \
    } while (0)

  #define PBL_ASSERTN(expr) \
    do { \
      if (UNLIKELY(!(expr))) { \
        passert_failed_hashed_no_message(); \
      } \
    } while (0)

#define PBL_ASSERTN_LR(expr, lr) \
    do { \
      if (UNLIKELY(!(expr))) { \
        passert_failed_hashed_no_message_with_lr(lr); \
      } \
    } while (0)

#else
  NORETURN passert_failed(const char* filename, int line_number, const char* message, ...);

  #define PBL_ASSERT(expr, ...) \
    do { \
      if (UNLIKELY(!(expr))) { \
        passert_failed(__FILE_NAME__, __LINE__, __VA_ARGS__); \
      } \
    } while (0)

  #define PBL_ASSERTN(expr) \
    do { \
      if (UNLIKELY(!(expr))) { \
        passert_failed_no_message(__FILE_NAME__, __LINE__); \
      } \
    } while (0)

  #define PBL_ASSERTN_LR(expr, lr) \
    do { \
      if (UNLIKELY(!(expr))) { \
        passert_failed_no_message_with_lr(__FILE_NAME__, __LINE__, lr); \
      } \
    } while (0)

#endif

NORETURN passert_failed_no_message(const char* filename, int line_number);

NORETURN passert_failed_no_message_with_lr(const char* filename, int line_number, uint32_t lr);

NORETURN wtf(void);

#define WTF wtf()

#if UNITTEST

#define PBL_ASSERT_TASK(task)
#define PBL_ASSERT_NOT_TASK(task)
#define PBL_ASSERT_RUNNING_FROM_EXPECTED_TASK(task)
#define BREAKPOINT

#else

// Insert a compiled-in breakpoint
#define BREAKPOINT __asm("bkpt")

enum PebbleTask;
void passert_check_task(enum PebbleTask expected_task);
void passert_check_not_task(enum PebbleTask unexpected_task);

#define PBL_ASSERT_TASK(task) passert_check_task(task);
#define PBL_ASSERT_NOT_TASK(task) passert_check_not_task(task);

// It's useful during development to insert asserts to make sure our callbacks
// are being dispatched as expected. It's wasteful (for codespace) to keep them
// on after that as it's only when code gets edited that the assert gets hit.
#ifdef CHECK_RUNNING_FROM_EXPECTED_TASK
#define PBL_ASSERT_RUNNING_FROM_EXPECTED_TASK(task) PBL_ASSERT_TASK(task)
#else
#define PBL_ASSERT_RUNNING_FROM_EXPECTED_TASK(task)
#endif

#endif  // UNITTEST

// extern void command_dump_malloc(void);

#ifdef PBL_LOGS_HASHED

  #define PBL_CROAK(msg, ...) \
    do { \
      NEW_LOG_HASH(passert_failed_hashed, LOG_LEVEL_ALWAYS, LOG_COLOR_RED, "*** CROAK: " msg, \
                   ## __VA_ARGS__); \
    } while (0)

#else // PBL_LOGS_HASHED

  #define PBL_CROAK(fmt, args...) \
      passert_failed(__FILE_NAME__, __LINE__, "*** CROAK: " fmt, ## args)

#endif // PBL_LOGS_HASHED

typedef struct Heap Heap;

NORETURN croak_oom(size_t bytes, int saved_lr, Heap *heap_ptr);

#define PBL_CROAK_OOM(bytes, saved_lr, heap_ptr)                 \
  croak_oom(bytes, saved_lr, heap_ptr)
