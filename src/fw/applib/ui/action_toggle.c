/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "action_toggle.h"

#include "applib/app_launch_button.h"
#include "applib/app_launch_reason.h"
#include "applib/applib_malloc.auto.h"
#include "applib/ui/dialogs/actionable_dialog.h"
#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/dialogs/simple_dialog.h"
#include "applib/ui/vibes.h"
#include "applib/ui/window_manager.h"
#include "kernel/ui/modals/modal_manager.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"
#include "system/passert.h"

typedef struct ActionToggleDialogConfig {
  const char *window_name;
  const char *message;
  ResourceId icon;
  GColor text_color;
  GColor background_color;
  unsigned int timeout_ms;
} ActionToggleDialogConfig;

typedef struct ActionToggleContext {
  ActionToggleConfig config;
  bool enabled;
  bool defer_destroy;
} ActionToggleContext;

static void prv_action_toggle_dialog_unload(void *context);
static bool prv_should_prompt(const ActionToggleConfig *config);

static ActionToggleState prv_get_toggled_state_index(ActionToggleContext *ctx) {
  return ctx->enabled ? ActionToggleState_Disabled : ActionToggleState_Enabled;
}

static void prv_setup_state_config(ActionToggleContext *ctx, ActionToggleDialogConfig *config,
                                   ActionToggleDialogType dialog_type) {
  if (!config->window_name) {
    config->window_name = ctx->config.impl->window_name;
  }
  if (!config->icon) {
    config->icon = ctx->config.impl->icons[dialog_type];
  }
  if (!config->timeout_ms) {
    // Set the default prompt or result dialog timeout
    config->timeout_ms = prv_should_prompt(&ctx->config) ? 4500 : 1800;
  }
  if (!config->text_color.argb) {
    config->text_color = GColorBlack;
  }
  if (!config->background_color.argb) {
    config->background_color = !ctx->enabled ? GColorMediumAquamarine : GColorMelon;
  }
}

static void prv_setup_dialog(Dialog *dialog, const ActionToggleDialogConfig *config,
                             void *context) {
  const char *msg = i18n_get(config->message, dialog);
  dialog_set_text(dialog, msg);
  i18n_free(msg, dialog);

  dialog_set_icon(dialog, config->icon);
  dialog_set_text_color(dialog, config->text_color);
  dialog_set_background_color(dialog, config->background_color);
  dialog_set_timeout(dialog, config->timeout_ms);
  dialog_set_callbacks(dialog, &(DialogCallbacks) {
    .unload = prv_action_toggle_dialog_unload,
  }, context);
}

static void prv_vibe(const bool enabled) {
  if (enabled) {
    vibes_short_pulse();
  } else {
    vibes_double_pulse();
  }
}

static WindowStack *prv_get_window_stack(void) {
  return window_manager_get_window_stack(ModalPriorityNotification);
}

static void prv_push_result_dialog(ActionToggleContext *ctx) {
  ActionToggleDialogConfig config = {
    .message = ctx->config.impl->result_messages[prv_get_toggled_state_index(ctx)],
  };
  prv_setup_state_config(ctx, &config, ActionToggleDialogType_Result);
  SimpleDialog *simple_dialog = simple_dialog_create(config.window_name);
  prv_setup_dialog(simple_dialog_get_dialog(simple_dialog), &config, (void *)ctx);
  simple_dialog_set_icon_animated(simple_dialog, !ctx->config.impl->result_icon_static);
  simple_dialog_push(simple_dialog, prv_get_window_stack());
}

static bool prv_call_get_state_callback(ActionToggleContext *ctx) {
  if (ctx->config.impl->callbacks.get_state) {
    ctx->enabled = ctx->config.impl->callbacks.get_state(ctx->config.context);
  }
  return ctx->enabled;
}

static void prv_call_set_state_callback(ActionToggleContext *ctx) {
  if (!ctx->config.impl->callbacks.set_state) {
    return;
  }
  const bool next_state = !ctx->enabled;
  ctx->config.impl->callbacks.set_state(next_state, ctx->config.context);
  ctx->enabled = next_state;
  if (ctx->config.set_exit_reason) {
    app_exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);
  }
  prv_vibe(next_state);
}

static void prv_handle_prompt_confirm(ClickRecognizerRef recognizer, void *context) {
  ActionableDialog *actionable_dialog = context;
  ActionToggleContext *ctx = actionable_dialog->dialog.callback_context;
  prv_push_result_dialog(ctx);
  // Don't destroy the context since it is being reused for the result dialog
  ctx->defer_destroy = true;
  actionable_dialog_pop(actionable_dialog);
  prv_call_set_state_callback(ctx);
}

static void prv_action_toggle_dialog_unload(void *context) {
  ActionToggleContext *ctx = context;
  if (!ctx) {
    return;
  }
  if (ctx->defer_destroy) {
    ctx->defer_destroy = false;
    return;
  }
  applib_free(ctx);
}

static void prv_prompt_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_handle_prompt_confirm);
}

static void prv_push_prompt_dialog(ActionToggleContext *ctx) {
  ActionToggleDialogConfig config = {
    .message = ctx->config.impl->prompt_messages[prv_get_toggled_state_index(ctx)],
  };
  prv_setup_state_config(ctx, &config, ActionToggleDialogType_Prompt);
  ActionableDialog *actionable_dialog = actionable_dialog_create(config.window_name);
  actionable_dialog_set_action_bar_type(actionable_dialog, DialogActionBarConfirm, NULL);
  actionable_dialog_set_click_config_provider(actionable_dialog, prv_prompt_click_config_provider);
  prv_setup_dialog(actionable_dialog_get_dialog(actionable_dialog), &config, (void *)ctx);
  actionable_dialog_push(actionable_dialog, prv_get_window_stack());
}

static bool prv_should_prompt(const ActionToggleConfig *config) {
  switch (config->prompt) {
    case ActionTogglePrompt_Auto:
#if PLATFORM_SPALDING
      return ((pebble_task_get_current() == PebbleTask_App) &&
              (app_launch_reason() == APP_LAUNCH_QUICK_LAUNCH) &&
              (app_launch_button() == BUTTON_ID_BACK));
#else
      return false;
#endif
    case ActionTogglePrompt_NoPrompt:
      return false;
    case ActionTogglePrompt_Prompt:
      return true;
  }
  return false;
}

void action_toggle_push(const ActionToggleConfig *config) {
  ActionToggleContext *context = applib_zalloc(sizeof(ActionToggleContext));
  PBL_ASSERTN(context);
  *context = (ActionToggleContext) {
    .config = *config,
  };
  prv_call_get_state_callback(context);
  if (prv_should_prompt(config)) {
    prv_push_prompt_dialog(context);
  } else {
    prv_push_result_dialog(context);
    prv_call_set_state_callback(context);
  }
}
