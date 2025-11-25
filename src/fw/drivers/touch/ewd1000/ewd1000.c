/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "touch_sensor_definitions.h"

#include "drivers/touch/touch_sensor.h"

#include "board/board.h"
#include "drivers/exti.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "drivers/rtc.h"
#include "kernel/events.h"
#include "kernel/util/delay.h"
#include "os/tick.h"
#include "services/common/touch/touch.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/attributes.h"
#include "util/net.h"

#include <stdint.h>

// general constants for the controller
#define INIT_TIMEOUT_S (1)

// definition of active region on touch panel
#define MIN_RAW_X (220)
#define MAX_RAW_X (820)
#define MIN_RAW_Y (120)
#define MAX_RAW_Y (820)

// packet ids
#define PACKET_ID_STATUS_RESPONSE (0x52)
#define PACKET_ID_STATUS_READ (0x53)
#define PACKET_ID_STATUS_WRITE (0x54)
#define PACKET_ID_RAM_RESPONSE (0x95)
#define PACKET_ID_RAM_READ (0x96)
#define PACKET_ID_RAM_WRITE (0x97)
#define PACKET_ID_FLASH_RESPONSE (0x97)
#define PACKET_ID_FLASH_READ (0x98)
#define PACKET_ID_HELLO (0x55)
#define PACKET_ID_TOUCH_STATUS (0x5A)
#define PACKET_ID_PALM_DETECTION (0xBA)

// packet footers
#define PACKET_FOOTER (0x01)
#define RAM_FLASH_FOOTER (0xF1)

// message-related defines
#define HELLO_MESSAGE_DATA (0x55555555)
#define MESSAGE_PADDING (0xFFFFFFFF)

// addresses in RAM of interesting settings
#define RAM_ADDR_UNLOCK (0xFFF1)
#define RAM_ADDR_LOCK (0xFFF0)
#define RAM_ADDR_PALM_DETECTION (0x04F1)
#define RAM_ADDR_TOUCHDOWN_RETRIES (0x0474)
#define RAM_ADDR_LIFTOFF_RETRIES (0x04D3)

// magic number for enabling palm detection + reporting
#define RAM_VALUE_ENABLE_PALM_DETECTION (0x02BC)

// If we don't service an interrupt, the controller will retry every 10ms. We never want to lose a
// touch event, so we set the number of retries to a very high value (currently 1 minute worth).
// HACK WARNING: This is a bit of a hack to get around their very low default retry count which
// could cause us to lose events if the system is busy doing something else.
#define NUM_RETRIES (0x10CC)

// data lengths
#define DATA_LEN_FINGER (3)
#define DATA_LEN_STATUS_RESPONSE (3)

typedef union {
  uint8_t packet_id;
  struct PACKED {
    uint32_t data;
    uint32_t padding;
  } raw;
  struct PACKED {
    uint8_t packet_id;
    uint8_t finger_data[MAX_NUM_TOUCHES][DATA_LEN_FINGER];
    uint8_t active_fingers;
  } touch;
} EventMessage;
_Static_assert(sizeof(EventMessage) == 8, "eWD1000 event messages should be 8 bytes.");

#if 0
// TODO: used by prv_status_register_read()
typedef enum {
  StatusReadType_8bit,
  StatusReadType_12bit,
  StatusReadType_16bit
} StatusReadType;

typedef struct PACKED {
  uint8_t packet_id;
  uint8_t address;
  uint8_t padding;
  uint8_t footer;
} StatusRegisterRequest;

typedef struct PACKED {
  uint8_t packet_id;
  uint8_t data[DATA_LEN_STATUS_RESPONSE];
  uint32_t padding;
} StatusRegisterResponse;
#endif

typedef struct PACKED {
  uint8_t packet_id;
  uint16_t address;
  uint16_t value;
  uint8_t footer;
} MemoryPacket;

static bool s_callback_scheduled = false;


// Low-level helper functions
////////////////////////////////////////////////////////////////////////////////

static void prv_write_data(const uint8_t *data, size_t len) {
  i2c_use(EWD1000->i2c);
  PBL_ASSERTN(i2c_write_block(EWD1000->i2c, len, data));
  i2c_release(EWD1000->i2c);
}

static void prv_read_data(uint8_t *data, size_t len) {
  i2c_use(EWD1000->i2c);
  PBL_ASSERTN(i2c_read_block(EWD1000->i2c, len, data));
  i2c_release(EWD1000->i2c);
}

static void prv_wait_for_interrupt(void) {
  const RtcTicks timeout = rtc_get_ticks() + RTC_TICKS_HZ * INIT_TIMEOUT_S;
  while (gpio_input_read(&EWD1000->int_gpio)) {
    if (rtc_get_ticks() > timeout) {
      PBL_CROAK("Touch controller didn't respond!");
    }
  }
}


// Status Register Operations
////////////////////////////////////////////////////////////////////////////////

#if 0
// TODO: this will be used to get FW information from the controller
static void prv_status_register_read(uint8_t address, void *result, StatusReadType type) {
  const StatusRegisterRequest request = {
    .packet_id = PACKET_ID_STATUS_READ,
    .address = address,
    .padding = 0,
    // 12-bit reads set the footer to 0
    .footer = type == StatusReadType_12bit ? 0x00 : PACKET_FOOTER
  };
  prv_write_data((uint8_t *)&request, sizeof(request));
  prv_wait_for_interrupt();

  StatusRegisterResponse response = { };
  prv_read_data((uint8_t *)&response, sizeof(response));
  PBL_ASSERTN(response.packet_id == PACKET_ID_STATUS_RESPONSE);
  // TODO: remove this assert and footer ones below once we're sure the controller FW is stable
  PBL_ASSERTN(response.padding == MESSAGE_PADDING);

  // What follows is some rather hacky code to get the data into a nice and neat integer.
  // More info: https://pebbletechnology.atlassian.net/wiki/display/PRODUCT/Elan+Protocol
  if (type == StatusReadType_8bit) {
    // The 3 data bytes are ST PQ 01 (in hex) with ST being the address and PQ being the value.
    PBL_ASSERTN(response.data[0] == address);
    PBL_ASSERTN(response.data[2] == PACKET_FOOTER);
    *(uint8_t *)result = response.data[1];
  } else if (type == StatusReadType_12bit) {
    // The 3 data bytes are ST PQ R1 (in hex) with ST being the address and RPQ being the value.
    PBL_ASSERTN(response.data[0] == address);
    PBL_ASSERTN((response.data[2] & 0x0F) == PACKET_FOOTER);
    *(uint16_t *)result = response.data[1] | ((uint16_t)(response.data[2] & 0xF0) << 4);
  } else if (type == StatusReadType_16bit) {
    // The 3 data bytes are SP QR S1 (in hex) with S being the first 4-bits of the address and PQRS
    // being the value.
    PBL_ASSERTN((response.data[0] & 0xF0) == (address & 0xF0));
    PBL_ASSERTN((response.data[2] & 0x0F) == PACKET_FOOTER);
    *(uint16_t *)result = (response.data[0] << 4) | (response.data[1] >> 4);
    *(uint16_t *)result <<= 8;
    *(uint16_t *)result |= (response.data[1] << 4) | (response.data[2] >> 4);
  } else {
    // invalid type
    WTF;
  }
}
#endif


// Memory Operations
////////////////////////////////////////////////////////////////////////////////

#if 0
// TODO: we might want to use these this to verify configuration settings
static uint16_t prv_read_ram(uint16_t address) {
  address = htons(address);
  MemoryPacket packet = {
    .packet_id = PACKET_ID_RAM_READ,
    .address = address,
    .value = 0x00,
    .footer = RAM_FLASH_FOOTER
  };
  prv_write_data((uint8_t *)&packet, sizeof(packet));
  prv_wait_for_interrupt();
  prv_read_data((uint8_t *)&packet, sizeof(packet));
  PBL_ASSERTN(packet.packet_id == PACKET_ID_RAM_RESPONSE);
  PBL_ASSERTN(packet.address == address);
  PBL_ASSERTN(packet.footer == RAM_FLASH_FOOTER);
  return ntohs(packet.value);
}
#endif

static void prv_write_ram(uint16_t address, uint16_t value) {
  address = htons(address);
  value = htons(value);
  MemoryPacket data = {
    .packet_id = PACKET_ID_RAM_WRITE,
    .address = address,
    .value = value,
    .footer = RAM_FLASH_FOOTER
  };
  prv_write_data((uint8_t *)&data, sizeof(data));
}

#if 0
// TODO: we will use this to verify firmware when upgrading
static uint16_t prv_read_flash(uint16_t address) {
  address = htons(address);
  MemoryPacket packet = {
    .packet_id = PACKET_ID_FLASH_READ,
    .address = address,
    .value = 0x00,
    .footer = RAM_FLASH_FOOTER
  };
  prv_write_data((uint8_t *)&packet, sizeof(packet));
  prv_wait_for_interrupt();
  prv_read_data((uint8_t *)&packet, sizeof(packet));
  PBL_ASSERTN(packet.packet_id == PACKET_ID_FLASH_RESPONSE);
  PBL_ASSERTN(packet.address == address);
  PBL_ASSERTN(packet.footer == RAM_FLASH_FOOTER);
  return ntohs(packet.value);
}
#endif


// Interrupt / Callback
////////////////////////////////////////////////////////////////////////////////

static void prv_scale_position(GPoint *pos) {
  // swap X and Y since the screen is rotated
  int32_t x = pos->y;
  int32_t y = pos->x;

  // clip down to the box we care about on the screen
  x = CLIP(x, MIN_RAW_X, MAX_RAW_X-1) - MIN_RAW_X;
  y = CLIP(y, MIN_RAW_Y, MAX_RAW_Y-1) - MIN_RAW_Y;

  // scale to our screen size
  x = (x * DISP_COLS) / (MAX_RAW_X - MIN_RAW_X);
  y = (y * DISP_ROWS) / (MAX_RAW_Y - MIN_RAW_Y);

  // fix the fact that Y is inverted
  y = DISP_ROWS - 1 - y;

  // set the result
  *pos = GPoint(x, y);
}

static void prv_process_pending_messages(void *context) {
  s_callback_scheduled = false;

  const uint64_t current_time_ms = ticks_to_milliseconds(rtc_get_ticks());
  while (!gpio_input_read(&EWD1000->int_gpio)) {
    EventMessage message = { };
    prv_read_data((uint8_t *)&message, sizeof(message));

    // Packet format: https://pebbletechnology.atlassian.net/wiki/display/PRODUCT/Elan+Protocol
    if (message.packet_id == PACKET_ID_TOUCH_STATUS) {
      for (uint32_t i = 0; i < ARRAY_LENGTH(message.touch.finger_data); i++) {
        const uint8_t msbs = message.touch.finger_data[i][0];
        const uint8_t x_lsb = message.touch.finger_data[i][1];
        const uint8_t y_lsb = message.touch.finger_data[i][2];
        GPoint point = {
          .x = ((uint16_t)msbs & 0xF0) << 4 | x_lsb,
          .y = ((uint16_t)msbs & 0x0F) << 8 | y_lsb
        };
        prv_scale_position(&point);
        if (message.touch.active_fingers & (1 << i)) {
          touch_handle_update(i, TouchState_FingerDown, &point, 0, current_time_ms);
        } else {
          touch_handle_update(i, TouchState_FingerUp, NULL, 0, current_time_ms);
        }
      }
    } else if (message.packet_id == PACKET_ID_PALM_DETECTION) {
      touch_handle_driver_event(TouchDriverEvent_PalmDetect);
    } else if (message.packet_id == PACKET_ID_HELLO) {
      // TODO: PBL-29944 handle this gracefully by re-initializing - should "never" happen
      PBL_CROAK("Touch controller reset!");
    } else {
      PBL_LOG(LOG_LEVEL_ERROR, "Got unexpected packet (%"PRIx8")", message.packet_id);
    }
  }
}

static void prv_exti_cb(bool *should_context_switch) {
  if (s_callback_scheduled) {
    return;
  }
  PebbleEvent e = {
    .type = PEBBLE_CALLBACK_EVENT,
    .callback.callback = prv_process_pending_messages
  };
  *should_context_switch = event_put_isr(&e);
  s_callback_scheduled = true;
}


// Initialization
////////////////////////////////////////////////////////////////////////////////

void touch_sensor_init(void) {
  // configure INT and RESET pins and INT exti
  // TODO: PBL-29944 Is this pull-up needed?
  gpio_input_init_pull_up_down(&EWD1000->int_gpio, GPIO_PuPd_UP);
  gpio_output_init(&EWD1000->reset_gpio, GPIO_OType_PP, GPIO_Speed_2MHz);

  // toggle the reset line and wait for the "Hello" message
  gpio_output_set(&EWD1000->reset_gpio, false);
  delay_us(1000);
  gpio_output_set(&EWD1000->reset_gpio, true);
  prv_wait_for_interrupt();

  // read the "Hello" message explicitly
  EventMessage message = { };
  prv_read_data((uint8_t *)&message, sizeof(message));
  PBL_ASSERTN(message.raw.data == HELLO_MESSAGE_DATA);
  // TODO: remove this assert once we're sure the controller FW is stable
  PBL_ASSERTN(message.raw.padding == MESSAGE_PADDING);

  // unlock the ram so we can modify it
  prv_write_ram(RAM_ADDR_UNLOCK, 0);

  // enable palm detection reporting
  prv_write_ram(RAM_ADDR_PALM_DETECTION, RAM_VALUE_ENABLE_PALM_DETECTION);

  // increase the retries
  prv_write_ram(RAM_ADDR_LIFTOFF_RETRIES, NUM_RETRIES);

  // lock the ram again
  prv_write_ram(RAM_ADDR_LOCK, 0);

  // initialize exti
  exti_configure_pin(EWD1000->int_exti, ExtiTrigger_Falling, prv_exti_cb);
  exti_enable(EWD1000->int_exti);
  PBL_LOG(LOG_LEVEL_DEBUG, "Initialized eWD1000 touch controller");
}
