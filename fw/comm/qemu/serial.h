/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <applib/accel_service.h>
#include <applib/compass_service.h>
#include <applib/preferred_content_size.h>
#include <pbl/drivers/button_id.h>
#include <pbl/kernel/compiler.h>

/**
 * @defgroup comm_qemu QEMU host channel
 * @brief Channel between the firmware and the QEMU host.
 *
 * The firmware exchanges framed messages with the host over a dedicated UART. Each frame is a
 * header (signature, protocol, length), a payload of up to 2048 bytes and a footer, all in
 * network byte order. Incoming messages are dispatched by protocol to the driver handling them.
 *
 * @code{.c}
 * QemuProtocolVibrationNotificationHeader msg = { .on = 1 };
 * qemu_serial_send(QemuProtocol_Vibration, (const uint8_t *)&msg, sizeof(msg));
 * @endcode
 * @{
 */

/** @brief QEMU channel protocols. */
typedef enum {
  /** Raw Pebble Protocol, both directions. */
  QemuProtocol_SPP = 1,
  /** Accelerometer tap, @ref QemuProtocolTapHeader. */
  QemuProtocol_Tap = 2,
  /** Phone connection state, @ref QemuProtocolBluetoothConnectionHeader. */
  QemuProtocol_BluetoothConnection = 3,
  /** Compass heading, @ref QemuProtocolCompassHeader. */
  QemuProtocol_Compass = 4,
  /** Battery state, @ref QemuProtocolBatteryHeader. */
  QemuProtocol_Battery = 5,
  /** Accelerometer samples, @ref QemuProtocolAccelHeader and its response. */
  QemuProtocol_Accel = 6,
  /** Vibration state sent to the host, @ref QemuProtocolVibrationNotificationHeader. */
  QemuProtocol_Vibration = 7,
  /** Button state, @ref QemuProtocolButtonHeader. */
  QemuProtocol_Button = 8,
  /** 12h/24h time format, @ref QemuProtocolTimeFormatHeader. */
  QemuProtocol_TimeFormat = 9,
  /** Timeline peek visibility, @ref QemuProtocolTimelinePeekHeader. */
  QemuProtocol_TimelinePeek = 10,
  /** System content size, @ref QemuProtocolContentSizeHeader. */
  QemuProtocol_ContentSize = 11,
  /** Health metric value, @ref QemuProtocolHealthMetricHeader. */
  QemuProtocol_HealthMetric = 12,
  /** Heart rate reading, @ref QemuProtocolHeartRateHeader. */
  QemuProtocol_HeartRate = 13,
} QemuProtocol;

/** @brief @ref QemuProtocol_Tap payload. */
typedef struct PBL_PACKED {
  /** Tap axis: 0 for X, 1 for Y, 2 for Z. */
  uint8_t axis;
  /** Tap direction: +1 or -1. */
  int8_t direction;
} QemuProtocolTapHeader;

/** @brief @ref QemuProtocol_BluetoothConnection payload. */
typedef struct PBL_PACKED {
  /** Non-zero if connected. */
  uint8_t connected;
} QemuProtocolBluetoothConnectionHeader;

/** @brief @ref QemuProtocol_Compass payload. */
typedef struct PBL_PACKED {
  /** Magnetic heading, 0x10000 being 360 degrees. */
  uint32_t magnetic_heading;
  /** Calibration status. */
  CompassStatus calib_status : 8;
} QemuProtocolCompassHeader;

/** @brief @ref QemuProtocol_Battery payload. */
typedef struct PBL_PACKED {
  /** Charge percentage, 0 to 100. */
  uint8_t battery_pct;
  /** Non-zero if the charger is connected. */
  uint8_t charger_connected;
} QemuProtocolBatteryHeader;

/** @brief @ref QemuProtocol_Accel request payload, from the host. */
typedef struct PBL_PACKED {
  /** Number of samples that follow. */
  uint8_t num_samples;
  /** Samples. */
  AccelRawData samples[0];
} QemuProtocolAccelHeader;

/** @brief @ref QemuProtocol_Accel response payload, to the host. */
typedef struct PBL_PACKED {
  /** Number of samples the firmware can accept. */
  uint16_t avail_space;
} QemuProtocolAccelResponseHeader;

/** @brief @ref QemuProtocol_Vibration payload, to the host. */
typedef struct PBL_PACKED {
  /** Non-zero if the vibration motor is on. */
  uint8_t on;
} QemuProtocolVibrationNotificationHeader;

/** @brief @ref QemuProtocol_Button payload. */
typedef struct PBL_PACKED {
  /** New button state: bit x is the state of the button with ButtonId x. */
  uint8_t button_state;
} QemuProtocolButtonHeader;

/** @brief @ref QemuProtocol_TimeFormat payload. */
typedef struct PBL_PACKED {
  /** Non-zero for 24h format, 0 for 12h format. */
  uint8_t is_24_hour;
} QemuProtocolTimeFormatHeader;

/** @brief @ref QemuProtocol_TimelinePeek payload. */
typedef struct PBL_PACKED {
  /**
   * Whether the timeline peek shows.
   *
   * The peek animates only when this state toggles; other changes to the peek apply without
   * animation. The state persists: with it enabled, leaving the watchface hides the peek
   * instantly and returning shows it instantly.
   */
  bool enabled;
} QemuProtocolTimelinePeekHeader;

/** @brief @ref QemuProtocol_ContentSize payload. */
typedef struct PBL_PACKED {
  /** New system content size. */
  uint8_t size;
} QemuProtocolContentSizeHeader;
#if !UNITTEST
_Static_assert(sizeof(PreferredContentSize) == sizeof(((QemuProtocolContentSizeHeader *)0)->size),
               "sizeof(PreferredContentSize) grew, need to update QemuContentSize in libpebble2 !");
#endif

/**
 * @brief Health metric identifiers on the wire.
 *
 * Stable identifiers mapped to ActivityMetric by the firmware, so reordering ActivityMetric
 * cannot change the protocol. The host tool uses the same values.
 */
typedef enum {
  /** Step count. */
  QemuHealthMetric_Steps = 0,
  /** Active time in seconds. */
  QemuHealthMetric_ActiveSeconds = 1,
  /** Resting kilocalories. */
  QemuHealthMetric_RestingCalories = 2,
  /** Active kilocalories. */
  QemuHealthMetric_ActiveCalories = 3,
  /** Distance in meters. */
  QemuHealthMetric_DistanceMeters = 4,
  /** Total sleep in seconds. */
  QemuHealthMetric_SleepTotalSeconds = 5,
  /** Restful sleep in seconds. */
  QemuHealthMetric_SleepRestfulSeconds = 6,
} QemuHealthMetric;

/** @brief @ref QemuProtocol_HealthMetric payload. */
typedef struct PBL_PACKED {
  /** Metric, a @ref QemuHealthMetric. */
  uint8_t metric;
  /** Metric value, big endian. */
  int32_t value;
} QemuProtocolHealthMetricHeader;

/** @brief @ref QemuProtocol_HeartRate payload. */
typedef struct PBL_PACKED {
  /** Heart rate in beats per minute. */
  uint8_t bpm;
  /** Reading quality, an HRMQuality value (signed: off-wrist is -1). */
  int8_t quality;
} QemuProtocolHeartRateHeader;

/**
 * @brief Initialize the QEMU channel.
 *
 * Configures the QEMU UART and starts receiving messages.
 */
void qemu_serial_init(void);

/**
 * @brief Send a message to the host.
 *
 * Blocks until the frame is transmitted. Does nothing before qemu_serial_init().
 *
 * @param protocol Message protocol.
 * @param data Payload.
 * @param len Length of @p data in bytes.
 */
void qemu_serial_send(QemuProtocol protocol, const uint8_t *data, uint32_t len);

/** @} */
