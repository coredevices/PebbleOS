/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/dma.h>

PBL_DEVICE_CLASS_DEFINE(pbl_dma_class, "dma");

static int prv_check(const struct pbl_dma_chan *chan) {
  if (!pbl_device_is_ready(&chan->ctrl->dev)) {
    return -ENODEV;
  }

  if (chan->channel >= chan->ctrl->num_channels) {
    return -EINVAL;
  }

  return 0;
}

static bool prv_valid_width(enum pbl_dma_slave_buswidth width) {
  return width == PBL_DMA_SLAVE_BUSWIDTH_1_BYTE || width == PBL_DMA_SLAVE_BUSWIDTH_2_BYTES ||
         width == PBL_DMA_SLAVE_BUSWIDTH_4_BYTES;
}

int pbl_dma_slave_config(const struct pbl_dma_chan *chan, const struct pbl_dma_slave_config *cfg) {
  int ret = prv_check(chan);

  if (ret != 0) {
    return ret;
  }

  if ((cfg->src_addr != 0U && !prv_valid_width(cfg->src_addr_width)) ||
      (cfg->dst_addr != 0U && !prv_valid_width(cfg->dst_addr_width))) {
    return -EINVAL;
  }

  ret = chan->ctrl->ops->slave_config(chan->ctrl, chan, cfg);
  if (ret != 0) {
    return ret;
  }

  chan->ctrl->chans[chan->channel].cfg = *cfg;

  return 0;
}

static int prv_check_dir(const struct pbl_dma_chan_state *state, enum pbl_dma_dir dir) {
  if ((dir == PBL_DMA_DEV_TO_MEM && state->cfg.src_addr == 0U) ||
      (dir == PBL_DMA_MEM_TO_DEV && state->cfg.dst_addr == 0U)) {
    return -EINVAL;
  }

  return 0;
}

int pbl_dma_prep_slave_single(const struct pbl_dma_chan *chan, void *buf, size_t len,
                              enum pbl_dma_dir dir, pbl_dma_callback_t callback, void *arg) {
  struct pbl_dma_chan_state *state;
  int ret = prv_check(chan);

  if (ret != 0) {
    return ret;
  }

  state = &chan->ctrl->chans[chan->channel];
  if (prv_check_dir(state, dir) != 0 || len == 0U) {
    return -EINVAL;
  }

  state->callback = callback;
  state->arg = arg;

  return chan->ctrl->ops->prep_slave_single(chan->ctrl, chan->channel, buf, len, dir);
}

int pbl_dma_prep_dma_cyclic(const struct pbl_dma_chan *chan, void *buf, size_t buf_len,
                            size_t period_len, enum pbl_dma_dir dir, pbl_dma_callback_t callback,
                            void *arg) {
  struct pbl_dma_chan_state *state;
  int ret = prv_check(chan);

  if (ret != 0) {
    return ret;
  }

  if (chan->ctrl->ops->prep_dma_cyclic == nullptr) {
    return -ENOTSUP;
  }

  state = &chan->ctrl->chans[chan->channel];
  if (prv_check_dir(state, dir) != 0 || period_len == 0U || buf_len < period_len ||
      (buf_len % period_len) != 0U) {
    return -EINVAL;
  }

  state->callback = callback;
  state->arg = arg;

  return chan->ctrl->ops->prep_dma_cyclic(chan->ctrl, chan->channel, buf, buf_len, period_len, dir);
}

int pbl_dma_terminate(const struct pbl_dma_chan *chan) {
  int ret = prv_check(chan);

  if (ret != 0) {
    return ret;
  }

  return chan->ctrl->ops->terminate(chan->ctrl, chan->channel);
}

int pbl_dma_tx_status(const struct pbl_dma_chan *chan, size_t *residue) {
  int ret = prv_check(chan);

  if (ret != 0) {
    return ret;
  }

  return chan->ctrl->ops->tx_status(chan->ctrl, chan->channel, residue);
}

void pbl_dma_complete(const struct pbl_dma_controller *ctrl, uint8_t channel, int status) {
  const struct pbl_dma_chan_state *state = &ctrl->chans[channel];

  if (state->callback != nullptr) {
    state->callback(state->arg, status);
  }
}
