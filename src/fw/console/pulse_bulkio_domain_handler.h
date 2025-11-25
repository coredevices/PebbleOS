/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system/status_codes.h"

#include <stddef.h>
#include <stdint.h>

typedef enum {
#define REGISTER_BULKIO_HANDLER(domain_type, domain_id, vtable) \
  PulseBulkIODomainType_ ## domain_type = domain_id,
#include "pulse_bulkio_handler.def"
#undef REGISTER_BULKIO_HANDLER
} PulseBulkIODomainType;

typedef const struct PulseBulkIODomainHandler {
  PulseBulkIODomainType id;

  //! Open a new Pulse BulkIO context
  //!
  //! @param packet_data pointer to domain specific data passed by the host
  //!        which you may use to identify what was requested to be opened
  //! @param length length of the provided packet_data buffer
  //! @param [out] resp state pointer that can be used by domains to store
  //!        state, to be passed to future calls to this handler for this
  //!        specific context
  //! @return S_SUCCESS if the domain context has been opened, E_INVALID_ARGUMENT
  //!         if opening the domain context has failed because the domain data is
  //!         malformed or otherwise bad, or E_INTERNAL if opening the domain
  //!         context has failed for reasons unrelated to the domain data. Any
  //!         other negative return values are treated the same as E_INTERNAL.
  //!
  //! Any resources that the open_proc method has acquired to open the domain
  //! context are owned by the domain handler; it is the domain handler's
  //! responsibility to release these resources in the domain handler's
  //! close_proc.
  //!
  //! The caller will discard the state pointer and will not call close_proc when
  //! open_proc returns an error status code. The open_proc method must
  //! release any resources it has acquired before returning an error status.
  status_t (*open_proc)(uint8_t *packet_data, size_t length, void **resp);

  //! Close an existing open Pulse BulkIO context
  //!
  //! @param data state pointer as set by the open_proc method
  //! @return non-negative value on success, negative status code if an
  //!         internal error occurred.
  //!
  //! The domain context is assumed to be closed and related resources released
  //! when this method returns, regardless of return value.
  status_t (*close_proc)(void *context);

  //! Read data from an open Pulse BulkIO context
  //!
  //! @param [out] buf buffer for read data to be copied into
  //! @param address offset which data has been requested to be read from
  //! @param length length of data requested to be read
  //! @param data state pointer as set by the open_proc method
  //! @return number of bytes read or a negative error code (see status_codes.h)
  //!         If an error code is returned, it is sent to the host as an internal
  //!         error response and no further read calls will be made until a new
  //!         command is received.
  int (*read_proc)(uint8_t *buf, uint32_t address, uint32_t length, void *context);

  //! Write data to an open Pulse BulkIO context
  //!
  //! @param buf buffer of data to be written
  //! @param address offset which data has been requested to be written to
  //! @param length length of data requested to be written
  //! @param data state pointer as set by the open_proc method
  //! @return number of bytes written or a negative error code (see status_codes.h)
  //!         If an error code is returned, it is sent to the host as an internal
  //!         error response.
  int (*write_proc)(uint8_t *buf, uint32_t address, uint32_t length, void *context);

  //! Stat an existing PULSE BulkIO context. This operation should be used to allow the host to
  //! query for information (size, flags etc) about a specific item within the domain
  //! or the entire domain if there is no concept of multiple items (eg framebuffer
  //! domain)
  //!
  //! @param [out] resp buffer for domain specific stat response to be written to.
  //! @param resp_max_len length of the resp buffer, response may be smaller than this
  //! @param data state pointer as set by the open_proc method
  //! @return number of bytes written to buffer or a negative error code (see status_codes.h)
  //!         If an error code is returned, it is sent to the host as an internal
  //!         error response and the data in the buffer is discarded.
  int (*stat_proc)(uint8_t *resp, size_t resp_max_len, void *context);

  //! Erase data in this domain
  //!
  //! @param packet_data pointer to domain specific data passed by the host
  //!        which erase_proc may use to identify what was requested to be erased
  //! @param length length of the provided packet_data buffer
  //! @param cookie an opaque cookie value to be passed through to all calls to
  //!        pulse_bulkio_erase_message_send
  //! @return status of erase operation - return S_TRUE if the erase is still in progress
  //!         and the handler will send status updates as it progresses, or S_SUCCESS
  //!         if the operation failed or was completed. If erase_proc returns S_TRUE
  //!         to denote an erase still in progress, the domain handler must send
  //!        further status updates using pulse_bulkio_erase_message_send. The
  //!        caller will not send any response itself.
  status_t (*erase_proc)(uint8_t *packet_data, size_t length, uint8_t cookie);
} PulseBulkIODomainHandler;

//! Send erase progress for an ongoing erase operation
//!
//! @param domain_type domain from which the message should originate
//! @param status status of erase operation. Status codes may be treated differently
//!        by different domain handlers, the code is sent verbatim in an erase response.
//! @param cookie an opaque cookie value to be passed through for all erase responses.
//!        Value provided via erase_proc argument.
void pulse_bulkio_erase_message_send(PulseBulkIODomainType domain_type, status_t status,
                                     uint8_t cookie);
