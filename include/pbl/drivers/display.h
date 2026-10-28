/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_display Display
 * @ingroup drivers
 * @brief Display class.
 *
 * A display is a struct pbl_display device that scans a framebuffer out to a panel: a controller
 * driving its panel, or a panel with its own memory on a bus. The framebuffer is owned by the
 * caller and has the display's size and @ref pbl_display_format; an update sends a range of its
 * rows. As in Linux, a panel's mounting rotation is part of the display, and the image is turned
 * by a further 180 degrees on request, e.g. for left-hand wear.
 *
 * @code{.c}
 * static void prv_done(int status, void *arg) {
 *   // Runs from the display's interrupt
 * }
 *
 * struct pbl_display_buffer buf = {.data = fb, .pitch = FB_PITCH};
 *
 * ret = pbl_display_update(DISPLAY, &buf, y0, y1, prv_done, nullptr);
 * @endcode
 *
 * An update runs in the background and reports its outcome to a callback run from the display's
 * interrupt, or before pbl_display_update() returns for displays that update synchronously. The
 * display may modify the updated rows while it runs, but restores them before the callback. Calls
 * are made from task context and return 0 or a negative errno: @c -EBUSY while an update is in
 * progress, @c -EINVAL for an invalid row range and @c -ENODEV if the display is not ready.
 * @{
 */

/** @brief Pixel formats of the framebuffer. */
enum pbl_display_format {
  /** 1 bit per pixel, leftmost pixel in the least significant bit, set for white. */
  PBL_DISPLAY_FORMAT_MONO,
  /** 8 bits per pixel, 2 bits per alpha, red, green and blue, from the most significant bit. */
  PBL_DISPLAY_FORMAT_ARGB2222,
};

/** @brief Framebuffer handed to an update. */
struct pbl_display_buffer {
  /** First row. */
  uint8_t *data;
  /** Bytes from one row to the next. */
  uint32_t pitch;
};

/**
 * @brief Completion callback of pbl_display_update().
 *
 * @param status 0 or a negative errno.
 * @param arg Callback argument.
 */
typedef void (*pbl_display_callback_t)(int status, void *arg);

/** @brief Class of the displays. */
extern const struct pbl_device_class pbl_display_class;

struct pbl_display;

/** @cond INTERNAL_HIDDEN */
struct pbl_display_state {
  volatile bool busy;
  bool rotated;
  pbl_display_callback_t cb;
  void *arg;
};
/** @endcond */

/**
 * @brief Display driver operations.
 *
 * The class layer owns the update state and the rotation request; drivers move rows to the panel
 * and report the end of an update with pbl_display_complete().
 */
struct pbl_display_ops {
  /** Set the display up, with the panel on. Returns 0 or a negative errno. */
  int (*init)(const struct pbl_display *disp);
  /** Turn the panel on. Returns 0 or a negative errno. */
  int (*enable)(const struct pbl_display *disp);
  /** Turn the panel off. Returns 0 or a negative errno. */
  int (*disable)(const struct pbl_display *disp);
  /**
   * Turn the image by 180 degrees, mounting rotation included, from the next update. Returns 0
   * or a negative errno.
   */
  int (*set_rotated)(const struct pbl_display *disp, bool rotated);
  /**
   * Start sending rows @p y0 to @p y1 of @p buf. Returns 0 if pbl_display_complete() will be
   * called, or a negative errno.
   */
  int (*update)(const struct pbl_display *disp, const struct pbl_display_buffer *buf, uint16_t y0,
                uint16_t y1);
  /** Clear the image held by the panel. Optional. Returns 0 or a negative errno. */
  int (*clear)(const struct pbl_display *disp);
};

/** @brief A display. */
struct pbl_display {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_display_ops *ops;
  /** Runtime state. */
  struct pbl_display_state *state;
  /** Width, in pixels. */
  uint16_t width;
  /** Height, in pixels. */
  uint16_t height;
  /** Framebuffer format. */
  enum pbl_display_format format;
  /** Panel mounted upside down. */
  bool rotated;
};

/** @brief The display, defined by the board. */
extern const struct pbl_display *const DISPLAY;

/**
 * @brief Define the class state of display @p sym, for the @c PBL_*_DEFINE() macro of a driver.
 *
 * @param sym Symbol of the display instance.
 */
#define PBL_DISPLAY_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);       \
  static struct pbl_display_state sym##_display_state

/**
 * @brief Initializer for the struct pbl_display of display @p sym.
 *
 * @param sym Symbol of the display instance, with its state defined by
 *            PBL_DISPLAY_STATE_DEFINE().
 * @param _name Name.
 * @param _ops Driver operations.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 * @param _width Width, in pixels.
 * @param _height Height, in pixels.
 * @param _format Framebuffer format.
 * @param _rotated Panel mounted upside down.
 */
#define PBL_DISPLAY_INIT(sym, _name, _ops, _deps, _width, _height, _format, _rotated)            \
  {                                                                                              \
    .dev =                                                                                       \
        PBL_DEVICE_INIT_CLASS(sym, _name, pbl_display_init, nullptr, _deps, &pbl_display_class), \
    .ops = (_ops),                                                                               \
    .state = &sym##_display_state,                                                               \
    .width = (_width),                                                                           \
    .height = (_height),                                                                         \
    .format = (_format),                                                                         \
    .rotated = (_rotated),                                                                       \
  }

/**
 * @brief Device init of every display: sets the class state up, then calls the driver's init.
 *
 * @param dev Display device.
 * @return 0 or a negative errno.
 */
int pbl_display_init(const struct pbl_device *dev);

/**
 * @brief Report the end of the update in flight.
 *
 * Called by the display driver, typically from its interrupt handler.
 *
 * @param disp Display.
 * @param status 0 or a negative errno.
 */
void pbl_display_complete(const struct pbl_display *disp, int status);

/**
 * @brief Turn the panel on.
 *
 * @param disp Display.
 * @return 0 or a negative errno.
 */
int pbl_display_enable(const struct pbl_display *disp);

/**
 * @brief Turn the panel off.
 *
 * @param disp Display.
 * @return 0 or a negative errno.
 */
int pbl_display_disable(const struct pbl_display *disp);

/**
 * @brief Turn the image by 180 degrees, on top of the mounting rotation, from the next update.
 *
 * @param disp Display.
 * @param rotated Turn the image.
 * @return 0 or a negative errno.
 */
int pbl_display_set_rotated(const struct pbl_display *disp, bool rotated);

/**
 * @brief Clear the image held by the panel, if the display supports it.
 *
 * @param disp Display.
 * @return 0, @c -ENOTSUP if the display cannot, or another negative errno.
 */
int pbl_display_clear(const struct pbl_display *disp);

/**
 * @brief Send rows of a framebuffer to the panel, returning once the update has started.
 *
 * @param disp Display.
 * @param buf Framebuffer, valid until @p cb is called.
 * @param y0 First row.
 * @param y1 Last row.
 * @param cb Completion callback.
 * @param arg Callback argument.
 * @return 0 if @p cb will be called, or a negative errno.
 */
int pbl_display_update(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                       uint16_t y0, uint16_t y1, pbl_display_callback_t cb, void *arg);

/**
 * @brief Send rows of a framebuffer to the panel and wait for the update to end.
 *
 * @param disp Display.
 * @param buf Framebuffer.
 * @param y0 First row.
 * @param y1 Last row.
 * @return 0 or a negative errno.
 */
int pbl_display_update_sync(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                            uint16_t y0, uint16_t y1);

/**
 * @brief Check whether an update is in progress.
 *
 * @param disp Display.
 * @return True from pbl_display_update() until the callback.
 */
bool pbl_display_update_in_progress(const struct pbl_display *disp);

/** @} */
