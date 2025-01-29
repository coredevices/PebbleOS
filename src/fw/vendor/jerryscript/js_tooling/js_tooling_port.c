#include "jerry-api.h"
#include "jerry-port.h"
#include "jcontext.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

jerry_context_t jerry_global_context;
jmem_heap_t jerry_global_heap __attribute__((__aligned__(JMEM_ALIGNMENT)));
jerry_hash_table_t jerry_global_hash_table;

size_t jerry_parse_and_save_snapshot_from_zt_utf8_string(
  const jerry_char_t *zt_utf8_source_p,
  bool is_for_global, bool is_strict,
  uint8_t *buffer_p, size_t buffer_size) {
  return jerry_parse_and_save_snapshot(
      zt_utf8_source_p, strlen((char *)zt_utf8_source_p),
      is_for_global, is_strict,
      buffer_p, buffer_size);
}

// helper routine to create proper snapshot header from js_tooling
size_t rocky_fill_header(uint8_t *buffer, size_t buffer_size) {
  const uint8_t ROCKY_EXPECTED_SNAPSHOT_HEADER[8] = {'P', 'J', 'S', 0, CAPABILITY_JAVASCRIPT_BYTECODE_VERSION, 0, 0, 0};
  const size_t ROCKY_HEADER_SIZE = sizeof(ROCKY_EXPECTED_SNAPSHOT_HEADER);
  if (buffer == NULL || buffer_size < ROCKY_HEADER_SIZE) {
    return 0;
  }
  memcpy(buffer, ROCKY_EXPECTED_SNAPSHOT_HEADER, ROCKY_HEADER_SIZE);
  return ROCKY_HEADER_SIZE;
}

int test_str_len(char * str) {
  return strlen(str);
}

// return true, if you handled the error message
typedef bool (*JerryPortErrorMsgHandler)(char *msg);
static JerryPortErrorMsgHandler s_jerry_port_errormsg_handler;

void jerry_port_set_errormsg_handler(JerryPortErrorMsgHandler handler) {
  s_jerry_port_errormsg_handler = handler;
}

static void prv_log(const char *format, va_list args) {
  char msg[1024] = {0};
  const int result = vsnprintf(msg, sizeof(msg), format, args);
  if (s_jerry_port_errormsg_handler == NULL || !s_jerry_port_errormsg_handler(msg)) {
    printf("%s", msg);
  }
}

void jerry_port_log(jerry_log_level_t level, const char* format, ...) {
  va_list args;
  va_start(args, format);
  prv_log(format, args);
  va_end(args);
}

void jerry_port_console(const char *format, ...) {
  if (format[0] == '\n' && strlen(format) == 1) {
    return;
  }
  va_list args;
  va_start(args, format);
  prv_log(format, args);
  va_end(args);
}
