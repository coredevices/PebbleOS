/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */


#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "drivers/qemu/qemu_serial.h"
#include "drivers/qemu/qemu_serial_private.h"
#include "util/net.h"

#include "clar.h"

extern bool qemu_test_add_byte_from_isr(QemuSerialGlobals *state, uint8_t byte);


// Stubs
////////////////////////////////////
#include "stubs_passert.h"
#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"
#include "stubs_mutex.h"


// Globals
QemuSerialGlobals s_state;

// Setup
////////////////////////////////////
void test_qemu_serial__initialize(void) {
  qemu_serial_private_init_state(&s_state);
}

void test_qemu_serial__cleanup(void) {
}


// ------------------------------------------------------------------------------------
static void prv_send_bytes(void *p, uint32_t size) {
  uint8_t *src = (uint8_t *)p;
  for (uint32_t i=0; i<size; i++) {
    qemu_test_add_byte_from_isr(&s_state, *src++);
  }
}


// ------------------------------------------------------------------------------------
static void prv_send_hdr(uint16_t protocol, uint16_t data_len) {
  QemuCommChannelHdr hdr = (QemuCommChannelHdr) {
    .signature = htons(QEMU_HEADER_SIGNATURE),
    .protocol = htons(protocol),
    .len = htons(data_len)
  };
  prv_send_bytes(&hdr, sizeof(hdr));
}

static void prv_send_footer(void) {
  QemuCommChannelFooter footer = (QemuCommChannelFooter) {
    .signature = htons(QEMU_FOOTER_SIGNATURE)
  };
  prv_send_bytes(&footer, sizeof(footer));
}


// ------------------------------------------------------------------------------------
// Tests
void test_qemu_serial__foo(void) {
  uint8_t *rcv_msg;
  uint32_t rcv_bytes;
  uint16_t rcv_protocol;


  // Our test message
  uint8_t msg_data[] = {0x11, 0x22, 0x33};


  // -----------------------------------------------------------------------------
  // Send message all at once before checking
  prv_send_hdr(QemuProtocol_SPP, sizeof(msg_data));
  prv_send_bytes(msg_data, sizeof(msg_data));
  prv_send_footer();

  rcv_msg = qemu_serial_private_assemble_message(&s_state, &rcv_bytes, &rcv_protocol);
  cl_assert(rcv_msg);
  cl_assert_equal_i(rcv_protocol, QemuProtocol_SPP);
  cl_assert_equal_i(rcv_bytes, sizeof(msg_data));
  cl_assert_equal_m(msg_data, rcv_msg, sizeof(msg_data));

  // -----------------------------------------------------------------------------
  // Send 2 messages before checking
  for (int i=0; i<2; i++) {
    prv_send_hdr(QemuProtocol_SPP, sizeof(msg_data));
    prv_send_bytes(msg_data, sizeof(msg_data));
    prv_send_footer();
  }
  for (int i=0; i<2; i++) {
    rcv_msg = qemu_serial_private_assemble_message(&s_state, &rcv_bytes, &rcv_protocol);
    cl_assert(rcv_msg);
    cl_assert_equal_i(rcv_protocol, QemuProtocol_SPP);
    cl_assert_equal_i(rcv_bytes, sizeof(msg_data));
    cl_assert_equal_m(msg_data, rcv_msg, sizeof(msg_data));
  }

  // -----------------------------------------------------------------------------
  // Check after each part
  prv_send_hdr(QemuProtocol_SPP, sizeof(msg_data));
  cl_assert(!qemu_serial_private_assemble_message(&s_state, &rcv_bytes, &rcv_protocol));
  prv_send_bytes(msg_data, sizeof(msg_data));
  // Message is available now
  cl_assert(qemu_serial_private_assemble_message(&s_state, &rcv_bytes, &rcv_protocol));
  prv_send_footer();
  cl_assert(!qemu_serial_private_assemble_message(&s_state, &rcv_bytes, &rcv_protocol));


  // -----------------------------------------------------------------------------
  // Send garbage before a good packet
  prv_send_bytes(msg_data, sizeof(msg_data));
  cl_assert(!qemu_serial_private_assemble_message(&s_state, &rcv_bytes, &rcv_protocol));
  prv_send_hdr(QemuProtocol_SPP, sizeof(msg_data));
  cl_assert(!qemu_serial_private_assemble_message(&s_state, &rcv_bytes, &rcv_protocol));
  prv_send_bytes(msg_data, sizeof(msg_data));
  cl_assert(qemu_serial_private_assemble_message(&s_state, &rcv_bytes, &rcv_protocol));
  prv_send_footer();
  cl_assert(!qemu_serial_private_assemble_message(&s_state, &rcv_bytes, &rcv_protocol));


  // -----------------------------------------------------------------------------
  // Check after just part of the data
  prv_send_hdr(QemuProtocol_SPP, 2*sizeof(msg_data));
  cl_assert(!qemu_serial_private_assemble_message(&s_state, &rcv_bytes, &rcv_protocol));
  prv_send_bytes(msg_data, sizeof(msg_data));
  cl_assert(!qemu_serial_private_assemble_message(&s_state, &rcv_bytes, &rcv_protocol));
  prv_send_bytes(msg_data, sizeof(msg_data));

  rcv_msg = qemu_serial_private_assemble_message(&s_state, &rcv_bytes, &rcv_protocol);
  cl_assert(rcv_msg);
  cl_assert_equal_i(rcv_bytes, 2*sizeof(msg_data));
  cl_assert_equal_m(msg_data, rcv_msg, sizeof(msg_data));
  cl_assert_equal_m(msg_data, rcv_msg+sizeof(msg_data), sizeof(msg_data));

  prv_send_footer();
  cl_assert(!qemu_serial_private_assemble_message(&s_state, &rcv_bytes, &rcv_protocol));

}

