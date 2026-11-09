/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/input/input.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/endpoint.h>
#include <pbl/pp/send.h>
#include <pbl/pp/session.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>

#include <applib/event_service_client.h>
#include <kernel/events.h>
#include <kernel/input_buttons.h>
#include <kernel/remote_input.h>

#if defined(CONFIG_SERVICE_TOUCH)
#include <pbl/services/touch/touch.h>

#include <applib/ui/recognizer/swipe.h>
#endif

#include <stdint.h>

// Buttons and swipes share one timer, so injected input is single-flight: a swipe can never land
// halfway through a button press, and either one reports Busy while the other runs.
static void (*s_sequence_cb)(void *context);
static void *s_sequence_context;

static void prv_input_timer_fn(struct pbl_work *work) {
  s_sequence_cb(s_sequence_context);
}

static PBL_DELAYED_WORK_DEFINE(s_input_timer, prv_input_timer_fn);

// Buttons currently held down by remote_input_button_set(). Bit x is ButtonId x. Released by
// another call, or when the phone's session closes.
static uint8_t s_held_buttons;

// Set from the moment a sequence is admitted until its last callback frees the context. The
// timer's own pending state cannot stand in for this: a work item reports not pending for the whole
// time its function runs, and a request arriving in that window would be admitted and then have its
// timer stolen when the running callback rearms.
static bool s_sequence_active;

// Serializes admission. Two controllers are exposed -- the serial console and the endpoint, on
// different tasks -- so without this both can read the state as idle, both be told Ok, and the
// second start of the shared timer replace the first callback, leaking its context.
static PBL_MUTEX_DEFINE(s_input_lock);

static void prv_comm_session_event_handler(PebbleEvent *e, void *context) {
  const PebbleCommSessionEvent *event = &e->bluetooth.comm_session_event;
  if (!event->is_system || event->is_open) {
    return;
  }
  // A held button is a level with no timeout, and only the phone's session can release it. When
  // that session closes, release here: otherwise the hold -- and the Busy it imposes on every
  // other injected input -- outlives its controller for good.
  remote_input_button_set(0);
}

void remote_input_init(void) {
  static EventServiceInfo s_comm_session_event_info;
  s_comm_session_event_info = (EventServiceInfo){
    .type = PEBBLE_COMM_SESSION_EVENT,
    .handler = prv_comm_session_event_handler,
  };
  event_service_client_subscribe(&s_comm_session_event_info);
}

static void prv_sequence_finished(void) {
  pbl_mutex_lock(&s_input_lock, PBL_FOREVER);
  s_sequence_active = false;
  pbl_mutex_unlock(&s_input_lock);
}

// Caller must hold s_input_lock.
static RemoteInputResult prv_claim_timer(void) {
  // A held button is a level rather than a sequence, but a press interleaved with one would emit
  // transitions the UI reads as chord edges. Keep the two mutually exclusive.
  if ((s_held_buttons != 0) || s_sequence_active) {
    return RemoteInputResult_Busy;
  }
  s_sequence_active = true;
  return RemoteInputResult_Ok;
}

// Admits one sequence and fires its first callback immediately.
static RemoteInputResult prv_start_sequence(void (*cb)(void *context), void *context) {
  pbl_mutex_lock(&s_input_lock, PBL_FOREVER);
  const RemoteInputResult claimed = prv_claim_timer();
  pbl_mutex_unlock(&s_input_lock);
  if (claimed != RemoteInputResult_Ok) {
    return claimed;
  }
  // Past this point the claim is ours: no other caller can be admitted, so no lock is needed.
  s_sequence_cb = cb;
  s_sequence_context = context;
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_input_timer, PBL_NO_WAIT);
  return RemoteInputResult_Ok;
}

// ---------------------------------------------------------------------------------------------
// Buttons

static void prv_put_button_event(ButtonId button, bool down) {
  pbl_input_report_key(input_buttons_code(button), down, true);
}

typedef struct ButtonPressContext {
  ButtonId button_id;
  bool button_is_held_down;
  // 32-bit: the console has always accepted counts and timeouts this wide, and narrowing them here
  // would silently shorten a long hold while still reporting OK.
  uint32_t presses_remaining;
  uint32_t hold_ms;
  uint32_t gap_ms;
} ButtonPressContext;

static void prv_button_timer_cb(void *cb_data) {
  ButtonPressContext *context = cb_data;

  const bool was_held_down = context->button_is_held_down;
  // Emit the transition opposite to the current state, then wait out whichever interval that
  // transition starts: a press is followed by the hold time, a release by the inter-press gap.
  prv_put_button_event(context->button_id, !was_held_down);

  if (was_held_down) {
    context->presses_remaining--;
  }

  if (context->presses_remaining > 0) {
    context->button_is_held_down = !was_held_down;
    pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_input_timer,
                                PBL_MSEC(was_held_down ? context->gap_ms : context->hold_ms));
  } else {
    pbl_free(context);
    prv_sequence_finished();
  }
}

RemoteInputResult remote_input_button_press(ButtonId button, uint32_t presses, uint32_t hold_ms,
                                            uint32_t gap_ms) {
  if (button >= NUM_BUTTONS) {
    return RemoteInputResult_Invalid;
  }
  if (presses == 0) {
    // Nothing to do. Returning early also keeps the timer chain from emitting a lone BUTTON_DOWN
    // and leaving the button stuck down.
    return RemoteInputResult_Ok;
  }
  ButtonPressContext *context = pbl_malloc(sizeof(ButtonPressContext));
  if (!context) {
    return RemoteInputResult_Invalid;
  }
  *context = (ButtonPressContext){
    .button_id = button,
    .button_is_held_down = false,
    .presses_remaining = presses,
    .hold_ms = hold_ms,
    .gap_ms = gap_ms,
  };

  // Drive the whole sequence from timer callbacks so the button events can't race the timers that
  // separate them.
  const RemoteInputResult result = prv_start_sequence(prv_button_timer_cb, context);
  if (result != RemoteInputResult_Ok) {
    pbl_free(context);
  }
  return result;
}

RemoteInputResult remote_input_button_set(uint8_t buttons) {
  if (buttons >= (1u << NUM_BUTTONS)) {
    return RemoteInputResult_Invalid;
  }
  // Deliberately not prv_claim_timer(): that reports Busy while buttons are held, which would
  // make the held state impossible to release.
  pbl_mutex_lock(&s_input_lock, PBL_FOREVER);
  if (s_sequence_active) {
    pbl_mutex_unlock(&s_input_lock);
    return RemoteInputResult_Busy;
  }

  const uint8_t released = s_held_buttons & ~buttons;
  const uint8_t pressed = buttons & ~s_held_buttons;
  s_held_buttons = buttons;
  // Emit outside the lock: the events go to a queue and nothing here needs the mask to stay put.
  pbl_mutex_unlock(&s_input_lock);

  // Release before pressing, so a mask that swaps one button for another never has more buttons
  // held at once than the caller asked for.
  for (ButtonId id = 0; id < NUM_BUTTONS; id++) {
    if (released & (1u << id)) {
      prv_put_button_event(id, false /* down */);
    }
  }
  for (ButtonId id = 0; id < NUM_BUTTONS; id++) {
    if (pressed & (1u << id)) {
      prv_put_button_event(id, true /* down */);
    }
  }
  return RemoteInputResult_Ok;
}

// ---------------------------------------------------------------------------------------------
// Touch paths

#if defined(CONFIG_SERVICE_TOUCH)

// Number of position updates streamed between touchdown and liftoff of a moving finger. The swipe
// recognizer only keeps the newest 3 samples for its velocity estimate, so a handful is plenty.
#define REMOTE_INPUT_PATH_STEPS 5

// Fraction of the traveled axis the finger covers, as numerator/denominator (60%). Must clear the
// recognizer's minimum length on every board.
#define REMOTE_INPUT_SWIPE_TRAVEL_NUM 3
#define REMOTE_INPUT_SWIPE_TRAVEL_DEN 5

static_assert((MIN(PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT) * REMOTE_INPUT_SWIPE_TRAVEL_NUM) /
                      REMOTE_INPUT_SWIPE_TRAVEL_DEN >=
                  SWIPE_MIN_LENGTH_PX,
              "swipe travel is below the swipe recognizer's minimum length");

typedef struct PathContext {
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
  int16_t x;
  int16_t y;
  uint8_t steps;
  uint8_t step;
  uint16_t step_ms;
  bool finger_down;
} PathContext;

static bool prv_path_emit(TouchInjectPhase phase, int16_t x, int16_t y) {
  // The injection entry point takes the coordinates the UI observes, so left-hand mode, the
  // interaction session and physical-touch arbitration are all the touch service's business.
  return touch_handle_injected_update(phase, x, y);
}

static void prv_path_timer_cb(void *cb_data) {
  PathContext *context = cb_data;

  bool emitted;
  if (!context->finger_down) {
    context->finger_down = true;
    emitted = prv_path_emit(TouchInjectPhase_Begin, context->x, context->y);
  } else if (context->step < context->steps) {
    context->step++;
    context->x =
        (int16_t)(context->x1 + (context->x2 - context->x1) * context->step / context->steps);
    context->y =
        (int16_t)(context->y1 + (context->y2 - context->y1) * context->step / context->steps);
    emitted = prv_path_emit(TouchInjectPhase_Move, context->x, context->y);
  } else {
    prv_path_emit(TouchInjectPhase_End, context->x, context->y);
    pbl_free(context);
    prv_sequence_finished();
    return;
  }

  if (!emitted) {
    // Ownership was lost mid-path: touch switched off, or the service reset under us. The service
    // refuses a Move from a gesture that no longer owns the sensor, so this is where we find out;
    // carrying on would strand a half-finished path. The caller was already told Ok, so leave a
    // trace of the abort.
    PBL_LOG_DBG("Remote input: touch path aborted, injection refused");
    pbl_free(context);
    prv_sequence_finished();
    return;
  }

  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_input_timer, PBL_MSEC(context->step_ms));
}

static RemoteInputResult prv_start_path(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                                        uint8_t steps, uint16_t step_ms) {
  PathContext *context = pbl_malloc(sizeof(PathContext));
  if (!context) {
    return RemoteInputResult_Invalid;
  }
  *context = (PathContext){
    .x1 = x1,
    .y1 = y1,
    .x2 = x2,
    .y2 = y2,
    .x = x1,
    .y = y1,
    .steps = steps,
    .step_ms = step_ms,
  };

  const RemoteInputResult result = prv_start_sequence(prv_path_timer_cb, context);
  if (result != RemoteInputResult_Ok) {
    pbl_free(context);
  }
  return result;
}

static bool prv_on_screen(int16_t x, int16_t y) {
  return (x >= 0) && (x < PBL_DISPLAY_WIDTH) && (y >= 0) && (y < PBL_DISPLAY_HEIGHT);
}

RemoteInputResult remote_input_touch(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                                     uint16_t duration_ms) {
  if (!prv_on_screen(x1, y1) || !prv_on_screen(x2, y2)) {
    return RemoteInputResult_Invalid;
  }
  if (!touch_injection_is_available()) {
    return RemoteInputResult_Invalid;
  }

  // A finger that stays put lifts without moving: a tap, or a long press.
  const uint8_t steps = ((x1 == x2) && (y1 == y2)) ? 0 : REMOTE_INPUT_PATH_STEPS;
  return prv_start_path(x1, y1, x2, y2, steps, MAX(1, duration_ms / (steps + 1)));
}

RemoteInputResult remote_input_swipe(RemoteInputSwipeDirection direction, uint16_t duration_ms) {
  if (direction >= RemoteInputSwipeDirectionCount) {
    return RemoteInputResult_Invalid;
  }
  // The recognizer reads a slower contact as a pan, so a longer request could never become a
  // swipe. Say so rather than acknowledging a gesture that is bound to be rejected.
  if (duration_ms > SWIPE_MAX_DURATION_MS) {
    return RemoteInputResult_Invalid;
  }
  // Likewise for a swipe the touch service would silently drop: touch switched off, or a physical
  // finger already on the screen.
  if (!touch_injection_is_available()) {
    return RemoteInputResult_Invalid;
  }

  const bool vertical =
      (direction == RemoteInputSwipeDirection_Up) || (direction == RemoteInputSwipeDirection_Down);
  const int16_t axis = vertical ? PBL_DISPLAY_HEIGHT : PBL_DISPLAY_WIDTH;
  const int16_t travel =
      (int16_t)((axis * REMOTE_INPUT_SWIPE_TRAVEL_NUM) / REMOTE_INPUT_SWIPE_TRAVEL_DEN);
  // The finger starts on the far side of centre and travels towards the named direction.
  const int16_t sign =
      ((direction == RemoteInputSwipeDirection_Up) || (direction == RemoteInputSwipeDirection_Left))
          ? -1
          : 1;
  const int16_t half = (int16_t)(travel / 2);
  const int16_t x1 = (int16_t)(PBL_DISPLAY_WIDTH / 2 - (vertical ? 0 : sign * half));
  const int16_t y1 = (int16_t)(PBL_DISPLAY_HEIGHT / 2 - (vertical ? sign * half : 0));

  // The touchdown-to-liftoff time covers the position updates plus the liftoff that follows
  // them. One divisor step beyond that count keeps the scheduled path strictly inside the
  // requested duration: the recognizer measures the gesture in wall-clock time, so a path
  // timed to land exactly on SWIPE_MAX_DURATION_MS would be rejected by any dispatch latency
  // at all. At least 1ms per step, or the chained timers never separate the velocity samples.
  return prv_start_path(x1, y1, (int16_t)(x1 + (vertical ? 0 : sign * travel)),
                        (int16_t)(y1 + (vertical ? sign * travel : 0)), REMOTE_INPUT_PATH_STEPS,
                        MAX(1, duration_ms / (REMOTE_INPUT_PATH_STEPS + 2)));
}

#else // !CONFIG_SERVICE_TOUCH

RemoteInputResult remote_input_touch(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                                     uint16_t duration_ms) {
  return RemoteInputResult_Invalid;
}

RemoteInputResult remote_input_swipe(RemoteInputSwipeDirection direction, uint16_t duration_ms) {
  return RemoteInputResult_Invalid;
}

#endif // CONFIG_SERVICE_TOUCH

// ---------------------------------------------------------------------------------------------
// Pebble protocol endpoint

typedef enum RemoteInputCommand {
  RemoteInputCommand_Button = 0x00,
  RemoteInputCommand_Swipe = 0x01,
  RemoteInputCommand_ButtonSet = 0x02,
  RemoteInputCommand_Touch = 0x03,
} RemoteInputCommand;

typedef struct PBL_PACKED RemoteInputButtonMsg {
  uint8_t command;
  uint8_t button_id;
  uint8_t presses;
  uint16_t hold_ms;
  uint16_t gap_ms;
} RemoteInputButtonMsg;

typedef struct PBL_PACKED RemoteInputButtonSetMsg {
  uint8_t command;
  uint8_t buttons;
} RemoteInputButtonSetMsg;

typedef struct PBL_PACKED RemoteInputSwipeMsg {
  uint8_t command;
  uint8_t direction;
  uint16_t duration_ms;
} RemoteInputSwipeMsg;

typedef struct PBL_PACKED RemoteInputTouchMsg {
  uint8_t command;
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
  uint16_t duration_ms;
} RemoteInputTouchMsg;

typedef struct PBL_PACKED RemoteInputAck {
  uint8_t command;
  uint8_t status;
} RemoteInputAck;

void remote_input_protocol_msg_callback(struct pbl_pp_session *session, const uint8_t *data,
                                        size_t length) {
  RemoteInputResult result = RemoteInputResult_Invalid;
  uint8_t command = 0xff;

  if (length >= 1) {
    command = data[0];
    switch (command) {
      case RemoteInputCommand_Button:
        if (length >= sizeof(RemoteInputButtonMsg)) {
          const RemoteInputButtonMsg *msg = (const RemoteInputButtonMsg *)data;
          result = remote_input_button_press((ButtonId)msg->button_id, msg->presses,
                                             pbl_be16_to_cpu(msg->hold_ms),
                                             pbl_be16_to_cpu(msg->gap_ms));
        }
        break;
      case RemoteInputCommand_ButtonSet:
        if (length >= sizeof(RemoteInputButtonSetMsg)) {
          const RemoteInputButtonSetMsg *msg = (const RemoteInputButtonSetMsg *)data;
          result = remote_input_button_set(msg->buttons);
        }
        break;
      case RemoteInputCommand_Swipe:
        if (length >= sizeof(RemoteInputSwipeMsg)) {
          const RemoteInputSwipeMsg *msg = (const RemoteInputSwipeMsg *)data;
          const uint16_t duration_ms = pbl_be16_to_cpu(msg->duration_ms);
          result = remote_input_swipe(
              (RemoteInputSwipeDirection)msg->direction,
              (duration_ms > 0) ? duration_ms : REMOTE_INPUT_SWIPE_DEFAULT_DURATION_MS);
        }
        break;
      case RemoteInputCommand_Touch:
        if (length >= sizeof(RemoteInputTouchMsg)) {
          const RemoteInputTouchMsg *msg = (const RemoteInputTouchMsg *)data;
          const uint16_t duration_ms = pbl_be16_to_cpu(msg->duration_ms);
          result = remote_input_touch(
              (int16_t)pbl_be16_to_cpu(msg->x1), (int16_t)pbl_be16_to_cpu(msg->y1),
              (int16_t)pbl_be16_to_cpu(msg->x2), (int16_t)pbl_be16_to_cpu(msg->y2),
              (duration_ms > 0) ? duration_ms : REMOTE_INPUT_TOUCH_DEFAULT_DURATION_MS);
        }
        break;
      default:
        break;
    }
  }

  PBL_LOG_DBG("Remote input: cmd %u -> %u", command, result);

  const RemoteInputAck ack = {
    .command = command,
    .status = (uint8_t)result,
  };
  pbl_pp_send(session, PBL_PP_ENDPOINT_REMOTE_INPUT, (const uint8_t *)&ack, sizeof(ack),
              PBL_PP_DEFAULT_TIMEOUT_MS);
}

PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_REMOTE_INPUT, PBL_PP_ACCESS_PRIVATE,
                       remote_input_protocol_msg_callback);
