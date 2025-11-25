/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/weather/weather_service.h"
#include "services/normal/weather/weather_types.h"
#include "system/status_codes.h"
#include "util/attributes.h"
#include "util/pstring.h"
#include "util/time/time.h"
#include "util/uuid.h"

#include <stdint.h>

#define WEATHER_DB_CURRENT_VERSION (3)

typedef Uuid WeatherDBKey;

typedef struct PACKED {
  uint8_t version;
  int16_t current_temp;
  WeatherType current_weather_type;
  int16_t today_high_temp;
  int16_t today_low_temp;
  WeatherType tomorrow_weather_type;
  int16_t tomorrow_high_temp;
  int16_t tomorrow_low_temp;
  time_t last_update_time_utc;
  bool is_current_location;
  SerializedArray pstring16s;
} WeatherDBEntry;

typedef enum WeatherDbStringIndex {
  WeatherDbStringIndex_LocationName,
  WeatherDbStringIndex_ShortPhrase,
  WeatherDbStringIndexCount,
} WeatherDbStringIndex;

#define MIN_ENTRY_SIZE (sizeof(WeatherDBEntry))
#define MAX_ENTRY_SIZE (MIN_ENTRY_SIZE + \
                        WEATHER_SERVICE_MAX_WEATHER_LOCATION_BUFFER_SIZE + \
                        WEATHER_SERVICE_MAX_SHORT_PHRASE_BUFFER_SIZE)

// Memory ownership: pointer to key and entry must not be saved, as they become invalid after
// the callback finishes
typedef void (*WeatherDBIteratorCallback)(WeatherDBKey *key, WeatherDBEntry *entry, void *context);

// ------------------------------------------------------------------------------------
// WeatherDB functions
status_t weather_db_for_each(WeatherDBIteratorCallback cb, void *context);

// ------------------------------------------------------------------------------------
// BlobDB Implementation

void weather_db_init(void);

status_t weather_db_flush(void);

status_t weather_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

int weather_db_get_len(const uint8_t *key, int key_len);

status_t weather_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

status_t weather_db_delete(const uint8_t *key, int key_len);
