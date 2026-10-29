/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>
#include <pbl/util/bits.h>

/**
 * @defgroup drivers_audio Audio
 * @ingroup drivers
 * @brief Audio stream class.
 *
 * An audio device is a struct pbl_audio streaming one direction, playback or capture, between a
 * ring of periods in memory and the hardware. The client owns the ring: interleaved signed 16-bit
 * samples, @c periods periods of @c period_frames frames each, aligned and padded to
 * @ref PBL_AUDIO_BUF_ALIGN (pbl_audio_buf_alloc() returns such a buffer). The hardware works
 * through the periods in order and wraps around.
 *
 * Each time the hardware finishes a period, the class calls the client's @c period_done callback
 * from the driver's interrupt with that period and the stream position. The client then owns the
 * period until it gives it back with pbl_audio_period_ready(), from the callback or later:
 *
 * - Playback: the period has been played and is handed out zero-filled; the client refills it.
 *   A period the client has not given back when the hardware reaches it again plays silence and
 *   sets @c xrun in the next callback. The ring is played from its initial contents, so fill it,
 *   with silence if nothing else, before starting.
 * - Capture: the period holds new samples; the client releases it once it has read them. A
 *   period still held when the hardware needs it again is overwritten, and @c xrun is set.
 *
 * @code{.c}
 * static void prv_period_done(const struct pbl_audio *audio, void *period,
 *                             const struct pbl_audio_pos *pos, void *arg) {
 *   fill(period);
 *   (void)pbl_audio_period_ready(audio, period);
 * }
 *
 * const struct pbl_audio_stream stream = {
 *   .rate = 16000,
 *   .channels = 1,
 *   .buf = ring,
 *   .period_frames = 512,
 *   .periods = 2,
 *   .period_done = prv_period_done,
 * };
 *
 * ret = pbl_audio_start(SPEAKER, &stream);
 * ...
 * ret = pbl_audio_stop(SPEAKER);
 * @endcode
 *
 * Positions count the frames the hardware has transferred since the stream started, stamped
 * with the uptime at the period boundary, on the same timebase for every device. A playback
 * period handed out at frame @c f starts playing at frame
 * @c f + (periods - 1) * period_frames; a capture period handed out at frame @c f holds frames
 * @c f - period_frames to @c f.
 *
 * Gain is in millibels (mdB) and mute is separate from it. Both take effect at once while
 * streaming and are kept for the next start otherwise.
 *
 * A device may be linked to a codec and an amplifier, described by the board as a struct
 * pbl_audio_link. The class then powers them around the stream: the amplifier on before the
 * stream and off after it, the codec up once the stream runs (it may need its clocks) and down
 * before the stream stops, and routes gain and mute to the codec.
 *
 * Calls other than pbl_audio_period_ready() must not be made from interrupts. They return 0 or a
 * negative errno, @c -ENODEV if the device is not ready.
 * @{
 */

struct pbl_audio;
struct pbl_audio_codec;
struct pbl_audio_amp;

/** @brief Alignment, in bytes, of rings and their size: the largest data cache line. */
#define PBL_AUDIO_BUF_ALIGN 32U

/** @brief Most periods in a ring. */
#define PBL_AUDIO_MAX_PERIODS 32U

/** @brief Stream direction. */
enum pbl_audio_dir {
  /** From memory to the hardware. */
  PBL_AUDIO_PLAYBACK,
  /** From the hardware to memory. */
  PBL_AUDIO_CAPTURE,
};

/**
 * @name Sample rates
 * Bits of struct pbl_audio_caps::rates.
 * @{
 */
/** 8 kHz. */
#define PBL_AUDIO_RATE_8000 PBL_BIT(0)
/** 11.025 kHz. */
#define PBL_AUDIO_RATE_11025 PBL_BIT(1)
/** 12 kHz. */
#define PBL_AUDIO_RATE_12000 PBL_BIT(2)
/** 16 kHz. */
#define PBL_AUDIO_RATE_16000 PBL_BIT(3)
/** 22.05 kHz. */
#define PBL_AUDIO_RATE_22050 PBL_BIT(4)
/** 24 kHz. */
#define PBL_AUDIO_RATE_24000 PBL_BIT(5)
/** 32 kHz. */
#define PBL_AUDIO_RATE_32000 PBL_BIT(6)
/** 44.1 kHz. */
#define PBL_AUDIO_RATE_44100 PBL_BIT(7)
/** 48 kHz. */
#define PBL_AUDIO_RATE_48000 PBL_BIT(8)
/** @} */

/**
 * @name Link formats
 * Flags of struct pbl_audio_link::fmt.
 * @{
 */
/** I2S framing. */
#define PBL_AUDIO_FMT_I2S PBL_BIT(0)
/** The codec drives the bit and frame clocks. */
#define PBL_AUDIO_FMT_CODEC_MASTER PBL_BIT(1)
/** @} */

/** @brief Stream position at a period boundary. */
struct pbl_audio_pos {
  /** Frames the hardware has transferred since the stream started. */
  uint64_t frame;
  /** Uptime when it reached @ref frame, in microseconds. */
  uint64_t ts_us;
  /** A period was missed since the previous callback: played as silence, or overwritten. */
  bool xrun;
};

/**
 * @brief Period callback, called from the driver's interrupt.
 *
 * @param audio Device.
 * @param period Period the client now owns.
 * @param pos Stream position.
 * @param arg Callback argument.
 */
typedef void (*pbl_audio_period_cb)(const struct pbl_audio *audio, void *period,
                                    const struct pbl_audio_pos *pos, void *arg);

/** @brief A stream. */
struct pbl_audio_stream {
  /** Sample rate, in Hz. */
  uint32_t rate;
  /** Channels per frame. */
  uint8_t channels;
  /** Ring, @ref periods periods of @ref period_frames frames. */
  void *buf;
  /** Frames per period. */
  size_t period_frames;
  /** Periods in the ring, at least 2. */
  uint8_t periods;
  /** Period callback. */
  pbl_audio_period_cb period_done;
  /** Callback argument. */
  void *arg;
};

/** @brief What a device can do. */
struct pbl_audio_caps {
  /** Supported rates, @c PBL_AUDIO_RATE_* bits. */
  uint32_t rates;
  /** Fewest channels per frame. */
  uint8_t min_channels;
  /** Most channels per frame. */
  uint8_t max_channels;
  /** Most periods in a ring, 0 for @ref PBL_AUDIO_MAX_PERIODS. */
  uint8_t max_periods;
  /** Lowest gain, in mdB. Equal to @ref gain_max_mdb if the gain is fixed. */
  int32_t gain_min_mdb;
  /** Highest gain, in mdB. */
  int32_t gain_max_mdb;
  /** Gain step, in mdB, 0 if the gain is fixed. */
  int32_t gain_step_mdb;
  /** Gain after initialization, in mdB. */
  int32_t gain_default_mdb;
};

/**
 * @brief Device driver operations.
 *
 * Called with the device lock held, except the sync and pointer operations, which may be called
 * from the driver's interrupt.
 */
struct pbl_audio_ops {
  /** Optional. Bring the hardware up. Returns 0, @c -ENODEV if absent, or a negative errno. */
  int (*init)(const struct pbl_audio *audio);
  /** Report the capabilities. Gain fields are ignored on a device linked to a codec. */
  void (*get_caps)(const struct pbl_audio *audio, struct pbl_audio_caps *caps);
  /**
   * Start streaming the ring, applying @p gain_mdb and @p mute before any sample reaches the
   * output. Returns 0, @c -EINVAL for a stream the hardware cannot run, or a negative errno.
   */
  int (*start)(const struct pbl_audio *audio, const struct pbl_audio_stream *stream,
               int32_t gain_mdb, bool mute);
  /** Stop streaming. pbl_audio_period_elapsed() is not called once it returns. */
  int (*stop)(const struct pbl_audio *audio);
  /** Optional, required with a variable gain. Set the gain while streaming. */
  int (*set_gain)(const struct pbl_audio *audio, int32_t gain_mdb);
  /** Optional. Set the mute while streaming; without it, mute sets the lowest gain. */
  int (*set_mute)(const struct pbl_audio *audio, bool mute);
  /** Optional. Make CPU writes to a playback period visible to the hardware. */
  void (*sync_for_device)(const struct pbl_audio *audio, void *buf, size_t len);
  /** Optional. Make hardware writes to a capture period visible to the CPU. */
  void (*sync_for_cpu)(const struct pbl_audio *audio, void *buf, size_t len);
  /** Optional. Report the frames transferred within the current period. */
  int (*pointer)(const struct pbl_audio *audio, size_t *frames);
};

/** @brief Codec configuration for a stream. */
struct pbl_audio_codec_cfg {
  /** Sample rate, in Hz. */
  uint32_t rate;
  /** Channels per frame on the link. */
  uint8_t channels;
  /** Link format, @c PBL_AUDIO_FMT_* flags. */
  uint32_t fmt;
  /** Gain, in mdB. */
  int32_t gain_mdb;
  /** Mute. */
  bool mute;
};

/** @brief Codec driver operations, called with the lock of the linked device held. */
struct pbl_audio_codec_ops {
  /** Report the gain range in the gain fields of @p caps. */
  void (*get_caps)(const struct pbl_audio_codec *codec, struct pbl_audio_caps *caps);
  /** Power up for a stream whose clocks run, ending with the gain and mute applied. */
  int (*enable)(const struct pbl_audio_codec *codec, const struct pbl_audio_codec_cfg *cfg);
  /** Power down. */
  int (*disable)(const struct pbl_audio_codec *codec);
  /** Set the gain while enabled. */
  int (*set_gain)(const struct pbl_audio_codec *codec, int32_t gain_mdb);
  /** Set the mute while enabled. */
  int (*set_mute)(const struct pbl_audio_codec *codec, bool mute);
};

/** @brief A codec: a separate chip converting between the link and the analog side. */
struct pbl_audio_codec {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_audio_codec_ops *ops;
};

/** @brief Amplifier driver operations, called with the lock of the linked device held. */
struct pbl_audio_amp_ops {
  /** Power up. */
  int (*enable)(const struct pbl_audio_amp *amp);
  /** Power down. */
  int (*disable)(const struct pbl_audio_amp *amp);
};

/** @brief An amplifier between the output and the speaker. */
struct pbl_audio_amp {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_audio_amp_ops *ops;
};

/** @brief What a device is wired to. */
struct pbl_audio_link {
  /** Codec on the link, or NULL. */
  const struct pbl_audio_codec *codec;
  /** Amplifier, or NULL. */
  const struct pbl_audio_amp *amp;
  /** Link format, @c PBL_AUDIO_FMT_* flags. */
  uint32_t fmt;
};

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_state {
  struct pbl_mutex lock;
  struct pbl_audio_stream stream;
  size_t period_bytes;
  int32_t gain_mdb;
  int32_t gain_min_mdb;
  bool mute;
  bool running;
  bool xrun;
  uint8_t hw;
  uint32_t owned;
  uint64_t frame;
  uint64_t ts_us;
};
/** @endcond */

/** @brief An audio device. */
struct pbl_audio {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_audio_ops *ops;
  /** Runtime state. */
  struct pbl_audio_state *state;
  /** What the device is wired to, or NULL. */
  const struct pbl_audio_link *link;
  /** Direction, one of @ref pbl_audio_dir. */
  uint8_t dir;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_audio_class;
int pbl_audio_dev_init(const struct pbl_device *dev);
/** @endcond */

/** @brief The board's speaker, a playback device. */
extern const struct pbl_audio *const SPEAKER;

/** @brief The board's microphone, a capture device. */
extern const struct pbl_audio *const MIC;

/**
 * @brief Define the class state of an audio device.
 *
 * @param sym Symbol of the instance.
 */
#define PBL_AUDIO_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);     \
  static struct pbl_audio_state sym##_audio_state

/**
 * @brief Initializer for a struct pbl_audio.
 *
 * @param sym Symbol of the instance, as passed to PBL_AUDIO_STATE_DEFINE().
 * @param _name Device name.
 * @param _dir Direction, one of @ref pbl_audio_dir.
 * @param _ops Driver operations.
 * @param _link What the device is wired to, or NULL.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL. The codec and amplifier of the link
 * are added by the class.
 */
#define PBL_AUDIO_INIT(sym, _name, _dir, _ops, _link, _deps)                                     \
  {                                                                                              \
    .dev =                                                                                       \
        PBL_DEVICE_INIT_CLASS(sym, _name, pbl_audio_dev_init, nullptr, _deps, &pbl_audio_class), \
    .ops = (_ops),                                                                               \
    .state = &sym##_audio_state,                                                                 \
    .link = (_link),                                                                             \
    .dir = (_dir),                                                                               \
  }

/**
 * @brief Report the end of the period the hardware was working on.
 *
 * Called by drivers from their interrupt handler, once per period, in ring order.
 *
 * @param audio Device.
 */
void pbl_audio_period_elapsed(const struct pbl_audio *audio);

/**
 * @brief Report a hardware overrun or underrun, flagged in the next callback.
 *
 * Called by drivers, from any context.
 *
 * @param audio Device.
 */
void pbl_audio_report_xrun(const struct pbl_audio *audio);

/**
 * @brief Get a period of the running stream, for drivers.
 *
 * @param audio Device.
 * @param idx Period index, below the number of periods.
 * @return Period.
 */
void *pbl_audio_period(const struct pbl_audio *audio, uint8_t idx);

/**
 * @brief Get what a device can do, including its codec.
 *
 * @param audio Device.
 * @param[out] caps Capabilities.
 * @return 0 or a negative errno.
 */
int pbl_audio_get_caps(const struct pbl_audio *audio, struct pbl_audio_caps *caps);

/**
 * @brief Start streaming.
 *
 * @param audio Device.
 * @param stream Stream; copied, but the ring must stay valid until pbl_audio_stop() returns.
 * @return 0, @c -EBUSY if streaming, @c -EINVAL for a stream the device cannot run, or another
 * negative errno.
 */
int pbl_audio_start(const struct pbl_audio *audio, const struct pbl_audio_stream *stream);

/**
 * @brief Give a period back to the hardware: refilled for playback, read for capture.
 *
 * May be called from the period callback or any other context.
 *
 * @param audio Device.
 * @param period Period from the callback.
 * @return 0, @c -EINVAL if the client does not own @p period, or @c -EAGAIN if not streaming.
 */
int pbl_audio_period_ready(const struct pbl_audio *audio, void *period);

/**
 * @brief Stop streaming. No callback runs once it returns. A no-op if not streaming.
 *
 * @param audio Device.
 * @return 0 or a negative errno.
 */
int pbl_audio_stop(const struct pbl_audio *audio);

/**
 * @brief Set the gain.
 *
 * @param audio Device.
 * @param gain_mdb Gain in mdB, clamped to the range of the device and rounded down to its step.
 * @return 0 or a negative errno.
 */
int pbl_audio_set_gain(const struct pbl_audio *audio, int32_t gain_mdb);

/**
 * @brief Mute or unmute.
 *
 * @param audio Device.
 * @param mute True to mute.
 * @return 0 or a negative errno.
 */
int pbl_audio_set_mute(const struct pbl_audio *audio, bool mute);

/**
 * @brief Get the current stream position.
 *
 * Exact to the frame for drivers that report their progress within a period, otherwise the
 * position at the last period boundary.
 *
 * @param audio Device.
 * @param[out] pos Position; @c xrun is not reported.
 * @return 0, or @c -EAGAIN if not streaming.
 */
int pbl_audio_get_position(const struct pbl_audio *audio, struct pbl_audio_pos *pos);

/**
 * @brief Check whether a device is streaming.
 *
 * @param audio Device.
 * @return True while streaming.
 */
bool pbl_audio_is_running(const struct pbl_audio *audio);

/**
 * @brief Get the bytes of a ring.
 *
 * @param stream Stream.
 * @return Size of the ring, rounded up to @ref PBL_AUDIO_BUF_ALIGN.
 */
size_t pbl_audio_ring_bytes(const struct pbl_audio_stream *stream);

/**
 * @brief Allocate a ring: aligned and padded to @ref PBL_AUDIO_BUF_ALIGN, reachable by DMA.
 *
 * @param size Bytes, from pbl_audio_ring_bytes().
 * @return Zeroed ring, or NULL if out of memory.
 */
void *pbl_audio_buf_alloc(size_t size);

/**
 * @brief Free a ring from pbl_audio_buf_alloc().
 *
 * @param buf Ring, or NULL.
 */
void pbl_audio_buf_free(void *buf);

/**
 * @brief Get the bit of a sample rate.
 *
 * @param rate Sample rate, in Hz.
 * @return One of the @c PBL_AUDIO_RATE_* bits, 0 for a rate without one.
 */
uint32_t pbl_audio_rate_bit(uint32_t rate);

/**
 * @brief Convert a gain in mdB to the register steps of a hardware gain stage.
 *
 * @param gain_mdb Gain in mdB.
 * @param min_mdb Gain of step 0, in mdB.
 * @param step_mdb Gain per step, in mdB.
 * @return Step, rounded down.
 */
static inline int32_t pbl_audio_gain_to_step(int32_t gain_mdb, int32_t min_mdb, int32_t step_mdb) {
  return (gain_mdb - min_mdb) / step_mdb;
}

/** @} */
