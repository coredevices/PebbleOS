/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "timezone_database.h"

#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "services/common/clock.h"
#include "system/passert.h"

#include <util/attributes.h>
#include <util/size.h>

#include <string.h>

// The format of the database is as follows
// Header
//   2 bytes  - Region count
//   2 bytes  - DST Rule count
//   2 bytes  - Link count
// Regions
//   For each region (24 bytes):
//     1 byte   - Continent index, @see CONTINENT_NAMES
//     15 bytes - City name
//     2 bytes  - GMT offset in minutes as a int16_t
//     5 bytes  - Timezone name abbreviation (aka tz_abbr)
//     1 byte   - DST Rule ID
// DST Rules
//   For each DST ID (16 bytes)
//     For each rule in the pair, first the start rule followed by the end rule (8 bytes)
//       @see TimezoneDSTRule for the structure
// Links
//   For each link (35 bytes)
//     2 bytes  - The region id this link maps to
//     33 bytes - The name of the link that should be treated as an alias to the linked region

typedef struct PACKED {
  uint16_t region_count;
  uint16_t dst_rule_count;
  uint16_t link_count;
} TimezoneDatabaseFlashHeader;
#define TZDATA_HEADER_BYTES (sizeof(TimezoneDatabaseFlashHeader))

#define TIMEZONE_CITY_LENGTH 15 // maximum length of the city name in timezone database
#define REGION_BYTES (1 + TIMEZONE_CITY_LENGTH + 2 + 5 + 1)

#define DST_RULE_BYTES (sizeof(TimezoneDSTRule))
#define DST_RULE_PAIR_BYTES (DST_RULE_BYTES * 2)

#define LINK_REGION_LENGTH 2
#define LINK_NAME_LENGTH 33
#define LINK_BYTES (LINK_REGION_LENGTH + LINK_NAME_LENGTH)


//! Names for all the continents we support. The timezone database stores continents as indexes
//! into this constant array.
const char * const CONTINENT_NAMES[] = { "Africa",
                                         "America",
                                         "Antarctica",
                                         "Asia",
                                         "Atlantic",
                                         "Australia",
                                         "Europe",
                                         "Indian",
                                         "Pacific",
                                         "Etc"};


//! Helper function to curry out some common arguments to the resource reads in this file.
static bool prv_database_read(uint32_t offset, void *data, size_t num_bytes) {
  return resource_load_byte_range_system(SYSTEM_APP, RESOURCE_ID_TIMEZONE_DATABASE,
                                         offset, data, num_bytes) == num_bytes;
}

//! Note! This count includes rule 0 which isn't actually stored in the database.
static int prv_get_dst_rule_count(void) {
  uint16_t dst_rule_count;
  prv_database_read(offsetof(TimezoneDatabaseFlashHeader, dst_rule_count),
                    &dst_rule_count, sizeof(dst_rule_count));
  return dst_rule_count;
}

static int prv_get_link_count(void) {
  uint16_t link_count;
  prv_database_read(offsetof(TimezoneDatabaseFlashHeader, link_count),
                    &link_count, sizeof(link_count));
  return link_count;
}

int timezone_database_get_region_count(void) {
  uint16_t region_count;
  prv_database_read(offsetof(TimezoneDatabaseFlashHeader, region_count),
                    &region_count, sizeof(region_count));
  return region_count;
}

bool timezone_database_load_region_info(uint16_t region_id, TimezoneInfo *tz_info) {
  const int region_offset =
      // Skip over the region count
      TZDATA_HEADER_BYTES +
      // Skip over the regions list
      (region_id * REGION_BYTES);

  //! Struct for reading data from a raw database of timezone information
  struct PACKED {
    int16_t gmt_offset_minutes;   //!< timezone offset from UTC time (in minutes)
    char tz_abbr[TZ_LEN - 1];     //!< timezone abbreviation (without terminating nul)
    int8_t dst_id;                //!< daylight savings time index identifier
  } tz_data;

  // Load the timezone information for the region_id, excluding the country + city_name itself
  if (!prv_database_read(region_offset + 1 + TIMEZONE_CITY_LENGTH, &tz_data, sizeof(tz_data))) {
    *tz_info = (TimezoneInfo) { .dst_id = 0 };
    return false;
  }

  *tz_info = (TimezoneInfo) {
    .dst_id = tz_data.dst_id,
    .timezone_id = region_id,
    .tm_gmtoff = tz_data.gmt_offset_minutes * SECONDS_PER_MINUTE,
    // Leave the dst_start and dst_end timestamps uninitialized
    .dst_start = 0,
    .dst_end = 0
  };
  memcpy(tz_info->tm_zone, tz_data.tz_abbr, sizeof(tz_data.tz_abbr));

  return true;
}

bool timezone_database_load_region_name(uint16_t region_id, char *region_name) {
  if (region_id > timezone_database_get_region_count()) {
    return false;
  }

  const int region_offset =
      // Skip over the region count
      TZDATA_HEADER_BYTES +
      // Skip over the regions list
      (region_id * REGION_BYTES);

  // Read the continent index, which is the first byte
  uint8_t continent_index = 0;
  prv_database_read(region_offset, &continent_index, sizeof(continent_index));
  PBL_ASSERTN(continent_index < ARRAY_LENGTH(CONTINENT_NAMES));

  // Copy the continent name into our buffer, followed by a slash.
  const int continent_name_length = strlen(CONTINENT_NAMES[continent_index]);
  memcpy(region_name, CONTINENT_NAMES[continent_index], continent_name_length);
  region_name[continent_name_length] = '/';

  char *city_name = region_name + continent_name_length + 1 /* slash */;

  // How many bytes left we have of our buffer to fill in the city information.
  const int remaining_size = (region_name + TIMEZONE_NAME_LENGTH) - city_name;

  // Fill the rest of our buffer with city name.
  // Our generation script will ensure that continent + slash + city name + null will always
  // fit in our buffer with a null terminator to spare.
  prv_database_read(region_offset + 1 /* continent index */, city_name, remaining_size);

  // FIXME: Perhaps we should refactor this to do one read instead of two? The information is
  // right beside each other and it's pretty wasteful to do a 1 byte read followed by a 15 byte
  // read as separate reads.

  return true;
}

bool timezone_database_load_dst_rule(uint8_t dst_id, TimezoneDSTRule *start, TimezoneDSTRule *end) {
  const int region_count = timezone_database_get_region_count();

  const int dst_rule_pair_offset =
      // Skip over the region count
      TZDATA_HEADER_BYTES +
      // Skip over the regions list
      (region_count * REGION_BYTES) +
      // Find the appropriate DST zone (DST ID is 1 indexed)
      ((dst_id - 1) * DST_RULE_PAIR_BYTES);

  // First half of the DST rule pair
  if (!prv_database_read(dst_rule_pair_offset, start, DST_RULE_BYTES) ||
      !prv_database_read(dst_rule_pair_offset + DST_RULE_BYTES, end, DST_RULE_BYTES)) {
    PBL_LOG(LOG_LEVEL_WARNING, "Failed to load timezone for DST ID %"PRIu8, dst_id);
    return false;
  }

  if (start->ds_label == '\0' || end->ds_label == '\0') {
    // Does not observe DST
    return false;
  }

  return true;
}

static int prv_search_regions_by_name(const char *region_name, int region_name_length) {
  const int region_count = timezone_database_get_region_count();

  for (int i = 0; i < region_count; i++) {
    char lookup_region_name[TIMEZONE_NAME_LENGTH];
    timezone_database_load_region_name(i, lookup_region_name);
    if (strncmp(region_name, lookup_region_name, region_name_length) == 0) {
      return i;
    }
  }

  return -1;
}

static int prv_search_links_by_name(const char *region_name, int region_name_length) {
  char name_asciz[256] = {0};
  memcpy(name_asciz, region_name, region_name_length);

  const int link_section_offset =
      // Skip over the region count
      TZDATA_HEADER_BYTES +
      // Skip over the regions list
      (timezone_database_get_region_count() * REGION_BYTES) +
      // Skip over the DST list
      ((prv_get_dst_rule_count() - 1) * DST_RULE_PAIR_BYTES);

  const uint16_t link_count = prv_get_link_count();

  for (int i = 0; i < link_count; i++) {
    const int link_offset = link_section_offset + (i * LINK_BYTES);

    char link_name[LINK_NAME_LENGTH + 1]; // + max length + null terminator
    prv_database_read(link_offset + LINK_REGION_LENGTH, link_name, LINK_NAME_LENGTH);
    link_name[sizeof(link_name) - 1] = '\0';

    if (strncmp(name_asciz, link_name, LINK_NAME_LENGTH) == 0) {
      // Found it!
      uint16_t linked_region_id;
      prv_database_read(link_offset, &linked_region_id, sizeof(linked_region_id));
      return linked_region_id;
    }
  }

  return -1;
}

int timezone_database_find_region_by_name(const char *region_name, int region_name_length) {
  int region_id = prv_search_regions_by_name(region_name, region_name_length);

  if (region_id == -1) {
    // Might be a Link, let's check.
    // To explain: iOS, when not synchronized from the internet, uses _ancient_ IANA region names.
    // For example, when in California, iOS will send "US/Pacific" which hasn't been the name of
    // that timezone since 1993. So we need to support linked timezones sent from the phone.
    region_id = prv_search_links_by_name(region_name, region_name_length);
  }

  return region_id;
}
