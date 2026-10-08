/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/timeline/attribute.h>
#include <pbl/services/timeline/layout_layer.h>
#include <pbl/services/timeline/timeline_resources.h>

#include <applib/graphics/text.h>
#include <applib/ui/kino/kino_layer.h>
#include <apps/system/timeline/text_node.h>
#include <resource/resource_ids.auto.h>
#include <shell/system_theme.h>

/**
 * @defgroup services_timeline_layout_node Layout nodes
 * @ingroup services_timeline
 * @brief Compact, declarative construction of GTextNode trees for layouts.
 *
 * A hierarchy of packed LayoutNode configs describes a tree of text nodes, instantiated with
 * layout_create_text_node_from_config(). Whole layouts, such as the timeline cards, are described
 * this way; the generic layout is the simplest example. Every config starts with a
 * LayoutNodeConfig whose type tells which config struct it is.
 *
 * For the best code size, use static configs without one-use callbacks. Configs on the stack cost
 * stack space, so create the nodes and return immediately, avoiding deeper work such as measuring
 * the view with graphics_text_node_get_size(), which is very stack intensive.
 *
 * @code{.c}
 * static const LayoutNodeTextAttributeConfig s_title_config = {
 *   .attr_id = AttributeIdTitle,
 *   .text.style_font = TextStyleFont_Header,
 *   .text.extent.node.type = LayoutNodeType_TextAttribute,
 * };
 * static const LayoutNodeTextAttributeConfig s_body_config = {
 *   .attr_id = AttributeIdBody,
 *   .text.style_font = TextStyleFont_Body,
 *   .text.extent.node.type = LayoutNodeType_TextAttribute,
 * };
 * static const LayoutNodeConfig *s_vertical_nodes[] = {
 *   &s_title_config.text.extent.node,
 *   &s_body_config.text.extent.node,
 * };
 * static const LayoutNodeVerticalConfig s_vertical_config = {
 *   .container.extent.node.type = LayoutNodeType_Vertical,
 *   .container.nodes = (LayoutNodeConfig **)s_vertical_nodes,
 *   .container.num_nodes = ARRAY_LENGTH(s_vertical_nodes),
 * };
 *
 * GTextNode *view = layout_create_text_node_from_config(layout,
 *                                                       &s_vertical_config.container.extent.node);
 * @endcode
 * @{
 */

/**
 * @brief Convert a GTextAlignment to a LayoutTextAlignment.
 *
 * @param alignment GTextAlignment.
 */
#define ToLayoutTextAlignment(alignment) (alignment + 1)
/**
 * @brief Convert a LayoutTextAlignment other than auto to a GTextAlignment.
 *
 * @param alignment LayoutTextAlignment.
 */
#define ToGTextAlignment(alignment) (alignment - 1)

/**
 * @brief Convert a GVerticalAlignment to a LayoutVerticalAlignment.
 *
 * @param alignment GVerticalAlignment.
 */
#define ToLayoutVerticalAlignment(alignment) (alignment + 1)
/**
 * @brief Convert a LayoutVerticalAlignment other than auto to a GVerticalAlignment.
 *
 * @param alignment LayoutVerticalAlignment.
 */
#define ToGVerticalAlignment(alignment) (alignment - 1)

/**
 * @brief Convert a PreferredContentSize to a LayoutContentSize.
 *
 * @param size PreferredContentSize.
 */
#define ToLayoutContentSize(size) ((LayoutContentSize)((size) + 1))
/**
 * @brief Convert a LayoutContentSize other than auto to a PreferredContentSize.
 *
 * @param size LayoutContentSize.
 */
#define ToPreferredContentSize(size) ((PreferredContentSize)((size) - 1))

/** @brief Horizontal text alignment; GTextAlignment shifted so that 0 means automatic. */
typedef enum {
  /** Left on rectangular displays, centered on round ones. */
  LayoutTextAlignment_Auto = 0,
  /** Left aligned. */
  LayoutTextAlignment_Left = GTextAlignmentLeft + 1,
  /** Centered. */
  LayoutTextAlignment_Center = GTextAlignmentCenter + 1,
  /** Right aligned. */
  LayoutTextAlignment_Right = GTextAlignmentRight + 1,
} LayoutTextAlignment;

/** @brief Vertical alignment; GVerticalAlignment shifted so that 0 means automatic. */
typedef enum {
  /** Default alignment of the node. */
  LayoutVerticalAlignment_Auto = 0,
  /** Top aligned. */
  LayoutVerticalAlignment_Left = GVerticalAlignmentTop + 1,
  /** Centered. */
  LayoutVerticalAlignment_Center = GVerticalAlignmentCenter + 1,
  /** Bottom aligned. */
  LayoutVerticalAlignment_Right = GVerticalAlignmentBottom + 1,
} LayoutVerticalAlignment;

/** @brief Layout color used for text. */
typedef enum {
  /** Keep the node's default color. */
  LayoutColor_None = 0,
  /** Primary color of the layout. */
  LayoutColor_Primary,
  /** Secondary color of the layout. */
  LayoutColor_Secondary,
  /** Background color of the layout. */
  LayoutColor_Background,
} LayoutColor;

/** @brief Content size; PreferredContentSize shifted so that 0 means the system size. */
typedef enum {
  /** System content size. */
  LayoutContentSize_Auto = 0,
  /** Small. */
  LayoutContentSize_Small = PreferredContentSizeSmall + 1,
  /** Medium. */
  LayoutContentSize_Medium = PreferredContentSizeMedium + 1,
  /** Large. */
  LayoutContentSize_Large = PreferredContentSizeLarge + 1,
  /** Extra large. */
  LayoutContentSize_ExtraLarge = PreferredContentSizeExtraLarge + 1,
  /** Platform default size. */
  LayoutContentSizeDefault = PreferredContentSizeDefault + 1,
} LayoutContentSize;

/** @brief Type of a node config, selecting the config struct. */
typedef enum {
  /** Text node whose text points to a string attribute (LayoutNodeTextAttributeConfig). */
  LayoutNodeType_TextAttribute = 0,
  /** Text node with a buffer initialized from a literal string (LayoutNodeTextBufferConfig). */
  LayoutNodeType_TextBuffer,
  /**
   * Dynamic text node with a buffer of a given size filled by an update function
   * (LayoutNodeTextDynamicConfig). Use sparingly, it is expensive in code space.
   */
  LayoutNodeType_TextDynamic,
  /** Text node without a buffer (LayoutNodeTextConfig). */
  LayoutNodeType_Text,
  /** Horizontal container populated with the given nodes (LayoutNodeHorizontalConfig). */
  LayoutNodeType_Horizontal,
  /** Vertical container populated with the given nodes (LayoutNodeVerticalConfig). */
  LayoutNodeType_Vertical,
  /**
   * Arbitrary node built by a constructor (LayoutNodeConstructorConfig). Use sparingly, it is
   * expensive in code space.
   */
  LayoutNodeType_Constructor,
  /**
   * Vertical container of text nodes pointing to the layout's headings and paragraphs attributes
   * (LayoutNodeHeadingsParagraphsConfig).
   */
  LayoutNodeType_HeadingsParagraphs,
  /** Custom node wrapping a newly created KinoLayer (LayoutNodeIconConfig). */
  LayoutNodeType_Icon,
  /**
   * Node positioning the TimelineLayout's icon layer (LayoutNodeExtentConfig). TimelineLayout
   * only.
   */
  LayoutNodeType_TimelineIcon,
  /**
   * Page break showing the glance arrow and marking the TimelineLayout as having a page break
   * (LayoutNodeConfig). TimelineLayout only.
   */
  LayoutNodeType_TimelinePageBreak,
  /**
   * Vertical container of icons, names and values from the layout's metric attributes
   * (LayoutNodeExtentConfig). TimelineLayout only.
   */
  LayoutNodeType_TimelineMetrics,
} LayoutNodeType;

/** @brief Base of every node config. */
typedef struct {
  /** Config type. */
  LayoutNodeType type;
} LayoutNodeConfig;

/** @brief Node config with an extent, added to the node's own. */
typedef struct {
  /** Base config. */
  LayoutNodeConfig node;
  /** Position delta, not affecting neighboring nodes. */
  struct {
    /** Horizontal offset in pixels. */
    int8_t x;
    /** Vertical offset in pixels. */
    int8_t y;
  } offset;
  /** Size delta, not affecting the node's internal positioning. */
  struct {
    /** Width delta in pixels. */
    int8_t w;
    /** Height delta in pixels. */
    int8_t h;
  } margin;
} LayoutNodeExtentConfig;

/** @brief Text node config. */
typedef struct {
  /** Extent. */
  LayoutNodeExtentConfig extent;
  /** Font key; if NULL the font is taken from @ref style and @ref style_font. */
  const char *font_key;
  /** Content size used to pick the font. */
  LayoutContentSize style : 8;
  /** System theme font used to pick the font. */
  TextStyleFont style_font : 8;
  /** Line spacing delta in pixels. */
  int8_t line_spacing_delta : 4;
  /**
   * Fixed height as a multiple of the font height (number of lines), 0 for none. Only use for text
   * on the first page: text after the first page fold with fixed lines is not guaranteed to draw.
   */
  uint8_t fixed_lines : 2;
  /** Horizontal alignment. */
  LayoutTextAlignment alignment : 2;
#if PBL_COLOR
  /** Text color. */
  LayoutColor color;
#endif
} LayoutNodeTextConfig;

/** @brief Headings and paragraphs node config. */
typedef struct {
  /** Extent. */
  LayoutNodeExtentConfig extent;
  /** Content size used to pick the fonts. */
  LayoutContentSize size : 8;
  /** Font of the headings. */
  TextStyleFont heading_style_font : 8;
  /** Font of the paragraphs. */
  TextStyleFont paragraph_style_font : 8;
} LayoutNodeHeadingsParagraphsConfig;

/** @brief Attribute text node config; no node is created if the attribute is empty. */
typedef struct {
  /** Text config. */
  LayoutNodeTextConfig text;
  /** String attribute to show. */
  AttributeId attr_id;
} LayoutNodeTextAttributeConfig;

/** @brief Buffer text node config; no node is created for an empty string. */
typedef struct {
  /** Text config. */
  LayoutNodeTextConfig text;
  /** String copied into the node. */
  const char *str;
  /** Whether @ref str is an i18n key to translate. */
  bool use_i18n;
} LayoutNodeTextBufferConfig;

/** @brief Dynamic text node config. */
typedef struct LayoutNodeTextDynamicConfig LayoutNodeTextDynamicConfig;

/**
 * @brief Fill the buffer of a dynamic text node.
 *
 * @param layout Layout the node belongs to.
 * @param config Config of the node.
 * @param[out] buffer Text buffer of @c config->buffer_size bytes.
 * @param render true when drawing, false when measuring.
 */
typedef void (*LayoutNodeTextDynamicUpdate)(const LayoutLayer *layout,
                                            const LayoutNodeTextDynamicConfig *config, char *buffer,
                                            bool render);

/** @brief Dynamic text node config. */
struct LayoutNodeTextDynamicConfig {
  /** Text config. */
  LayoutNodeTextConfig text;
  /** Update function. */
  LayoutNodeTextDynamicUpdate update;
  /** User data for @ref update. */
  void *context;
  /** Size of the text buffer in bytes. */
  uint16_t buffer_size;
};

/** @brief Container node config. */
typedef struct {
  /** Extent. */
  LayoutNodeExtentConfig extent;
  /** Configs of the child nodes. */
  LayoutNodeConfig **nodes;
  /** Number of child configs. */
  uint8_t num_nodes;
  /** Extra child capacity, for nodes added after creation. */
  uint8_t extra_capacity;
} LayoutNodeContainerConfig;

/** @brief Horizontal container node config. */
typedef struct {
  /** Container config. */
  LayoutNodeContainerConfig container;
  /** Horizontal alignment of the children. */
  LayoutTextAlignment horizontal_alignment;
} LayoutNodeHorizontalConfig;

/** @brief Vertical container node config. */
typedef struct {
  /** Container config. */
  LayoutNodeContainerConfig container;
  /** Vertical alignment of the children. */
  LayoutVerticalAlignment vertical_alignment;
} LayoutNodeVerticalConfig;

/** @brief Constructor node config. */
typedef struct LayoutNodeConstructorConfig LayoutNodeConstructorConfig;

/**
 * @brief Build the node of a constructor node config.
 *
 * @param layout Layout the node belongs to.
 * @param config Config of the node.
 * @return New node, or NULL.
 */
typedef GTextNode *(*LayoutNodeConstructor)(const LayoutLayer *layout,
                                            const LayoutNodeConstructorConfig *config);

/** @brief Constructor node config. */
struct LayoutNodeConstructorConfig {
  /** Extent, added to that of the constructed node. */
  LayoutNodeExtentConfig extent;
  /** Constructor. */
  LayoutNodeConstructor constructor;
  /** User data for @ref constructor. */
  void *context;
};

/** @brief Icon node config. */
typedef struct LayoutNodeIconConfig {
  /** Extent. */
  LayoutNodeExtentConfig extent;
  /** Set to the created KinoLayer, which is added to the layout; the caller destroys it. */
  KinoLayer **icon_layer;
  /** Resource of the icon. */
  AppResourceInfo *res_info;
  /** Alignment of the icon. */
  GAlign align;
} LayoutNodeIconConfig;

/**
 * @brief Create a vertical node with the layout's headings and paragraphs.
 *
 * @param layout Layout whose headings and paragraphs attributes are shown.
 * @param config Fonts and content size.
 * @return New node, or NULL if there are no headings.
 */
GTextNodeVertical *layout_create_headings_paragraphs_node(
    const LayoutLayer *layout, const LayoutNodeHeadingsParagraphsConfig *config);

/**
 * @brief Create a tree of text nodes from a config.
 *
 * @param layout Layout the nodes belong to.
 * @param config Root config, may be NULL.
 * @return New node tree, or NULL.
 */
GTextNode *layout_create_text_node_from_config(const LayoutLayer *layout,
                                               const LayoutNodeConfig *config);

/**
 * @brief Wrap a KinoLayer in a custom text node that positions the layer during layout.
 *
 * @param kino_layer Layer to position; not owned.
 * @return New node.
 */
GTextNodeCustom *layout_node_create_kino_layer_wrapper(KinoLayer *kino_layer);

/** @} */
