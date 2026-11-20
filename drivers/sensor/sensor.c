/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/sensor.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/irq.h>
#include <pbl/logging/logging.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_sensor, CONFIG_DRIVER_SENSOR_LOG_LEVEL);
PBL_DEVICE_CLASS_DEFINE(pbl_sensor_class, "sensor");

enum pbl_sensor__owner {
  OWNER_FREE,
  OWNER_DRIVER,
  OWNER_QUEUED,
  OWNER_LISTENER,
  OWNER_HELD,
};

static struct pbl_sensor_buf *prv_buf(const struct pbl_sensor *sensor, const uint8_t *data) {
  struct pbl_sensor_state *state = sensor->state;
  uintptr_t offset = (uintptr_t)(data - state->data);
  uint32_t idx = offset / state->buf_size;

  PBL_ASSERTN(data >= state->data && idx < state->num_bufs);

  return &state->bufs[idx];
}

static void prv_report_locked(const struct pbl_sensor *sensor, struct pbl_sensor_buf *buf) {
  const struct pbl_sensor_listener *listener = sensor->state->listener;

  if (listener != nullptr && listener->batch != nullptr && buf->batch.num_frames > 0U) {
    listener->batch(sensor, &buf->batch, listener->user_data);
  }

  pbl_irq_lock();
  if (buf->owner == OWNER_LISTENER) {
    buf->owner = OWNER_FREE;
  }
  pbl_irq_unlock();
}

static struct pbl_sensor_buf *prv_next_queued(struct pbl_sensor_state *state) {
  struct pbl_sensor_buf *next = nullptr;

  pbl_irq_lock();
  for (uint8_t i = 0U; i < state->num_bufs; i++) {
    struct pbl_sensor_buf *buf = &state->bufs[i];

    if (buf->owner == OWNER_QUEUED && (next == nullptr || (int32_t)(buf->seq - next->seq) < 0)) {
      next = buf;
    }
  }
  if (next != nullptr) {
    next->owner = OWNER_LISTENER;
  }
  pbl_irq_unlock();

  return next;
}

static void prv_work(struct pbl_work *work) {
  struct pbl_sensor_state *state = PBL_CONTAINER_OF(work, struct pbl_sensor_state, work);
  const struct pbl_sensor *sensor = state->sensor;
  struct pbl_sensor_buf *buf;

  pbl_sensor_lock(sensor);
  while ((buf = prv_next_queued(state)) != nullptr) {
    prv_report_locked(sensor, buf);
  }
  pbl_sensor_unlock(sensor);
}

int pbl_sensor_dev_init(const struct pbl_device *dev) {
  const struct pbl_sensor *sensor = PBL_CONTAINER_OF(dev, const struct pbl_sensor, dev);
  struct pbl_sensor_state *state = sensor->state;
  int ret = 0;

  state->sensor = sensor;
  pbl_work_init(&state->work, prv_work);

  if (sensor->ops->init != nullptr) {
    pbl_sensor_lock(sensor);
    ret = sensor->ops->init(sensor);
    pbl_sensor_unlock(sensor);
  }

  return ret;
}

void pbl_sensor_lock(const struct pbl_sensor *sensor) {
  pbl_mutex_lock(&sensor->state->lock, PBL_FOREVER);
}

void pbl_sensor_unlock(const struct pbl_sensor *sensor) {
  pbl_mutex_unlock(&sensor->state->lock);
}

static int32_t prv_raw(const uint8_t *p, uint8_t format) {
  switch (format) {
    case PBL_SENSOR_FMT_S8:
      return (int8_t)p[0];
    case PBL_SENSOR_FMT_S16_LE:
      return (int16_t)pbl_get_le16(p);
    case PBL_SENSOR_FMT_S16_BE:
      return (int16_t)pbl_get_be16(p);
    case PBL_SENSOR_FMT_U16_LE:
      return pbl_get_le16(p);
    case PBL_SENSOR_FMT_U16_BE:
      return pbl_get_be16(p);
    case PBL_SENSOR_FMT_S24_LE:
      return (int32_t)(pbl_get_le24(p) << 8) >> 8;
    case PBL_SENSOR_FMT_U24_LE:
      return (int32_t)pbl_get_le24(p);
    case PBL_SENSOR_FMT_S32_LE:
      return (int32_t)pbl_get_le32(p);
    default:
      return 0;
  }
}

static int32_t prv_value(const struct pbl_sensor_chan *chan, const uint8_t *frame) {
  int64_t raw = prv_raw(frame + chan->offset, chan->format);

  return (int32_t)(raw * chan->scale_num / chan->scale_den + chan->bias);
}

static uint8_t prv_watch_axis(const struct pbl_sensor *sensor, uint8_t sensor_axis) {
  for (uint8_t i = 0U; i < 3U; i++) {
    if (sensor->mount.map[i] == sensor_axis) {
      return i;
    }
  }

  return sensor_axis;
}

static int8_t prv_watch_dir(const struct pbl_sensor *sensor, uint8_t watch_axis) {
  int8_t dir = sensor->mount.dir[watch_axis];

  if (sensor->state->rotated && watch_axis != PBL_SENSOR_AXIS_Z) {
    dir = -dir;
  }

  return dir;
}

int pbl_sensor_decode(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch,
                      enum pbl_sensor_type type, uint16_t first, uint16_t count, int32_t *out) {
  const struct pbl_sensor_chan *chans[3] = {nullptr};
  uint8_t n = pbl_sensor_num_values(type);
  int8_t dir[3];

  for (uint8_t i = 0U; i < batch->num_chans; i++) {
    const struct pbl_sensor_chan *chan = &batch->chans[i];

    if (chan->type == type && (n == 1U ? chan->axis == 0U : chan->axis < 3U)) {
      chans[chan->axis] = chan;
    }
  }

  for (uint8_t i = 0U; i < n; i++) {
    if (chans[n == 1U ? 0U : sensor->mount.map[i]] == nullptr) {
      return -ENOENT;
    }
    dir[i] = prv_watch_dir(sensor, i);
  }

  for (uint16_t f = first; f < first + count; f++) {
    const uint8_t *frame = batch->data + (uint32_t)f * batch->stride;

    if (n == 1U) {
      *out++ = prv_value(chans[0], frame);
      continue;
    }

    for (uint8_t i = 0U; i < 3U; i++) {
      *out++ = dir[i] * prv_value(chans[sensor->mount.map[i]], frame);
    }
  }

  return n;
}

int pbl_sensor_read(const struct pbl_sensor *sensor, enum pbl_sensor_type type, int32_t *val,
                    uint64_t *timestamp_us) {
  struct pbl_sensor_frame frame = {0};
  struct pbl_sensor_batch batch;
  int ret;

  if (!pbl_device_is_ready(&sensor->dev)) {
    return -ENODEV;
  }

  if (!pbl_sensor_has_type(sensor, type)) {
    return -ENOTSUP;
  }

  pbl_sensor_lock(sensor);
  ret = sensor->ops->read(sensor, type, &frame);
  if (ret == 0) {
    batch = (struct pbl_sensor_batch){
      .data = frame.data,
      .chans = frame.chans,
      .num_chans = frame.num_chans,
      .num_frames = 1U,
      .timestamp_us = frame.timestamp_us,
    };
    ret = pbl_sensor_decode(sensor, &batch, type, 0U, 1U, val);
    ret = (ret < 0) ? -EIO : 0;
  }
  pbl_sensor_unlock(sensor);

  if (ret == 0 && timestamp_us != nullptr) {
    *timestamp_us = frame.timestamp_us;
  }

  return ret;
}

int pbl_sensor_attr_set(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val) {
  int ret;

  if (!pbl_device_is_ready(&sensor->dev)) {
    return -ENODEV;
  }

  if (!pbl_sensor_has_type(sensor, type) || sensor->ops->attr_set == nullptr) {
    return -ENOTSUP;
  }

  if (attr == PBL_SENSOR_ATTR_BATCH_MAX) {
    return -EACCES;
  }

  pbl_sensor_lock(sensor);
  ret = sensor->ops->attr_set(sensor, type, attr, val);
  pbl_sensor_unlock(sensor);

  return ret;
}

int pbl_sensor_attr_get(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val) {
  int ret;

  if (!pbl_device_is_ready(&sensor->dev)) {
    return -ENODEV;
  }

  if (!pbl_sensor_has_type(sensor, type) || sensor->ops->attr_get == nullptr) {
    return -ENOTSUP;
  }

  pbl_sensor_lock(sensor);
  ret = sensor->ops->attr_get(sensor, type, attr, val);
  pbl_sensor_unlock(sensor);

  return ret;
}

void pbl_sensor_set_listener(const struct pbl_sensor *sensor,
                             const struct pbl_sensor_listener *listener) {
  pbl_sensor_lock(sensor);
  sensor->state->listener = listener;
  pbl_sensor_unlock(sensor);
}

int pbl_sensor_set_event(const struct pbl_sensor *sensor, enum pbl_sensor_event_type type,
                         bool on) {
  int ret;

  if (!pbl_device_is_ready(&sensor->dev)) {
    return -ENODEV;
  }

  if (!pbl_sensor_has_event(sensor, type)) {
    return -ENOTSUP;
  }

  pbl_sensor_lock(sensor);
  ret = sensor->ops->set_event(sensor, type, on);
  if (ret == 0) {
    if (on) {
      sensor->state->events |= PBL_BIT(type);
    } else {
      sensor->state->events &= ~PBL_BIT(type);
    }
  }
  pbl_sensor_unlock(sensor);

  return ret;
}

bool pbl_sensor_get_event(const struct pbl_sensor *sensor, enum pbl_sensor_event_type type) {
  return (sensor->state->events & PBL_BIT(type)) != 0U;
}

void pbl_sensor_set_rotated(const struct pbl_sensor *sensor, bool rotated) {
  pbl_sensor_lock(sensor);
  sensor->state->rotated = rotated;
  pbl_sensor_unlock(sensor);
}

void pbl_sensor_hold(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch) {
  struct pbl_sensor_buf *buf = prv_buf(sensor, batch->data);

  pbl_irq_lock();
  PBL_ASSERTN(buf->owner == OWNER_LISTENER);
  buf->owner = OWNER_HELD;
  pbl_irq_unlock();
}

void pbl_sensor_release(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch) {
  struct pbl_sensor_buf *buf = prv_buf(sensor, batch->data);

  pbl_irq_lock();
  PBL_ASSERTN(buf->owner == OWNER_HELD);
  buf->owner = OWNER_FREE;
  pbl_irq_unlock();
}

uint8_t *pbl_sensor_buf_alloc(const struct pbl_sensor *sensor, uint16_t *size) {
  struct pbl_sensor_state *state = sensor->state;
  uint8_t *data = nullptr;

  pbl_irq_lock();
  for (uint8_t i = 0U; i < state->num_bufs; i++) {
    if (state->bufs[i].owner == OWNER_FREE) {
      state->bufs[i].owner = OWNER_DRIVER;
      data = state->data + (uint32_t)i * state->buf_size;
      break;
    }
  }
  pbl_irq_unlock();

  if (data != nullptr && size != nullptr) {
    *size = state->buf_size;
  }

  return data;
}

void pbl_sensor_buf_free(const struct pbl_sensor *sensor, const uint8_t *data) {
  struct pbl_sensor_buf *buf = prv_buf(sensor, data);

  pbl_irq_lock();
  PBL_ASSERTN(buf->owner == OWNER_DRIVER);
  buf->owner = OWNER_FREE;
  pbl_irq_unlock();
}

void pbl_sensor_report(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch) {
  struct pbl_sensor_buf *buf = prv_buf(sensor, batch->data);

  pbl_irq_lock();
  PBL_ASSERTN(buf->owner == OWNER_DRIVER);
  buf->batch = *batch;
  buf->owner = OWNER_LISTENER;
  pbl_irq_unlock();

  prv_report_locked(sensor, buf);
}

void pbl_sensor_submit(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch) {
  struct pbl_sensor_state *state = sensor->state;
  struct pbl_sensor_buf *buf = prv_buf(sensor, batch->data);

  pbl_irq_lock();
  PBL_ASSERTN(buf->owner == OWNER_DRIVER);
  buf->batch = *batch;
  buf->seq = state->seq++;
  buf->owner = OWNER_QUEUED;
  pbl_irq_unlock();

  (void)pbl_work_submit(&pbl_sys_workq_high, &state->work);
}

void pbl_sensor_report_event(const struct pbl_sensor *sensor,
                             const struct pbl_sensor_event *event) {
  const struct pbl_sensor_listener *listener = sensor->state->listener;
  struct pbl_sensor_event watch;
  uint8_t axis;

  if (!pbl_sensor_get_event(sensor, event->type) || listener == nullptr ||
      listener->event == nullptr) {
    return;
  }

  axis = prv_watch_axis(sensor, event->axis);
  watch = (struct pbl_sensor_event){
    .type = event->type,
    .axis = axis,
    .direction = (int8_t)(event->direction * prv_watch_dir(sensor, axis)),
  };

  listener->event(sensor, &watch, listener->user_data);
}
