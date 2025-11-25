/* SPDX-FileCopyrightText: 2016 Intel Corporation */
/* SPDX-License-Identifier: Apache-2.0 */

/**
 * Allow project to override this partition scheme
 * The following variables are allowed to be defined:
 *
 * QUARK_START_PAGE the first page where the QUARK code is located
 * QUARK_NB_PAGE the number of pages reserved for the QUARK. The ARC gets the
 *               remaining pages (out of 148).
 */
#ifndef PROJECT_MAPPING_H
#define PROJECT_MAPPING_H

#define QUARK_NB_PAGE 125
#include "machine/soc/intel/quark_se/quark_se_mapping.h"

#endif /* !PROJECT_MAPPING_H */
