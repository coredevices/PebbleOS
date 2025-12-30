/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "mfg/mfg_info.h"

#include <stdbool.h>


//! Which regulatory marks and/or IDs a given product should display.
typedef struct RegulatoryFlags {
//! Australia Regulatory Compliance Mark
  bool has_australia_rcm:1;
//! Canada IC ID
  bool has_canada_ic:1;
//! Canada ISED ID
  bool has_canada_ised:1;
//! China CMIIT ID
  bool has_china_cmiit:1;
//! EU CE Mark
  bool has_eu_ce:1;
//! EU WEEE Mark (wastebin with X)
  bool has_eu_weee:1;
//! UKCA Mark
  bool has_ukca:1;
//! Japan TELEC (Telecom Engineering Center) [R] mark and ID
//! (Radio equipment conformity)
  bool has_japan_telec_r:1;
//!  TELEC mark [T] mark and ID (Terminal equipment conformity)
  bool has_japan_telec_t:1;
//! Korea
//!  - KCC mark
//!  - Details window with KCC mark and KCC ID
  bool has_korea_kcc:1;
//! Mexico NOM NYCE mark
  bool has_mexico_nom_nyce:1;
//! USA FCC Mark and FCC ID
  bool has_usa_fcc:1;
} RegulatoryFlags;

typedef struct CertificationIds {
  const char *company_name;
  const char *canada_ic_id;
  const char *canada_ised_id;
  const char *china_cmiit_id;
  const char *japan_telec_r_id;
  const char *japan_telec_t_id;
  const char *korea_kcc_id;
  const char *mexico_ifetel_id;
  const char *usa_fcc_id;
} CertificationIds;


static const RegulatoryFlags s_regulatory_flags_fallback = {
};

// Certifiation ID strings used for bigboards and such.
static const CertificationIds s_certification_ids_fallback = {
  .company_name = "ACME Inc.",
  .canada_ic_id = "XXXXXX-YYY",
  .canada_ised_id = "XXXXXX-YYYYYYYYYYY",
  .china_cmiit_id = "ABCDEFGHIJ",
  .japan_telec_r_id = "XXX-YYYYYY",
  .japan_telec_t_id = "D XX YYYY ZZZ",
  .korea_kcc_id = "WWWW-XXX-YYY-ZZZ",
  .mexico_ifetel_id = "RCPPEXXXX-YYYY",
  .usa_fcc_id = "XXX-YYY",
};


static const RegulatoryFlags s_regulatory_flags_snowy = {
  .has_canada_ic = true,
  .has_china_cmiit = true,
  .has_eu_ce = true,
  .has_eu_weee = true,
  .has_japan_telec_r = true,
  .has_japan_telec_t = true,
  .has_korea_kcc = true,
  .has_usa_fcc = true,
};

static const CertificationIds s_certification_ids_snowy = {
  .company_name = "Pebble Technology Corp.",
  .canada_ic_id = "10805A-501",
  .china_cmiit_id = "2015DJ1504",
  .japan_telec_r_id = "201-150104",
  .japan_telec_t_id = "D 15 0015 201",
  .korea_kcc_id = "MSIP-CRM-PEB-WQ3",
  .usa_fcc_id = "RGQ-501",
};

static const CertificationIds s_certification_ids_bobby = {
  .company_name = "Pebble Technology Corp.",
  .canada_ic_id = "10805A-511",
  .china_cmiit_id = "2015DJ3458",
  .japan_telec_r_id = "201-150257",
  .japan_telec_t_id = "D 15 0065 201",
  .korea_kcc_id = "MSIP-CRM-PEB-WQ3",
  .usa_fcc_id = "RGQ-511",
};

static const RegulatoryFlags s_regulatory_flags_spalding = {
  .has_canada_ic = true,
  .has_eu_ce = true,
  .has_eu_weee = true,
  .has_usa_fcc = true,
};

static const CertificationIds s_certification_ids_spalding = {
  .company_name = "Pebble Technology Corp.",
  .canada_ic_id = "10805A-601",
  .usa_fcc_id = "RGQ-601",
};

static const RegulatoryFlags s_regulatory_flags_silk = {
  .has_australia_rcm = true,
  .has_canada_ic = true,
  .has_china_cmiit = true,
  .has_eu_ce = true,
  .has_eu_weee = true,
  .has_japan_telec_r = true,
  .has_mexico_nom_nyce = true,
  .has_usa_fcc = true,
};

static const CertificationIds s_certification_ids_silk = {
  .company_name = "Pebble Technology Corp.",
  .canada_ic_id = "10805A-1001",
  .china_cmiit_id = "2016DJ4469",
  .usa_fcc_id = "RGQ-1001",
  .japan_telec_r_id = "201-160535",
  .mexico_ifetel_id = "RCPPE1016-1161"
};

static const CertificationIds s_certification_ids_silk_hr = {
  .company_name = "Pebble Technology Corp.",
  .canada_ic_id = "10805A-1002",
  .china_cmiit_id = "2016DJ4931",
  .usa_fcc_id = "RGQ-1002",
  .japan_telec_r_id = "201-160558",
  .mexico_ifetel_id = "RCPPE1016-1238"
};

static const RegulatoryFlags s_regulatory_flags_obelix = {
  .has_canada_ised = true,
  .has_eu_ce = true,
  .has_ukca = true,
  .has_usa_fcc = true,
};

// FIXME(OBELIX): Replace with real IDs
static const CertificationIds s_certification_ids_obelix = {
  .company_name = "Core Devices LLC",
  .canada_ised_id = "XXXXXX-YYYYYYYYYYY",
  .usa_fcc_id = "XXX-YYY",
};

static const RegulatoryFlags * prv_get_regulatory_flags(void) {
#if PLATFORM_SNOWY
  return &s_regulatory_flags_snowy;
#elif PLATFORM_SPALDING
  return &s_regulatory_flags_spalding;
#elif PLATFORM_SILK
  return &s_regulatory_flags_silk;
#elif PLATFORM_ASTERIX
  // TODO: add applicable flags
  return &s_regulatory_flags_fallback;
#elif PLATFORM_OBELIX
  return &s_regulatory_flags_obelix;
#else
  return &s_regulatory_flags_fallback;
#endif
}

//! Don't call this function directly. Use the prv_get_*_id functions instead.
static const CertificationIds * prv_get_certification_ids(void) {
#if defined(BOARD_SNOWY_S3)
  return &s_certification_ids_bobby;
#elif defined(BOARD_SNOWY_EVT) || defined(BOARD_SNOWY_EVT2) || \
      defined(BOARD_SNOWY_DVT)
  return &s_certification_ids_snowy;
#elif defined(BOARD_SPALDING) || defined(BOARD_SPALDING_EVT)
  return &s_certification_ids_spalding;
#elif PLATFORM_SILK && !defined(BOARD_SILK_FLINT)
  if (mfg_info_is_hrm_present()) {
    return &s_certification_ids_silk_hr;
  } else {
    return &s_certification_ids_silk;
  }
#elif PLATFORM_ASTERIX
  // TODO: add real certification ids
  return &s_certification_ids_fallback;
#elif PLATFORM_OBELIX
  return &s_certification_ids_obelix;
#else
  return &s_certification_ids_fallback;
#endif
}

#define ID_GETTER(ID_KIND) \
  static const char * prv_get_##ID_KIND(void) { \
    return prv_get_certification_ids()->ID_KIND ?: \
      s_certification_ids_fallback.ID_KIND; \
  }

ID_GETTER(company_name)
ID_GETTER(canada_ic_id)
ID_GETTER(china_cmiit_id)
ID_GETTER(japan_telec_r_id)
ID_GETTER(japan_telec_t_id)
ID_GETTER(korea_kcc_id)
ID_GETTER(mexico_ifetel_id)
ID_GETTER(usa_fcc_id)
ID_GETTER(canada_ised_id)

#undef ID_GETTER

//! Get the model string from MFG storage
//! @param buffer a character array that's at least MFG_INFO_MODEL_STRING_LENGTH in size
static void prv_get_model(char *buffer) {
  mfg_info_get_model(buffer);
}
