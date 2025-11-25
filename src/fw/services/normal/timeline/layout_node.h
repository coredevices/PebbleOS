/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/text.h"
#include "applib/ui/kino/kino_layer.h"
#include "apps/system_apps/timeline/text_node.h"
#include "resource/resource_ids.auto.h"
#include "services/normal/timeline/attribute.h"
#include "services/normal/timeline/layout_layer.h"
#include "services/normal/timeline/timeline_resources.h"
#include "shell/system_theme.h"
#include "util/attributes.h"

//! LayoutNode is a compact TextNode constructor using packed structs. Using LayoutNode configs, a
//! hierarchy of nested TextNodes can be described and instantiated with
//! \ref layout_create_text_node_from_config. Entire layouts, such as the timeline cards, can be
//! described with a LayoutNode config hierarchy with the generic layout being the simplest
//! example. For maximum code space savings, it is best used with static structs that do not define
//! any one-use callbacks. When used with stack variables, be very conscious of the stack usage
//! (ideally measuring the usage), and immediately pop the stack frame after creation, avoiding
//! deeper calculation, such as initializing the view size with
//! \ref graphics_text_node_get_size which can be very stack intensive.

#define ToLayoutTextAlignment(alignment) (alignment + 1)
#define ToGTextAlignment(alignment) (alignment - 1)

#define ToLayoutVerticalAlignment(alignment) (alignment + 1)
#define ToGVerticalAlignment(alignment) (alignment - 1)

#define ToLayoutContentSize(size) ((LayoutContentSize)((size) + 1))
#define ToPreferredContentSize(size) ((PreferredContentSize)((size) - 1))

typedef enum {
  LayoutTextAlignment_Auto = 0,
  LayoutTextAlignment_Left = GTextAlignmentLeft + 1,
  LayoutTextAlignment_Center = GTextAlignmentCenter + 1,
  LayoutTextAlignment_Right = GTextAlignmentRight + 1,
} LayoutTextAlignment;

typedef enum {
  LayoutVerticalAlignment_Auto = 0,
  LayoutVerticalAlignment_Left = GVerticalAlignmentTop + 1,
  LayoutVerticalAlignment_Center = GVerticalAlignmentCenter + 1,
  LayoutVerticalAlignment_Right = GVerticalAlignmentBottom + 1,
} LayoutVerticalAlignment;

typedef enum {
  LayoutColor_None = 0,
  LayoutColor_Primary,
  LayoutColor_Secondary,
  LayoutColor_Background,
} LayoutColor;

typedef enum {
  LayoutContentSize_Auto = 0,
  LayoutContentSize_Small = PreferredContentSizeSmall + 1,
  LayoutContentSize_Medium = PreferredContentSizeMedium + 1,
  LayoutContentSize_Large = PreferredContentSizeLarge + 1,
  LayoutContentSize_ExtraLarge = PreferredContentSizeExtraLarge + 1,
  LayoutContentSizeDefault = PreferredContentSizeDefault + 1,
} LayoutContentSize;

typedef enum {
  //! Defines a Text TextNode with its text member pointing to a string in the layout's attributes
  LayoutNodeType_TextAttribute = 0,
  //! Defines a Text TextNode to be allocated with a buffer initialized by a literal string
  LayoutNodeType_TextBuffer,
  //! Defines a TextDynamic TextNode to be allocated with a user-defined sized buffer and a
  //! user-defined update function that operates on the buffer
  //! Use this type sparingly and only when required as it can be expensive in terms of code space
  LayoutNodeType_TextDynamic,
  //! Defines a Text TextNode with no buffer
  LayoutNodeType_Text,
  //! Defines a Horizontal TextNode that will be populated with the given nodes
  LayoutNodeType_Horizontal,
  //! Defines a Vertical TextNode that will be populated with the given nodes
  LayoutNodeType_Vertical,
  //! Defines an arbitrary TextNode to be constructed by a given TextNode constructor
  //! Use this type sparingly and only when required as it can be expensive in terms of code space
  LayoutNodeType_Constructor,
  //! Defines a Vertical TextNode that will be populated with Text TextNodes pointing to the
  //! layout's headings and paragraphs attributes
  LayoutNodeType_HeadingsParagraphs,
  //! Defines a Callback TextNode that wraps a constructed KinoLayer
  LayoutNodeType_Icon,

  //! These below are for TimelineLayouts ONLY
  //! Defines a Timeline icon TextNode that repositions the TimelineLayout's icon_layer based on
  //! the TextNode layout
  LayoutNodeType_TimelineIcon,
  //! Defines a Timeline page break TextNode that displays the glance arrow and marks the
  //! TimelineLayout as having a page break
  LayoutNodeType_TimelinePageBreak,
  //! Defines a Vertical TextNode that will be populated with icons, names, and values pointing
  //! to the layout's names and values attributes
  LayoutNodeType_TimelineMetrics,
} LayoutNodeType;

typedef struct PACKED {
  LayoutNodeType type;
} LayoutNodeConfig;

typedef struct PACKED {
  LayoutNodeConfig node;
  struct {
    int8_t x;
    int8_t y;
  } offset;
  struct {
    int8_t w;
    int8_t h;
  } margin;
} LayoutNodeExtentConfig;

typedef struct PACKED {
  LayoutNodeExtentConfig extent;
  const char *font_key;
  LayoutContentSize style:8;
  TextStyleFont style_font:8;
  int8_t line_spacing_delta:4;
  //! Specifies the fixed height as a function of the font height and number of lines.
  //! The lines corresponds to the multiplier against the font height to use, which correlates with
  //! the amount of lines that will render if used with other fixed components on the first page.
  //! Do not use fixed_lines for text that can appear after the first page fold. Doing so will
  //! result in text nodes that are not guaranteed to draw.
  uint8_t fixed_lines:2;
  LayoutTextAlignment alignment:2;
#if PBL_COLOR
  LayoutColor color;
#endif
} LayoutNodeTextConfig;

typedef struct PACKED {
  LayoutNodeExtentConfig extent;
  LayoutContentSize size:8;
  TextStyleFont heading_style_font:8;
  TextStyleFont paragraph_style_font:8;
} LayoutNodeHeadingsParagraphsConfig;

typedef struct PACKED {
  LayoutNodeTextConfig text;
  AttributeId attr_id;
} LayoutNodeTextAttributeConfig;

typedef struct PACKED {
  LayoutNodeTextConfig text;
  const char *str;
  bool use_i18n;
} LayoutNodeTextBufferConfig;

typedef struct LayoutNodeTextDynamicConfig LayoutNodeTextDynamicConfig;

typedef void (*LayoutNodeTextDynamicUpdate)(
    const LayoutLayer *layout, const LayoutNodeTextDynamicConfig *config, char *buffer,
    bool render);

struct PACKED LayoutNodeTextDynamicConfig {
  LayoutNodeTextConfig text;
  LayoutNodeTextDynamicUpdate update;
  void *context;
  uint16_t buffer_size;
};

typedef struct PACKED {
  LayoutNodeExtentConfig extent;
  LayoutNodeConfig **nodes;
  uint8_t num_nodes;
  uint8_t extra_capacity;
} LayoutNodeContainerConfig;

typedef struct PACKED {
  LayoutNodeContainerConfig container;
  LayoutTextAlignment horizontal_alignment;
} LayoutNodeHorizontalConfig;

typedef struct PACKED {
  LayoutNodeContainerConfig container;
  LayoutVerticalAlignment vertical_alignment;
} LayoutNodeVerticalConfig;

typedef struct LayoutNodeConstructorConfig LayoutNodeConstructorConfig;

typedef GTextNode *(*LayoutNodeConstructor)(
    const LayoutLayer *layout, const LayoutNodeConstructorConfig *config);

struct PACKED LayoutNodeConstructorConfig {
  LayoutNodeExtentConfig extent;
  LayoutNodeConstructor constructor;
  void *context;
};

typedef struct LayoutNodeIconConfig {
  LayoutNodeExtentConfig extent;
  KinoLayer **icon_layer;
  AppResourceInfo *res_info;
  GAlign align;
} LayoutNodeIconConfig;

GTextNodeVertical *layout_create_headings_paragraphs_node(
    const LayoutLayer *layout, const LayoutNodeHeadingsParagraphsConfig *config);

GTextNode *layout_create_text_node_from_config(const LayoutLayer *layout,
                                               const LayoutNodeConfig *config);

GTextNodeCustom *layout_node_create_kino_layer_wrapper(KinoLayer *kino_layer);
