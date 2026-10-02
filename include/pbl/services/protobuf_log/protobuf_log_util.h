/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_protobuf_log_protobuf_log_util Encoding helpers
 * @ingroup services_protobuf_log
 * @brief nanopb encode callbacks used to build payloads.
 *
 * Each callback encodes a field from the value @p arg points to, and returns false on encoding
 * errors.
 * @{
 */

/** @brief Argument of protobuf_log_util_encode_packed_varints(). */
typedef struct PLogPackedVarintsEncoderArg {
  /** Number of values. */
  uint16_t num_values;
  /** Values. */
  uint32_t *values;
} PLogPackedVarintsEncoderArg;

/** @brief Argument of protobuf_log_util_encode_measurement_types(). */
typedef struct PLogTypesEncoderArg {
  /** Number of types. */
  uint16_t num_types;
  /** Types. */
  ProtobufLogMeasurementType *types;
} PLogTypesEncoderArg;

/** @brief Argument of protobuf_log_util_encode_buffer(). */
typedef struct PLogBufferEncoderArg {
  /** Length of @ref buffer in bytes. */
  uint16_t len;
  /** Data. */
  uint8_t *buffer;
} PLogBufferEncoderArg;

// -----------------------------------------------------------------------------------------
/**
 * @brief Encode a UUID as a bytes field.
 *
 * @param stream Output stream.
 * @param field Field.
 * @param arg Pointer to a @c Uuid pointer.
 * @return true on success.
 */
bool protobuf_log_util_encode_uuid(pb_ostream_t *stream, const pb_field_t *field, void *const *arg);

// -----------------------------------------------------------------------------------------
/**
 * @brief Encode a NUL-terminated string.
 *
 * @param stream Output stream.
 * @param field Field.
 * @param arg Pointer to a string pointer.
 * @return true on success.
 */
bool protobuf_log_util_encode_string(pb_ostream_t *stream, const pb_field_t *field,
                                     void *const *arg);

// -----------------------------------------------------------------------------------------
/**
 * @brief Encode a packed array of varints.
 *
 * @param stream Output stream.
 * @param field Field.
 * @param arg Pointer to a @ref PLogPackedVarintsEncoderArg pointer.
 * @return true on success.
 */
bool protobuf_log_util_encode_packed_varints(pb_ostream_t *stream, const pb_field_t *field,
                                             void *const *arg);

// -----------------------------------------------------------------------------------------
/**
 * @brief Encode measurement types as a repeated field.
 *
 * @param stream Output stream.
 * @param field Field.
 * @param arg Pointer to a @ref PLogTypesEncoderArg pointer.
 * @return true on success.
 */
bool protobuf_log_util_encode_measurement_types(pb_ostream_t *stream, const pb_field_t *field,
                                                void *const *arg);

// -----------------------------------------------------------------------------------------
/**
 * @brief Encode an already encoded buffer, such as a measurement set, as a bytes field.
 *
 * @param stream Output stream.
 * @param field Field.
 * @param arg Pointer to a @ref PLogBufferEncoderArg pointer.
 * @return true on success.
 */
bool protobuf_log_util_encode_buffer(pb_ostream_t *stream, const pb_field_t *field,
                                     void *const *arg);

/** @} */
