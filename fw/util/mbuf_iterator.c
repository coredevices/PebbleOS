/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mbuf_iterator.h"

bool prv_iter_to_valid_mbuf(MBufIterator *iter) {
  // advance the iterator to the next MBuf with data
  while ((iter->m != NULL) && (mbuf_get_length(iter->m) == 0)) {
    iter->m = mbuf_get_next(iter->m);
    iter->data_index = 0;
  }
  return iter->m != NULL;
}

void mbuf_iterator_init(MBufIterator *iter, MBuf *m) {
  iter->m = m;
  iter->data_index = 0;
  prv_iter_to_valid_mbuf(iter);
}

bool mbuf_iterator_is_finished(MBufIterator *iter) {
  if (!prv_iter_to_valid_mbuf(iter)) {
    return true;
  }
  if (iter->data_index >= mbuf_get_length(iter->m)) {
    // we're at the end of this MBuf so move to the next one
    iter->m = mbuf_get_next(iter->m);
    iter->data_index = 0;
    // make sure this MBuf has data
    if (!prv_iter_to_valid_mbuf(iter)) {
      return true;
    }
  }
  return false;
}

bool mbuf_iterator_read_byte(MBufIterator *iter, uint8_t *data) {
  if (mbuf_iterator_is_finished(iter)) {
    return false;
  }
  uint8_t *buffer = mbuf_get_data(iter->m);
  *data = buffer[iter->data_index++];
  return true;
}

bool mbuf_iterator_write_byte(MBufIterator *iter, uint8_t data) {
  if (mbuf_iterator_is_finished(iter)) {
    return false;
  }
  uint8_t *buffer = mbuf_get_data(iter->m);
  buffer[iter->data_index++] = data;
  return true;
}

MBuf *mbuf_iterator_get_current_mbuf(MBufIterator *iter) {
  return iter->m;
}
