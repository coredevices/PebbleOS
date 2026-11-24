/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */

#ifndef _DT_BINDINGS_IIO_ADC_NORDIC_NRF52832_SAADC_H
#define _DT_BINDINGS_IIO_ADC_NORDIC_NRF52832_SAADC_H

/* Inputs of the nRF52 SAADC, as io-channels take them */
#define NRF52_SAADC_AIN0 0
#define NRF52_SAADC_AIN1 1
#define NRF52_SAADC_AIN2 2
#define NRF52_SAADC_AIN3 3
#define NRF52_SAADC_AIN4 4
#define NRF52_SAADC_AIN5 5
#define NRF52_SAADC_AIN6 6
#define NRF52_SAADC_AIN7 7
#define NRF52_SAADC_VDD  8
/* VDDH, through its divider by 5, on the nRF52833 and nRF52840 */
#define NRF52_SAADC_VDDHDIV5 9

#endif /* _DT_BINDINGS_IIO_ADC_NORDIC_NRF52832_SAADC_H */
