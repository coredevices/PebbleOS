/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

extern bool clar_expecting_passert;
extern bool clar_passert_occurred;
extern jmp_buf clar_passert_jmp_buf;

typedef enum ClarCmpOp {
  ClarCmpOp_EQ,
  ClarCmpOp_LE,
  ClarCmpOp_LT,
  ClarCmpOp_GE,
  ClarCmpOp_GT,
  ClarCmpOp_NE,
} ClarCmpOp;

void clar__assert(
	int condition,
	const char *file,
	int line,
	const char *error,
	const char *description,
	int should_abort);

void clar__assert_equal_s(const char *,const char *,const char *,int,const char *,int);
void clar__assert_equal_i(int,int,const char *,int,const char *,int);
void clar__assert_equal_d(double,double,const char *,int,const char *,int);
void clar__assert_equal_m(uint8_t *, uint8_t *, int, const char *, int, const char *, int);
void clar__assert_within(int,int,int,const char *,int,const char *,int);
void clar__assert_near(int,int,int,const char *,int,const char *,int);
void clar__assert_cmp_i(int,int,ClarCmpOp,const char *,int,const char *,int);

uintmax_t clar__mock(const char *const func, const char *const file, const size_t line);
void clar__will_return(const char *const func, const char *const file, const size_t line,
                       const uintmax_t value, const ssize_t count);

void cl_set_cleanup(void (*cleanup)(void *), void *opaque);
void cl_fs_cleanup(void);

#ifdef CLAR_FIXTURE_PATH
const char *cl_fixture(const char *fixture_name);
void cl_fixture_sandbox(const char *fixture_name);
void cl_fixture_cleanup(const char *fixture_name);
#endif

#define CL_IN_CATEGORY(CAT)

/**
 * Assertion macros with explicit error message
 */
#define cl_must_pass_(expr, desc) do { \
  long long int result = (expr); \
  if (result < 0) { \
    printf("Got failing result %lld\n", result); \
    clar__assert(false, __FILE__, __LINE__, "Function call failed: " #expr, desc, 1); \
  } \
} while (0)
#define cl_must_fail_(expr, desc) clar__assert((expr) < 0, __FILE__, __LINE__, "Expected function call to fail: " #expr, desc, 1)
#define cl_assert_(expr, desc) clar__assert((expr) != 0, __FILE__, __LINE__, "Expression is not true: " #expr, desc, 1)

/**
 * Check macros with explicit error message
 */
#define cl_check_pass_(expr, desc) clar__assert((expr) >= 0, __FILE__, __LINE__, "Function call failed: " #expr, desc, 0)
#define cl_check_fail_(expr, desc) clar__assert((expr) < 0, __FILE__, __LINE__, "Expected function call to fail: " #expr, desc, 0)
#define cl_check_(expr, desc) clar__assert((expr) != 0, __FILE__, __LINE__, "Expression is not true: " #expr, desc, 0)

/**
 * Assertion macros with no error message
 */
#define cl_must_pass(expr) cl_must_pass_(expr, NULL)
#define cl_must_fail(expr) cl_must_fail_(expr, NULL)
#define cl_assert(expr) cl_assert_(expr, NULL)

/**
 * Check macros with no error message
 */
#define cl_check_pass(expr) cl_check_pass_(expr, NULL)
#define cl_check_fail(expr) cl_check_fail_(expr, NULL)
#define cl_check(expr) cl_check_(expr, NULL)

/**
 * Forced failure/warning
 */
#define cl_fail(desc) clar__assert(0, __FILE__, __LINE__, "Test failed.", desc, 1)
#define cl_warning(desc) clar__assert(0, __FILE__, __LINE__, "Warning during test execution:", desc, 0)

/**
 * Typed assertion macros
 */
#define cl_assert_equal_s(s1,s2) clar__assert_equal_s((s1),(s2),__FILE__,__LINE__,"String mismatch: " #s1 " != " #s2, 1)
#define cl_assert_equal_b(b1,b2) clar__assert_equal_i(!!(b1),!!(b2),__FILE__,__LINE__,#b1 " != " #b2, 1)
#define cl_assert_equal_d(d1,d2) clar__assert_equal_d((d1),(d2),__FILE__,__LINE__,#d1 " != " #d2, 1)
#define cl_assert_equal_p(p1,p2) cl_assert((p1) == (p2))
#define cl_assert_equal_m(p1,p2,l) clar__assert_equal_m((uint8_t*)(p1),(uint8_t*)(p2),(l),__FILE__,__LINE__,"Memory mismatch: " #p1 " != " #p2, 1)

/**
 * Integer Expressions
 */
#define cl_assert_equal_i(i1,i2) clar__assert_cmp_i((i1),(i2),(ClarCmpOp_EQ),__FILE__,__LINE__,"Not True: " #i1 " == " #i2, 1)
#define cl_assert_le(i1,i2) clar__assert_cmp_i((i1),(i2),(ClarCmpOp_LE),__FILE__,__LINE__,"Not True: " #i1 " <= " #i2, 1)
#define cl_assert_lt(i1,i2) clar__assert_cmp_i((i1),(i2),(ClarCmpOp_LT),__FILE__,__LINE__,"Not True: " #i1 " < " #i2, 1)
#define cl_assert_ge(i1,i2) clar__assert_cmp_i((i1),(i2),(ClarCmpOp_GE),__FILE__,__LINE__,"Not True: " #i1 " >= " #i2, 1)
#define cl_assert_gt(i1,i2) clar__assert_cmp_i((i1),(i2),(ClarCmpOp_GT),__FILE__,__LINE__,"Not True: " #i1 " > " #i2, 1)
#define cl_assert_ne(i1,i2) clar__assert_cmp_i((i1),(i2),(ClarCmpOp_NE),__FILE__,__LINE__,"Not True: " #i1 " != " #i2, 1)
#define cl_assert_within(n,min,max) clar__assert_within((n),(min),(max),__FILE__,__LINE__,#n " not within [ "#min" , "#max" ]", 1)
#define cl_assert_near(i1,i2,abs_err) clar__assert_near((i1),(i2),(abs_err),__FILE__,__LINE__,"Difference between " #i1 " and " #i2 " exceeds "#abs_err, 1)
/**
 * Pebble assert macros:
 */
#define cl_assert_passert(expr) \
{ clar_expecting_passert = true; \
  int jumped = setjmp(clar_passert_jmp_buf); \
  if (jumped == 0) { \
    do { expr; } while(0); \
  } \
  clar__assert(clar_passert_occurred, __FILE__, __LINE__, "Expected passert_failed(): " #expr, NULL, 0); \
  clar_passert_occurred = false; \
  clar_expecting_passert = false; \
}

/**
 * Mocking macros:
 */
#define cl_mock() clar__mock(__func__, __FILE__, __LINE__)
#define cl_mock_type(type) ((type)cl_mock())
#define cl_mock_ptr_type(type) ((type)(uintptr_t)cl_mock())

#define cl_will_return_count(func, value, count) clar__will_return(#func, __FILE__, __LINE__, value, count)
#define cl_will_return_always(func, value) clar__will_return(#func, __FILE__, __LINE__, value, -1)
#define cl_will_return(func, value) clar__will_return(#func, __FILE__, __LINE__, value, 1)
