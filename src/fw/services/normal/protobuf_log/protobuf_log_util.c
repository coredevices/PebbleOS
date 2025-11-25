/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "protobuf_log.h"
#include "protobuf_log_util.h"

#include "pb_encode.h"

#include <util/uuid.h>



bool protobuf_log_util_encode_uuid(pb_ostream_t *stream, const pb_field_t *field,
                                  void * const *arg) {
  if (!pb_encode_tag_for_field(stream, field)) {
    return false;
  }
  Uuid *uuid_p = *(Uuid **)arg;
  return pb_encode_string(stream, (uint8_t *)uuid_p, sizeof(Uuid));
}

bool protobuf_log_util_encode_string(pb_ostream_t *stream, const pb_field_t *field,
                                           void * const *arg) {
  if (!pb_encode_tag_for_field(stream, field)) {
    return false;
  }

  const char *str = *(char **)arg;
  return pb_encode_string(stream, (uint8_t *)str, strlen(str));
}

bool protobuf_log_util_encode_packed_varints(pb_ostream_t *stream, const pb_field_t *field,
                                            void * const *arg) {
  PLogPackedVarintsEncoderArg *encoder_arg = *(PLogPackedVarintsEncoderArg **)arg;

  // We need to figure out the size of the packed array of varints first
  pb_ostream_t substream = PB_OSTREAM_SIZING;
  for (unsigned i = 0; i < encoder_arg->num_values; i++) {
    if (!pb_encode_varint(&substream, encoder_arg->values[i])) {
      stream->errmsg = substream.errmsg;
      return false;
    }
  }
  size_t packed_array_size = substream.bytes_written;

  // Encode the tag and wiretype
  if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
    return false;
  }

  // Encode the size
  if (!pb_encode_varint(stream, packed_array_size)) {
    return false;
  }

  // if just being called to size it up, the stream callback will be NULL
  if (!stream->callback) {
    return pb_write(stream, NULL, packed_array_size);
  }

  // Write out each of the values
  for (unsigned i = 0; i < encoder_arg->num_values; i++) {
    if (!pb_encode_varint(stream, encoder_arg->values[i])) {
      return false;
    }
  }

  return true;
}

bool protobuf_log_util_encode_measurement_types(pb_ostream_t *stream, const pb_field_t *field,
                                               void * const *arg) {
  PLogTypesEncoderArg *encoder_arg = *(PLogTypesEncoderArg **)arg;
  for (unsigned i = 0; i < encoder_arg->num_types; i++) {
    if (!pb_encode_tag_for_field(stream, field)) {
      return false;
    }
    if (!pb_encode_varint(stream, encoder_arg->types[i])) {
      return false;
    }
  }
  return true;
}

bool protobuf_log_util_encode_buffer(pb_ostream_t *stream, const pb_field_t *field,
                                                     void * const *arg) {
  PLogBufferEncoderArg *encoder_arg = *(PLogBufferEncoderArg **)arg;
  if (!pb_encode_tag_for_field(stream, field)) {
    return false;
  }
  return pb_encode_string(stream, encoder_arg->buffer, encoder_arg->len);
}
