/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/display.h"
#include "drivers/periph_config.h"
#include "drivers/gpio.h"
#include "drivers/dbgserial.h"
#include "util/attributes.h"
#include "util/delay.h"

#include "stm32f4xx_dma.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_spi.h"

#include <stdbool.h>
#include <stdint.h>

// Bootloader images
#include "drivers/display/resources/hex_digits.h"
#include "drivers/display/resources/dead_face.xbm"
#include "drivers/display/resources/empty_bar.xbm"
#include "drivers/display/resources/error_url.xbm"
#include "drivers/display/resources/pebble_logo.xbm"

#define DISP_LINE_BYTES (DISP_COLS / 8)
#define DISP_LINE_WORDS (((DISP_COLS - 1) / 32) + 1)

// GPIO constants
#define DISP_SPI (SPI2)
#define DISP_GPIO (GPIOB)
#define DISPLAY_SPI_CLOCK (RCC_APB1Periph_SPI2)
#define DISP_PIN_SCS (GPIO_Pin_9)
#define DISP_PINSOURCE_SCS (GPIO_PinSource9)
#define DISP_PIN_SCLK (GPIO_Pin_10)
#define DISP_PINSOURCE_SCKL (GPIO_PinSource10)
#define DISP_PIN_SI (GPIO_Pin_15)
#define DISP_PINSOURCE_SI (GPIO_PinSource15)
#define DISP_LCD_GPIO (GPIOA)
#define DISP_PIN_LCD (GPIO_Pin_0)
#define DISP_PINSOURCE_LCD (GPIO_PinSource0)
#define DISP_MODE_STATIC (0x00)
#define DISP_MODE_WRITE (0x80)
#define DISP_MODE_CLEAR (0x20)

// The bootloader leaves SYSCLK at defaults (connected to HSI at 16 Mhz),
// and there are no prescalers on any of the buses. Since the display
// can handle a max of 2 Mhz, we want to divide by 8
#define DISPLAY_PERIPH_PRESCALER (SPI_BaudRatePrescaler_8)

static void prv_enable_display_spi_clock() {
  periph_config_enable(RCC_APB1PeriphClockCmd, DISPLAY_SPI_CLOCK);
}

static void prv_disable_display_spi_clock() {
  periph_config_disable(RCC_APB1PeriphClockCmd, DISPLAY_SPI_CLOCK);
}

static void prv_enable_chip_select(void) {
  GPIO_WriteBit(DISP_GPIO, DISP_PIN_SCS, Bit_SET);
  // required setup time > 3us
  delay_us(7);
}

static void prv_disable_chip_select(void) {
  // delay while last byte is emitted by the SPI peripheral
  delay_us(7);

  GPIO_WriteBit(DISP_GPIO, DISP_PIN_SCS, Bit_RESET);
  // hold time > 1us
  // produce a delay 4ms
  delay_us(4);
}

//! These functions needed to be called around any commands that
//! are sent to the display. NOINLINE only for code size savings.
static NOINLINE void prv_enable_display_access(void) {
  prv_enable_display_spi_clock();
  prv_enable_chip_select();
}

static NOINLINE void prv_disable_display_access(void) {
  prv_disable_chip_select();
  prv_disable_display_spi_clock();
}

//! Write a single byte synchronously to the display. This is the only practical
//! way to write to the display in the bootloader since we don't have interrupts.
static void prv_display_write_byte(uint8_t d) {
  // Block until the tx buffer is empty
  SPI_I2S_SendData(DISP_SPI, d);
  while (!SPI_I2S_GetFlagStatus(DISP_SPI, SPI_I2S_FLAG_TXE)) {}
}

// Since all these values are constant we can save code space
// by storing the initialized struct in memory rather than
// needing to copy in each value
static GPIO_InitTypeDef s_disp_gpio_init = {
  .GPIO_OType = GPIO_OType_PP,
  .GPIO_PuPd = GPIO_PuPd_NOPULL,
  .GPIO_Mode = GPIO_Mode_AF,
  .GPIO_Speed = GPIO_Speed_50MHz,
  .GPIO_Pin = DISP_PIN_SCLK | DISP_PIN_SI
};

static SPI_InitTypeDef s_disp_spi_init = {
  .SPI_Direction = SPI_Direction_1Line_Tx, // Write-only SPI
  .SPI_Mode = SPI_Mode_Master,
  .SPI_DataSize = SPI_DataSize_8b,
  .SPI_CPOL = SPI_CPOL_Low,
  .SPI_CPHA = SPI_CPHA_1Edge,
  .SPI_NSS = SPI_NSS_Soft,
  // We want the SPI clock to run at 2MHz
  .SPI_BaudRatePrescaler = DISPLAY_PERIPH_PRESCALER,
  // MSB order allows us to write pixels out without reversing bytes, but command bytes
  // have to be reversed
  .SPI_FirstBit = SPI_FirstBit_MSB,
  .SPI_CRCPolynomial = 7 // default
};

static void prv_display_start(void) {
  // Setup alternate functions
  GPIO_PinAFConfig(DISP_GPIO, DISP_PINSOURCE_SCKL, GPIO_AF_SPI2);
  GPIO_PinAFConfig(DISP_GPIO, DISP_PINSOURCE_SI, GPIO_AF_SPI2);

  // Init display GPIOs
  GPIO_Init(DISP_GPIO, &s_disp_gpio_init);

  // Init CS
  s_disp_gpio_init.GPIO_Mode = GPIO_Mode_OUT;
  s_disp_gpio_init.GPIO_OType = GPIO_OType_PP;
  s_disp_gpio_init.GPIO_Pin = DISP_PIN_SCS;
  GPIO_Init(DISP_GPIO, &s_disp_gpio_init);

  // Init LCD Control
  s_disp_gpio_init.GPIO_Mode = GPIO_Mode_OUT;
  s_disp_gpio_init.GPIO_OType = GPIO_OType_PP;
  s_disp_gpio_init.GPIO_Pin = DISP_PIN_LCD;
  GPIO_Init(DISP_LCD_GPIO, &s_disp_gpio_init);

  // Set up a SPI bus on SPI2
  SPI_I2S_DeInit(DISP_SPI);
  SPI_Init(DISP_SPI, &s_disp_spi_init);

  SPI_Cmd(DISP_SPI, ENABLE);


  // Hold LCD on
  GPIO_WriteBit(DISP_LCD_GPIO, DISP_PIN_LCD, Bit_SET);
}

// Clear-all mode is entered by sending 0x04 to the panel
void display_clear(void) {
  prv_enable_display_access();

  prv_display_write_byte(DISP_MODE_CLEAR);
  prv_display_write_byte(0x00);

  prv_disable_display_access();
}

//! Static mode is entered by sending 0x00 to the panel
//! This stops any further updates being registered by
//! the display, preventing corruption on shutdown / boot
static void prv_display_enter_static(void) {
  prv_enable_display_access();

  prv_display_write_byte(DISP_MODE_STATIC);
  prv_display_write_byte(0x00);
  prv_display_write_byte(0x00);

  prv_disable_display_access();
}

// Helper to reverse command bytes
static uint8_t prv_reverse_bits(uint8_t input) {
  uint8_t result;
  __asm__ ("rev  %[result], %[input]\n\t"
           "rbit %[result], %[result]"
           : [result] "=r" (result)
           : [input] "r" (input));
  return result;
}

static void prv_display_start_write(void) {
  prv_enable_display_access();

  prv_display_write_byte(DISP_MODE_WRITE);
}

static void prv_display_write_line(uint8_t line_addr, const uint8_t *line) {
  // 1-indexed (ugh) 8bit line address (1-168)
  prv_display_write_byte(prv_reverse_bits(line_addr + 1));

  for (int i = 0; i < DISP_LINE_BYTES; ++i) {
    prv_display_write_byte(prv_reverse_bits(line[i]));
  }

  prv_display_write_byte(0x00);
}

static void prv_display_end_write(void) {
  prv_display_write_byte(0x00);


  prv_disable_display_access();
}

// Round a bit offset to a byte offset
static unsigned prv_round_to_byte(unsigned x) {
  return (x + 7) >> 3;
}

// Draw bitmap onto buffer.
static void prv_draw_bitmap(const uint8_t *bitmap, unsigned x_offset, unsigned y_offset,
                            unsigned width, unsigned height,
                            uint8_t buffer[DISP_ROWS][DISP_LINE_BYTES]) {
  // Need to convert offsets to bytes for the horizontal dimensions
  x_offset = prv_round_to_byte(x_offset);
  width = prv_round_to_byte(width);

  for (unsigned i = 0; i < height; i++) {
    memcpy(buffer[i + y_offset] + x_offset, bitmap + i * width, width);
  }
}

static void prv_display_buffer(uint8_t buffer[DISP_ROWS][DISP_LINE_BYTES]) {
  prv_display_start_write();
  for (int i = 0; i < DISP_ROWS; i++) {
    prv_display_write_line(i, buffer[i]);
  }
  prv_display_end_write();
}

void display_boot_splash(void) {
  uint8_t buffer[DISP_ROWS][DISP_LINE_BYTES];
  // Draw black
  memset(buffer, 0x00, sizeof(buffer));

  prv_draw_bitmap(pebble_logo_bits, 16, 64, pebble_logo_width, pebble_logo_height, buffer);

  prv_display_buffer(buffer);
}

static void prv_set_bit(uint8_t x, uint8_t y, uint8_t buffer[DISP_ROWS][DISP_LINE_BYTES]) {
  buffer[y][x / 8] |= (1 << (x % 8));
}

static void prv_render_char(unsigned digit, uint8_t x_offset_bits, uint8_t y_offset,
                            uint8_t buffer[DISP_ROWS][DISP_LINE_BYTES]) {
  const unsigned char_rows = 18, char_cols = 9;
  const uint8_t * char_data = hex_digits_bits[digit];

  // Each character requires 2 bytes of storage
  for (unsigned y = 0; y < char_rows; y++) {
    unsigned cur_y = y_offset + y;
    uint8_t first_byte = char_data[2 * y];

    for (unsigned x = 0; x < char_cols; x++) {
      bool pixel;
      if (x < 8) { // Pixel is in first byte
        pixel = first_byte & (1 << x);
      }
      else { // Last pixel is in second byte
        pixel = char_data[2 * y + 1] & 1;
      }

      // The buffer is already all black, so just set the white pixels
      if (pixel) {
        prv_set_bit(x_offset_bits + x, cur_y, buffer);
      }
    }
  }
}

static void prv_draw_code(uint32_t code, uint8_t buffer[DISP_ROWS][DISP_LINE_BYTES]) {
  const unsigned y_offset = 116; // beneath sad face, above url
  unsigned x_offset = 28; // Aligned with sad face

  // Extract and print digits
  for (int i = 7; i >= 0; i--) {
    // Mask off 4 bits at a time
    uint32_t mask = (0xf << (i * 4));
    unsigned digit = ((code & mask) >> (i * 4));
    prv_render_char(digit, x_offset, y_offset, buffer);

    // Each character is 9px wide plus 2px of padding
    x_offset += 11;
  }
}

void display_error_code(uint32_t code) {
  uint8_t buffer[DISP_ROWS][DISP_LINE_BYTES];
  memset(buffer, 0x00, sizeof(buffer));

  prv_draw_bitmap(dead_face_bits, 24, 32, dead_face_width, dead_face_height, buffer);

  prv_draw_code(code, buffer);

  prv_draw_bitmap(error_url_bits, 16, 144, error_url_width, error_url_height, buffer);

  prv_display_buffer(buffer);
}

//! Do whatever is necessary to prevent visual artifacts when resetting
//! the watch.
void display_prepare_for_reset(void) {
  prv_display_enter_static();
}

//! Display the progress of a firmware update.
//!
//! The progress is expressed as a rational number less than or equal to 1.
//! When numerator == denominator, the progress indicator shows that the update
//! is complete.
void display_firmware_update_progress(uint32_t numerator, uint32_t denominator) {
  // Dimensions for progress bar
  const unsigned x_offset = 24, y_offset = 106,
                 inner_bar_width = 94, inner_bar_height = 6;

  static unsigned s_prev_num_pixels = -1;
  // Calculate number of pixels to fill in
  unsigned num_pixels = inner_bar_width * numerator / denominator;

  if (num_pixels == s_prev_num_pixels) {
    return;
  }
  s_prev_num_pixels = num_pixels;

  uint8_t buffer[DISP_ROWS][DISP_LINE_BYTES];
  memset(buffer, 0x00, sizeof(buffer));

  prv_draw_bitmap(pebble_logo_bits, 16, 64, pebble_logo_width, pebble_logo_height, buffer);


  prv_draw_bitmap(empty_bar_bits, x_offset, y_offset, empty_bar_width, empty_bar_height, buffer);

  for (unsigned y = 0; y < inner_bar_height; y++) {
    for (unsigned x = 0; x < num_pixels; x++) {
      // Add 1 to offsets so we don't write into outer box
      prv_set_bit(x + x_offset + 1, y_offset + y + 1, buffer);
    }
  }

  prv_display_buffer(buffer);
}

void display_init(void) {
  prv_enable_display_spi_clock();
  prv_display_start();
  prv_disable_display_spi_clock();
}
