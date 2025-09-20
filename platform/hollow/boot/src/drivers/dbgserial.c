#include "drivers/dbgserial.h"

#include "util/cobs.h"
#include "util/crc32.h"
#include "util/misc.h"
#include "util/net.h"

#include <board.h>
#include <nrfx.h>

#include <stdbool.h>

#define MAX_MESSAGE (256)
#define FRAME_DELIMITER '\x55'
#define PULSE_TRANSPORT_PUSH (0x5021)
#define PULSE_PROTOCOL_LOGGING (0x0003)

typedef struct __attribute__((__packed__)) PulseFrame {
  net16 protocol;
  unsigned char information[];
} PulseFrame;

typedef struct __attribute__((__packed__)) PushPacket {
  net16 protocol;
  net16 length;
  unsigned char information[];
} PushPacket;

static const unsigned char s_message_header[] = {
  // Message type: text
  1,
  // Source filename
  'B', 'O', 'O', 'T', 'L', 'O', 'A', 'D', 'E', 'R', 0, 0, 0, 0, 0, 0,
  // Log level and task
  '*', '*',
  // Timestamp
  0, 0, 0, 0, 0, 0, 0, 0,
  // Line number
  0, 0,
};

static size_t s_message_length = 0;
static unsigned char s_message_buffer[MAX_MESSAGE];

void dbgserial_init(void) {
  NRF_UART0->BAUDRATE = UART_BAUDRATE_BAUDRATE_Baud1M;
  NRF_UART0->TASKS_STARTTX = 1;
  NRF_UART0->PSELTXD = BOARD_UART_TX_PIN;
}

static void prv_putchar(uint8_t c) {
  NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Enabled;
  NRF_UART0->TXD = c;
  while (NRF_UART0->EVENTS_TXDRDY != 1) {}
  NRF_UART0->EVENTS_TXDRDY = 0;
  NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Disabled;
}

void dbgserial_print(const char* str) {

  for (; *str && s_message_length < MAX_MESSAGE; ++str) {
    if (*str == '\n') {
      dbgserial_newline();
    } else if (*str != '\r') {
      s_message_buffer[s_message_length++] = *str;
    }
  }

}

void dbgserial_newline(void) {
  uint32_t crc;
  size_t raw_length = sizeof(PulseFrame) + sizeof(PushPacket) +
                      sizeof(s_message_header) + s_message_length + sizeof(crc);
  unsigned char raw_packet[raw_length];

  PulseFrame *frame = (PulseFrame *)raw_packet;
  frame->protocol = hton16(PULSE_TRANSPORT_PUSH);

  PushPacket *transport = (PushPacket *)frame->information;
  transport->protocol = hton16(PULSE_PROTOCOL_LOGGING);
  transport->length = hton16(sizeof(PushPacket) + sizeof(s_message_header) +
                             s_message_length);

  unsigned char *app = transport->information;
  memcpy(app, s_message_header, sizeof(s_message_header));
  memcpy(&app[sizeof(s_message_header)], s_message_buffer,
         s_message_length);

  crc = crc32(CRC32_INIT, raw_packet, raw_length - sizeof(crc));
  memcpy(&raw_packet[raw_length - sizeof(crc)], &crc, sizeof(crc));

  unsigned char cooked_packet[MAX_SIZE_AFTER_COBS_ENCODING(raw_length)];
  size_t cooked_length = cobs_encode(cooked_packet, raw_packet, raw_length);

  prv_putchar(FRAME_DELIMITER);
  for (size_t i = 0; i < cooked_length; ++i) {
    if (cooked_packet[i] == FRAME_DELIMITER) {
      prv_putchar('\0');
    } else {
      prv_putchar(cooked_packet[i]);
    }
  }
  prv_putchar(FRAME_DELIMITER);

  s_message_length = 0;
}

void dbgserial_putstr(const char* str) {
  dbgserial_print(str);

  dbgserial_newline();
}

void dbgserial_print_hex(uint32_t value) {
  char buf[12];
  itoa_hex(value, buf, sizeof(buf));
  dbgserial_print(buf);
}
