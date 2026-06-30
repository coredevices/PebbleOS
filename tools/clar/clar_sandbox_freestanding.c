/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Freestanding replacements for the static sandbox functions defined in
 * clar_sandbox.c.  Included into clar_main.c when clar.py is invoked with
 * --report-to=freestanding (which selects this file instead of
 * clar_sandbox.c).
 *
 * Freestanding builds do not fork child processes or write temporary
 * directories, so the sandbox is a no-op.
 *
 * clar_fixtures.c and clar_fs.c are also omitted on the freestanding path
 * because they depend on POSIX filesystem primitives (stat, fork, waitpid,
 * execv) that are not available.  The fixture_path() function stub below
 * satisfies the forward declaration in clar.c; it should never be called
 * from freestanding test suites.
 */

static int clar_sandbox(void) {
  return 0;
}

static void clar_unsandbox(void) {
}

/*
 * Stub for fixture_path(), normally defined in clar_fixtures.c.
 * Freestanding tests must not use fixtures; this exists only to satisfy
 * the static forward declaration in clar.c.
 */
static const char *fixture_path(const char *base, const char *fixture_name) {
  (void)base;
  (void)fixture_name;
  return "";
}

/* cl_fs_cleanup() stub — normally in clar_fs.c. */
void cl_fs_cleanup(void) {
}
