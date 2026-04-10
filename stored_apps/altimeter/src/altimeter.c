/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

typedef struct {
  Window *window;
  StatusBarLayer *status_layer;
  TextLayer *altitude_label;
  TextLayer *altitude_value;
  TextLayer *altitude_unit;
  TextLayer *pressure_label;
  TextLayer *pressure_value;
  char altitude_buf[16];
  char pressure_buf[32];
  PressureODR current_odr;
  bool ref_set;
  bool auto_high_rate;
} AppData;

static AppData s_data;

static const char *odr_name(PressureODR odr) {
  switch (odr) {
    case PRESSURE_ODR_1HZ:  return "1 Hz";
    case PRESSURE_ODR_5HZ:  return "5 Hz";
    case PRESSURE_ODR_10HZ: return "10 Hz";
    case PRESSURE_ODR_25HZ: return "25 Hz";
    case PRESSURE_ODR_50HZ: return "50 Hz";
    default: return "?";
  }
}

static void update_pressure_label(AppData *data, int32_t pressure_pa) {
  int32_t hpa = pressure_pa / 100;
  int32_t hpa_frac = (pressure_pa % 100) / 10;
  snprintf(data->pressure_buf, sizeof(data->pressure_buf), "%ld.%ld hPa (%s)",
           (long)hpa, (long)hpa_frac, odr_name(data->current_odr));
  text_layer_set_text(data->pressure_value, data->pressure_buf);
}

static void pressure_handler(PressureData *pdata) {
  AppData *data = &s_data;

  // Altitude in feet (1 cm = 0.0328084 ft, use integer math: ft = cm * 328084 / 10000000)
  int32_t alt_cm = pdata->altitude_cm;
  int32_t alt_ft_x10 = (int32_t)((int64_t)alt_cm * 328084 / 1000000);
  int32_t alt_ft = alt_ft_x10 / 10;
  int32_t alt_frac = (alt_ft_x10 < 0 ? -alt_ft_x10 : alt_ft_x10) % 10;
  snprintf(data->altitude_buf, sizeof(data->altitude_buf), "%ld.%ld",
           (long)alt_ft, (long)alt_frac);
  text_layer_set_text(data->altitude_value, data->altitude_buf);

  // Auto-switch to 25Hz when above 50ft relative altitude
  int32_t abs_ft = alt_ft < 0 ? -alt_ft : alt_ft;
  if (!data->auto_high_rate && abs_ft > 50) {
    data->auto_high_rate = true;
    data->current_odr = PRESSURE_ODR_25HZ;
    pressure_service_set_data_rate(data->current_odr);
  } else if (data->auto_high_rate && abs_ft <= 50) {
    data->auto_high_rate = false;
    data->current_odr = PRESSURE_ODR_1HZ;
    pressure_service_set_data_rate(data->current_odr);
  }

  // Pressure in hPa with rate in parentheses
  update_pressure_label(data, pdata->pressure_pa);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = &s_data;
  if (data->current_odr < PRESSURE_ODR_50HZ) {
    data->current_odr++;
    pressure_service_set_data_rate(data->current_odr);
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = &s_data;
  if (data->current_odr > PRESSURE_ODR_1HZ) {
    data->current_odr--;
    pressure_service_set_data_rate(data->current_odr);
  }
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = &s_data;
  pressure_service_set_reference();
  data->ref_set = true;
  vibes_short_pulse();
}

static void config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

static void window_load(Window *window) {
  AppData *data = &s_data;
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int16_t w = bounds.size.w;

  // Status bar — white on black
  data->status_layer = status_bar_layer_create();
  status_bar_layer_set_colors(data->status_layer, GColorBlack, GColorWhite);
  layer_add_child(root, status_bar_layer_get_layer(data->status_layer));

  int16_t y = STATUS_BAR_LAYER_HEIGHT + 2;

  // "REL ALT" label
  data->altitude_label = text_layer_create(GRect(0, y, w, 18));
  text_layer_set_font(data->altitude_label, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text(data->altitude_label, "REL ALTI");
  text_layer_set_text_alignment(data->altitude_label, GTextAlignmentCenter);
  text_layer_set_text_color(data->altitude_label, GColorWhite);
  text_layer_set_background_color(data->altitude_label, GColorClear);
  layer_add_child(root, text_layer_get_layer(data->altitude_label));
  y += 18;

  // Altitude value — big numbers
  data->altitude_value = text_layer_create(GRect(0, y, w, 58));
  text_layer_set_font(data->altitude_value,
                      fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text(data->altitude_value, "--.-");
  text_layer_set_text_alignment(data->altitude_value, GTextAlignmentCenter);
  text_layer_set_text_color(data->altitude_value, GColorWhite);
  text_layer_set_background_color(data->altitude_value, GColorClear);
  layer_add_child(root, text_layer_get_layer(data->altitude_value));
  y += 50;

  // Unit label
  data->altitude_unit = text_layer_create(GRect(0, y, w, 18));
  text_layer_set_font(data->altitude_unit, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text(data->altitude_unit, "feet");
  text_layer_set_text_alignment(data->altitude_unit, GTextAlignmentCenter);
  text_layer_set_text_color(data->altitude_unit, GColorWhite);
  text_layer_set_background_color(data->altitude_unit, GColorClear);
  layer_add_child(root, text_layer_get_layer(data->altitude_unit));
  y += 20;

  // Pressure label
  data->pressure_label = text_layer_create(GRect(0, y, w, 14));
  text_layer_set_font(data->pressure_label, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text(data->pressure_label, "PRESSURE");
  text_layer_set_text_alignment(data->pressure_label, GTextAlignmentCenter);
  text_layer_set_text_color(data->pressure_label, GColorWhite);
  text_layer_set_background_color(data->pressure_label, GColorClear);
  layer_add_child(root, text_layer_get_layer(data->pressure_label));
  y += 14;

  // Pressure value + rate
  data->pressure_value = text_layer_create(GRect(0, y, w, 24));
  text_layer_set_font(data->pressure_value,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text(data->pressure_value, "--.- hPa (1 Hz)");
  text_layer_set_text_alignment(data->pressure_value, GTextAlignmentCenter);
  text_layer_set_text_color(data->pressure_value, GColorWhite);
  text_layer_set_background_color(data->pressure_value, GColorClear);
  layer_add_child(root, text_layer_get_layer(data->pressure_value));

  data->current_odr = PRESSURE_ODR_1HZ;

  // Keep backlight on while altimeter is running
  light_enable(true);

  // Subscribe to pressure service and zero on launch
  pressure_service_subscribe(pressure_handler, data->current_odr);
  pressure_service_use_full_formula(true);
  pressure_service_set_reference();
  data->ref_set = true;
}

static void window_unload(Window *window) {
  light_enable(false);
  pressure_service_unsubscribe();
}

static void handle_init(void) {
  AppData *data = &s_data;
  data->window = window_create();
  window_set_background_color(data->window, GColorBlack);
  window_set_window_handlers(data->window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_set_click_config_provider(data->window, config_provider);
  window_stack_push(data->window, true);
}

int main(void) {
  handle_init();
  app_event_loop();
}
