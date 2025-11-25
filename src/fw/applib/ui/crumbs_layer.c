/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "crumbs_layer.h"

#include "applib/applib_malloc.auto.h"
#include "shell/system_theme.h"
#include "applib/ui/property_animation.h"
#include "process_management/process_manager.h"
#include "system/logging.h"
#include "util/trig.h"

typedef struct CrumbsLayerSizeConfig {
  int layer_width;
  int crumb_radius;
  int crumb_spacing;
  int crumb_space_from_top;
} CrumbsLayerSizeConfig;

static const CrumbsLayerSizeConfig s_crumb_configs[NumPreferredContentSizes] = {
  //! @note this is the same as Medium until Small is designed
  [PreferredContentSizeSmall] = {
    .layer_width = 14,
    .crumb_radius = 2,
    .crumb_spacing = 8,
    .crumb_space_from_top = 8,
  },
  [PreferredContentSizeMedium] = {
    .layer_width = 14,
    .crumb_radius = 2,
    .crumb_spacing = 8,
    .crumb_space_from_top = 8,
  },
  [PreferredContentSizeLarge] = {
    .layer_width = 16,
    .crumb_radius = 2,
    .crumb_spacing = 10,
    .crumb_space_from_top = 10,
  },
  //! @note this is the same as Large until ExtraLarge is designed
  [PreferredContentSizeExtraLarge] = {
    .layer_width = 16,
    .crumb_radius = 2,
    .crumb_spacing = 10,
    .crumb_space_from_top = 10,
  },
};

static const CrumbsLayerSizeConfig *prv_crumb_config(void) {
  const PreferredContentSize runtime_platform_default_size =
      system_theme_get_default_content_size_for_runtime_platform();
  return &s_crumb_configs[runtime_platform_default_size];
}

int crumbs_layer_width(void) {
  return prv_crumb_config()->layer_width;
}

static int prv_crumb_x_position(void) {
  return prv_crumb_config()->layer_width / 2;
}

static int prv_crumb_radius(void) {
  return prv_crumb_config()->crumb_radius;
}

static int prv_crumb_spacing(void) {
  return prv_crumb_config()->crumb_spacing;
}

static int prv_crumb_space_from_top(void) {
  return prv_crumb_config()->crumb_space_from_top;
}

static int prv_crumb_maximum_count(void) {
  // NOTE: Was originally:
  //         static const int MAX_CRUMBS = 16; // 168 / (4px diameter + 4px in between each)
  // However that math literally doesn't add up, it would've been 20 like that.
  // I'm going to just return 16 all the time like we used to, but leave a "correct" version
  // commented out.
  // return (PBL_DISPLAY_HEIGHT - prv_crumb_space_from_top()) / prv_crumb_spacing();
  return 16;
}

static void prv_crumbs_layer_update_proc_rect(Layer *layer, GContext *ctx) {
  const int crumb_radius = prv_crumb_radius();
  const int crumb_spacing = prv_crumb_spacing();
  const int xpos = prv_crumb_x_position();

  GPoint p = GPoint(xpos, crumb_radius + prv_crumb_space_from_top());
  CrumbsLayer *cl = (CrumbsLayer *)layer;
  graphics_context_set_fill_color(ctx, cl->bg_color);
  graphics_fill_rect(ctx, &layer->bounds);

  for (int i = cl->level; i > 0; --i) {
    p.x = xpos + (cl->crumbs_x_increment / i);
    graphics_context_set_fill_color(ctx, cl->fg_color);
    graphics_fill_circle(ctx, p, crumb_radius);
    p.y += crumb_spacing;
  }
}

#if PBL_ROUND
static void prv_crumbs_layer_update_proc_round(Layer *layer, GContext *ctx) {
  CrumbsLayer *cl = (CrumbsLayer *)layer;


  graphics_context_set_fill_color(ctx, cl->bg_color);
  // TODO: remove stroke color again, once it's been fixed in fill_radial
  graphics_context_set_stroke_color(ctx, cl->bg_color);


  // compensate for problems with rounding errors and physical display shape
  const uint16_t overdraw = 2;
  graphics_fill_radial(ctx, grect_inset(layer->bounds, GEdgeInsets(-overdraw)),
                       GOvalScaleModeFillCircle, crumbs_layer_width(), 0, TRIG_MAX_ANGLE);
}
#endif

void crumbs_layer_set_level(CrumbsLayer *crumbs_layer, int level) {
  const int max_crumbs = prv_crumb_maximum_count();
  if (level > max_crumbs) {
    PBL_LOG(LOG_LEVEL_WARNING, "exceeded max number of crumbs");
    level = max_crumbs;
  }
  crumbs_layer->level = level;
  layer_mark_dirty((Layer *)crumbs_layer);
}

void crumbs_layer_init(CrumbsLayer *crumbs_layer, const GRect *frame, GColor bg_color,
                       GColor fg_color) {
  layer_init(&crumbs_layer->layer, frame);
  crumbs_layer->level = 0;
  crumbs_layer->fg_color = fg_color;
  crumbs_layer->bg_color = bg_color;
  const LayerUpdateProc update_proc = PBL_IF_RECT_ELSE(prv_crumbs_layer_update_proc_rect,
                                                       prv_crumbs_layer_update_proc_round);
  layer_set_update_proc(&crumbs_layer->layer, update_proc);
}

CrumbsLayer *crumbs_layer_create(GRect frame, GColor bg_color, GColor fg_color) {
  // Note: Not yet exported for 3rd party applications so no padding needed
  CrumbsLayer *cl = applib_malloc(sizeof(CrumbsLayer));
  if (cl) {
    crumbs_layer_init(cl, &frame, fg_color, bg_color);
  }

  return cl;
}

void crumbs_layer_deinit(CrumbsLayer *crumbs_layer) {
  if (crumbs_layer == NULL) {
    return;
  }

  layer_deinit((Layer *)crumbs_layer);
}

void crumbs_layer_destroy(CrumbsLayer *crumbs_layer) {
  crumbs_layer_deinit(crumbs_layer);
  applib_free(crumbs_layer);
}

int16_t prv_x_getter(void *subject) {
  CrumbsLayer *crumbs_layer = subject;
  return crumbs_layer->crumbs_x_increment;
}

void prv_x_setter(void *subject, int16_t int16) {
  CrumbsLayer *crumbs_layer = subject;
  crumbs_layer->crumbs_x_increment = int16;
}

static const PropertyAnimationImplementation s_prop_impl = {
  .base = {
    .update = (AnimationUpdateImplementation)property_animation_update_int16,
  },
  .accessors = {
    .getter.int16 = prv_x_getter,
    .setter.int16 = prv_x_setter,
  },
};

Animation *crumbs_layer_get_animation(CrumbsLayer *crumbs_layer) {
  uint16_t from = crumbs_layer->level * 2 * prv_crumb_radius();
  uint16_t to = 0;
  PropertyAnimation *prop_anim = property_animation_create(&s_prop_impl, crumbs_layer, &from, &to);
  return property_animation_get_animation(prop_anim);
}
