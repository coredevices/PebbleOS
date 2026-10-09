/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup logging_binary_logging Binary logging
 * @ingroup logging
 * @brief Wire format of binary log messages.
 *
 * A message is a header, whose version byte says which optional fields follow, and a body:
 * hashed (a message ID and its parameters), unhashed (file, line and text) or a plain string.
 * Without PULSE, messages are SLIP framed as END, message, CRC-32, END.
 * @{
 */

/** @brief SLIP frame delimiter. */
#define END 0xC0
/** @brief SLIP escape. */
#define ESC 0xDB
/** @brief SLIP escaped frame delimiter. */
#define ESC_END 0xDC
/** @brief SLIP escaped escape. */
#define ESC_ESC 0xDD

/** @brief Message version, a set of flags describing the header and body. */
typedef struct BinLogMessage_Version {
  union {
    struct {
      /** Reserved. */
      uint8_t reserved : 4;
      /** The body is an unhashed message. */
      uint8_t unhashed_msg : 1;
      /** The body is a hashed message with parameters. */
      uint8_t parameterized : 1;
      /** The header has a tick count. */
      uint8_t tick_count : 1;
      /** The header has a date and time. */
      uint8_t time_date : 1;
    };
    /** All flags. */
    uint8_t version;
  };
} BinLogMessage_Version;

/** @brief Version flag: the body is an unhashed message. */
#define BINLOGMSG_VERSION_UNHASHED_MSG (1 << 3)
/** @brief Version flag: the body is a hashed message with parameters. */
#define BINLOGMSG_VERSION_PARAMETERIZED (1 << 2)
/** @brief Version flag: the header has a tick count. */
#define BINLOGMSG_VERSION_TICK_COUNT (1 << 1)
/** @brief Version flag: the header has a date and time. */
#define BINLOGMSG_VERSION_TIME_DATE (1 << 0)

static_assert(sizeof(BinLogMessage_Version) == 1, "BinLogMessage_Version size != 1");

/** @brief Time of day in UTC. All fields are 0-based. */
typedef struct Time_Full {
  /** Reserved. */
  uint32_t reserved : 5;
  /** Hour, 0 to 23. */
  uint32_t hour : 5;
  /** Minute, 0 to 59. */
  uint32_t minute : 6;
  /** Second, 0 to 59. */
  uint32_t second : 6;
  /** Millisecond, 0 to 999. */
  uint32_t millisecond : 10;
} Time_Full;

/** @brief 48-bit tick count. Total ticks = (count_high << 32) | count. */
typedef struct Time_Tick {
  /** Reserved. */
  uint16_t reserved;
  /** High 16 bits. */
  uint16_t count_high;
  /** Low 32 bits. */
  uint32_t count;
} Time_Tick;

/**
 * @brief Date.
 *
 * (0, 0, 0) is an invalid or unknown date, so a zero Date is not the start of the epoch, which
 * would be (0, 1, 1).
 */
typedef struct Date {
  /** Year offset from 2000, e.g. 16 for 2016. */
  uint16_t year : 7;
  /** Month, 1 to 12. */
  uint16_t month : 4;
  /** Day of the month, 1 to 31. */
  uint16_t day : 5;
} Date;

/** @brief Message ID of a hashed message. */
typedef struct MessageID {
  union {
    struct {
      /** Offset of the string in the log string table. */
      uint32_t msg_number : 19; // LSB
      /** Task that logged the message. */
      uint32_t task_id : 4;
      /** 1-based index of the first string parameter, 0 for none. */
      uint32_t str_index_1 : 3;
      /** 1-based index of the second string parameter, 0 for none. */
      uint32_t str_index_2 : 3;
      /** Reserved. */
      uint32_t reserved : 1;
      /** Core that logged the message. */
      uint32_t core_number : 2; // MSB
    };
    /** All fields. */
    uint32_t msg_id;
  };
} MessageID;

static_assert(sizeof(MessageID) == 4, "MessageID size != 4");

/** @brief Common start of every header. */
typedef struct BinLogMessage_Header {
  /** Version flags, see BinLogMessage_Version. */
  uint8_t version;
  /** Message length in bytes. */
  uint8_t length;
} BinLogMessage_Header;

/** @brief Header without time information. */
typedef struct BinLogMessage_Header_v0 {
  /** Version flags. */
  uint8_t version;
  /** Message length in bytes. */
  uint8_t length;
  /** Reserved. */
  uint8_t reserved[2];
} BinLogMessage_Header_v0;
/** @brief Version flags of BinLogMessage_Header_v0. */
#define BINLOGMSG_VERSION_HEADER_V0 (0)

/** @brief Header with date and time. */
typedef struct BinLogMessage_Header_v1 {
  /** Version flags. */
  uint8_t version;
  /** Message length in bytes. */
  uint8_t length;
  /** Date. */
  Date date;
  /** Time of day. */
  Time_Full time;
} BinLogMessage_Header_v1;
/** @brief Version flags of BinLogMessage_Header_v1. */
#define BINLOGMSG_VERSION_HEADER_V1 (BINLOGMSG_VERSION_TIME_DATE)

/** @brief Header with a tick count. */
typedef struct BinLogMessage_Header_v2 {
  /** Version flags. */
  uint8_t version;
  /** Message length in bytes. */
  uint8_t length;
  /** Reserved. */
  uint8_t reserved[2];
  /** Tick count. */
  Time_Tick tick_count;
} BinLogMessage_Header_v2;
/** @brief Version flags of BinLogMessage_Header_v2. */
#define BINLOGMSG_VERSION_HEADER_V2 (BINLOGMSG_VERSION_TICK_COUNT)

/** @brief Header with date, time and a tick count. */
typedef struct BinLogMessage_Header_v3 {
  /** Version flags. */
  uint8_t version;
  /** Message length in bytes. */
  uint8_t length;
  /** Date. */
  Date date;
  /** Time of day. */
  Time_Full time;
  /** Tick count. */
  Time_Tick tick_count;
} BinLogMessage_Header_v3;
/** @brief Version flags of BinLogMessage_Header_v3. */
#define BINLOGMSG_VERSION_HEADER_V3 (BINLOGMSG_VERSION_TIME_DATE | BINLOGMSG_VERSION_TICK_COUNT)

/** @brief Body of a hashed message. */
typedef struct BinLogMessage_ParamBody {
  /** Message ID. */
  MessageID msgid;
  /** Parameters: BinLogMessage_IntParam or BinLogMessage_StringParam entries. */
  uint32_t payload[0];
} BinLogMessage_ParamBody;

/** @brief String parameter, padded to a multiple of 4 bytes. */
typedef struct BinLogMessage_StringParam {
  /** Length of @ref string. */
  uint8_t length;
  /** Characters, followed by padding. */
  uint8_t string[0]; // string[length]
  // uint8_t padding[((length + sizeof(length) + 3) % 4)]
} BinLogMessage_StringParam;

/** @brief Integer parameter. */
typedef uint32_t BinLogMessage_IntParam;

/** @brief Body of an unhashed message. */
typedef struct BinLogMessage_UnhashedBody {
  /** Source line number. */
  uint16_t line_number;
  /** Source file name. */
  uint8_t filename[16];
  /** Reserved. */
  uint8_t reserved : 2;
  /** Core that logged the message. */
  uint8_t core_number : 2;
  /** Task that logged the message. */
  uint8_t task_id : 4;
  /** Log level. */
  uint8_t level;
  /** Length of @ref string. */
  uint8_t length;
  /** Message, followed by padding. */
  uint8_t string[0]; // string[length];
  // uint8_t padding[];
} BinLogMessage_UnhashedBody;

/*
int len = MAX(strlen(log_string), 255 - sizeof(BinLogMessage_Header_vX))
typedef struct BinLogMessage_SimpleBody {
  uint8_t string[len];
  uint8_t padding[((sizeof(BinLogMessage_Header_vX) + len + 3) % 4)];
} BinLogMessage_SimpleBody;
*/

/** @brief Hashed message with a v0 header. */
typedef struct BinLogMessage_Param_v0 {
  /** Header. */
  BinLogMessage_Header_v0 header;
  /** Body. */
  BinLogMessage_ParamBody body;
} BinLogMessage_Param_v0;
/** @brief Version flags of BinLogMessage_Param_v0. */
#define BINLOGMSG_VERSION_PARAM_V0 (BINLOGMSG_VERSION_HEADER_V0 | BINLOGMSG_VERSION_PARAMETERIZED)

/** @brief Hashed message with a v1 header. */
typedef struct BinLogMessage_Param_v1 {
  /** Header. */
  BinLogMessage_Header_v1 header;
  /** Body. */
  BinLogMessage_ParamBody body;
} BinLogMessage_Param_v1;
/** @brief Version flags of BinLogMessage_Param_v1. */
#define BINLOGMSG_VERSION_PARAM_V1 (BINLOGMSG_VERSION_HEADER_V1 | BINLOGMSG_VERSION_PARAMETERIZED)

/** @brief Hashed message with a v2 header. */
typedef struct BinLogMessage_Param_v2 {
  /** Header. */
  BinLogMessage_Header_v2 header;
  /** Body. */
  BinLogMessage_ParamBody body;
} BinLogMessage_Param_v2;
/** @brief Version flags of BinLogMessage_Param_v2. */
#define BINLOGMSG_VERSION_PARAM_V2 (BINLOGMSG_VERSION_HEADER_V2 | BINLOGMSG_VERSION_PARAMETERIZED)

/** @brief Hashed message with a v3 header. */
typedef struct BinLogMessage_Param_v3 {
  /** Header. */
  BinLogMessage_Header_v3 header;
  /** Body. */
  BinLogMessage_ParamBody body;
} BinLogMessage_Param_v3;
/** @brief Version flags of BinLogMessage_Param_v3. */
#define BINLOGMSG_VERSION_PARAM_V3 (BINLOGMSG_VERSION_HEADER_V3 | BINLOGMSG_VERSION_PARAMETERIZED)

/** @brief Unhashed message with a v0 header. */
typedef struct BinLogMessage_Unhashed_v0 {
  /** Header. */
  BinLogMessage_Header_v0 header;
  /** Body. */
  BinLogMessage_UnhashedBody body;
} BinLogMessage_Unhashed_v0;
/** @brief Version flags of BinLogMessage_Unhashed_v0. */
#define BINLOGMSG_VERSION_UNHASHED_V0 (BINLOGMSG_VERSION_HEADER_V0 | BINLOGMSG_VERSION_UNHASHED_MSG)

/** @brief Unhashed message with a v1 header. */
typedef struct BinLogMessage_Unhashed_v1 {
  /** Header. */
  BinLogMessage_Header_v1 header;
  /** Body. */
  BinLogMessage_UnhashedBody body;
} BinLogMessage_Unhashed_v1;
/** @brief Version flags of BinLogMessage_Unhashed_v1. */
#define BINLOGMSG_VERSION_UNHASHED_V1 (BINLOGMSG_VERSION_HEADER_V1 | BINLOGMSG_VERSION_UNHASHED_MSG)

/** @brief Unhashed message with a v2 header. */
typedef struct BinLogMessage_Unhashed_v2 {
  /** Header. */
  BinLogMessage_Header_v2 header;
  /** Body. */
  BinLogMessage_UnhashedBody body;
} BinLogMessage_Unhashed_v2;
/** @brief Version flags of BinLogMessage_Unhashed_v2. */
#define BINLOGMSG_VERSION_UNHASHED_V2 (BINLOGMSG_VERSION_HEADER_V2 | BINLOGMSG_VERSION_UNHASHED_MSG)

/** @brief Unhashed message with a v3 header. */
typedef struct BinLogMessage_Unhashed_v3 {
  /** Header. */
  BinLogMessage_Header_v3 header;
  /** Body. */
  BinLogMessage_UnhashedBody body;
} BinLogMessage_Unhashed_v3;
/** @brief Version flags of BinLogMessage_Unhashed_v3. */
#define BINLOGMSG_VERSION_UNHASHED_V3 (BINLOGMSG_VERSION_HEADER_V3 | BINLOGMSG_VERSION_UNHASHED_MSG)

/** @brief Plain string message with a v1 header. */
typedef struct BinLogMessage_String_v1 {
  /** Header. */
  BinLogMessage_Header_v1 header;
  /** Message. */
  uint8_t string[0];
} BinLogMessage_String_v1;
/** @brief Version flags of BinLogMessage_String_v1. */
#define BINLOGMSG_VERSION_STRING_V1 (BINLOGMSG_VERSION_HEADER_V1)

/** @} */
