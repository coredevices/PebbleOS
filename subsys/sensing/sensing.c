/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/sensor.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/sensing/sensing.h>
#include <pbl/util/math.h>

PBL_LOG_MODULE_DEFINE(sensing, CONFIG_SENSING_LOG_LEVEL);

#define REC_ALIGN 8U

// Largest merged record, as a fraction of the buffer
#define REC_MERGE_DIV 8U

// Frames of one batch, or of consecutive batches merged, as the driver produced them
struct prv_rec {
  uint64_t t_last_us;
  const struct pbl_sensor_chan *chans;
  uint32_t interval_us;
  uint32_t seq;
  uint16_t num_frames;
  uint16_t len;
  uint8_t num_chans;
  uint8_t stride;
};

#define REC_HDR_SIZE (PBL_DIV_ROUND_UP(sizeof(struct prv_rec), REC_ALIGN) * REC_ALIGN)

// Ring of records: [tail, head) or, wrapped, [tail, wrap) then [0, head)
struct pbl_sensing_stream {
  struct pbl_sensing_stream *next;
  uint8_t type;
  bool empty;
  uint32_t want_us;
  uint32_t interval_us;
  int32_t batch;
  int32_t next_batch;
  uint32_t seq;
  uint8_t *buf;
  uint16_t size;
  uint16_t tail;
  uint16_t head;
  uint16_t last;
  uint16_t wrap;
};

struct pbl_sensing_sensor {
  const struct pbl_sensor *sensor;
  struct pbl_sensor_listener listener;
  struct pbl_sensing_client *clients;
  struct pbl_sensing_stream *streams;
  uint8_t event_users[PBL_SENSOR_EVENT_COUNT];
  bool suspended;
};

static struct pbl_mutex s_lock = PBL_MUTEX_INITIALIZER;
static struct pbl_sensing_sensor s_sensors[CONFIG_SENSING_SENSORS];

static void prv_batch_cb(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch,
                         void *user_data);
static void prv_event_cb(const struct pbl_sensor *sensor, const struct pbl_sensor_event *event,
                         void *user_data);

static struct pbl_sensing_sensor *prv_ctx_get(const struct pbl_sensor *sensor, bool create) {
  struct pbl_sensing_sensor *ctx = nullptr;
  bool created = false;

  (void)pbl_mutex_lock(&s_lock, PBL_FOREVER);

  for (size_t i = 0U; i < CONFIG_SENSING_SENSORS; i++) {
    if (s_sensors[i].sensor == sensor) {
      ctx = &s_sensors[i];
      break;
    }

    if (create && ctx == nullptr && s_sensors[i].sensor == nullptr) {
      ctx = &s_sensors[i];
      ctx->sensor = sensor;
      ctx->listener = (struct pbl_sensor_listener){
        .batch = prv_batch_cb,
        .event = prv_event_cb,
        .user_data = ctx,
      };
      created = true;
      break;
    }
  }

  pbl_mutex_unlock(&s_lock);

  // Outside of s_lock, as it takes the device lock
  if (created) {
    pbl_sensor_set_listener(sensor, &ctx->listener);
  }

  if (ctx == nullptr && create) {
    PBL_LOG_ERR("Too many sensors");
  }

  return ctx;
}

static struct pbl_sensing_stream *prv_stream_get(struct pbl_sensing_sensor *ctx,
                                                 enum pbl_sensor_type type, bool create) {
  struct pbl_sensing_stream *s;

  for (s = ctx->streams; s != nullptr; s = s->next) {
    if (s->type == type) {
      return s;
    }
  }

  if (!create) {
    return nullptr;
  }

  s = pbl_zalloc(sizeof(*s));
  if (s == nullptr) {
    return nullptr;
  }

  s->type = type;
  s->empty = true;
  s->next = ctx->streams;
  ctx->streams = s;

  return s;
}

static inline struct prv_rec *prv_rec(const struct pbl_sensing_stream *s, uint16_t off) {
  return (struct prv_rec *)(void *)&s->buf[off];
}

static uint32_t prv_rec_len(uint32_t num_frames, uint8_t stride) {
  return PBL_DIV_ROUND_UP(REC_HDR_SIZE + num_frames * stride, REC_ALIGN) * REC_ALIGN;
}

static bool prv_rec_has(const struct prv_rec *rec, uint32_t pos) {
  return pos - rec->seq < rec->num_frames;
}

static void prv_rec_batch(const struct pbl_sensing_stream *s, uint16_t off,
                          struct pbl_sensor_batch *batch) {
  const struct prv_rec *rec = prv_rec(s, off);

  *batch = (struct pbl_sensor_batch){
    .data = &s->buf[off + REC_HDR_SIZE],
    .chans = rec->chans,
    .num_chans = rec->num_chans,
    .stride = rec->stride,
    .num_frames = rec->num_frames,
    .timestamp_us = rec->t_last_us,
    .interval_us = rec->interval_us,
  };
}

static uint16_t prv_next(const struct pbl_sensing_stream *s, uint16_t off) {
  uint16_t next = off + prv_rec(s, off)->len;

  return (next == s->wrap) ? 0U : next;
}

static bool prv_wrapped(const struct pbl_sensing_stream *s) {
  return !s->empty && s->head <= s->tail;
}

static void prv_reset(struct pbl_sensing_stream *s) {
  s->empty = true;
  s->tail = 0U;
  s->head = 0U;
  s->last = 0U;
  s->wrap = s->size;
}

static void prv_evict(struct pbl_sensing_sensor *ctx, struct pbl_sensing_stream *s) {
  const struct prv_rec *rec = prv_rec(s, s->tail);
  uint32_t end = rec->seq + rec->num_frames;

  for (struct pbl_sensing_client *c = ctx->clients; c != nullptr; c = c->next) {
    if (c->stream == s && (int32_t)(end - c->pos) > 0) {
      c->lost += end - c->pos;
      c->pos = end;
    }
  }

  if (s->tail == s->last) {
    prv_reset(s);
    return;
  }

  uint16_t next = s->tail + rec->len;
  if (next == s->wrap) {
    s->wrap = s->size;
    next = 0U;
  }
  s->tail = next;
}

// Contiguous room for a record of len bytes, evicting the oldest ones as needed
static uint16_t prv_alloc(struct pbl_sensing_sensor *ctx, struct pbl_sensing_stream *s,
                          uint16_t len) {
  for (;;) {
    if (s->empty) {
      prv_reset(s);
      return 0U;
    }

    if (!prv_wrapped(s)) {
      if (s->size - s->head >= len) {
        return s->head;
      }
      if (s->tail >= len) {
        s->wrap = s->head;
        s->head = 0U;
        return 0U;
      }
    } else if (s->tail - s->head >= len) {
      return s->head;
    }

    prv_evict(ctx, s);
  }
}

// Extend the newest record by len bytes in place, if there is room
static bool prv_extend(struct pbl_sensing_sensor *ctx, struct pbl_sensing_stream *s, uint16_t len) {
  if (!prv_wrapped(s)) {
    return s->size - s->head >= len;
  }

  while (s->tail != s->last && s->tail - s->head < len && prv_wrapped(s)) {
    prv_evict(ctx, s);
  }

  return prv_wrapped(s) ? (s->tail - s->head >= len) : (s->size - s->head >= len);
}

static bool prv_merges(const struct prv_rec *rec, const struct pbl_sensor_batch *batch) {
  if (rec->chans != batch->chans || rec->num_chans != batch->num_chans ||
      rec->stride != batch->stride || rec->interval_us != batch->interval_us) {
    return false;
  }

  int64_t t_first = (int64_t)pbl_sensor_frame_time(batch, 0U);
  int64_t gap = t_first - (int64_t)(rec->t_last_us + rec->interval_us);

  return gap <= (int64_t)(rec->interval_us / 2U) && -gap <= (int64_t)(rec->interval_us / 2U);
}

static bool prv_buf_alloc(struct pbl_sensing_sensor *ctx, struct pbl_sensing_stream *s,
                          uint8_t stride) {
  int32_t batch_max = 1;
  uint32_t size;

  (void)pbl_sensor_attr_get(ctx->sensor, s->type, PBL_SENSOR_ATTR_BATCH_MAX, &batch_max);

  size = PBL_MAX(2U * (uint32_t)PBL_MAX(batch_max, 1), (uint32_t)CONFIG_SENSING_BUFFER_FRAMES) *
             stride +
         8U * REC_HDR_SIZE;
  size = PBL_MIN(size, UINT16_MAX + 1U - REC_ALIGN) / REC_ALIGN * REC_ALIGN;

  s->buf = pbl_malloc(size);
  if (s->buf == nullptr) {
    PBL_LOG_ERR("No memory for a %u byte buffer", (unsigned int)size);
    return false;
  }

  s->size = (uint16_t)size;
  prv_reset(s);

  return true;
}

static void prv_store(struct pbl_sensing_sensor *ctx, struct pbl_sensing_stream *s,
                      const struct pbl_sensor_batch *batch) {
  const uint8_t *data = batch->data;
  uint32_t num_frames = batch->num_frames;
  uint32_t fit;
  struct prv_rec *rec;

  if (s->buf == nullptr && !prv_buf_alloc(ctx, s, batch->stride)) {
    return;
  }

  fit = (s->size - REC_HDR_SIZE) / batch->stride;
  if (num_frames > fit) {
    PBL_LOG_WRN("Batch of %u frames truncated", (unsigned int)num_frames);
    data += (num_frames - fit) * batch->stride;
    num_frames = fit;
  }

  if (!s->empty) {
    rec = prv_rec(s, s->last);
    if (prv_merges(rec, batch)) {
      uint32_t len = prv_rec_len(rec->num_frames + num_frames, rec->stride);

      // Records are what gets evicted: keep them small so eviction drops little
      if (len <= s->size / REC_MERGE_DIV && prv_extend(ctx, s, (uint16_t)(len - rec->len))) {
        rec = prv_rec(s, s->last);
        memcpy(&s->buf[s->last + REC_HDR_SIZE + rec->num_frames * rec->stride], data,
               num_frames * batch->stride);
        rec->num_frames += num_frames;
        rec->len = (uint16_t)len;
        rec->t_last_us = batch->timestamp_us;
        s->head = s->last + (uint16_t)len;
        s->seq += num_frames;
        return;
      }
    }
  }

  uint16_t len = (uint16_t)prv_rec_len(num_frames, batch->stride);
  uint16_t off = prv_alloc(ctx, s, len);

  rec = prv_rec(s, off);
  *rec = (struct prv_rec){
    .t_last_us = batch->timestamp_us,
    .chans = batch->chans,
    .interval_us = batch->interval_us,
    .seq = s->seq,
    .num_frames = (uint16_t)num_frames,
    .len = len,
    .num_chans = batch->num_chans,
    .stride = batch->stride,
  };
  memcpy(&s->buf[off + REC_HDR_SIZE], data, num_frames * batch->stride);

  s->last = off;
  s->head = off + len;
  s->empty = false;
  s->seq += num_frames;
}

// Offset of the record holding frame pos, or -1 if it is not acquired yet
static int32_t prv_find(const struct pbl_sensing_stream *s, uint32_t pos) {
  if (s->buf == nullptr || s->empty) {
    return -1;
  }

  for (uint16_t off = s->tail;; off = prv_next(s, off)) {
    if (prv_rec_has(prv_rec(s, off), pos)) {
      return off;
    }
    if (off == s->last) {
      return -1;
    }
  }
}

// Phase accumulator: whether to deliver the next frame, the first one at or after each due time
#define PHASE_FIRST UINT32_MAX

static bool prv_take(uint32_t *phase, uint32_t want_us, uint32_t interval_us) {
  if (*phase == PHASE_FIRST || want_us <= interval_us) {
    *phase = 0U;
    return true;
  }

  *phase += interval_us;
  if (*phase < want_us) {
    return false;
  }

  *phase -= want_us;

  return true;
}

static uint32_t prv_count(const struct pbl_sensing_client *c) {
  const struct pbl_sensing_stream *s = c->stream;
  uint32_t phase = c->phase;
  uint32_t count = 0U;
  int32_t found;

  if (s == nullptr) {
    return 0U;
  }

  found = prv_find(s, c->pos);
  if (found < 0) {
    return 0U;
  }

  for (uint16_t off = (uint16_t)found;; off = prv_next(s, off)) {
    const struct prv_rec *rec = prv_rec(s, off);
    uint32_t frames = rec->num_frames;

    if (prv_rec_has(rec, c->pos)) {
      frames -= c->pos - rec->seq;
    }

    if (frames != 0U && phase == PHASE_FIRST) {
      count++;
      frames--;
      phase = 0U;
    }

    if (c->interval_us <= rec->interval_us) {
      count += frames;
      phase = 0U;
    } else {
      uint64_t acc = (uint64_t)phase + (uint64_t)frames * rec->interval_us;

      count += (uint32_t)(acc / c->interval_us);
      phase = (uint32_t)(acc % c->interval_us);
    }

    if (off == s->last) {
      return count;
    }
  }
}

static bool prv_batch_has(const struct pbl_sensor_batch *batch, enum pbl_sensor_type type) {
  for (uint8_t i = 0U; i < batch->num_chans; i++) {
    if (batch->chans[i].type == type) {
      return true;
    }
  }

  return false;
}

static void prv_free_buf(struct pbl_sensing_stream *s) {
  pbl_free(s->buf);
  s->buf = nullptr;
  s->size = 0U;
  prv_reset(s);
}

static void prv_set_interval(struct pbl_sensing_sensor *ctx, struct pbl_sensing_stream *s,
                             uint32_t want_us) {
  int32_t val = (int32_t)PBL_MIN(want_us, (uint32_t)INT32_MAX);

  if (want_us == s->want_us) {
    return;
  }

  if (pbl_sensor_attr_set(ctx->sensor, s->type, PBL_SENSOR_ATTR_INTERVAL, &val) != 0 ||
      (want_us != 0U && val <= 0)) {
    val = (int32_t)want_us;
  }

  s->want_us = want_us;
  s->interval_us = (uint32_t)val;
}

static void prv_apply(struct pbl_sensing_sensor *ctx) {
  struct pbl_sensing_stream *fastest = nullptr;
  uint64_t latency_us = UINT64_MAX;
  bool changed = false;

  for (struct pbl_sensing_client *c = ctx->clients; c != nullptr && !ctx->suspended; c = c->next) {
    if (c->interval_us != 0U && c->batch != 0U) {
      latency_us = PBL_MIN(latency_us, (uint64_t)c->batch * c->interval_us);
    }
  }

  for (struct pbl_sensing_stream *s = ctx->streams; s != nullptr; s = s->next) {
    uint32_t want_us = UINT32_MAX;
    int32_t batch_max = INT32_MAX;

    for (struct pbl_sensing_client *c = ctx->clients; c != nullptr && !ctx->suspended;
         c = c->next) {
      if (c->stream == s && c->interval_us != 0U) {
        want_us = PBL_MIN(want_us, c->interval_us);
      }
    }

    if (want_us == UINT32_MAX) {
      prv_set_interval(ctx, s, 0U);
      if (s->buf != nullptr) {
        prv_free_buf(s);
      }
      s->next_batch = 0;
      continue;
    }

    prv_set_interval(ctx, s, want_us);

    s->next_batch = 0;
    if (latency_us != UINT64_MAX) {
      (void)pbl_sensor_attr_get(ctx->sensor, s->type, PBL_SENSOR_ATTR_BATCH_MAX, &batch_max);
      s->next_batch =
          (int32_t)PBL_CLAMP(latency_us / s->interval_us, 1U, (uint64_t)PBL_MAX(batch_max, 1));
    }

    if (fastest == nullptr || s->interval_us < fastest->interval_us) {
      fastest = s;
    }
  }

  for (struct pbl_sensing_stream *s = ctx->streams; s != nullptr; s = s->next) {
    changed = changed || (s->next_batch != s->batch);
  }

  if (!changed) {
    return;
  }

  // A batch size may apply to the whole device: the fastest type's goes last
  for (struct pbl_sensing_stream *s = ctx->streams; s != nullptr; s = s->next) {
    if (s != fastest) {
      s->batch = s->next_batch;
      if (fastest == nullptr || s->batch != 0) {
        (void)pbl_sensor_attr_set(ctx->sensor, s->type, PBL_SENSOR_ATTR_BATCH, &s->batch);
      }
    }
  }

  if (fastest != nullptr) {
    fastest->batch = fastest->next_batch;
    (void)pbl_sensor_attr_set(ctx->sensor, fastest->type, PBL_SENSOR_ATTR_BATCH, &fastest->batch);
  }
}

static void prv_set_event(struct pbl_sensing_sensor *ctx, enum pbl_sensor_event_type type,
                          bool on) {
  int ret = pbl_sensor_set_event(ctx->sensor, type, on);

  if (ret != 0) {
    PBL_LOG_WRN("Could not %s event %d (%d)", on ? "enable" : "disable", (int)type, ret);
  }
}

static void prv_update_events(struct pbl_sensing_sensor *ctx, uint8_t from, uint8_t to) {
  for (uint8_t type = 0U; type < PBL_SENSOR_EVENT_COUNT; type++) {
    bool was = (from & PBL_BIT(type)) != 0U;
    bool is = (to & PBL_BIT(type)) != 0U;

    if (is && !was && ctx->event_users[type]++ == 0U && !ctx->suspended) {
      prv_set_event(ctx, type, true);
    } else if (was && !is && --ctx->event_users[type] == 0U && !ctx->suspended) {
      prv_set_event(ctx, type, false);
    }
  }
}

static int prv_attach(struct pbl_sensing_client *c, uint32_t interval_us) {
  struct pbl_sensing_stream *s = nullptr;

  if (interval_us != 0U) {
    s = prv_stream_get(c->ctx, c->type, true);
    if (s == nullptr) {
      return -ENOMEM;
    }
  }

  if (s != c->stream) {
    c->stream = s;
    c->pos = (s != nullptr) ? s->seq : 0U;
    c->lost = 0U;
  }

  c->interval_us = interval_us;
  c->phase = PHASE_FIRST;

  return 0;
}

static void prv_batch_cb(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch,
                         void *user_data) {
  struct pbl_sensing_sensor *ctx = user_data;

  if (ctx->suspended || batch->num_frames == 0U || batch->stride == 0U) {
    return;
  }

  for (struct pbl_sensing_stream *s = ctx->streams; s != nullptr; s = s->next) {
    if (s->interval_us == 0U || !prv_batch_has(batch, s->type)) {
      continue;
    }

    prv_store(ctx, s, batch);

    for (struct pbl_sensing_client *c = ctx->clients, *next; c != nullptr; c = next) {
      next = c->next;

      if (c->stream != s) {
        continue;
      }

      if (c->batch == 0U) {
        c->pos = s->seq;
        continue;
      }

      if (c->armed != 0U && c->notify != nullptr && prv_count(c) >= c->armed) {
        uint16_t armed = c->armed;

        c->armed = 0U;
        if (!c->notify(c)) {
          c->armed = armed;
        }
      }
    }
  }
}

static void prv_event_cb(const struct pbl_sensor *sensor, const struct pbl_sensor_event *event,
                         void *user_data) {
  struct pbl_sensing_sensor *ctx = user_data;

  if (ctx->suspended || event->type >= PBL_SENSOR_EVENT_COUNT) {
    return;
  }

  for (struct pbl_sensing_client *c = ctx->clients, *next; c != nullptr; c = next) {
    next = c->next;
    if ((c->events & PBL_BIT(event->type)) != 0U && c->event != nullptr) {
      c->event(c, event);
    }
  }
}

int pbl_sensing_client_add(struct pbl_sensing_client *client) {
  struct pbl_sensing_sensor *ctx;
  uint32_t interval_us;
  uint8_t events;
  int ret;

  if (client == nullptr || client->sensor == nullptr || client->type >= PBL_SENSOR_TYPE_COUNT) {
    return -EINVAL;
  }

  if (client->interval_us != 0U && !pbl_sensor_has_type(client->sensor, client->type)) {
    return -ENOTSUP;
  }

  if ((client->events & ~client->sensor->events) != 0U) {
    return -ENOTSUP;
  }

  ctx = prv_ctx_get(client->sensor, true);
  if (ctx == nullptr) {
    return -ENOMEM;
  }

  pbl_sensor_lock(ctx->sensor);

  interval_us = client->interval_us;
  events = client->events;

  client->ctx = ctx;
  client->stream = nullptr;
  client->armed = 0U;
  ret = prv_attach(client, interval_us);
  if (ret == 0) {
    client->next = ctx->clients;
    ctx->clients = client;
    prv_update_events(ctx, 0U, events);
    prv_apply(ctx);
  }

  pbl_sensor_unlock(ctx->sensor);

  return ret;
}

void pbl_sensing_client_remove(struct pbl_sensing_client *client) {
  struct pbl_sensing_sensor *ctx = client->ctx;

  if (ctx == nullptr) {
    return;
  }

  pbl_sensor_lock(ctx->sensor);

  for (struct pbl_sensing_client **pp = &ctx->clients; *pp != nullptr; pp = &(*pp)->next) {
    if (*pp == client) {
      *pp = client->next;
      prv_update_events(ctx, client->events, 0U);
      client->stream = nullptr;
      client->ctx = nullptr;
      client->next = nullptr;
      prv_apply(ctx);
      break;
    }
  }

  pbl_sensor_unlock(ctx->sensor);
}

void pbl_sensing_client_set_interval(struct pbl_sensing_client *client, uint32_t interval_us) {
  struct pbl_sensing_sensor *ctx = client->ctx;

  if (ctx == nullptr) {
    client->interval_us = interval_us;
    return;
  }

  pbl_sensor_lock(ctx->sensor);

  if (interval_us != client->interval_us &&
      (interval_us == 0U || pbl_sensor_has_type(ctx->sensor, client->type)) &&
      prv_attach(client, interval_us) == 0) {
    prv_apply(ctx);
  }

  pbl_sensor_unlock(ctx->sensor);
}

void pbl_sensing_client_set_batch(struct pbl_sensing_client *client, uint16_t batch) {
  struct pbl_sensing_sensor *ctx = client->ctx;

  if (ctx == nullptr) {
    client->batch = batch;
    return;
  }

  pbl_sensor_lock(ctx->sensor);

  if (batch != client->batch) {
    client->batch = batch;
    prv_apply(ctx);
  }

  pbl_sensor_unlock(ctx->sensor);
}

void pbl_sensing_client_set_events(struct pbl_sensing_client *client, uint8_t events) {
  struct pbl_sensing_sensor *ctx = client->ctx;

  if (ctx == nullptr) {
    client->events = events;
    return;
  }

  events &= ctx->sensor->events;

  pbl_sensor_lock(ctx->sensor);

  prv_update_events(ctx, client->events, events);
  client->events = events;

  pbl_sensor_unlock(ctx->sensor);
}

uint32_t pbl_sensing_sampling_interval(const struct pbl_sensing_client *client) {
  uint32_t interval_us = 0U;

  pbl_sensor_lock(client->sensor);
  if (client->stream != nullptr) {
    interval_us = client->stream->interval_us;
  }
  pbl_sensor_unlock(client->sensor);

  return interval_us;
}

void pbl_sensing_arm(struct pbl_sensing_client *client, uint16_t frames) {
  pbl_sensor_lock(client->sensor);
  client->armed = frames;
  pbl_sensor_unlock(client->sensor);
}

uint32_t pbl_sensing_available(struct pbl_sensing_client *client) {
  uint32_t count;

  pbl_sensor_lock(client->sensor);
  count = prv_count(client);
  pbl_sensor_unlock(client->sensor);

  return count;
}

uint16_t pbl_sensing_read(struct pbl_sensing_client *client, int32_t *out, uint16_t max,
                          uint64_t *timestamp_us) {
  const struct pbl_sensing_stream *s;
  uint8_t num_values = pbl_sensor_num_values(client->type);
  uint16_t got = 0U;

  pbl_sensor_lock(client->sensor);

  s = client->stream;
  while (s != nullptr && got < max) {
    struct pbl_sensor_batch batch;
    int32_t off = prv_find(s, client->pos);

    if (off < 0) {
      break;
    }

    prv_rec_batch(s, (uint16_t)off, &batch);

    for (uint16_t i = client->pos - prv_rec(s, (uint16_t)off)->seq;
         i < batch.num_frames && got < max; i++) {
      client->pos++;

      if (!prv_take(&client->phase, client->interval_us, batch.interval_us)) {
        continue;
      }

      if (pbl_sensor_decode(client->sensor, &batch, client->type, i, 1U, &out[got * num_values]) <
          0) {
        continue;
      }

      if (got == 0U && timestamp_us != nullptr) {
        *timestamp_us = pbl_sensor_frame_time(&batch, i);
      }

      got++;
    }
  }

  pbl_sensor_unlock(client->sensor);

  return got;
}

void pbl_sensing_discard(struct pbl_sensing_client *client) {
  pbl_sensor_lock(client->sensor);
  if (client->stream != nullptr) {
    client->pos = client->stream->seq;
  }
  pbl_sensor_unlock(client->sensor);
}

uint32_t pbl_sensing_lost(struct pbl_sensing_client *client) {
  uint32_t lost;

  pbl_sensor_lock(client->sensor);
  lost = client->lost;
  client->lost = 0U;
  pbl_sensor_unlock(client->sensor);

  return lost;
}

int pbl_sensing_latest(const struct pbl_sensor *sensor, enum pbl_sensor_type type, int32_t *val,
                       uint64_t *timestamp_us) {
  struct pbl_sensing_sensor *ctx = prv_ctx_get(sensor, false);
  struct pbl_sensing_stream *s;
  int ret = -ENODATA;

  if (ctx == nullptr) {
    return -ENODATA;
  }

  pbl_sensor_lock(sensor);

  s = prv_stream_get(ctx, type, false);
  if (s != nullptr && s->buf != nullptr && !s->empty) {
    struct pbl_sensor_batch batch;

    prv_rec_batch(s, s->last, &batch);
    if (pbl_sensor_decode(sensor, &batch, type, batch.num_frames - 1U, 1U, val) > 0) {
      if (timestamp_us != nullptr) {
        *timestamp_us = batch.timestamp_us;
      }
      ret = 0;
    }
  }

  pbl_sensor_unlock(sensor);

  return ret;
}

uint32_t pbl_sensing_count(const struct pbl_sensor *sensor, enum pbl_sensor_type type) {
  struct pbl_sensing_sensor *ctx = prv_ctx_get(sensor, false);
  struct pbl_sensing_stream *s;
  uint32_t count = 0U;

  if (ctx == nullptr) {
    return 0U;
  }

  pbl_sensor_lock(sensor);
  s = prv_stream_get(ctx, type, false);
  if (s != nullptr) {
    count = s->seq;
  }
  pbl_sensor_unlock(sensor);

  return count;
}

void pbl_sensing_suspend(const struct pbl_sensor *sensor, bool suspend) {
  struct pbl_sensing_sensor *ctx = prv_ctx_get(sensor, true);

  if (ctx == nullptr) {
    return;
  }

  pbl_sensor_lock(sensor);

  if (suspend != ctx->suspended) {
    ctx->suspended = suspend;

    for (uint8_t type = 0U; type < PBL_SENSOR_EVENT_COUNT; type++) {
      if (ctx->event_users[type] != 0U) {
        prv_set_event(ctx, type, !suspend);
      }
    }

    prv_apply(ctx);
  }

  pbl_sensor_unlock(sensor);
}
