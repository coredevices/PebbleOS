/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "../../emscripten_resources.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT(expr) \
  do { \
    if (!(expr)) { \
      printf("%s:%d " #expr " false\n", __FILE__, __LINE__); \
      exit(-1); \
    } \
  } while (0)


#define CUSTOM_RES_GEN(x) \
  static uint32_t s_read_##x##_called = 0; \
  static uint32_t s_size_##x##_called = 0; \
  int custom_res_read_##x(int offset, uint8_t *buf, int num_bytes) { \
    uint32_t *buf_ptr = (uint32_t *)buf; \
    *buf_ptr = x; \
    s_read_##x##_called++; \
    return 4; \
  } \
  int custom_res_size_##x(void) { \
    s_size_##x##_called++; \
    return 4; \
  }

CUSTOM_RES_GEN(1);
CUSTOM_RES_GEN(2);
CUSTOM_RES_GEN(3);
CUSTOM_RES_GEN(4);

int main(int argc, char **argv) {
  // 1 res
  uint32_t id_1 = emx_resources_register_custom(custom_res_read_1, custom_res_size_1);
  ASSERT(emx_resources_get_size(1, id_1) == 4);
  uint32_t buf = 0;
  ASSERT(emx_resources_read(1, id_1, 0, (uint8_t *)&buf, 1) == 4);
  ASSERT(buf == 1);
  ASSERT(s_read_1_called == 1);
  ASSERT(s_size_1_called == 1);

  // 2nd res
  uint32_t id_2 = emx_resources_register_custom(custom_res_read_2, custom_res_size_2);
  ASSERT(emx_resources_get_size(1, id_2) == 4);
  buf = 0;
  ASSERT(emx_resources_read(1, id_2, 0, (uint8_t *)&buf, 1) == 4);
  ASSERT(buf == 2);
  ASSERT(s_read_2_called == 1);
  ASSERT(s_size_2_called == 1);

  // 3rd res
  uint32_t id_3 = emx_resources_register_custom(custom_res_read_3, custom_res_size_3);
  ASSERT(emx_resources_get_size(1, id_3) == 4);
  buf = 0;
  ASSERT(emx_resources_read(1, id_3, 0, (uint8_t *)&buf, 1) == 4);
  ASSERT(buf == 3);
  ASSERT(s_read_3_called == 1);
  ASSERT(s_size_3_called == 1);

  // remove 2
  emx_resources_remove_custom(id_2);
  ASSERT(emx_resources_get_size(1, id_2) == 0);
  buf = 0;
  ASSERT(emx_resources_read(1, id_2, 0, (uint8_t *)&buf, 1) == 0);
  ASSERT(buf == 0);
  // verify 1 & 3 are OK
  buf = 0;
  ASSERT(emx_resources_read(1, id_3, 0, (uint8_t *)&buf, 1) == 4);
  ASSERT(buf == 3);
  ASSERT(s_read_3_called == 2);
  buf = 0;
  ASSERT(emx_resources_read(1, id_1, 0, (uint8_t *)&buf, 1) == 4);
  ASSERT(buf == 1);
  ASSERT(s_read_1_called == 2);

  // add 4
  uint32_t id_4 = emx_resources_register_custom(custom_res_read_4, custom_res_size_4);
  ASSERT(emx_resources_get_size(1, id_4) == 4);
  buf = 0;
  ASSERT(emx_resources_read(1, id_4, 0, (uint8_t *)&buf, 1) == 4);
  ASSERT(buf == 4);
  ASSERT(s_read_4_called == 1);
  ASSERT(s_size_4_called == 1);

  // remove 1 & 3
  emx_resources_remove_custom(id_1);
  ASSERT(emx_resources_get_size(1, id_1) == 0);
  emx_resources_remove_custom(id_3);
  ASSERT(emx_resources_get_size(1, id_3) == 0);
  // verify 4 is ok
  buf = 0;
  ASSERT(emx_resources_read(1, id_4, 0, (uint8_t *)&buf, 1) == 4);
  ASSERT(buf == 4);
  ASSERT(s_read_4_called == 2);

  // remove 4
  emx_resources_remove_custom(id_4);
  ASSERT(emx_resources_get_size(1, id_4) == 0);
  ASSERT(s_size_4_called == 1);

  // add 4 again
  id_4 = emx_resources_register_custom(custom_res_read_4, custom_res_size_4);
  ASSERT(emx_resources_get_size(1, id_4) == 4);
  buf = 0;
  ASSERT(emx_resources_read(1, id_4, 0, (uint8_t *)&buf, 1) == 4);
  ASSERT(buf == 4);
  ASSERT(s_read_4_called == 3);
  ASSERT(s_size_4_called == 2);

  // remove 4 again
  emx_resources_remove_custom(id_4);
  ASSERT(emx_resources_get_size(1, id_4) == 0);
  ASSERT(s_size_4_called == 2);
}
