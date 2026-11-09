/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/audio/dmic.h>
#include <pbl/drivers/regulator.h>
#include <pbl/kernel/delay.h>
#include <pbl/util/misc.h>

static const struct pbl_audio_dmic *prv_dmic(const struct pbl_audio_codec *codec) {
  return container_of(codec, const struct pbl_audio_dmic, codec);
}

static int prv_prepare(const struct pbl_audio_codec *codec) {
  const struct pbl_audio_dmic *dmic = prv_dmic(codec);
  int ret;

  if (dmic->vref == nullptr) {
    return 0;
  }

  ret = pbl_device_init(&dmic->vref->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_regulator_enable(dmic->vref);
  if (ret != 0) {
    return ret;
  }

  if (dmic->wakeup_delay_ms > 0U) {
    pbl_sleep_ms(dmic->wakeup_delay_ms);
  }

  return 0;
}

static int prv_unprepare(const struct pbl_audio_codec *codec) {
  const struct pbl_audio_dmic *dmic = prv_dmic(codec);

  return (dmic->vref != nullptr) ? pbl_regulator_disable(dmic->vref) : 0;
}

const struct pbl_audio_codec_ops pbl_audio_dmic_ops = {
  .prepare = prv_prepare,
  .unprepare = prv_unprepare,
};

#include PBL_DT_INSTANCES(dmic_codec)
