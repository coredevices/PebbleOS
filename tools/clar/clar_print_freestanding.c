/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Freestanding print back-end for clar.
 *
 * Implements the clar_print_* interface using printf() (which in a
 * freestanding build is provided by clar_io_freestanding.c and routes
 * through freestanding_write() in shim.c).
 *
 * This file is selected as the clar print module when clar.py is invoked
 * with --report-to=freestanding.
 */

static void clar_print_init(int test_count, int suite_count,
                            const char *suite_names) {
  (void)test_count;
  printf("Loaded %d suites: %s\n", (int)suite_count, suite_names);
  printf("Started\n");
}

static void clar_print_shutdown(int test_count, int suite_count,
                                int error_count) {
  (void)test_count;
  (void)suite_count;
  (void)error_count;

  printf("\n\n");
  clar_report_errors();
}

static void clar_print_error(int num, const struct clar_error *error) {
  printf("  %d) Failure:\n", num);
  printf("%s::%s (%s) [%s:%d] [-t%d]\n",
         error->suite,
         error->test,
         "no description",
         error->file,
         error->line_number,
         error->test_number);
  printf("  %s\n", error->error_msg);
  if (error->description != NULL) {
    printf("  %s\n", error->description);
  }
  printf("\n");
}

static void clar_print_ontest(const char *test_name, int test_number,
                              int failed) {
  (void)test_name;
  (void)test_number;
  printf("%c", failed ? 'F' : '.');
}

static void clar_print_onsuite(const char *suite_name, int suite_index) {
  (void)suite_index;
  (void)suite_name;
}

static void clar_print_onabort(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
}
