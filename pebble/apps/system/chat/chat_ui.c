// Pebblegram's watch UI (the Chat watchapp, chat/pbw in pebble-apps), ported to a system app.
// Its AppMessage protocol is answered by chat_ds.c; see pebble_compat.h.
#include "pebble_compat.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kb_words.h"
#include "chat_warp.h"

#include <pbl/services/notifications/alerts_preferences_private.h>
#include <applib/dark_mode_service.h>
#include <shell/system_theme.h>
#ifdef CONFIG_TOUCH
#include <applib/ui/scroll_layer_private.h>  // the system lists' rubber band and fling
#endif

#define MAX_CHATS 20
#define MAX_MESSAGES 9
// Room above the first message for the "Loading..." pill while older history may exist
#define OLDER_LOADER_H 30
#define OLDER_LOADER_FRAME_MS 80
#define OLDER_SPINNER_R 10
#define MAX_TEXT 460
#define MAX_FULL_TEXT 1200
#define MESSAGE_PREVIEW_TEXT PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 132, 132, 132, 132, 220, 132, 220)
#define MAX_SENDER 36
#define MAX_REACTIONS 64
#define REACTION_LINE_H 17
#define REACTION_MAX_LINES 3
#define REACTION_OVERLAP 10  // how much of a reaction pill sits on its bubble
#define MAX_META 48  // "time|status": the status is "Seen by Anna, Bob" at its longest
#define MAX_CONTEXT_TEXT PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 60, 60, 60, 56, 72, 56, 64)
#define MAX_ID 24
#define MAX_IMAGE_ERROR 32
// Photos arrive as ready-to-draw Pebble bitmaps (see image_start), so the bytes
// on the wire are exactly the heap a photo occupies; nothing is decoded on the
// watch. MAX_IMAGE_BYTES caps one photo, IMAGE_HEAP_RESERVE is kept free for
// menus, the keyboard and incoming messages, and IMAGE_TALL_MAX_H is how tall a
// screenshot may be drawn before it is scaled down to fit.
#define MAX_IMAGE_BYTES PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 14000, 16000, 16000, 7000, 48000, 7000, 48000)
#define IMAGE_HEAP_RESERVE PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 5000, 5000, 5000, 4500, 12000, 4500, 12000)
#define IMAGE_MIN_BUDGET 1500
// Extra heap freed (from off-screen photos only) before a page of older or
// newer messages is loaded, so the new messages always have room.
#define PAGE_LOAD_HEADROOM PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 2500, 2500, 2500, 2500, 6000, 2500, 6000)
#define IMAGE_TALL_MAX_H PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 420, 460, 460, 420, 640, 420, 600)
#define IMAGE_BITS_PER_PIXEL PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 8, 8, 8, 1, 8, 1, 8)
#define MAX_AVATAR_BYTES PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 3000, 3000, 3000, 2200, 3000, 2200, 3000)
#define IMAGE_THUMB_SIZE PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 132, 108, 108, 108, 198, 108, 144)
#define APP_INBOX_SIZE 2048
#define APP_OUTBOX_SIZE PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 512, 512, 512, 512, 1024, 512, 1024)
#define BW_UI PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 0, 0, 0, 1, 0, 1, 0)
#define ROUND_UI PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 0, 0, 0, 0, 0, 0, 1)
#define CHAT_AVATAR_ARC 170
#define MAX_CANNED 10
#define CANNED_TEXT_LEN 40
#define PG_MIN(a, b) ((a) < (b) ? (a) : (b))
#define PG_MAX(a, b) ((a) > (b) ? (a) : (b))
#define APP_COLOR GColorCobaltBlue
#define ACTION_BG GColorBlack
#define ACTION_TEXT GColorDarkGray
#define ACTION_TEXT_SELECTED GColorWhite

// ---- Telegram-style theme -------------------------------------------------
// Colour watches get the Telegram look (blue header, green doodle wallpaper,
// white/mint bubbles) or its night mode. Black-and-white watches fall back to
// clean outlines, inverted in the dark theme. The TG_* names read the current
// theme, which theme_apply() refills whenever the Appearance settings change.
typedef struct {
  GColor header, list_bg, list_selected, text, subtext, separator, link, badge, badge_muted;
  GColor chat_bg, in_bubble, out_bubble, bubble_shadow, in_meta, out_meta, out_accent;
  GColor select_ring, accent, pill_in, pill_out, service_pill, ctx_in, ctx_out, wall_hi;
  // Text on the coloured surfaces above: black or white, whichever reads better.
  GColor on_header, on_selected, on_badge, on_service, on_out, on_in, on_accent, on_ring;
} Theme;

static Theme s_theme;

#define TG_HEADER s_theme.header
#define TG_LIST_BG s_theme.list_bg
#define TG_LIST_SELECTED s_theme.list_selected
#define TG_TEXT s_theme.text
#define TG_SUBTEXT s_theme.subtext
#define TG_SEPARATOR s_theme.separator
#define TG_LINK s_theme.link
#define TG_BADGE s_theme.badge
#define TG_BADGE_MUTED s_theme.badge_muted
#define TG_CHAT_BG s_theme.chat_bg
#define TG_IN_BUBBLE s_theme.in_bubble
#define TG_OUT_BUBBLE s_theme.out_bubble
#define TG_BUBBLE_SHADOW s_theme.bubble_shadow
#define TG_IN_META s_theme.in_meta
#define TG_OUT_META s_theme.out_meta
// Outgoing accent on a list or quote box rather than on the bubble itself.
#define TG_OUT_ACCENT s_theme.out_accent
#define TG_SELECT_RING s_theme.select_ring
#define TG_ACCENT s_theme.accent
#define TG_PILL_IN s_theme.pill_in
#define TG_PILL_OUT s_theme.pill_out
#define TG_SERVICE_PILL s_theme.service_pill
#define TG_CTX_IN s_theme.ctx_in
#define TG_CTX_OUT s_theme.ctx_out
#define TG_ON_HEADER s_theme.on_header
#define TG_ON_SELECTED s_theme.on_selected
#define TG_ON_BADGE s_theme.on_badge
#define TG_ON_SERVICE s_theme.on_service
#define TG_ON_OUT s_theme.on_out
#define TG_ON_IN s_theme.on_in
#define TG_ON_ACCENT s_theme.on_accent
#define TG_ON_RING s_theme.on_ring
#define HEADER_H PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 26, 26, 26, 26, 32, 26, 34)
// Chat list avatar radius. The phone sends profile photos at exactly this
// diameter (see AVATAR_SIZE in index.js), so they are drawn pixel for pixel.
#define CHAT_AVATAR_R PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 15, 15, 15, 15, 19, 15, 16)

// ---- Appearance settings (Clay page on the phone) -------------------------
typedef enum {
  PatternDoodles,
  PatternDots,
  PatternLines,
  PatternGrid,
  PatternNone,
  // Appended after None so settings saved by older versions keep their meaning.
  PatternSpace,
  PatternPets,
  PatternFood,
  PatternTravel,
  PatternParty,
  PatternLast = PatternParty
} WallpaperPattern;

// Saved with persist_write_data. Colours are stored as raw GColor bytes.
typedef struct {
  uint8_t version;
  uint8_t dark;
  uint8_t pattern;
  uint8_t text_size;  // 0-3: Gothic 14, 18, 24, 28
  uint8_t bold;
  uint8_t photos;
  uint8_t big_emoji;
  uint8_t accent;
  uint8_t chat_bg;
  uint8_t out_bubble;
} Appearance;

#define APPEARANCE_KEY 1
#define APPEARANCE_VERSION 2  // 2: Beeper look (plain background, blue bubbles)
// The pickers' defaults stand for "the theme's own colour"; see theme_apply().
#define DEFAULT_ACCENT GColorBlueMoon  // Beeper blue (#0D4FFB)
#define DEFAULT_CHAT_BG GColorWhite  // black in the dark theme, as in Beeper
#define DEFAULT_OUT_BUBBLE GColorBlueMoon  // Beeper blue, white text

static Appearance s_appearance;

static void appearance_defaults(Appearance *a) {
  memset(a, 0, sizeof(*a));
  a->version = APPEARANCE_VERSION;
  a->text_size = 1;
  a->pattern = PatternNone;
  a->photos = 1;
  a->big_emoji = 1;
#ifdef PBL_COLOR
  a->accent = DEFAULT_ACCENT.argb;
  a->chat_bg = DEFAULT_CHAT_BG.argb;
  a->out_bubble = DEFAULT_OUT_BUBBLE.argb;
#endif
}

// Black or white, whichever reads better on `c` (channels are 0-3).
static GColor contrast_on(GColor c) {
  return (c.r * 3 + c.g * 6 + c.b) >= 17 ? GColorBlack : GColorWhite;
}

#ifdef PBL_COLOR
// One step brighter (+1) or darker (-1) per channel: MayGreen <-> MintGreen.
static GColor shade(GColor c, int step) {
  GColor out = c;
  out.r = PG_MAX(0, PG_MIN(3, c.r + step));
  out.g = PG_MAX(0, PG_MIN(3, c.g + step));
  out.b = PG_MAX(0, PG_MIN(3, c.b + step));
  return out;
}
#endif

static void theme_apply(void) {
  Theme *t = &s_theme;
#ifdef PBL_COLOR
  bool dark = s_appearance.dark;
  GColor accent = (GColor){.argb = s_appearance.accent};
  GColor bg = (GColor){.argb = s_appearance.chat_bg};
  GColor out = (GColor){.argb = s_appearance.out_bubble};
  bool own_accent = !gcolor_equal(accent, DEFAULT_ACCENT);
  bool own_out = !gcolor_equal(out, DEFAULT_OUT_BUBBLE);
  if (dark) {
    t->header = own_accent ? accent : GColorBlack;
    t->list_bg = GColorBlack;
    t->list_selected = own_accent ? accent : GColorDarkGray;
    t->text = GColorWhite;
    t->subtext = GColorLightGray;
    t->separator = GColorDarkGray;
    t->link = own_accent ? accent : GColorCeleste;
    t->badge = accent;
    t->badge_muted = GColorDarkGray;
    t->in_bubble = GColorWhite;  // black on white: the most contrast a bubble can have on black
    t->in_meta = GColorDarkGray;
    t->select_ring = own_accent ? accent : GColorVividCerulean;
    t->accent = own_accent ? accent : GColorCobaltBlue;
    t->pill_in = t->pill_out = t->ctx_out = GColorBlack;
    t->ctx_in = GColorLightGray;
    if (gcolor_equal(bg, DEFAULT_CHAT_BG)) {
      bg = GColorBlack;
    }
  } else {
    t->header = accent;
    t->list_bg = GColorWhite;
    t->list_selected = own_accent ? accent : GColorVividCerulean;
    t->text = GColorBlack;
    t->subtext = GColorDarkGray;
    t->separator = GColorLightGray;
    t->link = accent;
    t->badge = accent;
    t->badge_muted = GColorLightGray;
    t->in_bubble = gcolor_equal(bg, GColorWhite) ? GColorLightGray : GColorWhite;
    t->in_meta = GColorDarkGray;
    t->select_ring = own_accent ? accent : GColorBlueMoon;
    t->accent = own_accent ? accent : GColorVividCerulean;
    t->pill_in = t->ctx_in = GColorCeleste;
    t->pill_out = t->ctx_out = GColorWhite;
  }
  t->chat_bg = bg;
  t->out_bubble = out;
  // Black has no darker shade, and a pill must show on it
  t->bubble_shadow = t->service_pill = gcolor_equal(bg, GColorBlack) ? GColorDarkGray : shade(bg, -1);
  t->wall_hi = shade(bg, gcolor_equal(contrast_on(bg), GColorWhite) ? 1 : -1);
  if (own_out) {
    t->out_meta = gcolor_equal(contrast_on(out), GColorBlack) ? GColorDarkGray : GColorLightGray;
    t->out_accent = t->link;
  } else {
    t->out_meta = t->out_accent = GColorCeleste;
  }
#else
  GColor fg = s_appearance.dark ? GColorWhite : GColorBlack;
  GColor bg = s_appearance.dark ? GColorBlack : GColorWhite;
  t->header = t->list_selected = t->text = t->subtext = t->separator = t->link = fg;
  t->badge = t->badge_muted = t->bubble_shadow = t->in_meta = t->out_meta = t->out_accent = fg;
  t->select_ring = t->accent = t->service_pill = fg;
  t->list_bg = t->chat_bg = t->in_bubble = t->out_bubble = bg;
  t->pill_in = t->pill_out = t->ctx_in = t->ctx_out = bg;
#endif
  t->on_header = contrast_on(t->header);
  t->on_selected = contrast_on(t->list_selected);
  t->on_badge = contrast_on(t->badge);
  t->on_service = contrast_on(t->service_pill);
  t->on_out = contrast_on(t->out_bubble);
  t->on_in = contrast_on(t->in_bubble);
  t->on_accent = contrast_on(t->accent);
  t->on_ring = contrast_on(t->select_ring);
}

// Telegram's seven avatar and name colours, mapped to the Pebble palette.
#ifdef PBL_COLOR
static const uint8_t TG_AVATAR_COLORS[7] = {
  GColorSunsetOrangeARGB8, GColorRajahARGB8, GColorLavenderIndigoARGB8, GColorMayGreenARGB8,
  GColorCadetBlueARGB8, GColorPictonBlueARGB8, GColorBrilliantRoseARGB8
};
static const uint8_t TG_NAME_COLORS[7] = {
  GColorDarkCandyAppleRedARGB8, GColorOrangeARGB8, GColorPurpleARGB8, GColorIslamicGreenARGB8,
  GColorTiffanyBlueARGB8, GColorBlueMoonARGB8, GColorJazzberryJamARGB8
};
#endif

// Chat flags sent by the phone in the chat row's meta ("time|flags").
#define CHAT_FLAG_GROUP 0x01
#define CHAT_FLAG_MUTED 0x02
#define CHAT_FLAG_PINNED 0x04
#define CHAT_FLAG_CONTACT 0x08  // a search result: a person with no conversation yet
// Scrolls ease out over CHAT_SCROLL_MS, however long a frame takes to draw.
#define CHAT_SCROLL_MS 220
#define CHAT_SCROLL_FRAME_MS 16
#define CHAT_SCROLL_DELTA 30
#define REPEAT_SCROLL_MS 140
#define MESSAGE_MODE_INITIAL 0
#define MESSAGE_MODE_OLDER 1
#define MESSAGE_MODE_NEWER 2
#define LONG_MESSAGE_SCROLL_DELTA PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 42, 42, 42, 42, 56, 42, 48)
#define MESSAGE_COMMAND_RETRY_MS 3000
#define MESSAGE_COMMAND_WAKE_RETRY_MS 650
#define MESSAGE_COMMAND_MAX_ATTEMPTS 3
#define MESSAGE_TRANSFER_TIMEOUT_MS 20000
#define IMAGE_COMMAND_RETRY_MS 350
#define IMAGE_PREPARE_STALL_MS 30000
#define IMAGE_TRANSFER_STALL_MS 12000
#define CHAT_COMMAND_WAKE_RETRY_MS 700
#define CHAT_COMMAND_MAX_ATTEMPTS 4
#define CHAT_FIRST_PAINT_ROWS PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 3, 3, 3, 3, 5, 3, 5)
#define PHONE_WAKE_DELAY_MS 180
// Photos within this many pixels of the screen start loading early; a photo
// that is loading keeps going until it is farther away than IMAGE_KEEP_MARGIN.
#define IMAGE_LOAD_MARGIN 40
#define IMAGE_KEEP_MARGIN 160
#define IMAGE_RETRY_MAX_LEVEL 3
#define IMAGE_DIAG_LOGS 0
#define STATUS_CLEAR_MS 1000
#define VIEW_TRANSITION_MS 120
#define TOUCH_KEYBOARD_ENABLED 1
// Chats come from several networks (chat_db.h ChatDbNetwork); each has its own features, sent
// by the phone, and a look. Adding a network is an entry here plus its phone side.
#define CHAT_NETWORK_BEEPER 0
#define CHAT_NETWORK_SMS 1
#define CHAT_NETWORKS 2
static const struct {
  const char *badge;     // shown on its chat rows; NULL for none
  uint8_t out_bubble;    // ARGB8 of my bubbles, 0 for the theme's
} s_network_look[CHAT_NETWORKS] = {
  [CHAT_NETWORK_BEEPER] = {NULL, 0},
  [CHAT_NETWORK_SMS] = {"SMS", GColorIslamicGreenARGB8},
};
static char s_network_features[CHAT_NETWORKS][96];
static uint8_t s_current_chat_network;
// Message actions (React, Edit) depend on the open chat's network: gomuks has them, SMS and
// Beeper's Android provider don't. Set from s_network_features when a chat opens.
static bool s_message_actions;

// Whether a network's comma separated features include name.
static bool network_has_feature(uint8_t network, const char *name) {
  if (network >= CHAT_NETWORKS) {
    return false;
  }
  const size_t n = strlen(name);
  for (const char *f = s_network_features[network]; *f;) {
    const char *end = strchr(f, ',');
    const size_t len = end ? (size_t)(end - f) : strlen(f);
    if (len == n && strncmp(f, name, n) == 0) {
      return true;
    }
    if (!end) {
      break;
    }
    f = end + 1;
  }
  return false;
}

static void refresh_message_actions(void) {
  s_message_actions = network_has_feature(s_current_chat_network, "react") &&
                      network_has_feature(s_current_chat_network, "edit");
}

// Favourites, for Send Message in the chat list's menu (chat_ds answers get_favorites).
#define MAX_FAVORITES 8
static struct {
  char id[24];
  char name[32];
  uint8_t network;
} s_favorites[MAX_FAVORITES];
static int s_favorite_count;
// Neither connection archives, mutes or marks chats unread yet; 1 brings Pebblegram's chat menu back.
#define BEEPER_CHAT_ACTIONS 1  // hold a chat: archive it, mark it read or unread
#define TOUCH_KEYBOARD_MAX_TEXT 400
// Only watches with a touchscreen. Pebble 2 Duo (flint) firmware exposes the
// touch API too, but has no touchscreen and needs that RAM for itself.
#if defined(_PBL_API_EXISTS_touch_service_subscribe) && (defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO))
#define TOUCH_KEYBOARD_AVAILABLE 1
#else
#define TOUCH_KEYBOARD_AVAILABLE 0
#endif

#if IMAGE_DIAG_LOGS
#define IMAGE_DIAG(...) APP_LOG(APP_LOG_LEVEL_INFO, __VA_ARGS__)
#else
#define IMAGE_DIAG(...)
#endif

// Platform constants are centralized here. Basalt/Diorite stay conservative on
// heap use; Emery/Gabbro can afford longer text and larger image payloads.
typedef enum {
  ViewStateChatList,
  ViewStateChat
} ViewState;

typedef enum {
  ActionMenuMain,
  ActionMenuChat,
  ActionMenuCanned,
  ActionMenuConfirm,
  ActionMenuReply,
  ActionMenuReactionGrid,
  ActionMenuEmojiReplyGrid,
  ActionMenuFullText
} ActionMenuMode;

typedef enum {
  ActionItemSendDraft,
  ActionItemRetryDraft,
  ActionItemEditDraft,
  ActionItemCompose,
  ActionItemCanned,
  ActionItemReply,
  ActionItemReplyDictate,
  ActionItemReplyCanned,
  ActionItemReplyEmoji,
  ActionItemEmoji,
  ActionItemReact,
  ActionItemEdit,
  ActionItemFullText,
  ActionItemFullContext,
  ActionItemGoToBottom,
  ActionItemArchiveChat,
  ActionItemDeleteChat,
  ActionItemMuteChat,
  ActionItemMarkUnread,
  ActionItemGoBack,
  ActionItemConfirmSend,
  ActionItemConfirmCancel,
  ActionItemKeyboard,
  ActionItemReplyKeyboard,
  ActionItemEditKeyboard,
  ActionItemReactGrid,
  ActionItemEmojiGrid,
  ActionItemReplyEmojiGrid,
  ActionItemSearch,
  ActionItemShowInbox,
  ActionItemFavorite,
  ActionItemOpenChat
} ActionItem;

typedef enum {
  KeyboardModeCompose,
  KeyboardModeReply,
  KeyboardModeEdit
} KeyboardMode;

typedef enum {
  EmojiGridReact,
  EmojiGridReply
} EmojiGridMode;

typedef struct {
  char id[MAX_ID];
  char *string_storage;
  char *title;
  char *preview;
  bool unread;
  uint8_t network;
  int unread_count;
  GBitmap *avatar_bitmap;
  uint8_t *avatar_data;  // pixels behind avatar_bitmap when it came as a ready bitmap
  char time[8];
  uint8_t flags;
  uint8_t receipt;
} Chat;

#define WRAP_LINES 14

typedef struct {
  char id[MAX_ID];
  char *string_storage;
  char *sender;
  char *text;
  char *reactions;
  char *meta;
  char *context;
  char image_token[MAX_ID];
  char image_error[MAX_IMAGE_ERROR];
  bool outgoing;
  bool image_placeholder;
  bool image_requested;
  bool image_failed;
  uint8_t image_progress;
  uint8_t image_retry_level;
  uint16_t image_width;
  uint16_t image_height;
  GBitmap *image_bitmap;
  uint8_t *image_data;
  // Cached measurements (see measure_message).
  const char *layout_key;
  int16_t layout_maxw;
  uint16_t layout_img;
  int16_t text_w, text_h, text_nw, text_nh;
  int16_t meta_w, name_w, ctx_w;
  int16_t pills_w, pills_h, pills_last_w;
  uint8_t emoji_only;
  uint8_t font_size;  // text size this message is drawn at (see message_font_size)
  // Where the text wraps (see wrap_message); wrap_n 0 means not computed yet.
  uint8_t wrap_n, wrap_w, wrap_lh;
  uint8_t wrap_end[WRAP_LINES];
} Message;

// Telegram standard reactions the watch can draw, most popular first, as NUL-separated items.
// PebbleOS 4.29+ has a much larger emoji font than older firmware; the older one draws only
// the items flagged in *_CLASSIC.
static const char REACTION_CHOICES[] =
  "👍\0" "❤\0" "🔥\0" "🥰\0" "👏\0" "😁\0" "🤔\0" "🤯\0"
  "😱\0" "🤬\0" "😢\0" "🎉\0" "🤩\0" "🤮\0" "💩\0" "🙏\0"
  "👌\0" "🕊\0" "🤡\0" "🥱\0" "🥴\0" "😍\0" "🐳\0" "🌚\0"
  "🌭\0" "💯\0" "🤣\0" "⚡\0" "🍌\0" "🏆\0" "💔\0" "🤨\0"
  "😐\0" "🍓\0" "🍾\0" "💋\0" "🖕\0" "😈\0" "😴\0" "😭\0"
  "🤓\0" "👻\0" "👀\0" "🎃\0" "🙈\0" "😇\0" "😨\0" "🤝\0"
  "✍\0" "🤗\0" "🫡\0" "🎅\0" "🎄\0" "☃\0" "💅\0" "🤪\0"
  "🗿\0" "🆒\0" "💘\0" "🙉\0" "🦄\0" "😘\0" "💊\0" "🙊\0"
  "😎\0" "👾\0" "🤷\0" "😡\0" "👎\0";
static const uint32_t REACTION_CHOICES_CLASSIC[3] = {0x4621ff3bu, 0x2482e4e9u, 0x00000019u};  // bit n: item n is in the classic font
#define REACTION_CHOICES_COUNT 69
#define REACTION_CHOICES_CLASSIC_COUNT 34

// Emoji offered as quick replies.
static const char EMOJI_REPLY_CHOICES[] =
  "👍\0" "👎\0" "❤\0" "😂\0" "🤣\0" "😊\0" "🙂\0" "😉\0"
  "😍\0" "🥰\0" "😘\0" "😎\0" "🤔\0" "🙄\0" "😅\0" "😬\0"
  "😐\0" "😴\0" "😢\0" "😭\0" "😱\0" "😡\0" "🤯\0" "🥺\0"
  "🥹\0" "🫠\0" "🤗\0" "🫡\0" "🤷\0" "🙏\0" "👏\0" "🙌\0"
  "👌\0" "✌\0" "🤞\0" "👋\0" "💪\0" "👀\0" "🔥\0" "✨\0"
  "🎉\0" "🥳\0" "💯\0" "✅\0" "❌\0" "❓\0" "❗\0" "⭐\0"
  "💔\0" "💕\0" "🤝\0" "🍻\0" "☕\0" "🍕\0" "🎂\0" "🎁\0"
  "🚗\0" "🏠\0" "💤\0" "🕐\0" "🆗\0" "😀\0" "😃\0" "😄\0"
  "😁\0" "😇\0" "😈\0" "🤪\0" "🤩\0" "🤓\0" "😷\0" "🤒\0"
  "🤢\0" "🤮\0" "💩\0" "👻\0" "🙈\0" "😺\0" "🐶\0" "🌞\0"
  "🌙\0" "⚡\0" "🌈\0" "☔\0";
static const uint32_t EMOJI_REPLY_CHOICES_CLASSIC[3] = {0xe4bfefbfu, 0xe00fcdabu, 0x0001065fu};  // bit n: item n is in the classic font
#define EMOJI_REPLY_CHOICES_COUNT 84
#define EMOJI_REPLY_CHOICES_CLASSIC_COUNT 51

static Window *s_main_window;
static MenuLayer *s_chat_menu;
static Layer *s_header_layer;
static uint8_t s_current_chat_flags;
static Layer *s_messages_root;

static __attribute__((noinline)) void mark_messages_dirty(void) {
  if (s_messages_root) {
    layer_mark_dirty(s_messages_root);
  }
}

static void cancel_timer(AppTimer **timer) {
  if (*timer) {
    app_timer_cancel(*timer);
    *timer = NULL;
  }
}

static PropertyAnimation *s_chat_menu_animation;
static PropertyAnimation *s_messages_animation;

static Window *s_action_window;
static Layer *s_action_layer;
static ActionMenu *s_native_action_menu;
static ActionMenuLevel *s_native_action_root;
static ActionMenuMode s_action_mode;
static ActionMenuMode s_native_deferred_mode;
static int s_action_selected;
static int s_full_text_scroll_offset;
static int s_full_text_height;
static bool s_full_text_context;
static bool s_native_has_deferred_mode;
static bool s_native_deferred_dictation;
static bool s_native_deferred_edit_draft;  // the keyboard opens on the draft once the menu closes
// Search: the list's own menu (tap the header, or Up twice at the top) asks for a name by voice;
// the phone then fills the chat list with the chats that match, newest message first.
static bool s_list_menu;          // the menu being built is the list's, not a chat's
static bool s_search_dictation;   // the dictation under way is a search, not a message
static char s_search_query[40];   // what the list shows results for; "" = the inbox
static uint8_t s_top_presses;     // Up presses on the first chat
static int8_t s_fav_focus = -1;   // favourite bubble the buttons are on; -1 = the chats
static int s_native_deferred_keyboard = -1;
static int s_native_deferred_grid = -1;
static bool s_native_deferred_grid_reply;
static char s_native_deferred_keyboard_target[MAX_ID];
static char s_full_text_title[MAX_SENDER + 10];
static char *s_full_text_body;

static DictationSession *s_dictation_session;

// The big arrays live on the heap (see init): an app's code + static data must
// stay under 64 KB, and every byte here counts against that.
static Chat *s_chats;
static Message *s_messages;
static Message *s_message_stage;
static int s_message_y[MAX_MESSAGES];
static int s_message_h[MAX_MESSAGES];
typedef struct {
  int16_t x;
  int16_t w;
  int16_t ctx_y;
  int16_t text_y;
  int16_t image_y;
  int16_t pills_y;
  int16_t meta_y;
  int16_t text_box_w;
  int16_t image_w;
  int16_t image_h;
  uint8_t flags;
} MsgBox;

static MsgBox s_box[MAX_MESSAGES];
static int s_compose_bubble_y;
// Scrolled up through the chat, a little scroll back down brings the input bar up over the
// messages, pinned at the bottom, so replying doesn't need a trip back to the end.
#define COMPOSE_FLOAT_TRIGGER 20  // px of scrolling, down to show it or up to hide it
static bool s_compose_floating;
static int s_compose_float_run;  // px scrolled in the current direction (+ down, - up)
// A tap on a cut-short message waits a moment to see if a second tap comes (View Full Message)
#define TOUCH_DOUBLE_TAP_MS 300
static AppTimer *s_tap_menu_timer;
static char s_tap_menu_id[MAX_ID];  // the tapped message, by id: indexes shift when one is deleted

static void cancel_tap_menu(void) {
  cancel_timer(&s_tap_menu_timer);
  s_tap_menu_id[0] = '\0';
}
static uint8_t *s_image_buffer;
static uint8_t *s_avatar_buffer;
static uint16_t s_image_buffer_capacity;
static uint16_t s_avatar_buffer_capacity;
static char s_image_message_id[MAX_ID];
static char s_selected_image_focus_id[MAX_ID];
static char s_avatar_chat_id[MAX_ID];
static int s_image_size;
static int s_image_received;
static int s_image_expected_offset;
static int s_image_transfer_id;
static bool s_image_packed;       // chunks are PackBits-compressed
static int s_image_written;       // bitmap bytes filled so far
static int16_t s_pack_literal;    // PackBits: literal bytes still to copy
static int16_t s_pack_repeat;     // PackBits: run waiting for its byte
static uint16_t s_image_budget_cap;  // lowered after an out-of-memory retry
static bool s_page_load_waiting;     // user asked for older/newer messages; photos wait
static int s_avatar_size;
static int s_avatar_received;
static int s_avatar_expected_offset;
static int s_avatar_transfer_id;
static char (*s_canned)[CANNED_TEXT_LEN];
static char *s_pending_text;
// The message bar's draft (s_bar_draft), from Index ("send a message to …" someone not in the
// favourites): the bar shows two lines of it with a send arrow; Retry dictates it again. Kept
// while scrolling the chat.
static bool s_dictation_to_draft;  // the dictation replaces the draft instead of sending
static char s_dictation_draft_chat[MAX_ID];  // the chat whose draft it replaces
static char s_pending_edit_message_id[MAX_ID];
static char s_pending_chat_command[24];
static bool s_pending_send_as_reply;
static bool s_touch_keyboard_open;
#if TOUCH_KEYBOARD_AVAILABLE
static bool s_touch_keyboard_symbols;
static bool s_touch_keyboard_shift;
// Set when only the keyboard changed, so the chat layout need not be redone.
static bool s_kb_redraw_only;
#endif
static char s_current_chat_id[MAX_ID];
static bool s_leave_pending;  // leave_chat waits for the outbox (a draft save is going out)
static char s_current_chat_title[48];
static char s_status_text[64];
static char s_loading_text[96] = "Loading...";
static char s_chat_refresh_selected_id[MAX_ID];
static char s_chat_list_selected_id[MAX_ID];

static int s_chat_count;
static int s_message_count;
static int s_selected_chat;
static int s_selected_message;
static int s_expected_rows;
static int s_message_transfer_id;
static int s_message_stream_mode;
static int s_message_stage_count;
static int s_chat_scroll_offset;
static int s_chat_scroll_start;
static int s_chat_scroll_target;
static uint32_t s_chat_scroll_started_ms;
// Where a scroll that was cut short had got to, so the next one carries on from there.
static int s_chat_scroll_resume;
static bool s_chat_scroll_resuming;
static int s_chat_content_height;
static int s_message_scroll_direction;
static ViewState s_view_state;
static bool s_bridge_ready;
static bool s_chats_loading;
static bool s_chat_first_paint_reported;
static bool s_loading_error;
static bool s_offer_qr;
#define QR_MAX_MODULES 73
static uint8_t s_qr_bits[(QR_MAX_MODULES * QR_MAX_MODULES + 7) / 8];
static int s_qr_modules;
static bool s_chat_view_pending;
static bool s_loading_messages;
static bool s_loading_older_messages;
static bool s_touch_paging;  // older messages asked for by a drag, which carries on when they come
static AppTimer *s_loader_timer;
static uint8_t s_loader_frame;
static uint8_t s_page_miss_count;  // pages in a row that didn't hold the selected message
static bool s_loading_newer_messages;
static bool s_user_scrolled_messages;
static bool s_at_newest;
static bool s_at_oldest;
static bool s_message_stream_silent;
static int s_chat_loading_progress;
static int s_older_anchor_y;
static int s_newer_anchor_y;
static char s_older_anchor_id[MAX_ID];
static char s_newer_anchor_id[MAX_ID];
static AppTimer *s_chat_scroll_timer;
static AppTimer *s_message_timeout_timer;
static AppTimer *s_message_retry_timer;
static AppTimer *s_image_retry_timer;
static AppTimer *s_status_clear_timer;
static AppTimer *s_chat_retry_timer;
static AppTimer *s_startup_wake_timer;
static int s_message_request_attempts;
static int s_chat_request_attempts;

static void request_chats(void);
static void request_messages(const char *chat_id);
static void request_next_image(void);
static void startup_wake_timer_callback(void *data);
static void chat_retry_timer_callback(void *data);
static void clear_active_image_request(void);
static void image_retry_timer_callback(void *data);
static void request_older_messages(bool silent);
static void request_newer_messages(bool silent);
static void destroy_message_images(void);
static void release_images_for_heap(void);
static int clamp_scroll_offset(int offset);
static void main_back_click_handler(ClickRecognizerRef recognizer, void *context);
static void open_favorite(void *data);
static void send_text_message(const char *text, bool as_reply);
static void edit_selected_message(const char *text);
static void send_selected_chat_action(const char *command);
static void select_chat_row(int row, bool animated);
static void remove_chat_at(int row);
static void destroy_chat_avatars(void);
static void mask_avatar_corners(GContext *ctx, GPoint center, int radius, GColor bg_color);
static void render_messages(void);
static void preserve_stream_anchor(const char *anchor_id, int anchor_y, bool dirty);
static void show_chat_view_timer(void *data);
static bool s_open_chat_instantly;
static void message_timeout_timer_callback(void *data);
static void cancel_message_timeout(void);
static void cancel_message_retry(void);
static void schedule_message_timeout(void);
static void schedule_message_send_retry(void);
static void show_status(const char *message);
static void show_pending_bubble(const char *text);
static void status_clear_timer_callback(void *data);
static const char *default_status_text(void);
static bool send_command_with_status(const char *command, const char *chat_id, const char *text,
                                     const char *reply_to, const char *message_id, bool show_failures);
static void show_loading_text(const char *message, bool is_error);
static void click_config_provider(void *context);
static void copy_cstr(char *dest, size_t dest_size, const char *src);
static int message_bubble_width(GRect bounds);
static int message_image_top(int index, GRect bounds);
static void show_action_window(ActionMenuMode mode);
static void end_search(void);
static void start_dictation(void);
static bool phone_ready(void);
static void action_click_config_provider(void *context);
static void action_window_unload(Window *window);
#if TOUCH_KEYBOARD_AVAILABLE
static int touch_keyboard_viewport_height(GRect bounds);
static void touch_handler(const TouchEvent *event, void *context);
#endif
static bool touch_keyboard_supported(void);
static void close_touch_keyboard(void);
static void reset_touch_keyboard(void);
static void open_touch_keyboard(KeyboardMode mode, const char *target_id, const char *initial_text);
static void touch_keyboard_send_result(bool ok);
static bool touch_keyboard_has_draft(void);
static void touch_keyboard_draft_received(DictionaryIterator *iter);
#if TOUCH_KEYBOARD_AVAILABLE
static void open_emoji_grid(EmojiGridMode mode, bool as_reply);
#endif
static int find_message_index_by_id(const char *id);
static void remove_message_at(int index);
static void set_chat_scroll_offset(int offset, bool animated);
static GRect compose_rect_for_bounds(GRect bounds);
static bool selected_message_is_truncated(void);
static bool selected_message_has_context(void);
static bool has_selected_message(void);
static bool compose_target_is_selected(void);
static void recalc_message_layout(void);
static void scroll_to_bottom(bool animated);
#if TOUCH_KEYBOARD_AVAILABLE
static void touch_chat_list_follow(void);
#endif
static void go_to_bottom(void);
static void set_chat_scroll_offset(int target, bool animated);
static void set_chat_scroll_offset_quiet(int target);
static void chat_scroll_timer_callback(void *data);
static void messages_root_update_proc(Layer *layer, GContext *ctx);

static void update_canned_replies(const char *packed) {
  if (!packed || !packed[0]) {
    return;
  }

  char buffer[MAX_CANNED * CANNED_TEXT_LEN];
  copy_cstr(buffer, sizeof(buffer), packed);
  for (int i = 0; i < MAX_CANNED; i++) {
    s_canned[i][0] = '\0';
  }

  char *cursor = buffer;
  for (int i = 0; i < MAX_CANNED; i++) {
    char *separator = strchr(cursor, '|');
    if (separator) {
      *separator = '\0';
    }
    if (cursor[0]) {
      copy_cstr(s_canned[i], sizeof(s_canned[i]), cursor);
    }
    if (!separator) {
      break;
    }
    cursor = separator + 1;
  }
}

static int canned_reply_count(void) {
  int count = 0;
  for (int i = 0; i < MAX_CANNED; i++) {
    if (s_canned[i][0]) {
      count++;
    }
  }
  return PG_MAX(1, count);
}

static int utf8_expected_bytes(unsigned char lead) {
  if (lead < 0x80) {
    return 1;
  }
  if ((lead & 0xe0) == 0xc0) {
    return 2;
  }
  if ((lead & 0xf0) == 0xe0) {
    return 3;
  }
  if ((lead & 0xf8) == 0xf0) {
    return 4;
  }
  return 0;
}

static void trim_incomplete_utf8(char *text) {
  size_t len;
  size_t lead;
  int expected;
  if (!text || !text[0]) {
    return;
  }
  len = strlen(text);
  lead = len;
  while (lead > 0 && (((unsigned char)text[lead - 1] & 0xc0) == 0x80)) {
    lead--;
  }
  if (lead == len) {
    expected = utf8_expected_bytes((unsigned char)text[len - 1]);
    if (expected == 1) {
      return;
    }
    if (expected == 0 || len - 1 + (size_t)expected > len) {
      text[len - 1] = '\0';
    }
    return;
  }
  if (lead == 0) {
    text[0] = '\0';
    return;
  }
  expected = utf8_expected_bytes((unsigned char)text[lead - 1]);
  if (expected == 0 || lead - 1 + (size_t)expected > len) {
    text[lead - 1] = '\0';
  }
}

// Pebble's string helpers do not consistently protect callers from NULL input.
static void copy_cstr(char *dest, size_t dest_size, const char *src) {
  if (!dest || dest_size == 0) {
    return;
  }
  if (!src) {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, src, dest_size - 1);
  dest[dest_size - 1] = '\0';
  trim_incomplete_utf8(dest);
}

static bool text_matches_ignoring_trailing_space(const char *a, const char *b) {
  if (!a || !b) {
    return false;
  }
  size_t la = strlen(a);
  size_t lb = strlen(b);
  while (la > 0 && (a[la - 1] == ' ' || a[la - 1] == '\n')) {
    la--;
  }
  while (lb > 0 && (b[lb - 1] == ' ' || b[lb - 1] == '\n')) {
    lb--;
  }
  return la == lb && strncmp(a, b, la) == 0;
}

static char s_empty_message_string[] = "";

static void release_message_strings(Message *message);

static size_t bounded_string_len(const char *src, size_t max_size) {
  size_t len;
  if (!src || !src[0] || max_size == 0) {
    return 0;
  }
  len = strlen(src);
  if (len >= max_size) {
    len = max_size - 1;
  }
  return len;
}

static char *pack_message_string(char **cursor, const char *src, size_t len) {
  char *dest;
  if (!cursor || !*cursor || !src || !src[0] || len == 0) {
    return s_empty_message_string;
  }
  dest = *cursor;
  memcpy(dest, src, len);
  dest[len] = '\0';
  trim_incomplete_utf8(dest);
  *cursor += len + 1;
  return dest[0] ? dest : s_empty_message_string;
}

static bool set_message_strings(Message *message, const char *sender, const char *text,
                                const char *reactions, const char *meta, const char *context) {
  size_t sender_len = bounded_string_len(sender, MAX_SENDER);
  size_t text_len = bounded_string_len(text, MAX_TEXT);
  size_t reactions_len = bounded_string_len(reactions, MAX_REACTIONS);
  size_t meta_len = bounded_string_len(meta, MAX_META);
  size_t context_len = bounded_string_len(context, MAX_CONTEXT_TEXT);
  size_t total = (sender_len ? sender_len + 1 : 0) +
                 (text_len ? text_len + 1 : 0) +
                 (reactions_len ? reactions_len + 1 : 0) +
                 (meta_len ? meta_len + 1 : 0) +
                 (context_len ? context_len + 1 : 0);
  char *storage = total ? malloc(total) : NULL;
  char *cursor = storage;
  if (!message) {
    if (storage) {
      free(storage);
    }
    return false;
  }
  release_message_strings(message);
  if (total && !storage) {
    return false;
  }
  message->string_storage = storage;
  message->sender = pack_message_string(&cursor, sender, sender_len);
  message->text = pack_message_string(&cursor, text, text_len);
  message->reactions = pack_message_string(&cursor, reactions, reactions_len);
  message->meta = pack_message_string(&cursor, meta, meta_len);
  message->context = pack_message_string(&cursor, context, context_len);
  return true;
}

static void init_message_strings(Message *message) {
  if (!message) {
    return;
  }
  message->string_storage = NULL;
  message->sender = s_empty_message_string;
  message->text = s_empty_message_string;
  message->reactions = s_empty_message_string;
  message->meta = s_empty_message_string;
  message->context = s_empty_message_string;
}

static void release_message_strings(Message *message) {
  if (!message) {
    return;
  }
  if (message->string_storage) {
    free(message->string_storage);
    message->string_storage = NULL;
  }
  message->sender = s_empty_message_string;
  message->text = s_empty_message_string;
  message->reactions = s_empty_message_string;
  message->meta = s_empty_message_string;
  message->context = s_empty_message_string;
}

static void truncate_cstr_bytes(char *text, size_t size, size_t max_bytes, const char *suffix) {
  size_t suffix_len = suffix ? strlen(suffix) : 0;
  size_t cut;
  if (!text || size == 0 || strlen(text) <= max_bytes || max_bytes + 1 > size) {
    return;
  }
  cut = max_bytes > suffix_len ? max_bytes - suffix_len : 0;
  text[cut] = '\0';
  trim_incomplete_utf8(text);
  if (suffix_len && strlen(text) + suffix_len < size) {
    strncat(text, suffix, size - strlen(text) - 1);
  }
}

static char *tuple_cstring(DictionaryIterator *iter, uint32_t key) {
  Tuple *tuple = dict_find(iter, key);
  return tuple ? tuple->value->cstring : NULL;
}

static int tuple_int(DictionaryIterator *iter, uint32_t key, int fallback) {
  Tuple *tuple = dict_find(iter, key);
  return tuple ? (int)tuple->value->int32 : fallback;
}

// Round screens need a narrower central column so rows stay clear of corners.
static GRect round_safe_rect(GRect bounds) {
  if (!ROUND_UI) {
    return bounds;
  }
  int inset = bounds.size.w >= 180 ? 28 : 22;
  return GRect(inset, bounds.origin.y, bounds.size.w - (inset * 2), bounds.size.h);
}

static int round_isqrt(int value) {
  int root = value;
  for (int next = (root + 1) / 2; value > 0 && next < root; next = (root + value / root) / 2) {
    root = next;
  }
  return value > 0 ? root : 0;
}

// Round screens: the x range inside the circle, shrunk by `inset` px, for the
// rows y0..y1 of the layer being drawn (the row nearest the bezel counts).
// Callers set s_round_org to that layer's screen position first, so what is
// drawn follows the curve as the layer scrolls or slides.
static GPoint s_round_org;
// While the messages are drawn for chat_warp (round), they are a flat column: the warp, not
// each bubble, keeps them inside the circle.
static bool s_round_flat;

static void round_span(int y0, int y1, int inset, int *left, int *right) {
  if (s_round_flat) {
    *left = inset - s_round_org.x;
    *right = PBL_DISPLAY_WIDTH - inset - s_round_org.x;
    return;
  }
  int r = PBL_DISPLAY_WIDTH / 2 - inset;
  int mid = PBL_DISPLAY_HEIGHT / 2 - s_round_org.y;
  int dy = PG_MAX(PG_MAX(y0, y1) - mid, mid - PG_MIN(y0, y1));
  int half = dy < r ? round_isqrt(r * r - dy * dy) : 0;
  *left = PBL_DISPLAY_WIDTH / 2 - s_round_org.x - half;
  *right = *left + half * 2;
}

// r narrowed to the part inside the circle (shrunk by inset) over its rows.
static GRect round_clamp(GRect r, int inset) {
  int left;
  int right;
  round_span(r.origin.y, r.origin.y + r.size.h, inset, &left, &right);
  int x = PG_MAX(r.origin.x, left);
  return GRect(x, r.origin.y, PG_MAX(0, PG_MIN(r.origin.x + r.size.w, right) - x), r.size.h);
}

// graphics_fill_rect with rounded corners, kept inside the circle: drawn a row at a
// time, following the bezel, so a bubble shortens smoothly as it scrolls past. Row by row
// also keeps the corners round when the chat warp renders a bubble across two strips (the
// system's rounded rect squares off corners a clip edge cuts through).
static void round_fill(GContext *ctx, GRect r, int radius, GCornerMask corners, int inset, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  for (int y = 0; y < r.size.h; y++) {
    const int h = 1;
    int left;
    int right;
    round_span(r.origin.y + y, r.origin.y + y + h, inset, &left, &right);
    const bool top = y < r.size.h - y - h;
    int edge = PG_MIN(y, r.size.h - y - h);
    int cut = edge < radius ? radius - round_isqrt(radius * radius - (radius - edge - 1) * (radius - edge - 1)) : 0;
    if (corners & (top ? GCornerTopLeft : GCornerBottomLeft)) {
      left = PG_MAX(left, r.origin.x + cut);
    } else {
      left = PG_MAX(left, r.origin.x);
    }
    if (corners & (top ? GCornerTopRight : GCornerBottomRight)) {
      right = PG_MIN(right, r.origin.x + r.size.w - cut);
    } else {
      right = PG_MIN(right, r.origin.x + r.size.w);
    }
    if (right > left) {
      graphics_fill_rect(ctx, GRect(left, r.origin.y + y, right - left, h), 0, GCornerNone);
    }
  }
}

static int chat_content_y(void) {
  return HEADER_H;
}

static __attribute__((unused)) uint32_t tg_hash(const char *text) {
  uint32_t hash = 5381;
  while (text && *text) {
    hash = ((hash << 5) + hash) + (uint8_t)*text++;
  }
  return hash;
}

// What each chat row showed before a refresh, to tell which chats have news afterwards.
static uint32_t s_chat_seen[MAX_CHATS];
static int s_chat_seen_count;

static uint32_t chat_row_hash(const Chat *chat) {
  return tg_hash(chat->id) * 31 + tg_hash(chat->preview) + (uint32_t)chat->unread_count;
}

static bool chat_row_was_seen(const Chat *chat) {
  const uint32_t hash = chat_row_hash(chat);
  for (int i = 0; i < s_chat_seen_count; i++) {
    if (s_chat_seen[i] == hash) {
      return true;
    }
  }
  return false;
}

static GColor tg_avatar_color(const char *key) {
  return PBL_IF_COLOR_ELSE((GColor){.argb = TG_AVATAR_COLORS[tg_hash(key) % 7]}, GColorBlack);
}

static __attribute__((unused)) GColor tg_name_color(const char *name) {
  return PBL_IF_COLOR_ELSE((GColor){.argb = TG_NAME_COLORS[tg_hash(name) % 7]}, TG_TEXT);
}

static void chat_initials(const char *title, char *initials, size_t initials_size) {
  initials[0] = '\0';
  if (!title || !title[0] || initials_size < 2) {
    return;
  }
  initials[0] = toupper((unsigned char)title[0]);
  initials[1] = '\0';
  for (int i = 1; title[i] && initials_size > 2; i++) {
    if (title[i - 1] == ' ' && title[i] != ' ') {
      initials[1] = toupper((unsigned char)title[i]);
      initials[2] = '\0';
      return;
    }
  }
}

static Message *find_message_by_id(const char *message_id) {
  if (!message_id || !message_id[0]) {
    return NULL;
  }
  for (int i = 0; i < s_message_count; i++) {
    if (strcmp(s_messages[i].id, message_id) == 0) {
      return &s_messages[i];
    }
  }
  return NULL;
}

static Message *find_message_by_image_token(const char *image_token) {
  if (!image_token || !image_token[0]) {
    return NULL;
  }
  for (int i = 0; i < s_message_count; i++) {
    if (strcmp(s_messages[i].image_token, image_token) == 0) {
      return &s_messages[i];
    }
  }
  return find_message_by_id(image_token);
}

static int find_message_index_by_id(const char *message_id) {
  if (!message_id || !message_id[0]) {
    return -1;
  }
  for (int i = 0; i < s_message_count; i++) {
    if (strcmp(s_messages[i].id, message_id) == 0) {
      return i;
    }
  }
  return -1;
}

static bool message_stage_contains_id(const char *message_id) {
  if (!s_message_stage || !message_id || !message_id[0]) {
    return false;
  }
  for (int i = 0; i < s_message_stage_count; i++) {
    if (strcmp(s_message_stage[i].id, message_id) == 0) {
      return true;
    }
  }
  return false;
}

static int find_chat_index_by_id(const char *chat_id) {
  if (!chat_id || !chat_id[0]) {
    return -1;
  }
  for (int i = 0; i < s_chat_count; i++) {
    if (strcmp(s_chats[i].id, chat_id) == 0) {
      return i;
    }
  }
  return -1;
}

static void free_image_transfer_buffer(void) {
  if (s_image_buffer) {
    free(s_image_buffer);
    s_image_buffer = NULL;
  }
  s_image_buffer_capacity = 0;
}

static void free_avatar_transfer_buffer(void) {
  if (s_avatar_buffer) {
    free(s_avatar_buffer);
    s_avatar_buffer = NULL;
  }
  s_avatar_buffer_capacity = 0;
}

static bool ensure_transfer_buffer(uint8_t **buffer, uint16_t *capacity,
                                   int size, int max_size) {
  if (size <= 0 || size > max_size) {
    return false;
  }
  if (*buffer && *capacity >= size) {
    return true;
  }
  if (*buffer) {
    free(*buffer);
    *buffer = NULL;
    *capacity = 0;
  }
  *buffer = malloc(size);
  if (!*buffer) {
    return false;
  }
  *capacity = (uint16_t)size;
  return true;
}


static bool ensure_avatar_transfer_buffer(int image_size) {
  return ensure_transfer_buffer(&s_avatar_buffer, &s_avatar_buffer_capacity,
                                image_size, MAX_AVATAR_BYTES);
}

static void reset_image_transfer_state(void) {
  free_image_transfer_buffer();
  s_image_message_id[0] = '\0';
  s_image_size = 0;
  s_image_received = 0;
  s_image_expected_offset = 0;
  s_image_transfer_id = 0;
  s_image_packed = false;
  s_image_written = 0;
  s_pack_literal = 0;
  s_pack_repeat = 0;
}

// Appends one received chunk to the photo bitmap. Packed chunks are PackBits
// runs (what the phone uses for flat screenshots) and are expanded straight
// into the bitmap, so compression costs no extra memory.
static bool image_write_chunk(const uint8_t *data, int length) {
  if (!s_image_packed) {
    if (s_image_written + length > s_image_size) {
      return false;
    }
    memcpy(s_image_buffer + s_image_written, data, length);
    s_image_written += length;
    return true;
  }
  for (int i = 0; i < length; i++) {
    uint8_t byte = data[i];
    if (s_pack_literal > 0) {
      if (s_image_written >= s_image_size) {
        return false;
      }
      s_image_buffer[s_image_written++] = byte;
      s_pack_literal--;
    } else if (s_pack_repeat > 0) {
      if (s_image_written + s_pack_repeat > s_image_size) {
        return false;
      }
      memset(s_image_buffer + s_image_written, byte, s_pack_repeat);
      s_image_written += s_pack_repeat;
      s_pack_repeat = 0;
    } else {
      int8_t header = (int8_t)byte;
      if (header >= 0) {
        s_pack_literal = header + 1;
      } else if (header != -128) {
        s_pack_repeat = 1 - header;
      }
    }
  }
  return true;
}

static void reset_avatar_transfer_state(void) {
  free_avatar_transfer_buffer();
  s_avatar_chat_id[0] = '\0';
  s_avatar_size = 0;
  s_avatar_received = 0;
  s_avatar_expected_offset = 0;
  s_avatar_transfer_id = 0;
}

static bool transfer_chunk_fits(int offset, int length, int expected_offset,
                                int total_size, int capacity) {
  return offset == expected_offset &&
         offset >= 0 &&
         length > 0 &&
         total_size > 0 &&
         capacity >= total_size &&
         offset <= total_size &&
         length <= total_size - offset &&
         length <= capacity - offset;
}

static bool ensure_full_text_body(void) {
  if (s_full_text_body) {
    return true;
  }
  s_full_text_body = malloc(MAX_FULL_TEXT);
  if (!s_full_text_body) {
    return false;
  }
  s_full_text_body[0] = '\0';
  return true;
}

static void free_full_text_body(void) {
  if (s_full_text_body) {
    free(s_full_text_body);
    s_full_text_body = NULL;
  }
}

static void init_chat_strings(Chat *chat) {
  if (!chat) {
    return;
  }
  chat->string_storage = NULL;
  chat->title = s_empty_message_string;
  chat->preview = s_empty_message_string;
}

static void release_chat_strings(Chat *chat) {
  if (!chat) {
    return;
  }
  if (chat->string_storage) {
    free(chat->string_storage);
    chat->string_storage = NULL;
  }
  chat->title = s_empty_message_string;
  chat->preview = s_empty_message_string;
}

static bool set_chat_strings(Chat *chat, const char *title, const char *preview) {
  size_t title_len = bounded_string_len(title, 48);
  size_t preview_len = bounded_string_len(preview, 72);
  size_t total = (title_len ? title_len + 1 : 0) +
                 (preview_len ? preview_len + 1 : 0);
  char *storage = total ? malloc(total) : NULL;
  char *cursor = storage;
  if (!chat) {
    if (storage) {
      free(storage);
    }
    return false;
  }
  release_chat_strings(chat);
  if (total && !storage) {
    return false;
  }
  chat->string_storage = storage;
  chat->title = pack_message_string(&cursor, title, title_len);
  chat->preview = pack_message_string(&cursor, preview, preview_len);
  return true;
}

// Chat row meta from the phone: "time|flags", flags g=group m=muted p=pinned 1/2=receipt.
static void parse_chat_meta(Chat *chat, const char *meta) {
  chat->time[0] = '\0';
  chat->flags = 0;
  chat->receipt = 0;
  if (!meta) {
    return;
  }
  const char *bar = strchr(meta, '|');
  size_t time_len = bar ? (size_t)(bar - meta) : strlen(meta);
  if (time_len >= sizeof(chat->time)) {
    time_len = sizeof(chat->time) - 1;
  }
  memcpy(chat->time, meta, time_len);
  chat->time[time_len] = '\0';
  for (const char *f = bar ? bar + 1 : ""; *f; f++) {
    if (*f == 'g') chat->flags |= CHAT_FLAG_GROUP;
    else if (*f == 'm') chat->flags |= CHAT_FLAG_MUTED;
    else if (*f == 'p') chat->flags |= CHAT_FLAG_PINNED;
    else if (*f == 'c') chat->flags |= CHAT_FLAG_CONTACT;
    else if (*f == '1' || *f == '2') chat->receipt = (uint8_t)(*f - '0');
  }
}

static void destroy_header_avatar(void);

static void destroy_chat_avatar(Chat *chat) {
  if (chat && chat->avatar_bitmap) {
    destroy_header_avatar();
    gbitmap_destroy(chat->avatar_bitmap);
    chat->avatar_bitmap = NULL;
  }
  if (chat && chat->avatar_data) {
    free(chat->avatar_data);
    chat->avatar_data = NULL;
  }
}

static void clear_chat_slot(Chat *chat) {
  if (!chat) {
    return;
  }
  destroy_chat_avatar(chat);
  release_chat_strings(chat);
  memset(chat, 0, sizeof(Chat));
  init_chat_strings(chat);
}

static void preserve_chat_avatar(Chat *chat, const char *incoming_id) {
  int existing_index = find_chat_index_by_id(incoming_id);
  if (existing_index < 0 || &s_chats[existing_index] == chat) {
    destroy_chat_avatar(chat);
    return;
  }
  Chat moved = s_chats[existing_index];
  s_chats[existing_index] = *chat;
  *chat = moved;
}

static void destroy_chat_avatars(void) {
  for (int i = 0; i < MAX_CHATS; i++) {
    clear_chat_slot(&s_chats[i]);
  }
  s_chat_count = 0;
  reset_avatar_transfer_state();
}

static void mask_avatar_corners(GContext *ctx, GPoint center, int radius, GColor bg_color) {
  graphics_context_set_fill_color(ctx, bg_color);
  for (int y = -radius; y <= radius; y++) {
    int extent = 0;
    while ((extent + 1) * (extent + 1) + y * y <= radius * radius) {
      extent++;
    }
    int corner_w = radius - extent;
    if (corner_w > 0) {
      graphics_fill_rect(ctx, GRect(center.x - radius, center.y + y, corner_w, 1), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(center.x + extent + 1, center.y + y, corner_w, 1), 0, GCornerNone);
    }
  }
}

static void destroy_message_bitmap(Message *message) {
  if (message && message->image_bitmap) {
    gbitmap_destroy(message->image_bitmap);
    message->image_bitmap = NULL;
    message->image_requested = false;
  }
  if (message && message->image_data) {
    free(message->image_data);
    message->image_data = NULL;
  }
}

static void destroy_message_images(void) {
  cancel_timer(&s_image_retry_timer);
  for (int i = 0; i < MAX_MESSAGES; i++) {
    destroy_message_bitmap(&s_messages[i]);
    s_messages[i].image_requested = false;
    s_messages[i].image_failed = false;
    s_messages[i].image_error[0] = '\0';
    s_messages[i].image_progress = 0;
  }
  reset_image_transfer_state();
  s_selected_image_focus_id[0] = '\0';
}

static void schedule_image_retry(void) {
  if (!s_image_retry_timer && s_view_state == ViewStateChat && s_messages_root) {
    s_image_retry_timer = app_timer_register(IMAGE_COMMAND_RETRY_MS, image_retry_timer_callback, NULL);
  }
}

static void schedule_image_timeout(uint32_t timeout_ms) {
  if (s_image_retry_timer) {
    app_timer_cancel(s_image_retry_timer);
  }
  if (s_view_state == ViewStateChat && s_messages_root) {
    s_image_retry_timer = app_timer_register(timeout_ms, image_retry_timer_callback, NULL);
  } else {
    s_image_retry_timer = NULL;
  }
}

static void schedule_image_prepare_timeout(void) {
  schedule_image_timeout(IMAGE_PREPARE_STALL_MS);
}

static void schedule_image_transfer_timeout(void) {
  schedule_image_timeout(IMAGE_TRANSFER_STALL_MS);
}

static bool message_needs_image(Message *message) {
  return message && message->image_placeholder && message->image_token[0] &&
         !message->image_requested && !message->image_failed && !message->image_bitmap;
}

// Size a photo is drawn at inside a bubble whose photo area is frame_w wide.
// Photos fill the bubble width (never upscaled); tall screenshots keep that
// width until they reach IMAGE_TALL_MAX_H, so their text stays readable. The
// size only depends on the photo's original shape, so the bubble never jumps
// when the photo arrives. The phone renders to exactly this size.
static GSize message_image_plan(const Message *message, int frame_w) {
  frame_w = PG_MAX(24, frame_w);
  if (!message || message->image_width == 0 || message->image_height == 0) {
    return GSize(frame_w, PG_MIN(frame_w, IMAGE_THUMB_SIZE));
  }
  int w = PG_MIN(frame_w, (int)message->image_width);
  int h = (int)(((int32_t)message->image_height * w) / message->image_width);
  if (h > IMAGE_TALL_MAX_H) {
    h = IMAGE_TALL_MAX_H;
    w = (int)(((int32_t)message->image_width * h) / message->image_height);
  }
  return GSize(PG_MAX(24, w), PG_MAX(24, h));
}

static int message_index_from_ptr(Message *message) {
  if (!message || message < s_messages || message >= s_messages + MAX_MESSAGES) {
    return -1;
  }
  return (int)(message - s_messages);
}

static void set_message_image_error(Message *message, const char *error) {
  if (!message) {
    return;
  }
  copy_cstr(message->image_error, sizeof(message->image_error), error && error[0] ? error : "Photo failed");
  message->image_progress = 0;
}

static void set_message_image_progress(Message *message, int percent) {
  if (!message) {
    return;
  }
  percent = PG_MAX(0, PG_MIN(100, percent));
  if (percent > message->image_progress) {
    message->image_progress = (uint8_t)percent;
  }
}

static int visible_message_height(GRect bounds) {
#if TOUCH_KEYBOARD_AVAILABLE
  return s_touch_keyboard_open ? touch_keyboard_viewport_height(bounds) : bounds.size.h;
#else
  return bounds.size.h;
#endif
}

static int message_image_top(int index, GRect bounds) {
  (void)bounds;
  return s_message_y[index] + s_box[index].image_y;
}

// How far (in pixels) a photo is from the part of the chat on screen; 0 when
// any of it is visible. While the chat is animating a scroll this measures from
// where the scroll is heading, so photos load for where the user is going.
static int message_image_distance(int index) {
  if (index < 0 || index >= s_message_count || !s_messages[index].image_placeholder || !s_messages_root) {
    return 0x3fffffff;
  }
  GRect bounds = layer_get_bounds(s_messages_root);
  int view_top = s_chat_scroll_timer ? clamp_scroll_offset(s_chat_scroll_target) : s_chat_scroll_offset;
  int view_bottom = view_top + visible_message_height(bounds);
  int top = message_image_top(index, bounds);
  int bottom = top + s_box[index].image_h;
  if (bottom < view_top) {
    return view_top - bottom;
  }
  if (top > view_bottom) {
    return top - view_bottom;
  }
  return 0;
}

static bool message_image_near_viewport(int index, int margin) {
  return message_image_distance(index) <= margin;
}

static bool message_image_scrolls_tall(int index, GRect bounds, int margin) {
  if (index < 0 || index >= s_message_count || !s_messages[index].image_placeholder) {
    return false;
  }
  return s_box[index].image_h > visible_message_height(bounds) - (margin * 2);
}

static int message_image_edge_scroll_target(int index, GRect bounds, int margin, bool align_top) {
  int image_top = message_image_top(index, bounds);
  int image_bottom = image_top + s_box[index].image_h;
  return align_top ? image_top - margin : image_bottom + margin - visible_message_height(bounds);
}

static size_t image_heap_free(void) {
#ifdef _PBL_API_EXISTS_heap_bytes_free
  return heap_bytes_free();
#else
  return MAX_IMAGE_BYTES + IMAGE_HEAP_RESERVE;
#endif
}

// Heap a new photo may use without eating into the reserve.
static size_t image_heap_available(void) {
  size_t free_bytes = image_heap_free();
  return free_bytes > IMAGE_HEAP_RESERVE ? free_bytes - IMAGE_HEAP_RESERVE : 0;
}

#if IMAGE_DIAG_LOGS
static unsigned image_diag_heap_free(void) {
  return (unsigned)image_heap_free();
}
#endif

static bool message_is_gif(const Message *message) {
  return message && strncmp(message->text, "[GIF", 4) == 0;
}

// Frees the loaded photo farthest from the screen, but only one that is
// farther away than min_distance and not on screen. Photos are only unloaded
// to make room for a photo closer to the screen, or when the heap runs low, so
// two photos can never keep evicting each other.
static bool evict_farthest_image(int min_distance) {
  int farthest_index = -1;
  int farthest_distance = min_distance;
  for (int i = 0; i < s_message_count; i++) {
    if (!s_messages[i].image_bitmap) {
      continue;
    }
    int distance = message_image_distance(i);
    if (distance > farthest_distance) {
      farthest_distance = distance;
      farthest_index = i;
    }
  }
  if (farthest_index < 0) {
    return false;
  }
  IMAGE_DIAG("PGIMG evict index=%d distance=%d heap=%u", farthest_index, farthest_distance,
             image_diag_heap_free());
  destroy_message_bitmap(&s_messages[farthest_index]);
  return true;
}

// Keeps the reserve free: if something else (a menu, new messages) has used
// heap, drop off-screen photos until the reserve is back.
static void release_images_for_heap(void) {
  while (image_heap_free() < IMAGE_HEAP_RESERVE && evict_farthest_image(0)) {
  }
}

// Before asking for a page of messages: free off-screen photos (never the ones
// on screen) until the page is sure to fit.
static void make_room_for_message_page(void) {
  while (image_heap_free() < IMAGE_HEAP_RESERVE + PAGE_LOAD_HEADROOM && evict_farthest_image(0)) {
  }
}

// Bytes a photo would take at the best quality this watch can show.
static size_t image_ideal_bytes(int index) {
  size_t w = (size_t)s_box[index].image_w;
  size_t h = (size_t)s_box[index].image_h;
  size_t row = BW_UI ? ((w + 31) / 32) * 4 : (w * IMAGE_BITS_PER_PIXEL + 7) / 8;
  return PG_MIN((size_t)MAX_IMAGE_BYTES, row * h + 32);
}

// How many bytes the phone may send for this photo. Makes room first by
// unloading photos that are farther from the screen than this one.
static size_t image_budget_for(int index) {
  size_t wanted = image_ideal_bytes(index);
  int distance = message_image_distance(index);
  size_t available = image_heap_available();
  while (available < wanted && evict_farthest_image(distance)) {
    available = image_heap_available();
  }
  return PG_MIN(available, (size_t)MAX_IMAGE_BYTES);
}

static void prepare_selected_image_request(void) {
  if (s_selected_message < 0 || s_selected_message >= s_message_count ||
      !s_messages[s_selected_message].image_placeholder ||
      !s_messages[s_selected_message].image_token[0]) {
    s_selected_image_focus_id[0] = '\0';
    return;
  }
  Message *selected = &s_messages[s_selected_message];
  if (strcmp(s_selected_image_focus_id, selected->image_token) != 0) {
    copy_cstr(s_selected_image_focus_id, sizeof(s_selected_image_focus_id), selected->image_token);
    // Selecting a photo that failed gives it a fresh try.
    if (selected->image_failed) {
      selected->image_failed = false;
      selected->image_error[0] = '\0';
      selected->image_progress = 0;
      selected->image_retry_level = 0;
    }
  }
}

static void clear_active_image_request(void) {
  Message *message = find_message_by_image_token(s_image_message_id);
  cancel_timer(&s_image_retry_timer);
  if (message) {
    message->image_requested = false;
    if (!message->image_bitmap && !message->image_failed) {
      message->image_error[0] = '\0';
      message->image_progress = 0;
    }
  }
  reset_image_transfer_state();
}

static bool click_is_repeating(ClickRecognizerRef recognizer) {
  return recognizer && click_number_of_clicks_counted(recognizer) > 1;
}

// Next photo to fetch: the one closest to the screen, preferring the selected
// message when several are on screen.
static int find_best_image_candidate(void) {
  int best_index = -1;
  int best_score = 0x3fffffff;
  for (int i = 0; i < s_message_count; i++) {
    if (!message_needs_image(&s_messages[i])) {
      continue;
    }
    int distance = message_image_distance(i);
    if (distance > IMAGE_LOAD_MARGIN) {
      continue;
    }
    int score = distance * 4 + (i == s_selected_message ? 0 : 1);
    if (score < best_score) {
      best_score = score;
      best_index = i;
    }
  }
  return best_index;
}

static bool send_command_with_status(const char *command, const char *chat_id, const char *text,
                                     const char *reply_to, const char *message_id, bool show_failures) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK || !iter) {
    if (show_failures) {
      show_status("Bridge busy");
    }
    return false;
  }

  bool ok = dict_write_cstring(iter, MESSAGE_KEY_Command, command) == DICT_OK;
  if (chat_id) {
    ok = ok && dict_write_cstring(iter, MESSAGE_KEY_ChatId, chat_id) == DICT_OK;
  }
  if (text) {
    ok = ok && dict_write_cstring(iter, MESSAGE_KEY_Text, text) == DICT_OK;
  }
  if (reply_to) {
    ok = ok && dict_write_cstring(iter, MESSAGE_KEY_ReplyTo, reply_to) == DICT_OK;
  }
  if (text && message_id && strcmp(command, "edit_message") == 0) {
    ok = ok && dict_write_cstring(iter, MESSAGE_KEY_EditMessageId, message_id) == DICT_OK;
  }
  if (message_id) {
    ok = ok && dict_write_cstring(iter, MESSAGE_KEY_MessageId, message_id) == DICT_OK;
  }
  if (ok) {
    dict_write_end(iter);
    ok = app_message_outbox_send() == APP_MSG_OK;
  }
  if (!ok && show_failures) {
    show_status("Command send fail");
  }
  return ok;
}

static bool send_command(const char *command, const char *chat_id, const char *text,
                         const char *reply_to, const char *message_id) {
  return send_command_with_status(command, chat_id, text, reply_to, message_id, true);
}

static const char *default_status_text(void) {
  if (s_view_state == ViewStateChat && s_current_chat_title[0]) {
    return s_current_chat_title;
  }
  return "Chat";
}

static bool status_message_should_persist(const char *message) {
  if (!message || !message[0] || strcmp(message, default_status_text()) == 0) {
    return true;
  }
  return strncmp(message, "Loading", 7) == 0 ||
         strncmp(message, "Connecting", 10) == 0 ||
         strncmp(message, "Fetching", 8) == 0 ||
         strncmp(message, "Requesting", 10) == 0;
}

static void cancel_status_clear(void) {
  cancel_timer(&s_status_clear_timer);
}

static void schedule_status_clear(const char *message) {
  cancel_status_clear();
  if (!s_chats_loading && !status_message_should_persist(message)) {
    s_status_clear_timer = app_timer_register(STATUS_CLEAR_MS, status_clear_timer_callback, NULL);
  }
}

static void status_clear_timer_callback(void *data) {
  s_status_clear_timer = NULL;
  if (!s_chats_loading) {
    show_status(default_status_text());
  }
}

static void show_status(const char *message) {
  const char *shown = s_chats_loading ? "Chat" :
                      (message && message[0] ? message : default_status_text());
  copy_cstr(s_status_text, sizeof(s_status_text), shown);
  if (s_header_layer) {
    layer_mark_dirty(s_header_layer);
    schedule_status_clear(shown);
  }
}

static void show_loading_text(const char *message, bool is_error) {
  copy_cstr(s_loading_text, sizeof(s_loading_text), message && message[0] ? message : "Loading...");
  s_loading_error = is_error;
  if (s_chat_menu) {
    menu_layer_reload_data(s_chat_menu);
  }
}

static int progress_percent(int current, int total) {
  if (total <= 0) {
    return current > 0 ? 100 : 0;
  }
  return PG_MAX(0, PG_MIN(100, (current * 100) / total));
}

static int image_loading_phase_percent(const char *detail) {
  if (!detail || !detail[0]) {
    return 5;
  }
  if (strcmp(detail, "Waiting phone") == 0) {
    return 5;
  }
  if (strcmp(detail, "Preparing") == 0) {
    return 10;
  }
  if (strcmp(detail, "Downloading") == 0) {
    return 15;
  }
  if (strcmp(detail, "Decoding") == 0) {
    return 20;
  }
  if (strcmp(detail, "Sending") == 0) {
    return 25;
  }
  if (strcmp(detail, "Receiving") == 0) {
    return 25;
  }
  return 10;
}

static int chat_loading_percent(void) {
  int percent;
  if (s_expected_rows > 0) {
    percent = 90 + ((PG_MAX(0, PG_MIN(s_chat_count, s_expected_rows)) * 10) / s_expected_rows);
  } else if (strcmp(s_loading_text, "Connecting...") == 0) {
    percent = 15;
  } else if (strcmp(s_loading_text, "Fetching chats...") == 0) {
    percent = 45;
  } else if (strcmp(s_loading_text, "Sending chats...") == 0) {
    percent = 90;
  } else {
    percent = 8;
  }
  if (percent > s_chat_loading_progress) {
    s_chat_loading_progress = percent;
  }
  return s_chat_loading_progress;
}

// ---- Telegram-style drawing helpers ----------------------------------------

static GFont font_bold18(void) { return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD); }
static GFont font_18(void) { return fonts_get_system_font(FONT_KEY_GOTHIC_18); }
static GFont font_bold14(void) { return fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD); }
static GFont font_14(void) { return fonts_get_system_font(FONT_KEY_GOTHIC_14); }

static int text_width(const char *text, GFont font) {
  if (!text || !text[0]) {
    return 0;
  }
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, 400, 40),
                                               GTextOverflowModeTrailingEllipsis,
                                               GTextAlignmentLeft).w;
}

static void fill_polygon(GContext *ctx, GPoint *points, int count, GColor color) {
  GPathInfo info = {(uint32_t)count, points};
  GPath *path = gpath_create(&info);
  if (!path) {
    return;
  }
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

// Beeper's mark (a speech-bubble "B", from beeper.com's logo), drawn in a (16 * scale / 4) box
// where the plane from the Telegram logo used to be: `color` fills it, `fold` cuts the bubble.
static void draw_beeper_mark(GContext *ctx, GPoint origin, int scale, GColor color, GColor fold) {
  #define PB(px, py) GPoint(origin.x + ((px) * scale) / 40, origin.y + ((py) * scale) / 40)
  GPoint body[19] = {PB(0, 45), PB(5, 23), PB(20, 7), PB(45, 0), PB(82, 0), PB(107, 7), PB(122, 23),
                     PB(127, 45), PB(122, 69), PB(113, 78), PB(105, 86), PB(120, 98), PB(127, 120),
                     PB(120, 144), PB(104, 157), PB(85, 160), PB(22, 160), PB(22, 84), PB(9, 72)};
  GPoint hole[15] = {PB(13, 45), PB(18, 27), PB(29, 16), PB(45, 13), PB(82, 13), PB(100, 19),
                     PB(111, 31), PB(115, 45), PB(111, 58), PB(105, 69), PB(52, 121), PB(60, 78),
                     PB(45, 78), PB(29, 73), PB(18, 63)};
  fill_polygon(ctx, body, 19, color);
  fill_polygon(ctx, hole, 15, fold);
  #undef PB
}

// The mark is 127 x 160 units of scale / 40 px; centred on c.
static void draw_beeper_mark_centered(GContext *ctx, GPoint c, int scale, GColor color, GColor fold) {
  draw_beeper_mark(ctx, GPoint(c.x - (127 * scale) / 80, c.y - (160 * scale) / 80), scale, color, fold);
}

static void draw_back_chevron(GContext *ctx, GPoint center, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, GPoint(center.x + 3, center.y - 6), GPoint(center.x - 3, center.y));
  graphics_draw_line(ctx, GPoint(center.x - 3, center.y), GPoint(center.x + 3, center.y + 6));
  graphics_context_set_stroke_width(ctx, 1);
}

// Telegram read receipts: one tick = sent, two = read.
static void draw_ticks(GContext *ctx, int x, int y, int count, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  for (int i = 0; i < count; i++) {
    int ox = x + i * 4;
    graphics_draw_line(ctx, GPoint(ox, y + 4), GPoint(ox + 2, y + 6));
    graphics_draw_line(ctx, GPoint(ox + 2, y + 6), GPoint(ox + 7, y + 1));
  }
}

static int ticks_width(int count) {
  return count <= 0 ? 0 : (count == 1 ? 8 : 12);
}

// Coloured circle with initials, or the chat photo when one has loaded.
// The chat header shows the avatar smaller than the chat list does (Pebble Time
// 2). Keep one scaled-down copy of the open chat's avatar for it.
static GBitmap *s_header_avatar;
static GBitmap *s_header_avatar_source;

static void destroy_header_avatar(void) {
  if (s_header_avatar) {
    gbitmap_destroy(s_header_avatar);
    s_header_avatar = NULL;
  }
  s_header_avatar_source = NULL;
}

// Area-averaged downscale of a 4-bit palettized avatar: each output pixel takes
// the palette colour closest to the average of the source pixels it covers.
static GBitmap *scale_avatar(GBitmap *source, int size) {
  if (!source || gbitmap_get_format(source) != GBitmapFormat4BitPalette) {
    return NULL;
  }
  GSize src = gbitmap_get_bounds(source).size;
  GColor *palette = gbitmap_get_palette(source);
  GBitmap *out = gbitmap_create_blank_with_palette(GSize(size, size), GBitmapFormat4BitPalette, palette, false);
  if (!out) {
    return NULL;
  }
  uint8_t *src_data = gbitmap_get_data(source);
  uint8_t *dst_data = gbitmap_get_data(out);
  int src_row = gbitmap_get_bytes_per_row(source);
  int dst_row = gbitmap_get_bytes_per_row(out);
  for (int y = 0; y < size; y++) {
    int y0 = (y * src.h) / size;
    int y1 = PG_MAX(y0 + 1, ((y + 1) * src.h) / size);
    for (int x = 0; x < size; x++) {
      int x0 = (x * src.w) / size;
      int x1 = PG_MAX(x0 + 1, ((x + 1) * src.w) / size);
      int r = 0, g = 0, b = 0, n = 0;
      for (int sy = y0; sy < y1; sy++) {
        for (int sx = x0; sx < x1; sx++) {
          uint8_t byte = src_data[sy * src_row + sx / 2];
          GColor c = palette[(sx & 1) ? (byte & 0x0f) : (byte >> 4)];
          r += c.r;
          g += c.g;
          b += c.b;
          n++;
        }
      }
      int best = 0;
      int best_d = 0x7fff;
      for (int i = 0; i < 16; i++) {
        int dr = palette[i].r * n - r;
        int dg = palette[i].g * n - g;
        int db = palette[i].b * n - b;
        int d = dr * dr + dg * dg + db * db;
        if (d < best_d) {
          best_d = d;
          best = i;
        }
      }
      uint8_t *cell = &dst_data[y * dst_row + x / 2];
      *cell = (x & 1) ? ((*cell & 0xf0) | best) : ((*cell & 0x0f) | (best << 4));
    }
  }
  return out;
}

static GBitmap *header_avatar_for(GBitmap *source, int radius) {
  if (!source) {
    return NULL;
  }
  if (gbitmap_get_bounds(source).size.w == radius * 2) {
    return source;
  }
  if (s_header_avatar_source != source) {
    destroy_header_avatar();
    s_header_avatar = scale_avatar(source, radius * 2);
    s_header_avatar_source = source;
  }
  return s_header_avatar ? s_header_avatar : source;
}

static void draw_avatar(GContext *ctx, GPoint center, int radius, const char *key,
                        const char *title, GBitmap *photo, GColor bg) {
  if (photo) {
    GSize size = gbitmap_get_bounds(photo).size;
    GRect rect = GRect(center.x - radius, center.y - radius, radius * 2, radius * 2);
    if (size.w == rect.size.w && size.h == rect.size.h) {
      graphics_draw_bitmap_in_rect(ctx, photo, rect);
    } else {
      // Other sizes are drawn centred (cropped if bigger), never tiled.
      int sw = PG_MIN(size.w, rect.size.w);
      int sh = PG_MIN(size.h, rect.size.h);
      GBitmap *view = gbitmap_create_as_sub_bitmap(photo, GRect((size.w - sw) / 2, (size.h - sh) / 2, sw, sh));
      if (view) {
        graphics_context_set_fill_color(ctx, GColorLightGray);
        graphics_fill_circle(ctx, center, radius);
        graphics_draw_bitmap_in_rect(ctx, view, GRect(center.x - sw / 2, center.y - sh / 2, sw, sh));
        gbitmap_destroy(view);
      }
    }
    mask_avatar_corners(ctx, center, radius, bg);
    return;
  }
  char initials[3];
  chat_initials(title, initials, sizeof(initials));
  graphics_context_set_fill_color(ctx, tg_avatar_color(key && key[0] ? key : title));
  graphics_fill_circle(ctx, center, radius);
  if (BW_UI) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_circle(ctx, center, radius - 1);
  }
  GFont font = radius >= 16 ? font_bold18() : font_bold14();
  int font_h = radius >= 16 ? 18 : 14;
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, initials, font,
                     GRect(center.x - radius, center.y - (font_h / 2) - (radius >= 16 ? 4 : 3),
                           radius * 2, font_h + 4),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// Rounded count badge ("12", "99+"), or a dot when there's no count.
static int draw_badge(GContext *ctx, int right, int center_y, int count, GColor fill, GColor text) {
  graphics_context_set_fill_color(ctx, fill);
  if (count <= 0) {
    graphics_fill_circle(ctx, GPoint(right - 5, center_y), 5);
    return 10;
  }
  char label[8];
  if (count > 999) {
    copy_cstr(label, sizeof(label), "1k+");
  } else {
    snprintf(label, sizeof(label), "%d", count);
  }
  int w = PG_MAX(20, text_width(label, font_bold14()) + 11);
  GRect pill = GRect(right - w, center_y - 10, w, 20);
  graphics_fill_rect(ctx, pill, 10, GCornersAll);
  graphics_context_set_text_color(ctx, text);
  graphics_draw_text(ctx, label, font_bold14(), GRect(pill.origin.x, pill.origin.y + 1, w, 18),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  return w;
}

static bool header_has_subtitle(void) {
  return s_view_state == ViewStateChat && s_status_text[0] &&
         strcmp(s_status_text, default_status_text()) != 0;
}

static void header_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, TG_HEADER);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  bool chat = s_view_state == ViewStateChat;
  GColor on_header = TG_ON_HEADER;
  GColor sub_color = PBL_IF_COLOR_ELSE(gcolor_equal(on_header, GColorWhite) ? GColorCeleste : GColorDarkGray,
                                       on_header);
  // In the chat list, a status ("Connecting...") replaces the title, as in Telegram.
  // Search results are titled with what was searched for
  const char *idle_title = s_search_query[0] ? s_search_query : "Chat";
  const char *title = chat ? s_current_chat_title :
                      (s_status_text[0] && strcmp(s_status_text, "Chat") != 0 ? s_status_text : idle_title);
  const char *subtitle = header_has_subtitle() ? s_status_text : NULL;
  graphics_context_set_text_color(ctx, on_header);

  if (ROUND_UI) {
    // The cap of the circle is narrow: bold 18 if the title fits there, else 14, then an ellipsis.
    s_round_org = GPointZero;
    int icon_w = 0;
    int left;
    int right;
    round_span(14, 14, 3, &left, &right);
    int avail = right - left - icon_w;
    bool big = !subtitle && text_width(title, font_bold18()) <= avail;
    GFont font = big ? font_bold18() : font_bold14();
    int title_y = big ? 10 : (subtitle ? 3 : 12);
    int tw = PG_MIN(avail, text_width(title, font));
    int x = (b.size.w - (tw + icon_w)) / 2;
    graphics_draw_text(ctx, title, font, GRect(x + icon_w, title_y, tw + 2, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    if (subtitle) {
      graphics_context_set_text_color(ctx, sub_color);
      graphics_draw_text(ctx, subtitle, font_14(), GRect(0, title_y + 13, b.size.w, 16),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
    return;
  }

  int x = 8;
  int mid = b.size.h / 2;
  if (chat) {
    draw_back_chevron(ctx, GPoint(x + 2, mid), on_header);
    x += 12;
    if (b.size.h >= 30) {
      int r = PG_MIN((b.size.h / 2) - 4, CHAT_AVATAR_R);
      Chat *current = (s_selected_chat >= 0 && s_selected_chat < s_chat_count &&
                       strcmp(s_chats[s_selected_chat].id, s_current_chat_id) == 0) ?
                      &s_chats[s_selected_chat] : NULL;
      draw_avatar(ctx, GPoint(x + r, mid), r, s_current_chat_id, s_current_chat_title,
                  current && s_appearance.photos ? header_avatar_for(current->avatar_bitmap, r) : NULL, TG_HEADER);
      x += (r * 2) + 7;
    }
  }
  int w = b.size.w - x - 4;
  if (subtitle) {
    graphics_draw_text(ctx, title, font_bold18(), GRect(x, mid - 17, w, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_context_set_text_color(ctx, sub_color);
    graphics_draw_text(ctx, subtitle, font_14(), GRect(x, mid - 1, w, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  } else {
    graphics_draw_text(ctx, title, font_bold18(), GRect(x, mid - 13, w, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static uint16_t chat_menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return 1;
}

static uint16_t chat_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return s_chat_count > 0 ? s_chat_count : 1;
}

// Favourites as bubbles above the inbox, three to a row
#define FAV_COLS 3
#define FAV_ROW_H (CHAT_AVATAR_R * 2 + 24)

static int fav_strip_h(void) {
  if (s_chat_count == 0 || s_search_query[0] || s_favorite_count == 0) {
    return 0;
  }
  return ((s_favorite_count + FAV_COLS - 1) / FAV_COLS) * FAV_ROW_H + 4;
}

// Whether the buttons are on a bubble; a bubble gone (fewer favourites, a search) drops the focus
static bool fav_focused(void) {
  if (s_fav_focus >= s_favorite_count || fav_strip_h() == 0) {
    s_fav_focus = -1;
  }
  return s_fav_focus >= 0;
}

// Scrolls the focused bubble's row into view (a long strip is taller than the screen)
static void fav_reveal(void) {
  int frame_h = layer_get_frame(menu_layer_get_layer(s_chat_menu)).size.h;
  int bottom = (s_fav_focus / FAV_COLS + 1) * FAV_ROW_H + 4;
  scroll_layer_set_content_offset(menu_layer_get_scroll_layer(s_chat_menu),
                                  GPoint(0, -PG_MAX(0, bottom - frame_h)), false);
}

static void set_fav_focus(int focus) {
  s_fav_focus = (int8_t)focus;
  menu_layer_reload_data(s_chat_menu);
  if (focus >= 0) {  // after the reload, which scrolls back to the selected chat
    fav_reveal();
  } else {
    select_chat_row(menu_layer_get_selected_index(s_chat_menu).row, false);  // back on screen
  }
}

static int16_t chat_menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return fav_strip_h();
}

static GPoint fav_center(int i, int width) {
  int pitch = PG_MIN(width / FAV_COLS, CHAT_AVATAR_R * 2 + 26);
  int in_row = PG_MIN(FAV_COLS, s_favorite_count - (i / FAV_COLS) * FAV_COLS);
  int left = width / 2 - (in_row * pitch) / 2 + pitch / 2;
  return GPoint(left + (i % FAV_COLS) * pitch, (i / FAV_COLS) * FAV_ROW_H + CHAT_AVATAR_R + 4);
}

static void chat_menu_draw_header_callback(GContext *ctx, const Layer *cell_layer, uint16_t section_index,
                                           void *data) {
  GRect bounds = layer_get_bounds(cell_layer);
  graphics_context_set_fill_color(ctx, TG_LIST_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  int pitch = PG_MIN(bounds.size.w / FAV_COLS, CHAT_AVATAR_R * 2 + 26);
  for (int i = 0; i < s_favorite_count; i++) {
    GPoint c = fav_center(i, bounds.size.w);
    int chat = find_chat_index_by_id(s_favorites[i].id);
    GBitmap *photo = chat >= 0 && s_appearance.photos ? s_chats[chat].avatar_bitmap : NULL;
    const char *name = s_favorites[i].name[0] ? s_favorites[i].name : "Favorite";
    if (i == s_fav_focus) {
      graphics_context_set_fill_color(ctx, TG_LIST_SELECTED);
      graphics_fill_circle(ctx, c, CHAT_AVATAR_R + 3);
    }
    draw_avatar(ctx, c, CHAT_AVATAR_R, s_favorites[i].id, name, photo,
                i == s_fav_focus ? TG_LIST_SELECTED : TG_LIST_BG);
    graphics_context_set_text_color(ctx, i == s_fav_focus ? TG_LINK : TG_TEXT);
    graphics_draw_text(ctx, name, font_14(), GRect(c.x - pitch / 2 + 1, c.y + CHAT_AVATAR_R + 1, pitch - 2, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void clear_qr(void) {
  s_qr_modules = 0;
}

static bool qr_module_dark(int row, int col) {
  int index = (row * s_qr_modules) + col;
  return (s_qr_bits[index / 8] & (1 << (index % 8))) != 0;
}

static void mark_chat_menu_dirty(void) {
  if (s_chat_menu) {
    layer_mark_dirty(menu_layer_get_layer(s_chat_menu));
  }
}

// Login QR on a white plate. Four modules of white is the quiet zone QR
// scanners require; the plate is that margin, so a dark theme behind it
// does not eat the zone. The reserves below keep a 3px module on 144px watches.
static void draw_login_qr(GContext *ctx, GRect bounds) {
  int quiet = 4;
  int caption_h = 16;
  int span = s_qr_modules + (quiet * 2);
  int avail_w = bounds.size.w - (ROUND_UI ? 36 : 8);
  int avail_h = bounds.size.h - (ROUND_UI ? 28 : caption_h);
  int scale = PG_MIN(avail_w, avail_h) / span;
  int plate;
  int x0;
  int y0;
  int row;
  if (scale < 1) {
    scale = 1;
  }
  plate = span * scale;
  x0 = bounds.origin.x + (bounds.size.w - plate) / 2;
  y0 = bounds.origin.y + PG_MAX(0, (bounds.size.h - plate - caption_h) / 2);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(x0, y0, plate, plate), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorBlack);
  for (row = 0; row < s_qr_modules; row++) {
    int col = 0;
    while (col < s_qr_modules) {
      int start;
      while (col < s_qr_modules && !qr_module_dark(row, col)) {
        col++;
      }
      start = col;
      while (col < s_qr_modules && qr_module_dark(row, col)) {
        col++;
      }
      if (col > start) {
        graphics_fill_rect(ctx,
                           GRect(x0 + ((quiet + start) * scale),
                                 y0 + ((quiet + row) * scale),
                                 (col - start) * scale,
                                 scale),
                           0, GCornerNone);
      }
    }
  }
  graphics_context_set_text_color(ctx, TG_TEXT);
  graphics_draw_text(ctx, "Scan in Telegram", font_14(),
                     GRect(bounds.origin.x, y0 + plate + 1, bounds.size.w, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void chat_menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  GRect bounds = layer_get_bounds(cell_layer);
  GRect safe = round_safe_rect(bounds);
  bool selected = menu_layer_is_index_selected(s_chat_menu, cell_index) && !fav_focused();
  if (ROUND_UI) {
    s_round_org = layer_convert_point_to_screen(cell_layer, GPointZero);
  }

  graphics_context_set_fill_color(ctx, TG_LIST_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (s_chat_count == 0) {
    if (s_qr_modules > 0) {
      draw_login_qr(ctx, bounds);
      return;
    }
    // Splash / login screen: Telegram-style logo, progress and status.
    int logo_r = PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 24, 24, 24, 24, 30, 24, 30);
    int block_h = logo_r * 2 + 64;
    int top = PG_MAX(4, (bounds.size.h - block_h) / 2 - (ROUND_UI ? 8 : 0));
    GPoint logo_c = GPoint(bounds.size.w / 2, top + logo_r);
    graphics_context_set_fill_color(ctx, s_loading_error ? PBL_IF_COLOR_ELSE(GColorSunsetOrange, GColorBlack) :
                                                           PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorBlack));
    graphics_fill_circle(ctx, logo_c, logo_r);
    if (s_loading_error) {
      graphics_context_set_text_color(ctx, GColorWhite);
      graphics_draw_text(ctx, "!", fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                         GRect(logo_c.x - logo_r, logo_c.y - 20, logo_r * 2, 34),
                         GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    } else {
      int scale = (logo_r * 4) / 15;
      draw_beeper_mark_centered(ctx, logo_c, scale, GColorWhite, PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorBlack));
    }
    int text_y = top + logo_r * 2 + 4;
    const char *headline = s_loading_error ? "Login needs attention" :
                           (s_bridge_ready ? "No chats yet" : "Chat");
    graphics_context_set_text_color(ctx, TG_TEXT);
    graphics_draw_text(ctx, headline, font_bold18(), GRect(safe.origin.x, text_y, safe.size.w, 24),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    if (s_loading_error) {
      graphics_context_set_text_color(ctx, TG_SUBTEXT);
      graphics_draw_text(ctx, s_loading_text, font_14(),
                         GRect(safe.origin.x + 6, text_y + 22, safe.size.w - 12, bounds.size.h - text_y - 40),
                         GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
      if (s_offer_qr) {
        graphics_draw_text(ctx, "Select: QR login", font_bold14(),
                           GRect(safe.origin.x, bounds.size.h - 22, safe.size.w, 18),
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
      }
    } else if (s_chats_loading) {
      int bar_w = PG_MIN(safe.size.w - 40, 110);
      GRect track = GRect((bounds.size.w - bar_w) / 2, text_y + 28, bar_w, 4);
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
      graphics_fill_rect(ctx, track, 2, GCornersAll);
      if (BW_UI) {
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_draw_round_rect(ctx, track, 2);
      }
      int fill_w = (bar_w * chat_loading_percent()) / 100;
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorBlack));
      graphics_fill_rect(ctx, GRect(track.origin.x, track.origin.y, PG_MAX(4, fill_w), 4), 2, GCornersAll);
      graphics_context_set_text_color(ctx, TG_SUBTEXT);
      graphics_draw_text(ctx, s_loading_text, font_14(), GRect(safe.origin.x, track.origin.y + 8, safe.size.w, 18),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
    return;
  }

  Chat *chat = &s_chats[cell_index->row];
  GColor row_bg = selected ? TG_LIST_SELECTED : TG_LIST_BG;
  GColor on_selected = TG_ON_SELECTED;
  GColor title_color = selected ? on_selected : TG_TEXT;
  GColor sub_color = selected ? on_selected : TG_SUBTEXT;
  if (selected) {
    graphics_context_set_fill_color(ctx, row_bg);
    if (ROUND_UI) {
      // From the row's first pixel: a 1px gap showed as a line under the header on the first chat
      // and out to the bezel: a 2px inset left a rim of the list's background around it
      round_fill(ctx, GRect(0, 0, bounds.size.w, bounds.size.h - 1), 0, GCornerNone, 0, row_bg);
    } else {
      // Leave a 1px gap above the highlight and above the separator line.
      graphics_fill_rect(ctx, GRect(0, 1, bounds.size.w, bounds.size.h - 3), 0, GCornerNone);
    }
  }

  int avatar_r = CHAT_AVATAR_R;
  GPoint avatar_c = GPoint(safe.origin.x + avatar_r + 4, bounds.size.h / 2);
  int line1_y = PBL_IF_ROUND_ELSE(-1, 1);
  int line2_y = line1_y + 21;
  int right = safe.origin.x + safe.size.w - 5;
  int right2 = right;
  if (ROUND_UI) {
    // Avatars ride a wide arc (radius CHAT_AVATAR_ARC) that touches the screen's
    // left edge at mid-height: a gentle circular sweep that lets them slide off
    // the bezel near the top and bottom instead of swinging into the middle.
    // Text follows the avatar; each line ends at the circle's right edge at its
    // own middle height (an edge pixel of a glyph may touch the bezel).
    int left;
    int dy = avatar_c.y + s_round_org.y - PBL_DISPLAY_HEIGHT / 2;
    dy = PG_MIN(dy < 0 ? -dy : dy, CHAT_AVATAR_ARC);
    avatar_c.x = avatar_r + 5 - s_round_org.x + CHAT_AVATAR_ARC -
                 round_isqrt(CHAT_AVATAR_ARC * CHAT_AVATAR_ARC - dy * dy);
    round_span(line1_y + 10, line1_y + 10, 6, &left, &right);
    round_span(line2_y + 10, line2_y + 10, 6, &left, &right2);
  }
  draw_avatar(ctx, avatar_c, avatar_r, chat->id, chat->title, s_appearance.photos ? chat->avatar_bitmap : NULL, row_bg);
  if (selected && BW_UI) {
    graphics_context_set_stroke_color(ctx, on_selected);
    graphics_draw_circle(ctx, avatar_c, avatar_r);
  }

  int text_x = avatar_c.x + avatar_r + 7;

  // Line 1: title, then receipt ticks + time on the right.
  int time_w = chat->time[0] ? text_width(chat->time, font_14()) : 0;
  int meta_w = time_w + (chat->receipt ? ticks_width(chat->receipt) + 3 : 0);
  if (time_w) {
    graphics_context_set_text_color(ctx, sub_color);
    graphics_draw_text(ctx, chat->time, font_14(), GRect(right - time_w - 1, line1_y + 3, time_w + 2, 16),
                       GTextOverflowModeFill, GTextAlignmentRight, NULL);
  }
  if (chat->receipt) {
    draw_ticks(ctx, right - meta_w, line1_y + 8, chat->receipt,
               selected ? on_selected : TG_OUT_ACCENT);
  }
  const char *badge = chat->network < CHAT_NETWORKS ? s_network_look[chat->network].badge : NULL;
  if (badge) {
    int badge_text_w = text_width(badge, font_14());
    meta_w += badge_text_w + 4;
    graphics_context_set_text_color(ctx, selected ? on_selected : TG_LINK);
    graphics_draw_text(ctx, badge, font_14(), GRect(right - meta_w - 1, line1_y + 3, badge_text_w + 2, 16),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }
  int title_w = PG_MAX(0, right - text_x - (meta_w ? meta_w + 4 : 0));
  graphics_context_set_text_color(ctx, title_color);
  graphics_draw_text(ctx, chat->title, font_bold18(), GRect(text_x, line1_y - 2, title_w, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Line 2: preview (group sender in the link colour), unread badge on the right.
  int badge_w = 0;
  if (chat->unread) {
    bool muted = chat->flags & CHAT_FLAG_MUTED;
    GColor fill = selected ? on_selected : (muted ? TG_BADGE_MUTED : TG_BADGE);
    GColor text = selected ? TG_LIST_SELECTED : TG_ON_BADGE;
    badge_w = draw_badge(ctx, right2, line2_y + 11, chat->unread_count, fill, text) + 5;
  }
  int preview_x = text_x;
  int preview_w = PG_MAX(0, right2 - text_x - badge_w);
  const char *preview = chat->preview;
  const char *colon = NULL;
  if (chat->flags & CHAT_FLAG_GROUP) {
    // Not strstr(): libc's two-way search costs over 1 KB, and Emery/Gabbro are at the 64 KB app limit.
    for (const char *c = preview; *c && !colon; c++) {
      colon = (c[0] == ':' && c[1] == ' ') ? c : NULL;
    }
  }
  if (colon && colon - preview > 0 && colon - preview < 24) {
    char sender[26];
    size_t len = (size_t)(colon - preview) + 1;
    memcpy(sender, preview, len);
    sender[len] = '\0';
    int sender_w = PG_MIN(preview_w / 2, text_width(sender, font_18()));
    graphics_context_set_text_color(ctx, selected ? on_selected : TG_LINK);
    graphics_draw_text(ctx, sender, font_18(), GRect(preview_x, line2_y - 2, sender_w + 2, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    preview_x += sender_w + 4;
    preview_w -= sender_w + 4;
    preview = colon + 2;
  }
  graphics_context_set_text_color(ctx, sub_color);
  graphics_draw_text(ctx, preview, font_18(), GRect(preview_x, line2_y - 2, preview_w, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  if (!ROUND_UI) {
    graphics_context_set_stroke_color(ctx, TG_SEPARATOR);
    graphics_draw_line(ctx, GPoint(text_x, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
  }
}

#define CHAT_ROW_H PBL_PLATFORM_SWITCH(PBL_PLATFORM_TYPE_CURRENT, 44, 44, 44, 44, 48, 44, 46)

static int16_t chat_menu_get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  if (s_chat_count == 0) {
    Layer *layer = menu_layer_get_layer(menu_layer);
    return layer_get_bounds(layer).size.h;
  }
  return CHAT_ROW_H;
}

static void chat_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  if (s_loading_messages) {
    show_status("Loading messages...");
    return;
  }
  if (s_chat_count == 0) {
    if (s_offer_qr) {
      if (send_command("start_qr", NULL, NULL, NULL, NULL)) {
        s_offer_qr = false;
        s_loading_error = false;
        show_loading_text("Starting QR login...", false);
      }
    } else {
      request_chats();
    }
    return;
  }
  s_selected_chat = cell_index->row;
  copy_cstr(s_current_chat_id, sizeof(s_current_chat_id), s_chats[s_selected_chat].id);
  copy_cstr(s_current_chat_title, sizeof(s_current_chat_title), s_chats[s_selected_chat].title);
  s_current_chat_flags = s_chats[s_selected_chat].flags;
  s_current_chat_network = s_chats[s_selected_chat].network;
  refresh_message_actions();
  request_messages(s_current_chat_id);
}

static int clamp_scroll_offset(int offset) {
  if (!s_messages_root) {
    return 0;
  }
  GRect bounds = layer_get_bounds(s_messages_root);
  int visible_h = bounds.size.h;
#if TOUCH_KEYBOARD_AVAILABLE
  if (s_touch_keyboard_open) {
    visible_h = touch_keyboard_viewport_height(bounds);
  }
#endif
  int max_offset = PG_MAX(0, s_chat_content_height - visible_h);
  return PG_MAX(0, PG_MIN(offset, max_offset));
}


static bool message_has_context(Message *message) {
  return message && message->context[0];
}

static void copy_context_part(char *dest, size_t dest_size, const char *start, const char *end) {
  size_t len;
  if (!dest || dest_size == 0) {
    return;
  }
  if (!start) {
    dest[0] = '\0';
    return;
  }
  len = end && end > start ? (size_t)(end - start) : strlen(start);
  if (len >= dest_size) {
    len = dest_size - 1;
  }
  memcpy(dest, start, len);
  dest[len] = '\0';
  trim_incomplete_utf8(dest);
}

static void message_context_strings(Message *message, char *title, size_t title_size,
                                    char *body, size_t body_size) {
  char *separator = strchr(message->context, '\n');
  copy_context_part(title, title_size, message->context, separator);
  copy_context_part(body, body_size, separator ? separator + 1 : "Message", NULL);
}

static void build_message_context(char *context, size_t context_size,
                                  const char *reply_sender, const char *reply_text,
                                  const char *forward_sender, const char *forward_text) {
  const char *sender;
  const char *text;
  if (!context || context_size == 0) {
    return;
  }
  context[0] = '\0';
  if ((reply_sender && reply_sender[0]) || (reply_text && reply_text[0])) {
    sender = reply_sender && reply_sender[0] ? reply_sender : "Reply";
    text = reply_text && reply_text[0] ? reply_text : "Message";
    snprintf(context, context_size, "%s\n%s", sender, text);
    trim_incomplete_utf8(context);
    return;
  }
  if ((forward_sender && forward_sender[0]) || (forward_text && forward_text[0])) {
    sender = forward_sender && forward_sender[0] ? forward_sender : "Forwarded";
    text = forward_text && forward_text[0] ? forward_text : "Message";
    snprintf(context, context_size, "Fwd from %s\n%s", sender, text);
    trim_incomplete_utf8(context);
    return;
  }
}


static void clear_message_slot(Message *message) {
  if (!message) {
    return;
  }
  if (s_image_message_id[0] && strcmp(s_image_message_id, message->image_token) == 0) {
    clear_active_image_request();
  }
  destroy_message_bitmap(message);
  release_message_strings(message);
  memset(message, 0, sizeof(Message));
  init_message_strings(message);
}

static void clear_message_rows(void) {
  for (int i = 0; i < MAX_MESSAGES; i++) {
    clear_message_slot(&s_messages[i]);
  }
  s_message_count = 0;
  s_selected_message = -1;
  s_expected_rows = 0;
  s_chat_scroll_offset = 0;
  s_chat_content_height = 0;
}

static void reset_message_stream_state(void) {
  s_message_stream_silent = false;
  s_message_stream_mode = MESSAGE_MODE_INITIAL;
}

static void clear_message_stage(void) {
  if (!s_message_stage) {
    s_message_stage_count = 0;
    return;
  }
  for (int i = 0; i < MAX_MESSAGES; i++) {
    clear_message_slot(&s_message_stage[i]);
  }
  free(s_message_stage);
  s_message_stage = NULL;
  s_message_stage_count = 0;
}

static void end_page_load(void) {
  s_loading_older_messages = false;
  s_loading_newer_messages = false;
  s_older_anchor_id[0] = '\0';
  s_newer_anchor_id[0] = '\0';
  clear_message_stage();
  reset_message_stream_state();
}

static bool prepare_message_stage(void) {
  clear_message_stage();
  s_message_stage = malloc(sizeof(Message) * MAX_MESSAGES);
  while (!s_message_stage && evict_farthest_image(0)) {
    s_message_stage = malloc(sizeof(Message) * MAX_MESSAGES);
  }
  if (!s_message_stage) {
    destroy_message_images();
    s_message_stage = malloc(sizeof(Message) * MAX_MESSAGES);
  }
  if (!s_message_stage) {
    return false;
  }
  memset(s_message_stage, 0, sizeof(Message) * MAX_MESSAGES);
  for (int i = 0; i < MAX_MESSAGES; i++) {
    init_message_strings(&s_message_stage[i]);
  }
  s_message_stage_count = 0;
  return true;
}

static bool messages_match_image(const Message *a, const Message *b) {
  return a && b && a->id[0] && b->id[0] && a->image_token[0] &&
         strcmp(a->id, b->id) == 0 && strcmp(a->image_token, b->image_token) == 0;
}

static void preserve_stage_image_state(void) {
  if (!s_message_stage) {
    return;
  }
  int rows = PG_MIN(s_message_stage_count, MAX_MESSAGES);
  for (int i = 0; i < rows; i++) {
    Message *stage = &s_message_stage[i];
    for (int j = 0; j < s_message_count; j++) {
      Message *current = &s_messages[j];
      if (!messages_match_image(stage, current)) {
        continue;
      }
      if (!stage->image_bitmap && current->image_bitmap) {
        stage->image_bitmap = current->image_bitmap;
        stage->image_data = current->image_data;
        current->image_bitmap = NULL;
        current->image_data = NULL;
      }
      stage->image_requested = current->image_requested;
      stage->image_failed = current->image_failed;
      stage->image_retry_level = current->image_retry_level;
      copy_cstr(stage->image_error, sizeof(stage->image_error), current->image_error);
      current->image_token[0] = '\0';
      break;
    }
  }
}

static void commit_message_stage(int count) {
  if (!s_message_stage) {
    return;
  }
  preserve_stage_image_state();
  clear_message_rows();
  int rows = PG_MIN(PG_MIN(count, s_message_stage_count), MAX_MESSAGES);
  for (int i = 0; i < rows; i++) {
    s_messages[i] = s_message_stage[i];
    memset(&s_message_stage[i], 0, sizeof(Message));
  }
  for (int i = rows; i < MAX_MESSAGES; i++) {
    clear_message_slot(&s_message_stage[i]);
  }
  s_message_count = rows;
  free(s_message_stage);
  s_message_stage = NULL;
  s_message_stage_count = 0;
}

static bool message_transfer_matches(DictionaryIterator *iter) {
  int transfer_id = tuple_int(iter, MESSAGE_KEY_ImageTransferId, 0);
  return transfer_id == 0 || (s_message_transfer_id != 0 && transfer_id == s_message_transfer_id);
}

static void populate_message_from_tuple(Message *message, DictionaryIterator *iter) {
  char context[MAX_CONTEXT_TEXT];
  char *incoming_message_id = tuple_cstring(iter, MESSAGE_KEY_MessageId);
  char *incoming_image_token = tuple_cstring(iter, MESSAGE_KEY_ImageToken);
  bool preserve_image_state = incoming_message_id && incoming_image_token &&
                              strcmp(message->id, incoming_message_id) == 0 &&
                              strcmp(message->image_token, incoming_image_token) == 0;
  if (!preserve_image_state) {
    destroy_message_bitmap(message);
  }
  copy_cstr(message->id, sizeof(message->id), incoming_message_id);
  build_message_context(context, sizeof(context),
                        tuple_cstring(iter, MESSAGE_KEY_ReplySender),
                        tuple_cstring(iter, MESSAGE_KEY_ReplyText),
                        tuple_cstring(iter, MESSAGE_KEY_ForwardSender),
                        tuple_cstring(iter, MESSAGE_KEY_ForwardText));
  set_message_strings(message,
                      tuple_cstring(iter, MESSAGE_KEY_Sender),
                      tuple_cstring(iter, MESSAGE_KEY_Text),
                      tuple_cstring(iter, MESSAGE_KEY_Reactions),
                      tuple_cstring(iter, MESSAGE_KEY_MessageMeta),
                      context);
  message->outgoing = tuple_int(iter, MESSAGE_KEY_IsOutgoing, 0) != 0;
  copy_cstr(message->image_token, sizeof(message->image_token), incoming_image_token);
  message->image_placeholder = message->image_token[0] != '\0';
  message->image_width = tuple_int(iter, MESSAGE_KEY_ImageWidth, 0);
  message->image_height = tuple_int(iter, MESSAGE_KEY_ImageHeight, 0);
  if (!preserve_image_state) {
    message->image_requested = false;
    message->image_failed = false;
    message->image_error[0] = '\0';
    message->image_progress = 0;
    message->image_retry_level = 0;
    message->image_bitmap = NULL;
  }
}

static Message *prepend_message_slot(void) {
  if (s_message_count >= MAX_MESSAGES) {
    clear_message_slot(&s_messages[MAX_MESSAGES - 1]);
    s_message_count = MAX_MESSAGES - 1;
  }
  for (int i = s_message_count; i > 0; i--) {
    s_messages[i] = s_messages[i - 1];
  }
  memset(&s_messages[0], 0, sizeof(Message));
  init_message_strings(&s_messages[0]);
  s_message_count++;
  return &s_messages[0];
}

static Message *append_message_slot(void) {
  if (s_message_count >= MAX_MESSAGES) {
    clear_message_slot(&s_messages[0]);
    for (int i = 0; i < MAX_MESSAGES - 1; i++) {
      s_messages[i] = s_messages[i + 1];
    }
    s_message_count = MAX_MESSAGES - 1;
    if (s_selected_message > 0) {
      s_selected_message--;
    }
  }
  memset(&s_messages[s_message_count], 0, sizeof(Message));
  init_message_strings(&s_messages[s_message_count]);
  s_message_count++;
  return &s_messages[s_message_count - 1];
}

static void remove_message_at(int index) {
  char anchor_id[MAX_ID];
  int anchor_y = 0;
  anchor_id[0] = '\0';
  if (index < 0 || index >= s_message_count) {
    return;
  }
  if (s_messages_root) {
    recalc_message_layout();
    int anchor_index = index + 1 < s_message_count ? index + 1 : index - 1;
    if (anchor_index >= 0 && anchor_index < s_message_count) {
      copy_cstr(anchor_id, sizeof(anchor_id), s_messages[anchor_index].id);
      anchor_y = s_message_y[anchor_index];
    }
  }
  clear_message_slot(&s_messages[index]);
  for (int i = index; i < s_message_count - 1; i++) {
    s_messages[i] = s_messages[i + 1];
  }
  memset(&s_messages[s_message_count - 1], 0, sizeof(Message));
  init_message_strings(&s_messages[s_message_count - 1]);
  s_message_count--;
  if (s_message_count <= 0) {
    s_selected_message = s_at_newest ? s_message_count : -1;
  } else if (index < s_message_count) {
    s_selected_message = index;
  } else {
    s_selected_message = s_message_count - 1;
  }
  if (s_messages_root) {
    if (anchor_id[0]) {
      preserve_stream_anchor(anchor_id, anchor_y, true);
    } else {
      recalc_message_layout();
      set_chat_scroll_offset(s_chat_scroll_offset, false);
      mark_messages_dirty();
      request_next_image();
    }
  }
}

static void preserve_stream_anchor(const char *anchor_id, int anchor_y, bool dirty) {
  recalc_message_layout();
  if (anchor_id && anchor_id[0]) {
    int anchor_index = find_message_index_by_id(anchor_id);
    if (anchor_index >= 0) {
      s_selected_message = anchor_index;
      int target = s_chat_scroll_offset + (s_message_y[anchor_index] - anchor_y);
      if (dirty) {
        set_chat_scroll_offset(target, false);
      } else {
        set_chat_scroll_offset_quiet(target);
      }
    }
  }
  if (dirty && s_messages_root) {
    mark_messages_dirty();
  }
  if (dirty) {
    request_next_image();
  }
}

static void render_after_stream_append(const char *anchor_id, int anchor_y) {
  if (s_message_stream_silent) {
    preserve_stream_anchor(anchor_id, anchor_y, false);
    return;
  }
  preserve_stream_anchor(anchor_id, anchor_y, true);
}

static void render_after_stream_prepend(const char *anchor_id, int anchor_y) {
  if (s_message_stream_silent) {
    preserve_stream_anchor(anchor_id, anchor_y, false);
    return;
  }
  if (!s_user_scrolled_messages && (!anchor_id || !anchor_id[0])) {
    recalc_message_layout();
    scroll_to_bottom(false);
    return;
  }
  preserve_stream_anchor(anchor_id, anchor_y, true);
}


static uint8_t message_meta_receipts(const char *meta) {
  char *separator = meta ? strchr(meta, '|') : NULL;
  if (!separator || !separator[1]) {
    return 0;
  }
  return separator[1] == '2' ? 2 : 1;
}

static void message_meta_time(const char *meta, char *dest, size_t dest_size) {
  char *separator = meta ? strchr(meta, '|') : NULL;
  if (!dest || dest_size == 0) {
    return;
  }
  if (!meta) {
    dest[0] = '\0';
    return;
  }
  if (separator) {
    copy_context_part(dest, dest_size, meta, separator);
  } else {
    copy_cstr(dest, dest_size, meta);
  }
}

// ---- Days and status (Beeper-style) ----------------------------------------

static bool s_day_label[MAX_MESSAGES];  // the message is the first of its day on screen
static bool s_day_pill;                 // the floating day shows: the chat just scrolled
static AppTimer *s_day_pill_timer;
static AppTimer *s_fling_timer;  // momentum after a swipe, see fling_start()
static int s_day_pill_offset = -1;      // scroll offset the pill last saw

// A message id is its time in ms x 10 plus a tie-breaker (the phone's numbering); 0 for any other id.
static time_t message_time(const Message *m) {
  int64_t v = 0;
  for (const char *p = m->id; *p; p++) {
    if (*p < '0' || *p > '9' || v > (INT64_MAX - 9) / 10) {  // not a number, or past any real time
      return 0;
    }
    v = v * 10 + (*p - '0');
  }
  return v > 10000000000000LL ? (time_t)(v / 10000) : 0;
}

// Local midnight of t's day, as a time.
static time_t day_start(time_t t) {
  struct pbl_tm tm = *localtime(&t);
  return t - (tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
}

// Whole days from a's day to b's; the rounding absorbs a daylight-saving hour.
static int days_between(time_t a, time_t b) {
  return (int)((day_start(b) - day_start(a) + 43200) / 86400);
}

// "Today", "Yesterday", the weekday within a week, else "Sat, Oct 31".
static void day_label_text(time_t t, char *out, size_t size) {
  static const char *const days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  static const char *const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  int ago = days_between(t, time(NULL));
  struct pbl_tm tm = *localtime(&t);
  if (ago <= 0) {
    copy_cstr(out, size, "Today");
  } else if (ago == 1) {
    copy_cstr(out, size, "Yesterday");
  } else if (ago < 7) {
    copy_cstr(out, size, days[tm.tm_wday % 7]);
  } else {
    snprintf(out, size, "%.3s, %s %d", days[tm.tm_wday % 7], months[tm.tm_mon % 12], tm.tm_mday);
  }
}

// The status of my message, sent by the phone after the time: "Sent", "Seen at 10:52", ...
static const char *message_status(const Message *m) {
  const char *separator = m->meta ? strchr(m->meta, '|') : NULL;
  return separator && separator[1] ? separator + 1 : NULL;
}

// How far a message's reaction pills reach below its bubble.
static int reactions_overhang(const Message *m) {
  return m->pills_h ? m->pills_h - REACTION_OVERLAP + 2 : 0;
}

// The status shows under the chat's newest message when it is mine.
static bool message_shows_status(int index) {
  return s_at_newest && index == s_message_count - 1 && s_messages[index].outgoing &&
         message_status(&s_messages[index]);
}

// ---- Message layout (Telegram-style bubbles) ------------------------------
//
// Bubbles shrink to fit their content like Telegram's. Measuring text is the
// expensive part, so each message caches its measurements (keyed on its
// string storage, which is reallocated whenever the text changes) and the
// per-frame layout only does cheap arithmetic.

#define BUBBLE_R 12
#define BUBBLE_PAD_X 7
#define BUBBLE_INSET 4  // round: gap kept between a bubble and the bezel
#define BUBBLE_PAD_TOP 3
#define BUBBLE_PAD_BOTTOM 4
#define BUBBLE_NAME_H 16
#define BUBBLE_CTX_H 34
#define BUBBLE_META_H 15
#define BUBBLE_GAP_SAME 4
#define BUBBLE_GAP_OTHER 6
#define DAY_LABEL_H 20  // "Today", "Friday", "Sat, Oct 31" above a day's first message
#define STATUS_LINE_H 16  // "Sent", "Seen at 10:52" under my last message
#define DAY_PILL_MS 1500
#define PILL_H 22
#define PILL_GAP 4
#define GROUP_AVATAR_R 11
#define GROUP_AVATAR_COL 24
#define COMPOSE_BAR_H 32
#define COMPOSE_BAR_DRAFT_H 50  // two lines of a draft from Index, then "…"
static char s_bar_draft[MAX_TEXT];
static int compose_bar_h(void) {
  return s_bar_draft[0] ? COMPOSE_BAR_DRAFT_H : COMPOSE_BAR_H;
}

#define BOX_NAME 0x01
#define BOX_TAIL 0x02
#define BOX_AVATAR 0x04
#define BOX_META_INLINE 0x08
#define BOX_EMOJI 0x10
#define BOX_META_ON_PILLS 0x20
#define BOX_META_ON_PHOTO 0x40

#ifdef PBL_COLOR
#define PATTERN_TILE 32

static GBitmap *s_wallpaper;
// Index 0 is the background and index 1 the pattern colour, as in the doodle
// resource, so both kinds of wallpaper share this palette.
static GColor s_wallpaper_palette[2];

static bool pattern_pixel(int pattern, int x, int y) {
  switch (pattern) {
    case PatternDots:
      return (x % 8) < 2 && (y % 8) < 2;
    case PatternLines:
      return (x + y) % 8 == 0;
    default:
      return x % 16 == 0 || y % 16 == 0;
  }
}

// The doodle wallpapers are 1-bit resources; 0 means the pattern is drawn in code.
static uint32_t wallpaper_resource(int pattern) {
  switch (pattern) {
    case PatternDoodles: return RESOURCE_ID_IMAGE_WALLPAPER;
    case PatternSpace: return RESOURCE_ID_IMAGE_WALLPAPER_SPACE;
    case PatternPets: return RESOURCE_ID_IMAGE_WALLPAPER_PETS;
    case PatternFood: return RESOURCE_ID_IMAGE_WALLPAPER_FOOD;
    case PatternTravel: return RESOURCE_ID_IMAGE_WALLPAPER_TRAVEL;
    case PatternParty: return RESOURCE_ID_IMAGE_WALLPAPER_PARTY;
    default: return 0;
  }
}

// Doodles are tiled from the resource; the other patterns are tiny 1-bit
// tiles drawn here. None leaves s_wallpaper empty so the colour is solid.
static void wallpaper_rebuild(void) {
  if (s_wallpaper) {
    gbitmap_destroy(s_wallpaper);
    s_wallpaper = NULL;
  }
  s_wallpaper_palette[0] = s_theme.chat_bg;
  s_wallpaper_palette[1] = s_theme.wall_hi;
  if (wallpaper_resource(s_appearance.pattern)) {
    s_wallpaper = gbitmap_create_with_resource(wallpaper_resource(s_appearance.pattern));
    if (s_wallpaper) {
      gbitmap_set_palette(s_wallpaper, s_wallpaper_palette, false);
    }
  } else if (s_appearance.pattern != PatternNone) {
    s_wallpaper = gbitmap_create_blank_with_palette(GSize(PATTERN_TILE, PATTERN_TILE), GBitmapFormat1BitPalette,
                                                    s_wallpaper_palette, false);
    if (s_wallpaper) {
      uint8_t *data = gbitmap_get_data(s_wallpaper);
      int stride = gbitmap_get_bytes_per_row(s_wallpaper);
      memset(data, 0, stride * PATTERN_TILE);
      for (int y = 0; y < PATTERN_TILE; y++) {
        for (int x = 0; x < PATTERN_TILE; x++) {
          if (pattern_pixel(s_appearance.pattern, x, y)) {
            data[y * stride + x / 8] |= 0x80 >> (x % 8);  // leftmost pixel is the high bit
          }
        }
      }
    }
  }
}
#endif

static bool chat_is_group(void) {
  return s_current_chat_flags & CHAT_FLAG_GROUP;
}

static bool group_avatars_enabled(void) {
  return chat_is_group() && !ROUND_UI;
}

static int message_side_inset(GRect bounds) {
  // Round: nearly the full width; chat_warp squeezes rows near the bezel into the circle
  return ROUND_UI ? CHAT_WARP_SIDE_INSET : 6;
}

// Widest a bubble may be (images are sized against this too).
static int message_bubble_width(GRect bounds) {
  if (ROUND_UI) {
    return PG_MAX(112, bounds.size.w - (message_side_inset(bounds) * 2));
  }
  int max_w = bounds.size.w <= 160 ? bounds.size.w - 18 : (bounds.size.w * 84) / 100;
  if (group_avatars_enabled()) {
    max_w = PG_MIN(max_w, bounds.size.w - GROUP_AVATAR_COL - 16);
  }
  return max_w;
}

// Photo bubbles may be wider than text bubbles: every extra pixel of width
// makes text in a screenshot easier to read.
static int message_photo_bubble_width(GRect bounds) {
  if (ROUND_UI) {
    return message_bubble_width(bounds);
  }
  int w = bounds.size.w - (message_side_inset(bounds) * 2);
  if (group_avatars_enabled()) {
    w -= GROUP_AVATAR_COL;
  }
  return w;
}

// Photos sit 2px inside their bubble, like Telegram's.
static int message_image_frame_width(int bubble_w) {
  return bubble_w - 4;
}

static GFont bubble_text_font(uint8_t size) {
  static const char *const keys[2][4] = {
    {FONT_KEY_GOTHIC_14, FONT_KEY_GOTHIC_18, FONT_KEY_GOTHIC_24, FONT_KEY_GOTHIC_28},
    {FONT_KEY_GOTHIC_14_BOLD, FONT_KEY_GOTHIC_18_BOLD, FONT_KEY_GOTHIC_24_BOLD, FONT_KEY_GOTHIC_28_BOLD}
  };
  // Gothic 24 is bold at Medium, as in a notification's body: plain 24 looks thin and rough
  return fonts_get_system_font(keys[(s_appearance.bold || size == 2) ? 1 : 0][size]);
}

// Bigger fonts start their glyphs lower in the text box, so draw them higher.
static int bubble_text_trim(uint8_t size) {
  return 2 + size;
}

// Emoji-only messages use the biggest system font: PebbleOS draws its emoji at
// the size of the font they are in (up to 28px).
static GFont big_emoji_font(void) {
  return fonts_get_system_font(FONT_KEY_GOTHIC_28);
}

static GFont pill_font(void) {
  return fonts_get_system_font(FONT_KEY_GOTHIC_18);
}

// Number of emoji in `text` if it holds nothing else (and no more than `max`), else 0.
static uint8_t count_emoji_only(const char *text, uint8_t max) {
  uint8_t count = 0;
  const unsigned char *p = (const unsigned char *)text;
  if (!p || !*p) {
    return 0;
  }
  while (*p) {
    if (*p == ' ') {
      p++;
      continue;
    }
    if (*p < 0x80) {
      return 0;
    }
    uint32_t cp;
    int len;
    if ((*p & 0xe0) == 0xc0) { cp = *p & 0x1f; len = 2; }
    else if ((*p & 0xf0) == 0xe0) { cp = *p & 0x0f; len = 3; }
    else if ((*p & 0xf8) == 0xf0) { cp = *p & 0x07; len = 4; }
    else { return 0; }
    for (int i = 1; i < len; i++) {
      if ((p[i] & 0xc0) != 0x80) {
        return 0;
      }
      cp = (cp << 6) | (p[i] & 0x3f);
    }
    p += len;
    if (cp == 0x200d || cp == 0xfe0f) {
      continue;
    }
    if (cp < 0x2000) {
      return 0;
    }
    if (++count > max) {
      return 0;
    }
  }
  return count;
}

// Byte offset of the UTF-8 character after the one at `pos`.
static int text_next_char(const char *text, int pos) {
  if (text[pos]) {
    pos++;
  }
  while (text[pos] && ((unsigned char)text[pos] & 0xc0) == 0x80) {
    pos++;
  }
  return pos;
}

// Width of text[from, to). The text is cut with a temporary NUL instead of
// copied, to keep RAM and time down.
static int text_span_width(char *text, GFont font, int from, int to) {
  if (to <= from) {
    return 0;
  }
  char saved = text[to];
  text[to] = '\0';
  GSize size = graphics_text_layout_get_content_size(text + from, font, GRect(0, 0, 2000, 24),
                                                     GTextOverflowModeFill, GTextAlignmentLeft);
  text[to] = saved;
  return size.w;
}

// Start of the line after the one starting at `start`: breaks after the last
// word that fits, at a newline, or inside a word that is wider than the line.
static int line_next(char *text, GFont font, int start, int width, int len) {
  int pos = start;
  while (pos < len) {
    while (pos < len && text[pos] == ' ') {
      pos++;  // spaces may hang past the edge
    }
    if (pos >= len) {
      return len;
    }
    if (text[pos] == '\n') {
      return pos + 1;
    }
    int word_end = pos;
    while (word_end < len && text[word_end] != ' ' && text[word_end] != '\n') {
      word_end++;
    }
    if (text_span_width(text, font, start, word_end) > width) {
      if (pos > start) {
        return pos;
      }
      int end = start;
      int next = text_next_char(text, end);
      while (next <= word_end && text_span_width(text, font, start, next) <= width) {
        end = next;
        next = text_next_char(text, end);
      }
      return end > start ? end : text_next_char(text, start);
    }
    pos = word_end;
  }
  return len;
}

static void message_meta_time(const char *meta, char *dest, size_t dest_size);
static uint8_t message_meta_receipts(const char *meta);

static int meta_width(const char *meta) {
  char time_text[8];
  // Beeper's bubbles carry no time: days are labelled between messages, and my last message
  // has its status under it. The meta drawing below stays for a theme that wants times back.
  if (true || !meta || !meta[0]) {
    return 0;
  }
  message_meta_time(meta, time_text, sizeof(time_text));
  uint8_t receipts = message_meta_receipts(meta);
  return text_width(time_text, font_14()) + (receipts ? ticks_width(receipts) + 3 : 0);
}

// Reaction pills: "👍3 🔥2" becomes [👍 3] [🔥 2]. Walks the tokens either
// to measure (ctx == NULL) or to draw at `origin`.
static void layout_pills(GContext *ctx, const char *reactions, int max_w, GPoint origin,
                         bool outgoing, int16_t *out_w, int16_t *out_h, int16_t *out_last_w) {
  int x = 0;
  int y = 0;
  int widest = 0;
  char token[24];
  const char *p = reactions;
  bool any = false;
  while (p && *p) {
    while (*p == ' ') {
      p++;
    }
    if (!*p) {
      break;
    }
    const char *end = p;
    while (*end && *end != ' ') {
      end++;
    }
    size_t len = PG_MIN((size_t)(end - p), sizeof(token) - 1);
    memcpy(token, p, len);
    token[len] = '\0';
    trim_incomplete_utf8(token);
    p = end;
    // Word-wrap measuring: the ellipsis mode text_width() uses gives an emoji next to no width
    int token_w = graphics_text_layout_get_content_size(token, pill_font(), GRect(0, 0, 200, 40),
                                                        GTextOverflowModeWordWrap, GTextAlignmentLeft).w;
    int w = PG_MIN(max_w, PG_MAX(token_w, 14) + 14);
    if (any && x + PILL_GAP + w > max_w) {
      x = 0;
      y += PILL_H + PILL_GAP;
    }
    int px = any && x > 0 ? x + PILL_GAP : x;
    if (ctx) {
      GRect pill = GRect(origin.x + px, origin.y + y, w, PILL_H);
      if (ROUND_UI && !s_round_flat) {  // in the warped column the warp keeps it inside the circle
        pill = round_clamp(pill, BUBBLE_INSET);
      }
      if (pill.size.w >= PG_MIN(w, 28)) {  // round: a pill squeezed by the bezel goes away
        // A rim in the chat's background lifts the pill off the bubble it overlaps
        GRect rim = GRect(pill.origin.x - 2, pill.origin.y - 2, pill.size.w + 4, pill.size.h + 4);
        if (ROUND_UI) {
          // Row by row, like the bubbles: the system's rounded fill loses most of a pill in the warp's strips
          round_fill(ctx, rim, PILL_H / 2 + 2, GCornersAll, 0, TG_CHAT_BG);
          round_fill(ctx, pill, PILL_H / 2, GCornersAll, 0, TG_IN_BUBBLE);
        } else {
          graphics_context_set_fill_color(ctx, TG_CHAT_BG);
          graphics_fill_rect(ctx, rim, PILL_H / 2 + 2, GCornersAll);
          graphics_context_set_fill_color(ctx, TG_IN_BUBBLE);
          graphics_fill_rect(ctx, pill, PILL_H / 2, GCornersAll);
        }
        if (BW_UI) {
          graphics_context_set_stroke_color(ctx, TG_TEXT);
          graphics_draw_round_rect(ctx, pill, PILL_H / 2);
        }
        graphics_context_set_text_color(ctx, TG_ON_IN);
        graphics_draw_text(ctx, token, pill_font(),
                           GRect(pill.origin.x, pill.origin.y - 1, pill.size.w, PILL_H),
                           GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
      }
    }
    x = px + w;
    widest = PG_MAX(widest, x);
    any = true;
  }
  if (out_w) *out_w = any ? widest : 0;
  if (out_h) *out_h = any ? y + PILL_H : 0;
  if (out_last_w) *out_last_w = any ? x : 0;
}

// Emoji are wider than letters, so a row like a Wordle grid can be too wide for
// the bubble at a big text size and wrap in the middle. A message with such
// rows is drawn at the largest size where every one of them fits on its line.
static uint8_t message_font_size(char *text, int inner) {
  for (uint8_t size = s_appearance.text_size; size > 0; size--) {
    bool fits = true;
    for (char *line = text; line && fits;) {
      char *end = strchr(line, '\n');
      if (end) {
        *end = '\0';
      }
      fits = !count_emoji_only(line, 255) || text_width(line, bubble_text_font(size)) <= inner;
      if (end) {
        *end = '\n';
      }
      line = end ? end + 1 : NULL;
    }
    if (fits) {
      return size;
    }
  }
  return 0;
}

// Measure a message once per content change.
static void measure_message(Message *m, int max_w) {
  uint16_t img_key = m->image_placeholder ? (uint16_t)(1 + m->image_width * 31 + m->image_height) : 0;
  if (m->layout_key == m->string_storage && m->string_storage && m->layout_maxw == max_w &&
      m->layout_img == img_key) {
    return;
  }
  int inner = max_w - (BUBBLE_PAD_X * 2);
  char display_text[MESSAGE_PREVIEW_TEXT + 8];
  copy_cstr(display_text, sizeof(display_text), m->text);
  truncate_cstr_bytes(display_text, sizeof(display_text), MESSAGE_PREVIEW_TEXT, " ...");

  m->emoji_only = (m->image_placeholder || message_has_context(m) || m->reactions[0] || !s_appearance.big_emoji) ? 0 :
                  count_emoji_only(display_text, 3);
  m->font_size = m->emoji_only ? s_appearance.text_size : message_font_size(display_text, inner);
  GFont font = m->emoji_only ? big_emoji_font() : bubble_text_font(m->font_size);
  m->meta_w = meta_width(m->meta);
  m->text_w = m->text_h = m->text_nw = m->text_nh = 0;
  if (display_text[0]) {
    GSize full = graphics_text_layout_get_content_size(display_text, font, GRect(0, 0, inner, 2000),
                                                       GTextOverflowModeWordWrap, GTextAlignmentLeft);
    m->text_w = full.w;
    m->text_h = PG_MIN(full.h, MAX_TEXT * 2);
    int narrow = inner - m->meta_w - 6;
    if (m->meta_w && narrow > 20 && !m->emoji_only) {
      GSize tight = graphics_text_layout_get_content_size(display_text, font, GRect(0, 0, narrow, 2000),
                                                          GTextOverflowModeWordWrap, GTextAlignmentLeft);
      m->text_nw = tight.w;
      m->text_nh = PG_MIN(tight.h, MAX_TEXT * 2);
    }
  }
  m->name_w = m->sender[0] ? PG_MIN(inner, text_width(m->sender, font_bold14())) : 0;
  m->ctx_w = 0;
  if (message_has_context(m)) {
    char title[MAX_SENDER + 10];
    char body[MAX_CONTEXT_TEXT];
    message_context_strings(m, title, sizeof(title), body, sizeof(body));
    m->ctx_w = PG_MIN(inner, PG_MAX(40, PG_MAX(text_width(title, font_bold14()),
                                               text_width(body, font_14())) + 8));
  }
  layout_pills(NULL, m->reactions, inner, GPointZero, m->outgoing, &m->pills_w, &m->pills_h,
               &m->pills_last_w);
  m->wrap_n = 0;
  m->layout_key = m->string_storage;
  m->layout_maxw = max_w;
  m->layout_img = img_key;
}

static bool same_author(const Message *a, const Message *b) {
  return a && b && a->outgoing == b->outgoing && strcmp(a->sender, b->sender) == 0;
}

// Position one bubble; returns its height. Cheap: uses cached measurements.
static int layout_message(int index, GRect bounds, int max_w) {
  Message *m = &s_messages[index];
  MsgBox *box = &s_box[index];
  Message *prev = index > 0 ? &s_messages[index - 1] : NULL;
  Message *next = index + 1 < s_message_count ? &s_messages[index + 1] : NULL;
  if (m->image_placeholder) {
    max_w = message_photo_bubble_width(bounds);
  }
  measure_message(m, max_w);
  memset(box, 0, sizeof(*box));

  bool first = !same_author(prev, m);
  bool last = !same_author(m, next);
  if (last) box->flags |= BOX_TAIL;
  if (!m->outgoing && group_avatars_enabled() && last) box->flags |= BOX_AVATAR;
  int inner = max_w - (BUBBLE_PAD_X * 2);
  int content_w = 0;
  int h;

  if (m->emoji_only) {
    box->flags |= BOX_EMOJI;
    box->text_y = 0;
    box->text_box_w = inner;
    content_w = PG_MAX(m->text_w, m->meta_w + 10);
    box->meta_y = m->text_h + 1;
    h = m->meta_w ? box->meta_y + 16 : m->text_h + 2;
    box->w = content_w;
  } else {
    h = BUBBLE_PAD_TOP;
    if (first && chat_is_group() && !m->outgoing && m->sender[0]) {
      box->flags |= BOX_NAME;
      content_w = m->name_w;
      h += BUBBLE_NAME_H;
    }
    if (message_has_context(m)) {
      box->ctx_y = h + 1;
      content_w = PG_MAX(content_w, m->ctx_w);
      h += BUBBLE_CTX_H + 2;
    }
    if (m->image_placeholder) {
      GSize plan = message_image_plan(m, message_image_frame_width(max_w));
      box->image_w = plan.w;
      box->image_h = plan.h;
      if (h == BUBBLE_PAD_TOP) {
        h = 2;  // photo at the top of the bubble sits on the 2px border
      }
      box->image_y = h;
      content_w = PG_MAX(content_w, plan.w - (BUBBLE_PAD_X * 2) + 4);
      h += plan.h + 3;
      if (!m->text_h && !m->pills_h && m->meta_w) {
        // Photo-only message: time sits on the photo, as in Telegram.
        box->flags |= BOX_META_ON_PHOTO;
        box->meta_y = box->image_y + plan.h - 18;
        h = box->image_y + plan.h + 2 - BUBBLE_PAD_BOTTOM;
      }
    }
    bool meta_placed = false;
    box->text_y = h;
    box->text_box_w = inner;
    if (m->text_h) {
      if (!m->reactions[0] && m->text_nh && m->text_nh == m->text_h) {
        // Time fits on the last line, as Telegram does it.
        box->flags |= BOX_META_INLINE;
        box->text_box_w = inner - m->meta_w - 6;
        content_w = PG_MAX(content_w, m->text_nw + 6 + m->meta_w);
        box->meta_y = h + m->text_h - BUBBLE_META_H - 1;
        meta_placed = true;
      } else {
        content_w = PG_MAX(content_w, m->text_w);
      }
      h += m->text_h;
    }
    if (m->pills_h) {
      // Reactions aren't in the bubble: they overlap its bottom corner (see reactions_overhang)
      content_w = PG_MAX(content_w, m->pills_w - (BUBBLE_PAD_X * 2) + 12);
    }
    if (box->flags & BOX_META_ON_PHOTO) {
      meta_placed = true;
    }
    if (!meta_placed && m->meta_w) {
      content_w = PG_MAX(content_w, m->meta_w);
      box->meta_y = h;
      h += BUBBLE_META_H;
    }
    h += BUBBLE_PAD_BOTTOM;
    box->pills_y = h - REACTION_OVERLAP;
    box->w = PG_MIN(max_w, PG_MAX(40, content_w + (BUBBLE_PAD_X * 2)));
  }

  int inset = message_side_inset(bounds);
  if (ROUND_UI) {
    box->x = m->outgoing ? bounds.size.w - inset + 6 - box->w : inset - 6;
  } else if (m->outgoing) {
    box->x = bounds.size.w - inset - box->w;
  } else {
    box->x = inset + (group_avatars_enabled() ? GROUP_AVATAR_COL : 0);
  }
  box->x = PG_MAX(2, PG_MIN(box->x, bounds.size.w - box->w - 2));
  return PG_MAX(m->emoji_only ? 20 : 24, h);
}

static void recalc_message_layout(void) {
  if (!s_messages_root) {
    return;
  }

  GRect bounds = layer_get_bounds(s_messages_root);
#if TOUCH_KEYBOARD_AVAILABLE
  s_kb_redraw_only = false;
  int visible_h = s_touch_keyboard_open ? touch_keyboard_viewport_height(bounds) : bounds.size.h;
#else
  int visible_h = bounds.size.h;
#endif
  int max_w = message_bubble_width(bounds);
  int y = ROUND_UI ? 8 : 5;
  if (!s_at_oldest && s_message_count > 0) {
    y += OLDER_LOADER_H;  // where "Loading..." shows when scrolling up to the top
  }

  for (int i = 0; i < s_message_count; i++) {
    if (i > 0) {
      // Photos are square-edged, so they always get the wider gap.
      bool photo = s_messages[i - 1].image_placeholder || s_messages[i].image_placeholder;
      y += (same_author(&s_messages[i - 1], &s_messages[i]) && !photo) ? BUBBLE_GAP_SAME : BUBBLE_GAP_OTHER;
    }
    // A day's first message gets its label. The oldest one loaded only when nothing is older:
    // otherwise the page above may be the same day, and a label there would come and go.
    time_t t = message_time(&s_messages[i]);
    time_t before = i > 0 ? message_time(&s_messages[i - 1]) : 0;
    s_day_label[i] = t && (i > 0 ? (before && days_between(before, t) != 0) : s_at_oldest);
    if (s_day_label[i]) {
      y += DAY_LABEL_H;
    }
    s_message_y[i] = y;
    s_message_h[i] = layout_message(i, bounds, max_w);
    y += s_message_h[i] + reactions_overhang(&s_messages[i]);
    if (message_shows_status(i)) {
      y += STATUS_LINE_H;
    }
  }
  int bottom_pad = ROUND_UI ? 28 : 5;  // round: keeps the input bar inside the circle
  int compose_min_y = visible_h - compose_bar_h() - (ROUND_UI ? 28 : 5);
  bool reserve_compose_bubble = s_at_newest && !s_touch_keyboard_open;
  // Short chats sit at the bottom, just above the input bar, like Telegram.
  if (reserve_compose_bubble && s_message_count > 0 && y + BUBBLE_GAP_OTHER < compose_min_y) {
    int shift = compose_min_y - BUBBLE_GAP_OTHER - y;
    for (int i = 0; i < s_message_count; i++) {
      s_message_y[i] += shift;
    }
    y += shift;
  }
  if (reserve_compose_bubble) {
    s_compose_bubble_y = PG_MAX(y + BUBBLE_GAP_OTHER, compose_min_y);
    s_chat_content_height = s_compose_bubble_y + compose_bar_h() + bottom_pad;
  } else {
    s_compose_bubble_y = y + BUBBLE_GAP_OTHER;
    s_chat_content_height = y + bottom_pad;
  }
  s_chat_scroll_offset = clamp_scroll_offset(s_chat_scroll_offset);
}


static void scroll_to_bottom(bool animated) {
  recalc_message_layout();
  s_selected_message = s_at_newest ? s_message_count : (s_message_count > 0 ? s_message_count - 1 : -1);
  set_chat_scroll_offset(s_chat_content_height, animated);
  request_next_image();
}

// delta: px the view just scrolled, + toward the newest messages
static void compose_float_track(int delta) {
  if (delta == 0) {
    return;
  }
  if ((delta > 0) != (s_compose_float_run > 0)) {
    s_compose_float_run = 0;  // turned around
  }
  s_compose_float_run += delta;
  if (s_compose_float_run >= COMPOSE_FLOAT_TRIGGER) {
    s_compose_floating = true;
  } else if (s_compose_float_run <= -COMPOSE_FLOAT_TRIGGER) {
    s_compose_floating = false;
  }
}

static void go_to_bottom(void) {
  s_compose_floating = false;
  s_compose_float_run = 0;
  cancel_message_timeout();
  cancel_message_retry();
  s_loading_older_messages = false;
  s_loading_newer_messages = false;
  s_older_anchor_id[0] = '\0';
  s_newer_anchor_id[0] = '\0';
  s_older_anchor_y = 0;
  s_newer_anchor_y = 0;
  clear_message_stage();
  reset_message_stream_state();
  if (s_at_newest) {
    scroll_to_bottom(true);
  } else {
    s_user_scrolled_messages = false;
    s_message_scroll_direction = 1;
    request_newer_messages(false);
  }
}

static uint32_t chat_now_ms(void) {
  time_t seconds;
  uint16_t millis;
  time_ms(&seconds, &millis);
  return (uint32_t)seconds * 1000 + millis;
}

static void chat_scroll_timer_callback(void *data) {
  s_chat_scroll_timer = NULL;
  uint32_t elapsed = chat_now_ms() - s_chat_scroll_started_ms;
  if (elapsed >= CHAT_SCROLL_MS) {
    s_chat_scroll_offset = s_chat_scroll_target;
  } else {
    // Ease out (cubic): fast at first, settling gently, in 1/1024ths.
    int remaining = 1024 - (int)(elapsed * 1024 / CHAT_SCROLL_MS);
    int eased = 1024 - (remaining * remaining / 1024) * remaining / 1024;
    int delta = s_chat_scroll_target - s_chat_scroll_start;
    s_chat_scroll_offset = s_chat_scroll_start + (delta * eased) / 1024;
    s_chat_scroll_timer = app_timer_register(CHAT_SCROLL_FRAME_MS, chat_scroll_timer_callback, NULL);
  }

  mark_messages_dirty();
  request_next_image();
}

static void set_chat_scroll_offset_quiet(int target) {
  cancel_timer(&s_chat_scroll_timer);
  s_chat_scroll_offset = clamp_scroll_offset(target);
}

static void finish_pending_chat_scroll(void) {
  if (!s_chat_scroll_timer) {
    return;
  }
  app_timer_cancel(s_chat_scroll_timer);
  s_chat_scroll_timer = NULL;
  // The next scroll animates from what is on screen, not from the jump to this one's end.
  s_chat_scroll_resume = s_chat_scroll_offset;
  s_chat_scroll_resuming = true;
  s_chat_scroll_offset = clamp_scroll_offset(s_chat_scroll_target);
}

static void set_chat_scroll_offset(int target, bool animated) {
  target = clamp_scroll_offset(target);
  cancel_timer(&s_chat_scroll_timer);

  bool resuming = s_chat_scroll_resuming;
  s_chat_scroll_resuming = false;
  if (!animated) {
    s_chat_scroll_offset = target;
    mark_messages_dirty();
    request_next_image();
    return;
  }

  if (resuming) {
    s_chat_scroll_offset = s_chat_scroll_resume;
  }
  s_chat_scroll_start = s_chat_scroll_offset;
  s_chat_scroll_target = target;
  if (s_chat_scroll_start == s_chat_scroll_target) {
    mark_messages_dirty();
    request_next_image();
    return;
  }
  s_chat_scroll_started_ms = chat_now_ms();
  s_chat_scroll_timer = app_timer_register(CHAT_SCROLL_FRAME_MS, chat_scroll_timer_callback, NULL);
}

static void render_messages(void) {
  if (!s_messages_root) {
    return;
  }
  recalc_message_layout();
  if (!has_selected_message() && s_at_newest && s_message_count > 0) {
    s_selected_message = s_message_count;
    set_chat_scroll_offset(s_chat_content_height, false);
    return;
  }
  mark_messages_dirty();
  request_next_image();
}

#if TOUCH_KEYBOARD_AVAILABLE
// ---------------------------------------------------------------------------
// On-screen keyboard (touch watches: Pebble Time 2 / Emery, Round 2 / Gabbro)
//
// Layout is 20 "units" wide per row so every row lines up:
//   row 0: 10 letters x 2 units
//   row 1:  9 letters x 2 units, centred (1 unit pad each side)
//   row 2: shift 3 + 7 letters x 2 + delete 3
//   row 3: 123 4 + comma 2 + space 8 + period 2 + send 4
// Above the keys sit a 3-slot suggestion strip and the text field, which grows
// upward (up to KB_MAX_LINES) while the keys stay put. Taps are matched to the
// nearest key in the touched row, so there are no dead zones between keys.
// Keys commit on lift-off (slide to correct), except delete, which fires on
// touch-down and repeats while held. A drag across 2+ other letters is a swipe
// word instead; a drag along the space bar or the text moves the cursor.
// ---------------------------------------------------------------------------

#define KB_UNITS 20
#define KB_LINE_H 20
#define KB_FIELD_PAD 2
#define KB_INPUT_TOP_GAP 3
#define KB_INPUT_BOTTOM_GAP 2
#define KB_MAX_LINES 3
#define KB_MIN_CHAT_H 8
#define KB_WRAP_CAP 24
#define KB_STRIP_H 22
#define KB_ROW_H PBL_IF_ROUND_ELSE(24, 22)
#define KB_ROWS 4
#define KB_PAD 3
#define KB_ROUND_BOTTOM_GAP 22  // the keyboard colour continues below the keys, to the bezel
#define KB_REPEAT_DELAY_MS 450
#define KB_REPEAT_MS 90
#define KB_WORD_DELETE_AFTER 12  // repeats (about 1.5 s with the delay) before delete takes words
#define KB_SAVE_RETRY_MS 600
#define KB_DOUBLE_TAP_MS 400
#define KB_SLIDE_HOLD 3
#define KB_SPACE_DRAG_START 6
#define KB_FIELD_DRAG_START 6
#define KB_SPACE_DRAG_STEP 7
#define KB_SWIPE_OTHER_KEYS 2
#define KB_SWIPE_KEYS_MAX 41  // the word engine reads up to 40
#define KB_SWIPE_SAMPLE_PX 4
#define KB_TRAIL_POINTS 10
#define KB_SUGGESTIONS 3

typedef enum {
  KeyNone,
  KeyChar,
  KeyShift,
  KeyDelete,
  KeySymbols,
  KeySpace,
  KeySend,
  KeyText,
  KeySuggestion
} KeyKind;

typedef struct {
  KeyKind kind;
  char ch;
  int8_t row;
  int8_t index;
} KeyHit;

typedef enum {
  KbStripWords,  // completions for the word being typed
  KbStripUndo,   // slot 0 holds the word autocorrect replaced
  KbStripSwipe   // best swiped word plus alternatives
} KbStripMode;

static const char *const KB_ALPHA[3] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
static const char *const KB_SYMBOLS[3] = {"1234567890", "-/:;()$&@", ".,?!'\"+"};
static const char *const KB_SYMBOLS_2[3] = {"[]{}#%^*+=", "_\\|~<>`;:", ".,?!'\"@"};

static KeyboardMode s_kb_mode;
static char *s_kb_draft;
static char s_kb_target_id[MAX_ID];
static bool s_kb_caps_lock;
static bool s_kb_press_active;
static KeyHit s_kb_pressed;
static AppTimer *s_kb_repeat_timer;
static uint32_t s_kb_last_shift_ms;
static bool s_kb_touch_checked;
static bool s_kb_touch_ok;
static bool s_kb_awaiting_send;
static bool s_kb_send_keeps_draft;  // sent with no bubble shown: the draft holds the text until "sent"

// Cursor (byte offset into the draft) and the word-wrapped view of the draft.
static uint16_t s_kb_cursor;
static uint16_t s_kb_line_start[KB_WRAP_CAP];
static uint8_t s_kb_line_count = 1;
static uint8_t s_kb_first_line;
static uint8_t s_kb_visible_lines = 1;

// Suggestion strip, autocorrect undo and swipe state.
static bool s_kb_words_ready;
static KbStripMode s_kb_strip_mode;
static char s_kb_sugg[KB_SUGGESTIONS][KB_WORD_MAX];
// What a tap on the strip replaces in Undo / Swipe mode (Words mode finds the word itself).
static uint16_t s_kb_rep_start;
static uint16_t s_kb_rep_len;
static int s_kb_skip_ac_start = -1;
static bool s_kb_swiping;
static uint8_t s_kb_swipe_others;
static char s_kb_swipe_keys[KB_SWIPE_KEYS_MAX];
// The finger path in word-engine keyboard units (see kb_words.h), for shape matching.
#define KB_SWIPE_PTS 64
static uint8_t s_kb_swipe_x[KB_SWIPE_PTS];
static uint8_t s_kb_swipe_y[KB_SWIPE_PTS];
static uint8_t s_kb_swipe_pts;
static GPoint s_kb_last_point;
static GPoint s_kb_trail[KB_TRAIL_POINTS];
static uint8_t s_kb_trail_count;
static bool s_kb_space_drag;
static int s_kb_drag_anchor;  // space bar: finger x; text field: finger y
static uint8_t s_kb_repeats;  // delete repeats so far in this press
static bool s_kb_field_scroll;  // the drag in the field scrolls it instead of moving the cursor
static int s_kb_auto_sp = -1;  // cursor position right after an automatic space, if untouched since
static uint32_t s_kb_synced;  // hash of the draft Telegram has (0: none)
static AppTimer *s_kb_save_timer;

static bool kb_hit_equal(KeyHit a, KeyHit b) {
  return a.kind == b.kind && a.row == b.row && a.index == b.index;
}

static uint32_t kb_now_ms(void) {
  time_t seconds;
  uint16_t millis;
  time_ms(&seconds, &millis);
  return (uint32_t)seconds * 1000 + millis;
}

// Touch arrived in PebbleOS 4.9.164, and the "non-navigational" flag in 4.32.
static WatchInfoVersion kb_firmware(void) {
  return watch_info_get_firmware_version();
}

static bool kb_firmware_at_least(int major, int minor, int patch) {
  WatchInfoVersion v = kb_firmware();
  if (v.major != major) {
    return v.major > major;
  }
  if (v.minor != minor) {
    return v.minor > minor;
  }
  return v.patch >= patch;
}

static bool touch_keyboard_supported(void) {
  if (!TOUCH_KEYBOARD_ENABLED) {
    return false;
  }
  if (!s_kb_touch_checked) {
    s_kb_touch_checked = true;
    // Emery/Gabbro builds require PebbleOS 4.32+, which always has the touch API.
    s_kb_touch_ok = touch_service_is_enabled();
  }
  return s_kb_touch_ok;
}

static const char *const *kb_rows(void) {
  if (!s_touch_keyboard_symbols) {
    return KB_ALPHA;
  }
  return s_touch_keyboard_shift ? KB_SYMBOLS_2 : KB_SYMBOLS;
}

static int kb_keys_block_height(void) {
  return (KB_ROW_H * KB_ROWS) + KB_PAD;
}

static int kb_field_height_for(int lines) {
  return (lines * KB_LINE_H) + (KB_FIELD_PAD * 2);
}

static int kb_field_height(void) {
  return kb_field_height_for(s_kb_visible_lines);
}

static int kb_input_height(void) {
  return KB_INPUT_TOP_GAP + kb_field_height() + KB_INPUT_BOTTOM_GAP;
}

static int kb_block_height(void) {
  return kb_input_height() + KB_STRIP_H + kb_keys_block_height();
}

// Whole keyboard (input field + suggestion strip + keys) in message-layer
// coordinates. The keys stay at the bottom; the input field grows upward.
static GRect touch_keyboard_rect_for_bounds(GRect bounds) {
  int h = kb_block_height();
  int bottom = bounds.size.h - (ROUND_UI ? KB_ROUND_BOTTOM_GAP : 0);
  return GRect(0, PG_MAX(0, bottom - h), bounds.size.w, h);
}

// Height of the message area left visible above the keyboard.
static int touch_keyboard_viewport_height(GRect bounds) {
  return PG_MAX(1, touch_keyboard_rect_for_bounds(bounds).origin.y);
}

// Horizontal span usable between y_top and y_bottom. On round screens this is
// the circle's chord, so keys never fall off the curved edge.
static void kb_span(GRect bounds, int y_top, int y_bottom, int *x, int *w) {
  *x = 2;
  *w = bounds.size.w - 4;
#if defined(PBL_ROUND)
  if (s_messages_root) {
    int right;
    s_round_org = layer_get_frame(s_messages_root).origin;
    round_span(y_top, y_bottom, 2, x, &right);
    *w = PG_MAX(48, right - *x);
  }
#endif
}

static int kb_keys_top(GRect keyboard_rect) {
  return keyboard_rect.origin.y + keyboard_rect.size.h - kb_keys_block_height();
}

static int kb_strip_top(GRect keyboard_rect) {
  return kb_keys_top(keyboard_rect) - KB_STRIP_H;
}

static int kb_row_top(GRect keyboard_rect, int row) {
  return kb_keys_top(keyboard_rect) + (row * KB_ROW_H);
}

// Unit range [start, start + count) for a key in a row.
static void kb_key_units(int row, int index, int *start, int *count) {
  if (row == 0) {
    *start = index * 2;
    *count = 2;
  } else if (row == 1) {
    *start = 1 + index * 2;
    *count = 2;
  } else if (row == 2) {
    if (index == 0) {
      *start = 0;
      *count = 3;
    } else if (index == 8) {
      *start = 17;
      *count = 3;
    } else {
      *start = 3 + (index - 1) * 2;
      *count = 2;
    }
  } else {
    static const int starts[5] = {0, 4, 6, 14, 16};
    static const int counts[5] = {4, 2, 8, 2, 4};
    *start = starts[index];
    *count = counts[index];
  }
}

static int kb_row_key_count(int row) {
  return row == 0 ? 10 : row == 1 ? 9 : row == 2 ? 9 : 5;
}

static KeyHit kb_key_at(int row, int index) {
  KeyHit hit = {KeyNone, '\0', (int8_t)row, (int8_t)index};
  const char *const *rows = kb_rows();
  if (row == 0 || row == 1) {
    hit.kind = KeyChar;
    hit.ch = rows[row][index];
  } else if (row == 2) {
    if (index == 0) {
      hit.kind = KeyShift;
    } else if (index == 8) {
      hit.kind = KeyDelete;
    } else {
      hit.kind = KeyChar;
      hit.ch = rows[2][index - 1];
    }
  } else if (row == 3) {
    if (index == 1 || index == 3) {
      // Comma and period live on the bottom row: the most used punctuation.
      hit.kind = KeyChar;
      hit.ch = index == 1 ? ',' : '.';
    } else {
      hit.kind = index == 0 ? KeySymbols : index == 2 ? KeySpace : KeySend;
    }
  }
  return hit;
}

static GRect kb_key_rect(GRect bounds, GRect keyboard_rect, int row, int index) {
  int top = kb_row_top(keyboard_rect, row);
  int x;
  int w;
  int start;
  int count;
  kb_span(bounds, top, top + KB_ROW_H, &x, &w);
  kb_key_units(row, index, &start, &count);
  int left = x + (w * start) / KB_UNITS;
  int right = x + (w * (start + count)) / KB_UNITS;
  return GRect(left + 1, top + 1, PG_MAX(1, right - left - 2), KB_ROW_H - 2);
}

static KeyHit kb_hit_test(GRect bounds, GPoint point) {
  KeyHit none = {KeyNone, '\0', -1, -1};
  GRect keyboard_rect = touch_keyboard_rect_for_bounds(bounds);
  int rows_top = kb_row_top(keyboard_rect, 0);
  int rows_bottom = kb_row_top(keyboard_rect, KB_ROWS);
  int strip_top = kb_strip_top(keyboard_rect);
  if (point.y < keyboard_rect.origin.y) {
    return none;
  }
  if (point.y < strip_top) {
    KeyHit text = {KeyText, '\0', -1, -1};
    return text;
  }
  if (point.y < rows_top) {
    int x;
    int w;
    kb_span(bounds, strip_top, rows_top, &x, &w);
    int slot = w > 0 ? ((point.x - x) * KB_SUGGESTIONS) / w : 0;
    KeyHit strip = {KeySuggestion, '\0', -1, (int8_t)PG_MAX(0, PG_MIN(KB_SUGGESTIONS - 1, slot))};
    return strip;
  }
  // A little slack below the bottom row, where the bezel is.
  if (point.y > rows_bottom + 10) {
    return none;
  }
  int row = PG_MAX(0, PG_MIN(KB_ROWS - 1, (point.y - rows_top) / KB_ROW_H));
  int top = kb_row_top(keyboard_rect, row);
  int x;
  int w;
  kb_span(bounds, top, top + KB_ROW_H, &x, &w);
  int unit = w > 0 ? ((point.x - x) * KB_UNITS) / w : 0;
  unit = PG_MAX(0, PG_MIN(KB_UNITS - 1, unit));
  int count = kb_row_key_count(row);
  for (int i = 0; i < count; i++) {
    int start;
    int units;
    kb_key_units(row, i, &start, &units);
    if (unit < start + units || i == count - 1) {
      return kb_key_at(row, i);
    }
  }
  return none;
}

static bool kb_sentence_start(void) {
  size_t len = s_kb_cursor;
  if (len == 0) {
    return true;
  }
  if (s_kb_draft[len - 1] != ' ') {
    return false;
  }
  while (len > 0 && s_kb_draft[len - 1] == ' ') {
    len--;
  }
  if (len == 0) {
    return true;
  }
  char last = s_kb_draft[len - 1];
  return last == '.' || last == '!' || last == '?';
}

// Auto-capitalise the first letter of the message and of each sentence.
static void kb_update_auto_shift(void) {
  if (s_touch_keyboard_symbols || s_kb_caps_lock || s_kb_mode == KeyboardModeEdit) {
    return;
  }
  s_touch_keyboard_shift = kb_sentence_start();
}

// Only the keyboard changed, so the chat layout is still valid.
static void kb_dirty(void) {
  if (s_messages_root) {
    s_kb_redraw_only = true;
    mark_messages_dirty();
  }
}

static bool kb_is_continuation(char c) {
  return ((unsigned char)c & 0xc0) == 0x80;
}

// Byte offsets of the UTF-8 characters around `pos` (emoji are multi-byte).
static int kb_prev_char(int pos) {
  if (pos > 0) {
    pos--;
  }
  while (pos > 0 && kb_is_continuation(s_kb_draft[pos])) {
    pos--;
  }
  return pos;
}

static int kb_next_char(int pos) {
  return text_next_char(s_kb_draft, pos);
}

static int kb_measure(int from, int to) {
  return text_span_width(s_kb_draft, font_18(), from, to);
}

// Field rect span. Taken from the lowest line, the narrowest on a round
// screen, so the wrapping never depends on how many lines are showing.
static void kb_field_span(GRect bounds, int *x, int *w) {
  int bottom = kb_strip_top(touch_keyboard_rect_for_bounds(bounds)) - KB_INPUT_BOTTOM_GAP;
  kb_span(bounds, bottom - kb_field_height_for(1), bottom, x, w);
  *x += 4;
  *w -= 8;
}

static int kb_input_text_width(void) {
  GRect bounds = s_messages_root ? layer_get_bounds(s_messages_root) : GRect(0, 0, 200, 200);
  int x;
  int w;
  kb_field_span(bounds, &x, &w);
  return PG_MAX(20, w - 16);
}

static void kb_wrap(void) {
  int len = (int)strlen(s_kb_draft);
  int width = kb_input_text_width();
  int count = 1;
  int start = 0;
  s_kb_line_start[0] = 0;
  while (count < KB_WRAP_CAP) {
    int next = line_next(s_kb_draft, font_18(), start, width, len);
    // A trailing newline leaves one more (empty) line to type on.
    if (next <= start || (next >= len && s_kb_draft[next - 1] != '\n')) {
      break;
    }
    s_kb_line_start[count++] = (uint16_t)next;
    start = next;
  }
  s_kb_line_count = (uint8_t)count;
}

static int kb_line_of(int pos) {
  int line = 0;
  while (line + 1 < s_kb_line_count && s_kb_line_start[line + 1] <= pos) {
    line++;
  }
  return line;
}

// End of the text drawn on a line (its newline is not drawn).
static int kb_line_end(int line) {
  if (line + 1 >= s_kb_line_count) {
    return (int)strlen(s_kb_draft);
  }
  int end = s_kb_line_start[line + 1];
  return s_kb_draft[end - 1] == '\n' ? end - 1 : end;
}

// Keeps the cursor line inside the field and sizes the field to the text. When
// the field grows or shrinks the chat above is re-laid out; the keys stay put.
static void kb_update_cursor_view(void) {
  // As many lines as fit above the strip while leaving a sliver of chat (2 on Round 2).
  int room = layer_get_bounds(s_messages_root).size.h - (ROUND_UI ? KB_ROUND_BOTTOM_GAP : 0) -
             kb_keys_block_height() - KB_STRIP_H - KB_INPUT_TOP_GAP - KB_INPUT_BOTTOM_GAP -
             (KB_FIELD_PAD * 2) - KB_MIN_CHAT_H;
  int visible = PG_MAX(1, PG_MIN((int)s_kb_line_count, PG_MIN(KB_MAX_LINES, room / KB_LINE_H)));
  int line = kb_line_of(s_kb_cursor);
  int first = PG_MIN((int)s_kb_first_line, s_kb_line_count - visible);
  if (line < first) {
    first = line;
  } else if (line >= first + visible) {
    first = line - visible + 1;
  }
  s_kb_first_line = (uint8_t)first;
  if (visible != s_kb_visible_lines) {
    s_kb_visible_lines = (uint8_t)visible;
    if (s_touch_keyboard_open && s_messages_root) {
      recalc_message_layout();
      set_chat_scroll_offset(s_chat_content_height, false);
    }
  }
}

static bool kb_is_letter(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// The word that ends at the cursor, as plain letters and apostrophes. False
// when the cursor is mid-word or next to digits / non-ASCII text.
static bool kb_word_before_cursor(int *start) {
  int end = s_kb_cursor;
  char after = s_kb_draft[end];
  if (kb_is_letter(after) || after == '\'' || (unsigned char)after >= 0x80) {
    return false;
  }
  int first = end;
  while (first > 0 && (kb_is_letter(s_kb_draft[first - 1]) || s_kb_draft[first - 1] == '\'')) {
    first--;
  }
  while (first < end && s_kb_draft[first] == '\'') {
    first++;  // an opening quote is not part of the word
  }
  if (first == end || end - first >= KB_WORD_MAX) {
    return false;
  }
  char before = first > 0 ? s_kb_draft[first - 1] : ' ';
  if ((before >= '0' && before <= '9') || (unsigned char)before >= 0x80) {
    return false;
  }
  *start = first;
  return true;
}

// Gives the lowercase `word` the letter case of `typed`: Teh -> The, TEH -> THE.
static void kb_match_case(const char *typed, char *word) {
  int letters = 0;
  int upper = 0;
  for (const char *c = typed; *c; c++) {
    if (kb_is_letter(*c)) {
      letters++;
      upper += *c <= 'Z';
    }
  }
  bool all_caps = letters >= 2 && upper == letters;
  bool first_cap = typed[0] >= 'A' && typed[0] <= 'Z';
  for (char *c = word; *c; c++) {
    if ((all_caps || (first_cap && c == word)) && *c >= 'a' && *c <= 'z') {
      *c = (char)(*c - 'a' + 'A');
    }
  }
}

static void kb_clear_strip(void) {
  memset(s_kb_sugg, 0, sizeof(s_kb_sugg));
}

// Copies the word draft[start, cursor) to `typed` and its lowercase form to `lower`.
static void kb_copy_word(int start, char *typed, char *lower) {
  int length = s_kb_cursor - start;
  memcpy(typed, s_kb_draft + start, length);
  typed[length] = '\0';
  for (int i = 0; i <= length; i++) {
    lower[i] = (typed[i] >= 'A' && typed[i] <= 'Z') ? (char)(typed[i] - 'A' + 'a') : typed[i];
  }
}

// Completions for the word being typed.
static void kb_refresh_strip(void) {
  kb_clear_strip();
  int start;
  if (!s_kb_words_ready || !kb_word_before_cursor(&start)) {
    return;
  }
  char typed[KB_WORD_MAX];
  char prefix[KB_WORD_MAX];
  kb_copy_word(start, typed, prefix);
  char found[KB_SUGGESTIONS][KB_WORD_MAX];
  int count = kb_words_complete(prefix, found, KB_SUGGESTIONS);
  for (int i = 0; i < count && i < KB_SUGGESTIONS; i++) {
    copy_cstr(s_kb_sugg[i], KB_WORD_MAX, found[i]);
    kb_match_case(typed, s_kb_sugg[i]);
  }
}

// The text changed in a way that is not an autocorrect or a swipe.
static void kb_layout_changed(void) {
  kb_wrap();
  kb_update_cursor_view();
  kb_update_auto_shift();
  kb_dirty();
}

static void kb_edited(void) {
  s_kb_auto_sp = -1;
  s_kb_strip_mode = KbStripWords;
  kb_layout_changed();
}

static void kb_draft_changed(void) {
  kb_edited();
  kb_refresh_strip();
}

// The cursor moved without an edit. The strip is refreshed once the finger lifts.
static void kb_cursor_moved(void) {
  s_kb_auto_sp = -1;
  s_kb_skip_ac_start = -1;
  s_kb_strip_mode = KbStripWords;
  kb_clear_strip();
  kb_update_cursor_view();
  kb_update_auto_shift();
  kb_dirty();
}

// Replaces draft[start, start + old_len) with text; false (draft untouched)
// when it would not fit.
static bool kb_replace(int start, int old_len, const char *text) {
  int len = (int)strlen(s_kb_draft);
  int add = (int)strlen(text);
  if (len - old_len + add + 1 > (int)TOUCH_KEYBOARD_MAX_TEXT) {
    show_status("Message full");
    vibes_short_pulse();
    return false;
  }
  memmove(s_kb_draft + start + add, s_kb_draft + start + old_len, len - start - old_len + 1);
  memcpy(s_kb_draft + start, text, add);
  return true;
}

static bool kb_insert(const char *text) {
  if (!kb_replace(s_kb_cursor, 0, text)) {
    return false;
  }
  s_kb_cursor = (uint16_t)(s_kb_cursor + strlen(text));
  return true;
}

// Punctuation typed right after an automatic space takes the space back.
static void kb_take_back_space(char ch) {
  int pos = s_kb_cursor;
  if (pos == s_kb_auto_sp && s_kb_draft[pos - 1] == ' ' && strchr(".,!?;:)'\"", ch) &&
      kb_replace(pos - 1, 1, "")) {
    s_kb_cursor = (uint16_t)(pos - 1);
    s_kb_skip_ac_start = s_kb_rep_start;  // the word before it was entered on purpose
  }
}

// A space after a space that followed a word becomes ". ".
static bool kb_double_space(void) {
  int pos = s_kb_cursor;
  if (pos < 2 || s_kb_draft[pos - 1] != ' ') {
    return false;
  }
  char before = s_kb_draft[pos - 2];
  return (kb_is_letter(before) || (before >= '0' && before <= '9') || strchr("\"')]}", before)) &&
         kb_replace(pos - 1, 1, ". ");
}

static void kb_type_char(char ch) {
  kb_take_back_space(ch);
  if (!s_touch_keyboard_symbols && s_touch_keyboard_shift && ch >= 'a' && ch <= 'z') {
    ch = (char)(ch - 'a' + 'A');
  }
  char text[2] = {ch, '\0'};
  kb_insert(text);
  if (!s_kb_caps_lock && !s_touch_keyboard_symbols) {
    s_touch_keyboard_shift = false;
  }
  kb_draft_changed();
}

static bool kb_is_terminator(char ch) {
  return ch == ' ' || (ch && strchr(".,!?;:)", ch));
}

// A lone "i" (also i'm, i'll...) is always "I", with or without the word engine.
static void kb_capitalise_i(char *word) {
  if (word[0] == 'i' && (word[1] == '\0' || word[1] == '\'')) {
    word[0] = 'I';
  }
}

// Corrects the word before the cursor (case kept: Teh -> The, TEH -> THE,
// i'm -> I'm). Returns true when the text changed; the original goes in the
// first strip slot, where a tap undoes the correction.
static bool kb_autocorrect_word(int start) {
  char typed[KB_WORD_MAX];
  char lower[KB_WORD_MAX];
  char fixed[KB_WORD_MAX];
  kb_copy_word(start, typed, lower);
  if (s_kb_words_ready && kb_words_correct(lower, fixed, sizeof(fixed))) {
    kb_match_case(typed, fixed);
  } else {
    copy_cstr(fixed, sizeof(fixed), typed);
  }
  kb_capitalise_i(fixed);
  int length = s_kb_cursor - start;
  if (strcmp(fixed, typed) == 0 || !kb_replace(start, length, fixed)) {
    return false;
  }
  kb_clear_strip();
  copy_cstr(s_kb_sugg[0], KB_WORD_MAX, typed);
  s_kb_rep_start = (uint16_t)start;
  s_kb_rep_len = (uint16_t)strlen(fixed);
  s_kb_cursor = (uint16_t)(start + s_kb_rep_len);
  return true;
}

// Types a character that ends a word, autocorrecting that word first.
static void kb_type_terminator(char ch) {
  if (ch == ' ' && kb_double_space()) {
    s_kb_cursor++;
    kb_draft_changed();
    return;
  }
  kb_take_back_space(ch);
  int start;
  bool corrected = kb_word_before_cursor(&start) && start != s_kb_skip_ac_start &&
                   kb_autocorrect_word(start);
  s_kb_skip_ac_start = -1;
  char text[2] = {ch, '\0'};
  kb_insert(text);
  kb_edited();
  if (corrected) {
    s_kb_strip_mode = KbStripUndo;  // backspace undoes it as well
  } else {
    kb_refresh_strip();
  }
}

// Puts a space after the word just entered (or steps over the one already there).
// The caller marks it with s_kb_auto_sp once the edit is done.
static void kb_auto_space(void) {
  if (s_kb_draft[s_kb_cursor] == ' ') {
    s_kb_cursor++;
  } else {
    kb_insert(" ");
  }
}

// Tap on a strip slot: replace the word (or the swiped word) and add a space.
// In Undo mode slot 0 puts the original word back and leaves the text after it.
static void kb_apply_suggestion(int slot) {
  if (!s_kb_sugg[slot][0] || (s_kb_strip_mode == KbStripUndo && slot > 0)) {
    return;
  }
  int start;
  int old_len;
  if (s_kb_strip_mode != KbStripWords) {
    start = s_kb_rep_start;
    old_len = s_kb_rep_len;
  } else if (kb_word_before_cursor(&start)) {
    old_len = s_kb_cursor - start;
  } else {
    return;
  }
  if (!kb_replace(start, old_len, s_kb_sugg[slot])) {
    return;
  }
  int end = start + (int)strlen(s_kb_sugg[slot]);
  bool undo = s_kb_strip_mode == KbStripUndo;
  if (undo) {
    s_kb_cursor = (uint16_t)(s_kb_cursor + end - (start + old_len));
    s_kb_skip_ac_start = start;  // do not correct this word again
  } else {
    s_kb_cursor = (uint16_t)end;
    kb_auto_space();
    s_kb_rep_start = (uint16_t)start;
  }
  kb_draft_changed();
  if (!undo) {
    s_kb_auto_sp = s_kb_cursor;
  }
}

// Deletes one whole character, including multi-byte UTF-8 (emoji in an edit).
// Right after an autocorrect it first puts the original word back.
static void kb_delete(void) {
  if (s_kb_strip_mode == KbStripUndo) {
    kb_apply_suggestion(0);
  }
  if (s_kb_cursor == 0) {
    return;
  }
  // Right after a swipe: the whole word and its space.
  int start = s_kb_strip_mode == KbStripSwipe ? s_kb_rep_start : kb_prev_char(s_kb_cursor);
  kb_replace(start, s_kb_cursor - start, "");
  s_kb_cursor = (uint16_t)start;
  kb_draft_changed();
}

// Inserts the best swiped word (a space first when the text before it needs
// one, and one after it) and offers the alternatives in the strip.
static void kb_commit_swipe(void) {
  char found[KB_SUGGESTIONS][KB_WORD_MAX];
  int count = s_kb_words_ready ? kb_words_swipe(s_kb_swipe_keys, s_kb_swipe_x, s_kb_swipe_y, s_kb_swipe_pts,
                                                   found, KB_SUGGESTIONS) : 0;
  if (count <= 0) {
    show_status("No word found");
    return;
  }
  const char *style = s_kb_caps_lock ? "AA" : (s_touch_keyboard_shift && !s_touch_keyboard_symbols) ? "A" : "a";
  for (int i = 0; i < count && i < KB_SUGGESTIONS; i++) {
    kb_match_case(style, found[i]);
    kb_capitalise_i(found[i]);
  }
  char previous = s_kb_cursor > 0 ? s_kb_draft[s_kb_cursor - 1] : ' ';
  if (previous != ' ' && previous != '\n' && previous != '(' && !kb_insert(" ")) {
    return;
  }
  s_kb_rep_start = s_kb_cursor;
  s_kb_rep_len = (uint16_t)strlen(found[0]);
  if (!kb_insert(found[0])) {
    return;
  }
  kb_auto_space();
  if (!s_kb_caps_lock) {
    s_touch_keyboard_shift = false;
  }
  kb_edited();
  s_kb_auto_sp = s_kb_cursor;
  s_kb_strip_mode = KbStripSwipe;
  kb_clear_strip();
  for (int i = 0; i < count && i < KB_SUGGESTIONS; i++) {
    copy_cstr(s_kb_sugg[i], KB_WORD_MAX, found[i]);
  }
}

// Moves the cursor to the character nearest a touch in the field.
static void kb_set_cursor_from_point(GRect bounds, GPoint point) {
  GRect rect = touch_keyboard_rect_for_bounds(bounds);
  int rows = point.y - (rect.origin.y + KB_INPUT_TOP_GAP + KB_FIELD_PAD);
  int line = s_kb_first_line + (rows < 0 ? -1 : rows / KB_LINE_H);
  line = PG_MAX(0, PG_MIN(line, s_kb_line_count - 1));
  int start = s_kb_line_start[line];
  int end = kb_line_end(line);
  if (line + 1 < s_kb_line_count && end == s_kb_line_start[line + 1]) {
    end = kb_prev_char(end);  // end of a wrapped line is the start of the next
  }
  int field_x;
  int field_w;
  kb_field_span(bounds, &field_x, &field_w);
  int target = point.x - (field_x + 8);
  int best = start;
  int best_w = 0;
  while (best < end) {
    int next = kb_next_char(best);
    int w = kb_measure(start, next);
    if (target < (best_w + w) / 2) {
      break;
    }
    best = next;
    best_w = w;
  }
  if (best != s_kb_cursor) {
    s_kb_cursor = (uint16_t)best;
    kb_cursor_moved();
  }
}

// Vertical drag in the field: one line per line height of finger travel.
// The view stays where it is put until the cursor moves or the text changes.
static void kb_scroll_field(int y) {
  int lines = (y - s_kb_drag_anchor) / KB_LINE_H;
  if (lines == 0) {
    return;
  }
  s_kb_drag_anchor += lines * KB_LINE_H;
  int first = PG_MAX(0, PG_MIN((int)s_kb_first_line - lines, s_kb_line_count - s_kb_visible_lines));
  if (first != s_kb_first_line) {
    s_kb_first_line = (uint8_t)first;
    kb_dirty();
  }
}

// Drag along the space bar: one character per KB_SPACE_DRAG_STEP pixels.
static void kb_space_drag(int x) {
  int steps = (x - s_kb_drag_anchor) / KB_SPACE_DRAG_STEP;
  if (steps == 0) {
    return;
  }
  s_kb_drag_anchor += steps * KB_SPACE_DRAG_STEP;
  for (; steps > 0 && s_kb_cursor < (int)strlen(s_kb_draft); steps--) {
    s_kb_cursor = (uint16_t)kb_next_char(s_kb_cursor);
  }
  for (; steps < 0 && s_kb_cursor > 0; steps++) {
    s_kb_cursor = (uint16_t)kb_prev_char(s_kb_cursor);
  }
  kb_cursor_moved();
}

// Held long enough, delete takes whole words (back to the start of the previous one).
static void kb_delete_word(void) {
  int start = s_kb_cursor;
  while (start > 0 && s_kb_draft[start - 1] == ' ') {
    start--;
  }
  while (start > 0 && s_kb_draft[start - 1] != ' ') {
    start--;
  }
  kb_replace(start, s_kb_cursor - start, "");
  s_kb_cursor = (uint16_t)start;
  kb_draft_changed();
}

static void kb_cancel_repeat(void) {
  cancel_timer(&s_kb_repeat_timer);
}

static void kb_repeat_callback(void *context) {
  s_kb_repeat_timer = NULL;
  if (!s_touch_keyboard_open || !s_kb_press_active || s_kb_pressed.kind != KeyDelete ||
      s_kb_cursor == 0) {
    return;
  }
  if (++s_kb_repeats > KB_WORD_DELETE_AFTER) {
    kb_delete_word();
  } else {
    kb_delete();
  }
  s_kb_repeat_timer = app_timer_register(KB_REPEAT_MS, kb_repeat_callback, NULL);
}

static void kb_reset_press(void) {
  kb_cancel_repeat();
  s_kb_repeats = 0;
  s_kb_press_active = false;
  s_kb_pressed.kind = KeyNone;
  s_kb_pressed.row = -1;
  s_kb_pressed.index = -1;
  s_kb_swiping = false;
  s_kb_swipe_others = 0;
  s_kb_swipe_keys[0] = '\0';
  s_kb_swipe_pts = 0;
  s_kb_trail_count = 0;
  s_kb_space_drag = false;
  s_kb_field_scroll = false;
}

static const char *kb_placeholder(void) {
  switch (s_kb_mode) {
    case KeyboardModeReply:
      return "Reply...";
    case KeyboardModeEdit:
      return "Edit message...";
    case KeyboardModeCompose:
      break;
  }
  return "Message...";
}

static const char *kb_send_label(void) {
  return s_kb_mode == KeyboardModeEdit ? "save" : "send";
}

static const char *kb_reply_target(void) {
  return s_kb_mode == KeyboardModeReply ? s_kb_target_id : "";
}

// 0 for no draft; otherwise a hash of the text and its reply target.
static uint32_t kb_draft_hash(const char *text, const char *reply_to) {
  uint32_t hash = 0;
  for (; *text; text++) {
    hash = hash * 31 + (uint8_t)*text;
  }
  for (; hash && *reply_to; reply_to++) {
    hash = hash * 31 + (uint8_t)*reply_to;
  }
  return hash;
}

static void kb_save_retry(void *context);

// Hands the draft to Telegram, via the phone, unless it is what Telegram has.
// When the outbox is busy it tries once more shortly. Edits are not drafts.
static void kb_save_draft(bool retry_later) {
  const char *reply_to = kb_reply_target();
  uint32_t hash = kb_draft_hash(s_kb_draft, reply_to);
  if (s_kb_mode == KeyboardModeEdit || hash == s_kb_synced) {
    return;
  }
  if (send_command_with_status("save_draft", s_current_chat_id, s_kb_draft,
                               reply_to[0] ? reply_to : NULL, NULL, false)) {
    s_kb_synced = hash;
  } else if (retry_later && !s_kb_save_timer) {
    s_kb_save_timer = app_timer_register(KB_SAVE_RETRY_MS, kb_save_retry, NULL);
  }
}

static void kb_save_retry(void *context) {
  s_kb_save_timer = NULL;
  kb_save_draft(false);
}

// The phone sends the Telegram draft of the open chat (also when it changes on another device).
// A draft being typed here wins; otherwise the draft is loaded, but the keyboard stays shut.
static void touch_keyboard_draft_received(DictionaryIterator *iter) {
  const char *chat_id = tuple_cstring(iter, MESSAGE_KEY_ChatId);
  const char *text = tuple_cstring(iter, MESSAGE_KEY_Text);
  const char *reply_to = tuple_cstring(iter, MESSAGE_KEY_ReplyTo);
  if (!chat_id || !text || strcmp(chat_id, s_current_chat_id) != 0 || s_kb_mode == KeyboardModeEdit ||
      kb_draft_hash(s_kb_draft, kb_reply_target()) != s_kb_synced) {
    return;
  }
  reply_to = text[0] && reply_to ? reply_to : "";
  s_kb_mode = reply_to[0] ? KeyboardModeReply : KeyboardModeCompose;
  copy_cstr(s_kb_target_id, sizeof(s_kb_target_id), reply_to);
  copy_cstr(s_kb_draft, TOUCH_KEYBOARD_MAX_TEXT, text);
  s_kb_cursor = (uint16_t)strlen(s_kb_draft);
  s_kb_synced = kb_draft_hash(s_kb_draft, reply_to);
  if (s_touch_keyboard_open) {
    kb_draft_changed();
  } else {
    mark_messages_dirty();
  }
}

static void close_touch_keyboard(void) {
  kb_reset_press();
  if (!s_touch_keyboard_open) {
    return;
  }
  // The draft is kept, so reopening the keyboard picks up where you left off.
  s_touch_keyboard_open = false;
  kb_save_draft(true);
  if (s_kb_words_ready) {
    kb_words_deinit();  // the word lists are only held while typing
    s_kb_words_ready = false;
  }
  if (s_messages_root) {
    recalc_message_layout();
    mark_messages_dirty();
  }
}

// Forget the draft entirely (after sending, or when leaving the chat, once Telegram has it).
static void reset_touch_keyboard(void) {
  kb_save_draft(true);
  close_touch_keyboard();
  s_kb_synced = 0;
  s_kb_draft[0] = '\0';
  s_kb_target_id[0] = '\0';
  s_kb_mode = KeyboardModeCompose;
  s_kb_cursor = 0;
  s_kb_line_count = 1;
  s_kb_first_line = 0;
  s_kb_visible_lines = 1;
  s_kb_skip_ac_start = -1;
  s_kb_strip_mode = KbStripWords;
  kb_clear_strip();
  s_kb_caps_lock = false;
  s_touch_keyboard_symbols = false;
  s_touch_keyboard_shift = false;
}

static void open_touch_keyboard(KeyboardMode mode, const char *target_id, const char *initial_text) {
  if (!touch_keyboard_supported() || s_view_state != ViewStateChat || !s_messages_root) {
    return;
  }
  if (mode == KeyboardModeCompose && s_kb_draft[0]) {
    // "New message" / "Keyboard" resumes whatever you were typing.
    mode = s_kb_mode;
    target_id = s_kb_target_id[0] ? s_kb_target_id : NULL;
  }
  bool same_target = mode == s_kb_mode &&
                     strcmp(s_kb_target_id, target_id ? target_id : "") == 0;
  if (!same_target) {
    // Different conversation target: start fresh (edits start from the message text).
    s_kb_draft[0] = '\0';
    s_kb_cursor = 0;
  }
  s_kb_mode = mode;
  copy_cstr(s_kb_target_id, sizeof(s_kb_target_id), target_id);
  if (initial_text && !s_kb_draft[0]) {
    copy_cstr(s_kb_draft, TOUCH_KEYBOARD_MAX_TEXT, initial_text);
    s_kb_cursor = (uint16_t)strlen(s_kb_draft);
  }
  s_kb_cursor = (uint16_t)PG_MIN((int)s_kb_cursor, (int)strlen(s_kb_draft));
  s_touch_keyboard_symbols = false;
  s_kb_caps_lock = false;
  s_touch_keyboard_shift = false;
  s_kb_skip_ac_start = -1;
  s_kb_strip_mode = KbStripWords;
  kb_reset_press();
  kb_wrap();
  kb_update_cursor_view();  // sizes the field before the chat is laid out
  s_touch_keyboard_open = true;
  if (!s_kb_words_ready) {
    kb_words_init();
    s_kb_words_ready = true;
  }
  kb_update_auto_shift();
  kb_refresh_strip();
  show_status(mode == KeyboardModeEdit ? "Edit message" : mode == KeyboardModeReply ? "Reply" :
                                                                                       "Type message");
  recalc_message_layout();
  set_chat_scroll_offset(s_chat_content_height, true);
  mark_messages_dirty();
}

static void kb_mark_pending_failed(void) {
  int index = find_message_index_by_id("pending");
  if (index >= 0) {
    Message *message = &s_messages[index];
    char text[TOUCH_KEYBOARD_MAX_TEXT];
    copy_cstr(text, sizeof(text), message->text);
    set_message_strings(message, NULL, text, NULL, "not sent", NULL);
    copy_cstr(message->id, sizeof(message->id), "failed");
    // The failed bubble still holds the text, so it comes back as the draft.
    if (!s_kb_draft[0]) {
      copy_cstr(s_kb_draft, TOUCH_KEYBOARD_MAX_TEXT, text);
      s_kb_cursor = (uint16_t)strlen(s_kb_draft);
      if (s_touch_keyboard_open) {
        kb_draft_changed();
      }
    }
  }
  s_kb_awaiting_send = false;
  if (s_messages_root) {
    recalc_message_layout();
    mark_messages_dirty();
  }
}

// Called from the inbox for "sent" / "error" replies.
static bool touch_keyboard_has_draft(void) {
  return s_kb_draft[0] != '\0';
}

static void touch_keyboard_send_result(bool ok) {
  if (!s_kb_awaiting_send) {
    if (!ok) {
      kb_mark_pending_failed();  // a voice or draft bar send: its bubble says it didn't go
    }
    return;
  }
  if (ok) {
    s_kb_awaiting_send = false;
    if (s_kb_send_keeps_draft) {
      s_kb_send_keeps_draft = false;
      s_kb_draft[0] = '\0';
      s_kb_cursor = 0;
      s_kb_synced = 0;
    }
  } else {
    s_kb_send_keeps_draft = false;
    kb_mark_pending_failed();
    show_status("Not sent - reopen keyboard to retry");
  }
}

static void kb_trim_trailing_spaces(char *text) {
  size_t len = strlen(text);
  while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\n')) {
    text[--len] = '\0';
  }
}

static void kb_send(void) {
  char text[TOUCH_KEYBOARD_MAX_TEXT];
  copy_cstr(text, sizeof(text), s_kb_draft);
  kb_trim_trailing_spaces(text);
  if (!text[0]) {
    show_status("Type a message first");
    return;
  }

  if (s_kb_mode == KeyboardModeEdit) {
    if (!s_kb_target_id[0]) {
      show_status("No edit target");
      return;
    }
    show_status("Editing...");
    if (!send_command("edit_message", s_current_chat_id, text, NULL, s_kb_target_id)) {
      return;  // Busy: keep the keyboard open so nothing is lost.
    }
    reset_touch_keyboard();
    return;
  }

  const char *reply_to = (s_kb_mode == KeyboardModeReply && s_kb_target_id[0]) ? s_kb_target_id : NULL;
  show_status("Sending...");
  if (!send_command("send_message", s_current_chat_id, text, reply_to, NULL)) {
    return;  // Busy: keep the draft and keyboard so the user can tap send again.
  }

  s_kb_awaiting_send = true;
  if (!s_at_newest) {
    // Sent from the floating bar on an older page: load the newest page, which brings the new
    // message with it, instead of adding it after messages that skip the ones in between. No
    // bubble holds the text meanwhile, so the draft keeps it until the phone says "sent".
    s_kb_send_keeps_draft = true;
    s_kb_draft[0] = '\0';  // the phone clears the Telegram draft itself
    s_kb_synced = 0;
    reset_touch_keyboard();
    copy_cstr(s_kb_draft, TOUCH_KEYBOARD_MAX_TEXT, text);
    s_kb_cursor = (uint16_t)strlen(s_kb_draft);
    s_kb_synced = kb_draft_hash(s_kb_draft, kb_reply_target());  // local only, not saved as a draft
    go_to_bottom();
    return;
  }
  s_kb_send_keeps_draft = false;
  s_kb_draft[0] = '\0';  // the phone clears the Telegram draft itself
  s_kb_synced = 0;
  reset_touch_keyboard();

  show_pending_bubble(text);
}

// A message just sent shows straight away, with nothing under it: the real one replaces it when
// the phone's sync brings it, with its time and status (Sent, Delivered, Seen).
static void show_pending_bubble(const char *text) {
  int stale = find_message_index_by_id("failed");
  if (stale >= 0) {
    remove_message_at(stale);
  }
  Message *slot = append_message_slot();
  if (!slot) {
    return;
  }
  copy_cstr(slot->id, sizeof(slot->id), "pending");
  set_message_strings(slot, NULL, text, NULL, NULL, NULL);
  slot->outgoing = true;
  s_at_newest = true;
  s_user_scrolled_messages = false;
  s_selected_message = s_message_count - 1;
  if (s_messages_root) {
    recalc_message_layout();
    set_chat_scroll_offset(s_chat_content_height, true);
    mark_messages_dirty();
  }
}

static void kb_activate(KeyHit hit) {
  switch (hit.kind) {
    case KeyChar:
      if (kb_is_terminator(hit.ch)) {
        kb_type_terminator(hit.ch);
      } else {
        kb_type_char(hit.ch);
      }
      return;
    case KeySpace:
      kb_type_terminator(' ');
      return;
    case KeySuggestion:
      kb_apply_suggestion(hit.index);
      return;
    case KeyShift: {
      uint32_t now = kb_now_ms();
      if (s_touch_keyboard_symbols) {
        s_touch_keyboard_shift = !s_touch_keyboard_shift;  // second symbols page
      } else if (s_kb_caps_lock) {
        s_kb_caps_lock = false;
        s_touch_keyboard_shift = false;
      } else if (s_touch_keyboard_shift && now - s_kb_last_shift_ms < KB_DOUBLE_TAP_MS) {
        s_kb_caps_lock = true;  // double-tap shift = caps lock
      } else {
        s_touch_keyboard_shift = !s_touch_keyboard_shift;
      }
      s_kb_last_shift_ms = now;
      kb_dirty();
      return;
    }
    case KeySymbols:
      s_touch_keyboard_symbols = !s_touch_keyboard_symbols;
      s_touch_keyboard_shift = false;
      s_kb_caps_lock = false;
      kb_update_auto_shift();
      kb_dirty();
      return;
    case KeySend:
      kb_send();
      return;
    case KeyDelete:
    case KeyText:
    case KeyNone:
      return;
  }
}

static void draw_touch_keyboard_key(GContext *ctx, GRect rect, const char *label, bool pressed,
                                    bool special, bool accent) {
  GColor fill = pressed ? APP_COLOR : accent ? APP_COLOR : special ? GColorLightGray : GColorWhite;
  GColor text = (pressed || accent) ? GColorWhite : GColorBlack;
  if (pressed && accent) {
    fill = GColorPictonBlue;
  }
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, rect, 3, GCornersAll);
  graphics_context_set_text_color(ctx, text);
  GFont font = fonts_get_system_font(strlen(label) > 1 ? FONT_KEY_GOTHIC_14_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  int text_y = rect.origin.y + (strlen(label) > 1 ? 2 : -2);
  graphics_draw_text(ctx, label, font, GRect(rect.origin.x, text_y, rect.size.w, rect.size.h),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// Shift is a drawn up arrow: outline when off, filled when on, with a bar
// underneath for caps lock. Fonts can't be relied on for an arrow glyph.
static void kb_draw_shift_arrow(GContext *ctx, GRect rect, GColor color, bool filled, bool caps) {
  int cx = rect.origin.x + rect.size.w / 2;
  int size = PG_MIN(rect.size.w - 4, rect.size.h - 6);
  int half = PG_MAX(3, size / 2);
  int top = rect.origin.y + (rect.size.h - size) / 2 - (caps ? 1 : 0);
  int mid = top + half;
  int stem_w = PG_MAX(2, half / 2);
  int bottom = top + size - (caps ? 2 : 0);
  GPoint points[7] = {
    {cx, top},
    {cx + half, mid},
    {cx + stem_w, mid},
    {cx + stem_w, bottom},
    {cx - stem_w, bottom},
    {cx - stem_w, mid},
    {cx - half, mid}
  };
  GPathInfo info = {7, points};
  GPath *path = gpath_create(&info);
  if (!path) {
    return;
  }
  graphics_context_set_fill_color(ctx, color);
  graphics_context_set_stroke_color(ctx, color);
  if (filled) {
    gpath_draw_filled(ctx, path);
  }
  gpath_draw_outline(ctx, path);
  gpath_destroy(path);
  if (caps) {
    graphics_fill_rect(ctx, GRect(cx - half + 1, bottom + 2, (half * 2) - 1, 2), 0, GCornerNone);
  }
}

static void kb_key_label(KeyHit key, char *buffer, size_t size) {
  switch (key.kind) {
    case KeyChar:
      buffer[0] = key.ch;
      if (!s_touch_keyboard_symbols && s_touch_keyboard_shift && key.ch >= 'a' && key.ch <= 'z') {
        buffer[0] = (char)(key.ch - 'a' + 'A');
      }
      buffer[1] = '\0';
      return;
    case KeyShift:
      copy_cstr(buffer, size, s_touch_keyboard_symbols ? (s_touch_keyboard_shift ? "123" : "#+=") :
                              s_kb_caps_lock ? "CAPS" : s_touch_keyboard_shift ? "SHIFT" : "shift");
      return;
    case KeyDelete:
      copy_cstr(buffer, size, "del");
      return;
    case KeySymbols:
      copy_cstr(buffer, size, s_touch_keyboard_symbols ? "ABC" : "123");
      return;
    case KeySpace:
      copy_cstr(buffer, size, "space");
      return;
    case KeySend:
      copy_cstr(buffer, size, kb_send_label());
      return;
    case KeyText:
    case KeySuggestion:
    case KeyNone:
      break;
  }
  buffer[0] = '\0';
}

// Big preview of the key under your finger, drawn well above it so the finger
// doesn't hide it: a keyboard-coloured rounded square with a tapered tail
// pointing down toward the key. It follows the finger while sliding.
#define KB_PREVIEW_W 36
#define KB_PREVIEW_H 40
#define KB_PREVIEW_GAP 48
#define KB_PREVIEW_TAIL 12

static void kb_draw_key_preview(GContext *ctx, GRect bounds, GRect key_rect, KeyHit key) {
  char label[2] = {key.ch, '\0'};
  if (!s_touch_keyboard_symbols && s_touch_keyboard_shift && key.ch >= 'a' && key.ch <= 'z') {
    label[0] = (char)(key.ch - 'a' + 'A');
  }
  int key_cx = key_rect.origin.x + key_rect.size.w / 2;
  int head_bottom = key_rect.origin.y - KB_PREVIEW_GAP;
  int head_top = PG_MAX(2, head_bottom - KB_PREVIEW_H);
  head_bottom = head_top + KB_PREVIEW_H;

  // Keep the bubble on screen (inside the circle on Round 2).
  int span_x;
  int span_w;
  kb_span(bounds, head_top, head_bottom, &span_x, &span_w);
  int head_x = key_cx - KB_PREVIEW_W / 2;
  head_x = PG_MAX(span_x, PG_MIN(head_x, span_x + span_w - KB_PREVIEW_W));
  GRect head = GRect(head_x, head_top, KB_PREVIEW_W, KB_PREVIEW_H);

  // Tail: tapers from the bottom of the square toward the pressed key.
  int tail_cx = PG_MAX(head.origin.x + 8, PG_MIN(key_cx, head.origin.x + KB_PREVIEW_W - 8));
  GPoint tail_points[3] = {
    {tail_cx - 9, head_bottom - 2},
    {tail_cx + 9, head_bottom - 2},
    {tail_cx, head_bottom + KB_PREVIEW_TAIL}
  };
  GPathInfo tail_info = {3, tail_points};
  GPath *tail = gpath_create(&tail_info);

  // Outline in the keyboard's dark grey, then the white key face.
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, grect_inset(head, GEdgeInsets(-2)), 8, GCornersAll);
  if (tail) {
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
    graphics_context_set_stroke_width(ctx, 3);
    gpath_draw_outline(ctx, tail);
    graphics_context_set_stroke_width(ctx, 1);
  }
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, head, 6, GCornersAll);
  if (tail) {
    gpath_draw_filled(ctx, tail);
    gpath_destroy(tail);
  }
  // Accent bar ties it to the pressed-key colour.
  graphics_context_set_fill_color(ctx, APP_COLOR);
  graphics_fill_rect(ctx, GRect(head.origin.x + 6, head.origin.y + head.size.h - 5, head.size.w - 12, 2),
                     0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     GRect(head.origin.x, head.origin.y + 1, head.size.w, head.size.h - 6),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// Text field: the visible lines of the draft and the cursor.
static void kb_draw_field(GContext *ctx, GRect bounds, GRect keyboard_rect) {
  int span_x;
  int span_w;
  kb_field_span(bounds, &span_x, &span_w);
  GRect field = GRect(span_x, keyboard_rect.origin.y + KB_INPUT_TOP_GAP, span_w, kb_field_height());
  int radius = (KB_LINE_H / 2) + KB_FIELD_PAD;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, field, radius, GCornersAll);
  graphics_context_set_stroke_color(ctx, APP_COLOR);
  graphics_draw_round_rect(ctx, field, radius);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  int text_x = field.origin.x + 8;
  int text_w = field.size.w - 16;
  if (!s_kb_draft[0]) {
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, kb_placeholder(), font,
                       GRect(text_x, field.origin.y + KB_FIELD_PAD - 3, text_w, KB_LINE_H + 4),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    return;
  }
  graphics_context_set_text_color(ctx, GColorBlack);
  for (int row = 0; row < s_kb_visible_lines && s_kb_first_line + row < s_kb_line_count; row++) {
    int line = s_kb_first_line + row;
    int start = s_kb_line_start[line];
    int end = kb_line_end(line);
    if (end <= start) {
      continue;
    }
    char saved = s_kb_draft[end];
    s_kb_draft[end] = '\0';  // draw just this line
    graphics_draw_text(ctx, s_kb_draft + start, font,
                       GRect(text_x, field.origin.y + KB_FIELD_PAD - 3 + (row * KB_LINE_H), text_w + 8,
                             KB_LINE_H + 4),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    s_kb_draft[end] = saved;
  }
  int cursor_line = kb_line_of(s_kb_cursor);
  int cursor_row = cursor_line - s_kb_first_line;
  if (cursor_row >= 0 && cursor_row < s_kb_visible_lines) {
    int cursor_x = text_x + PG_MIN(text_w, kb_measure(s_kb_line_start[cursor_line], s_kb_cursor) + 1);
    graphics_context_set_fill_color(ctx, APP_COLOR);
    graphics_fill_rect(ctx, GRect(cursor_x, field.origin.y + KB_FIELD_PAD + (cursor_row * KB_LINE_H) + 2,
                                  2, KB_LINE_H - 4), 0, GCornerNone);
  }
}

// Three suggestion slots between the field and the keys. An accent first slot
// marks the autocorrect undo (original word) or the swiped word.
static void kb_draw_strip(GContext *ctx, GRect bounds, GRect keyboard_rect) {
  int top = kb_strip_top(keyboard_rect);
  int x;
  int w;
  kb_span(bounds, top, top + KB_STRIP_H, &x, &w);
  for (int slot = 0; slot < KB_SUGGESTIONS; slot++) {
    if (s_kb_sugg[slot][0]) {
      int left = x + (w * slot) / KB_SUGGESTIONS;
      int right = x + (w * (slot + 1)) / KB_SUGGESTIONS;
      draw_touch_keyboard_key(ctx, GRect(left + 1, top + 1, right - left - 2, KB_STRIP_H - 2),
                              s_kb_sugg[slot],
                              s_kb_press_active && s_kb_pressed.kind == KeySuggestion &&
                              s_kb_pressed.index == slot, false,
                              slot == 0 && s_kb_strip_mode != KbStripWords);
    }
  }
}

// Fading trail of a swipe: thin and pale where it started, thick near the finger.
static void kb_draw_trail(GContext *ctx) {
  for (int i = 1; i < s_kb_trail_count; i++) {
    graphics_context_set_stroke_color(ctx, i * 2 > s_kb_trail_count ? APP_COLOR : GColorVividCerulean);
    graphics_context_set_stroke_width(ctx, 2 + (i * 3) / s_kb_trail_count);
    graphics_draw_line(ctx, s_kb_trail[i - 1], s_kb_trail[i]);
  }
  graphics_context_set_stroke_width(ctx, 1);
}

static void draw_touch_keyboard(GContext *ctx, GRect bounds) {
  GRect keyboard_rect = touch_keyboard_rect_for_bounds(bounds);
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, GRect(0, keyboard_rect.origin.y, bounds.size.w,
                                bounds.size.h - keyboard_rect.origin.y), 0, GCornerNone);

  kb_draw_field(ctx, bounds, keyboard_rect);
  kb_draw_strip(ctx, bounds, keyboard_rect);

  // Keys
  char label[8];
  for (int row = 0; row < KB_ROWS; row++) {
    int count = kb_row_key_count(row);
    for (int i = 0; i < count; i++) {
      KeyHit key = kb_key_at(row, i);
      bool pressed = s_kb_press_active && kb_hit_equal(key, s_kb_pressed);
      bool special = key.kind != KeyChar && key.kind != KeySpace;
      bool accent = key.kind == KeySend ||
                    (key.kind == KeyShift && !s_touch_keyboard_symbols &&
                     (s_touch_keyboard_shift || s_kb_caps_lock));
      GRect key_rect = kb_key_rect(bounds, keyboard_rect, row, i);
      if (key.kind == KeyShift && !s_touch_keyboard_symbols) {
        draw_touch_keyboard_key(ctx, key_rect, "", pressed, special, accent);
        kb_draw_shift_arrow(ctx, key_rect, (pressed || accent) ? GColorWhite : GColorBlack,
                            s_touch_keyboard_shift || s_kb_caps_lock, s_kb_caps_lock);
        continue;
      }
      kb_key_label(key, label, sizeof(label));
      draw_touch_keyboard_key(ctx, key_rect, label, pressed, special, accent);
    }
  }

  if (s_kb_press_active && s_kb_pressed.kind == KeyChar) {
    kb_draw_key_preview(ctx, bounds, kb_key_rect(bounds, keyboard_rect, s_kb_pressed.row,
                                                 s_kb_pressed.index), s_kb_pressed);
  }
  if (s_kb_swiping) {
    kb_draw_trail(ctx);
  }
}

static bool kb_is_swipe_letter(KeyHit hit) {
  return hit.kind == KeyChar && hit.ch >= 'a' && hit.ch <= 'z' && !s_touch_keyboard_symbols;
}

// Records a finger position for the word engine, in its units: 8 per half key
// across the letter rows' span (works on the round layout too), 16 per row.
static void kb_swipe_add_point(GRect bounds, GPoint point) {
  GRect keyboard_rect = touch_keyboard_rect_for_bounds(bounds);
  int rows_top = kb_row_top(keyboard_rect, 0);
  int y = ((point.y - rows_top) * 16) / KB_ROW_H;
  y = PG_MAX(0, PG_MIN(47, y));
  int row_top = kb_row_top(keyboard_rect, y / 16);
  int x0;
  int w;
  kb_span(bounds, row_top, row_top + KB_ROW_H, &x0, &w);
  int x = w > 0 ? ((point.x - x0) * KB_UNITS * 8) / w : 0;
  x = PG_MAX(0, PG_MIN(KB_UNITS * 8 - 1, x));
  int n = s_kb_swipe_pts;
  if (n > 0) {
    int dx = x - s_kb_swipe_x[n - 1];
    int dy = y - s_kb_swipe_y[n - 1];
    if (dx * dx + dy * dy < 16) {
      return;  // under a quarter key from the last point
    }
    if (n == KB_SWIPE_PTS) {
      n--;  // full: keep the latest point, so the end of the swipe is never lost
    }
  }
  s_kb_swipe_x[n] = (uint8_t)x;
  s_kb_swipe_y[n] = (uint8_t)y;
  s_kb_swipe_pts = (uint8_t)(n + 1);
}

// Follows a drag that started on a letter: every letter key it crosses joins
// the swipe path, and crossing KB_SWIPE_OTHER_KEYS other letters makes it a
// swipe rather than a slide to a neighbouring key. Samples between two touch
// events so a fast drag does not skip keys.
static void kb_swipe_track(GRect bounds, GPoint point) {
  GPoint from = s_kb_last_point;
  int steps = (PG_MAX(abs(point.x - from.x), abs(point.y - from.y)) / KB_SWIPE_SAMPLE_PX) + 1;
  for (int i = 1; i <= steps; i++) {
    GPoint sample = GPoint(from.x + ((point.x - from.x) * i) / steps,
                           from.y + ((point.y - from.y) * i) / steps);
    KeyHit hit = kb_hit_test(bounds, sample);
    kb_swipe_add_point(bounds, sample);
    size_t used = strlen(s_kb_swipe_keys);
    if (!kb_is_swipe_letter(hit) || hit.ch == s_kb_swipe_keys[used - 1]) {
      continue;
    }
    if (used + 1 >= sizeof(s_kb_swipe_keys)) {
      used--;  // full: keep the latest key, so the end of the swipe is never lost
    }
    s_kb_swipe_others += strchr(s_kb_swipe_keys, hit.ch) == NULL;  // a key not crossed before
    s_kb_swipe_keys[used] = hit.ch;
    s_kb_swipe_keys[used + 1] = '\0';
  }
  if (!s_kb_swiping && s_kb_swipe_others >= KB_SWIPE_OTHER_KEYS) {
    s_kb_swiping = true;
    s_kb_pressed.kind = KeyNone;  // no key preview while swiping
  }
  s_kb_last_point = point;
  GPoint last = s_kb_trail[s_kb_trail_count - 1];
  if (abs(point.x - last.x) + abs(point.y - last.y) < 3) {
    return;
  }
  if (s_kb_trail_count == KB_TRAIL_POINTS) {
    memmove(s_kb_trail, s_kb_trail + 1, (KB_TRAIL_POINTS - 1) * sizeof(GPoint));
    s_kb_trail_count--;
  }
  s_kb_trail[s_kb_trail_count++] = point;
  if (s_kb_swiping) {
    kb_dirty();
  }
}

// Slide to correct: the highlight follows the finger onto a neighbouring key,
// but only once it is clearly inside it, so a rolling fingertip does not flip keys.
static void kb_slide(GRect bounds, GPoint point, KeyHit hit) {
  if (hit.kind != KeyChar || kb_hit_equal(hit, s_kb_pressed)) {
    return;
  }
  GRect pressed_rect = grect_inset(kb_key_rect(bounds, touch_keyboard_rect_for_bounds(bounds),
                                               s_kb_pressed.row, s_kb_pressed.index),
                                   GEdgeInsets(-KB_SLIDE_HOLD));
  if (!grect_contains_point(&pressed_rect, &point)) {
    s_kb_pressed = hit;
    kb_dirty();
  }
}

// Keyboard part of touch handling. Returns true when the keyboard used the event.
static bool kb_handle_touch(const TouchEvent *event, GRect bounds, GPoint point) {
  if (!s_touch_keyboard_open) {
    return false;
  }
  GRect keyboard_rect = touch_keyboard_rect_for_bounds(bounds);
  bool in_keyboard = point.y >= keyboard_rect.origin.y;
  if (!in_keyboard && !s_kb_press_active) {
    return false;  // Above the keyboard: scroll the chat like normal.
  }

  KeyHit hit = kb_hit_test(bounds, point);
  switch (event->type) {
    case TouchEvent_Touchdown:
      kb_reset_press();
      s_kb_press_active = hit.kind != KeyNone;
      s_kb_pressed = hit;
      s_kb_last_point = point;
      if (hit.kind == KeyDelete) {
        kb_delete();
        s_kb_repeat_timer = app_timer_register(KB_REPEAT_DELAY_MS, kb_repeat_callback, NULL);
      } else if (hit.kind == KeyText) {
        s_kb_drag_anchor = point.y;  // a tap places the cursor on lift, a drag moves or scrolls
      } else if (hit.kind == KeySpace) {
        s_kb_drag_anchor = point.x;
      } else if (kb_is_swipe_letter(hit)) {
        s_kb_swipe_keys[0] = hit.ch;
        s_kb_swipe_keys[1] = '\0';
        s_kb_swipe_pts = 0;
        kb_swipe_add_point(bounds, point);
        s_kb_trail[0] = point;
        s_kb_trail_count = 1;
      }
      kb_dirty();
      break;
    case TouchEvent_PositionUpdate:
      if (!s_kb_press_active) {
        break;
      }
      if (s_kb_pressed.kind == KeyText) {
        if (!s_kb_space_drag) {
          int dx = point.x - s_kb_last_point.x;
          int dy = point.y - s_kb_last_point.y;
          if (abs(dx) < KB_FIELD_DRAG_START && abs(dy) < KB_FIELD_DRAG_START) {
            break;
          }
          s_kb_space_drag = true;
          s_kb_field_scroll = abs(dy) > abs(dx);
        }
        if (s_kb_field_scroll) {
          kb_scroll_field(point.y);
        } else {
          kb_set_cursor_from_point(bounds, point);
        }
      } else if (s_kb_pressed.kind == KeySpace) {
        if (!s_kb_space_drag && abs(point.x - s_kb_drag_anchor) >= KB_SPACE_DRAG_START) {
          s_kb_space_drag = true;
        }
        if (s_kb_space_drag) {
          kb_space_drag(point.x);
        }
      } else if (s_kb_swipe_keys[0]) {
        kb_swipe_track(bounds, point);
        if (!s_kb_swiping) {
          kb_slide(bounds, point, hit);
        }
      }
      break;
    case TouchEvent_Liftoff: {
      bool was_active = s_kb_press_active;
      KeyHit pressed = s_kb_pressed;
      bool swiped = s_kb_swiping;
      bool dragged = s_kb_space_drag;
      if (swiped) {
        kb_commit_swipe();  // before the reset, which forgets the path
      }
      kb_reset_press();
      if (was_active && !swiped) {
        if (pressed.kind == KeyText && !dragged) {
          kb_set_cursor_from_point(bounds, point);
        }
        if (pressed.kind == KeyText || (pressed.kind == KeySpace && dragged)) {
          kb_refresh_strip();
        } else if (pressed.kind == KeyChar ? hit.kind == KeyChar :
                   pressed.kind != KeyDelete && kb_hit_equal(hit, pressed)) {
          kb_activate(pressed);  // a key commits where the highlight is, not where the finger lifted
        }
      }
      kb_dirty();
      break;
    }
  }
  return true;
}
#else
static bool touch_keyboard_supported(void) {
  return false;
}

static void close_touch_keyboard(void) {
}

static void reset_touch_keyboard(void) {
}

static void open_touch_keyboard(KeyboardMode mode, const char *target_id, const char *initial_text) {
}

static void touch_keyboard_send_result(bool ok) {
}

static bool touch_keyboard_has_draft(void) {
  return false;
}

static void touch_keyboard_draft_received(DictionaryIterator *iter) {
}
#endif

static void draw_message_meta_at(GContext *ctx, const Message *m, int right, int y, GColor color) {
  char time_text[8];
  uint8_t receipts = message_meta_receipts(m->meta);
  int tick_w = receipts ? ticks_width(receipts) : 0;
  message_meta_time(m->meta, time_text, sizeof(time_text));
  int time_w = text_width(time_text, font_14());
  int x = right - tick_w - (tick_w ? 3 : 0) - time_w;
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, time_text, font_14(), GRect(x, y - 1, time_w + 2, 16),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  if (tick_w) {
    draw_ticks(ctx, right - tick_w, y + 4, receipts, color);
  }
}

static void draw_bubble_shape(GContext *ctx, GRect r, bool outgoing, bool tail, GColor fill) {
  GCornerMask corners = GCornersAll;
  if (tail) {
    corners &= outgoing ? ~GCornerBottomRight : ~GCornerBottomLeft;
  }
  const int radius = PG_MIN(BUBBLE_R, r.size.h / 2);  // a one-line bubble is a pill
  if (ROUND_UI) {
    if (round_clamp(r, BUBBLE_INSET).size.w < r.size.w) {
      round_fill(ctx, r, radius, GCornersAll, BUBBLE_INSET, fill);  // at the bezel: follows the curve, no tail
      return;
    }
    round_fill(ctx, r, radius, corners, BUBBLE_INSET, fill);
  } else {
    graphics_context_set_fill_color(ctx, fill);
    graphics_fill_rect(ctx, r, radius, corners);
  }
  if (tail) {
    int b = r.origin.y + r.size.h - 1;
    if (outgoing) {
      int e = r.origin.x + r.size.w;
      GPoint pts[4] = {{e - 2, b - 9}, {e - 2, b}, {e + 5, b}, {e + 1, b - 3}};
      fill_polygon(ctx, pts, 4, fill);
    } else {
      int e = r.origin.x;
      GPoint pts[4] = {{e + 1, b - 9}, {e + 1, b}, {e - 6, b}, {e - 2, b - 3}};
      fill_polygon(ctx, pts, 4, fill);
    }
  }
}

static void draw_bubble(GContext *ctx, GRect r, bool outgoing, bool tail, bool selected) {
#ifdef PBL_COLOR
  // Beeper's bubbles: flat, fully rounded, no tail. The selection ring is the same shape, 2px larger.
  tail = false;
  if (selected) {
    draw_bubble_shape(ctx, GRect(r.origin.x - 2, r.origin.y - 2, r.size.w + 4, r.size.h + 4), outgoing, tail,
                      TG_SELECT_RING);
  }
  GColor out = TG_OUT_BUBBLE;
  if (s_current_chat_network < CHAT_NETWORKS && s_network_look[s_current_chat_network].out_bubble) {
    out = (GColor){.argb = s_network_look[s_current_chat_network].out_bubble};
  }
  draw_bubble_shape(ctx, r, outgoing, tail, outgoing ? out : TG_IN_BUBBLE);
#else
  draw_bubble_shape(ctx, r, outgoing, false, TG_IN_BUBBLE);
  graphics_context_set_stroke_color(ctx, TG_TEXT);
  graphics_draw_round_rect(ctx, r, BUBBLE_R);
  if (selected) {
    graphics_draw_round_rect(ctx, GRect(r.origin.x + 1, r.origin.y + 1, r.size.w - 2, r.size.h - 2),
                             BUBBLE_R - 1);
    graphics_draw_round_rect(ctx, GRect(r.origin.x + 2, r.origin.y + 2, r.size.w - 4, r.size.h - 4),
                             BUBBLE_R - 2);
  }
#endif
}

// The lines of `text` as the system wraps it at `width`, so near the bezel each
// can be drawn on its own. Returns 0 when ours would not match the system's
// layout of the same text (text_h), which then draws it.
static int wrap_message(Message *m, char *text, GFont font, int width) {
  if (!m->wrap_n || m->wrap_w != width) {
    int len = (int)strlen(text);
    int pos = 0;
    int n = 0;
    while (pos < len && n < WRAP_LINES) {
      pos = line_next(text, font, pos, width, len);
      m->wrap_end[n++] = (uint8_t)pos;
    }
    GSize one = graphics_text_layout_get_content_size("Ag", font, GRect(0, 0, 100, 100),
                                                      GTextOverflowModeFill, GTextAlignmentLeft);
    m->wrap_lh = (uint8_t)one.h;
    m->wrap_w = (uint8_t)width;
    // wrap_end holds bytes, so longer texts keep the system layout.
    m->wrap_n = (len > 255 || pos < len || n * one.h != m->text_h) ? 255 : n;
  }
  return m->wrap_n == 255 ? 0 : m->wrap_n;
}

// Message text in `box`. Lines that reach the bezel are drawn one at a time, in
// the circle's width at their own height with an ellipsis, so they shorten
// smoothly as the chat scrolls. `meta_x` is where the time starts on the last line.
static void draw_message_text(GContext *ctx, Message *m, char *text, GRect box, int meta_x) {
  GFont font = bubble_text_font(m->font_size);
  int trim = bubble_text_trim(m->font_size);
  int inset = BUBBLE_INSET + BUBBLE_PAD_X;
  int n = ROUND_UI && round_clamp(box, inset).size.w < box.size.w ? wrap_message(m, text, font, box.size.w) : 0;
  if (!n) {
    graphics_draw_text(ctx, text, font, GRect(box.origin.x, box.origin.y - trim, box.size.w, box.size.h + trim + 1),
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    return;
  }
  for (int i = 0; i < n; i++) {
    int start = i ? m->wrap_end[i - 1] : 0;
    int end = m->wrap_end[i];
    while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\n')) {
      end--;
    }
    GRect line = round_clamp(GRect(box.origin.x, box.origin.y + i * m->wrap_lh, box.size.w, m->wrap_lh), inset);
    if (i == n - 1) {
      line.size.w = PG_MIN(line.size.w, meta_x - line.origin.x);
    }
    char saved = text[end];
    text[end] = '\0';
    graphics_draw_text(ctx, text + start, font, GRect(line.origin.x, line.origin.y - trim, line.size.w, line.size.h + trim + 1),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    text[end] = saved;
  }
}

// Reply / forward quote: coloured bar, coloured name, one line of text.
static void draw_message_context(GContext *ctx, Message *m, GRect rect) {
  char title[MAX_SENDER + 10];
  char body[MAX_CONTEXT_TEXT];
  if (ROUND_UI) {
    rect = round_clamp(rect, BUBBLE_INSET + BUBBLE_PAD_X);
  }
  if (!message_has_context(m) || rect.size.h <= 0 || rect.size.w < 20) {
    return;
  }
  message_context_strings(m, title, sizeof(title), body, sizeof(body));
  GColor accent = m->outgoing ? TG_OUT_ACCENT :
                  TG_ON_IN;
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, m->outgoing ? TG_CTX_OUT : TG_CTX_IN);
  graphics_fill_rect(ctx, rect, 4, GCornersAll);
#endif
  graphics_context_set_fill_color(ctx, accent);
  graphics_fill_rect(ctx, GRect(rect.origin.x, rect.origin.y, 3, rect.size.h), 1, GCornersLeft);
  graphics_context_set_text_color(ctx, accent);
  graphics_draw_text(ctx, strncmp(title, "Fwd from ", 9) == 0 ? title + 9 : title, font_bold14(),
                     GRect(rect.origin.x + 7, rect.origin.y - 1, rect.size.w - 9, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, m->outgoing ? TG_TEXT : TG_ON_IN);
  graphics_draw_text(ctx, body, font_14(), GRect(rect.origin.x + 7, rect.origin.y + 14, rect.size.w - 9, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void draw_message_image(GContext *ctx, Message *message, GRect image_rect) {
  if (message->image_bitmap) {
    GSize size = gbitmap_get_bounds(message->image_bitmap).size;
    if (size.w < image_rect.size.w || size.h < image_rect.size.h) {
      // Sent smaller than planned (the watch was short on memory): centre it.
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
      graphics_fill_rect(ctx, image_rect, 0, GCornerNone);
    }
    GRect draw = GRect(image_rect.origin.x + (image_rect.size.w - size.w) / 2,
                       image_rect.origin.y + (image_rect.size.h - size.h) / 2,
                       PG_MIN(size.w, image_rect.size.w), PG_MIN(size.h, image_rect.size.h));
    if (size.w > image_rect.size.w || size.h > image_rect.size.h) {
      draw.origin = image_rect.origin;
    }
    graphics_draw_bitmap_in_rect(ctx, message->image_bitmap, draw);
    return;
  }
  bool gif = message_is_gif(message);
  const char *media_name = gif ? "GIF" : "Photo";
  const char *label = message->image_failed ?
                      (message->image_error[0] ? message->image_error : (gif ? "GIF failed" : "Photo failed")) :
                      (message->image_requested ? "Loading..." : media_name);
  const char *loading_detail = (!message->image_failed && message->image_requested && message->image_error[0]) ?
                               message->image_error : "";
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, message->outgoing ? GColorMayGreen : GColorLightGray);
  graphics_fill_rect(ctx, image_rect, 6, GCornersAll);
#else
  graphics_context_set_stroke_color(ctx, TG_TEXT);
  graphics_draw_round_rect(ctx, image_rect, 6);
#endif
  int label_h = message->image_failed ? PG_MIN(image_rect.size.h - 4, 46) : 22;
  int label_y = image_rect.origin.y + PG_MAX(2, (image_rect.size.h - label_h - (message->image_requested ? 16 : 0)) / 2);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorWhite, TG_TEXT));
  graphics_draw_text(ctx, label, font_bold18(), GRect(image_rect.origin.x + 4, label_y, image_rect.size.w - 8, label_h),
                     message->image_failed ? GTextOverflowModeWordWrap : GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
  if (message->image_requested && image_rect.size.h >= 48) {
    int bar_w = PG_MIN(image_rect.size.w - 24, 100);
    GRect track = GRect(image_rect.origin.x + (image_rect.size.w - bar_w) / 2, label_y + 26, bar_w, 4);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorWhite, GColorWhite));
    graphics_fill_rect(ctx, track, 2, GCornersAll);
    int fill_w = (bar_w * PG_MAX(0, PG_MIN(100, message->image_progress))) / 100;
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorBlueMoon, TG_TEXT));
    graphics_fill_rect(ctx, GRect(track.origin.x, track.origin.y, PG_MAX(4, fill_w), 4), 2, GCornersAll);
    if (loading_detail[0] && image_rect.size.h >= 64) {
      graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorWhite, TG_TEXT));
      graphics_draw_text(ctx, loading_detail, font_14(),
                         GRect(image_rect.origin.x + 4, track.origin.y + 6, image_rect.size.w - 8, 18),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
  }
}

static void draw_service_pill(GContext *ctx, GRect bounds, int y, const char *text) {
  int w = PG_MIN(bounds.size.w - 20, text_width(text, font_bold14()) + 20);
  GRect pill = GRect((bounds.size.w - w) / 2, y, w, 22);
  graphics_context_set_fill_color(ctx, TG_SERVICE_PILL);
  graphics_fill_rect(ctx, pill, 11, GCornersAll);
  graphics_context_set_text_color(ctx, TG_ON_SERVICE);
  graphics_draw_text(ctx, text, font_bold14(), GRect(pill.origin.x, pill.origin.y + 2, w, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// Centre row (content coordinates) of the spinner above the oldest message while older ones load.
static int older_spinner_y(void) {
  return s_message_y[0] - OLDER_LOADER_H / 2;
}

// A white disc with a three-quarter accent ring turning 45 degrees a frame.
static void draw_spinner(GContext *ctx, GPoint c) {
  const int r = OLDER_SPINNER_R;
  graphics_context_set_fill_color(ctx, TG_BUBBLE_SHADOW);
  graphics_fill_circle(ctx, GPoint(c.x, c.y + 1), r + 2);
  graphics_context_set_fill_color(ctx, TG_IN_BUBBLE);
  graphics_fill_circle(ctx, c, r + 2);
  const int32_t start = PBL_DEG_TO_TRIGANGLE((s_loader_frame % 8) * 45);
  graphics_context_set_stroke_color(ctx, TG_ACCENT);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_arc(ctx, GRect(c.x - r + 2, c.y - r + 2, (r - 2) * 2 + 1, (r - 2) * 2 + 1),
                    GOvalScaleModeFitCircle, start, start + PBL_DEG_TO_TRIGANGLE(270));
  graphics_context_set_stroke_width(ctx, 1);
}

// Round mic button used on the input bar.
static void draw_mic_icon(GContext *ctx, GPoint c, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, GRect(c.x - 3, c.y - 8, 7, 11), 3, GCornersAll);
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(c.x - 6, c.y - 1), GPoint(c.x - 6, c.y + 1));
  graphics_draw_line(ctx, GPoint(c.x + 6, c.y - 1), GPoint(c.x + 6, c.y + 1));
  graphics_draw_line(ctx, GPoint(c.x - 5, c.y + 3), GPoint(c.x - 2, c.y + 6));
  graphics_draw_line(ctx, GPoint(c.x + 5, c.y + 3), GPoint(c.x + 2, c.y + 6));
  graphics_draw_line(ctx, GPoint(c.x, c.y + 6), GPoint(c.x, c.y + 9));
  graphics_context_set_stroke_width(ctx, 1);
}

static void draw_send_icon(GContext *ctx, GPoint c, GColor color) {
  GPoint points[] = {{c.x - 6, c.y - 7}, {c.x + 8, c.y}, {c.x - 6, c.y + 7}, {c.x - 3, c.y}};
  GPathInfo info = {.num_points = 4, .points = points};
  GPath path;
  gpath_init(&path, &info);
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, &path);
}

static GRect compose_mic_rect(GRect compose) {
  return GRect(compose.origin.x + compose.size.w - COMPOSE_BAR_H,
               compose.origin.y + compose.size.h - COMPOSE_BAR_H, COMPOSE_BAR_H, COMPOSE_BAR_H);
}

static void draw_compose_bar(GContext *ctx, GRect compose, bool selected) {
  GRect mic = compose_mic_rect(compose);
  GRect field = GRect(compose.origin.x, compose.origin.y, compose.size.w - mic.size.w - 4, compose.size.h);
  int r = COMPOSE_BAR_H / 2;
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, TG_BUBBLE_SHADOW);
  graphics_fill_rect(ctx, GRect(field.origin.x, field.origin.y + 1, field.size.w, field.size.h), r, GCornersAll);
#endif
  graphics_context_set_fill_color(ctx, TG_IN_BUBBLE);
  graphics_fill_rect(ctx, field, r, GCornersAll);
  if (selected || BW_UI) {
    graphics_context_set_stroke_color(ctx, selected ? TG_SELECT_RING : TG_TEXT);
    graphics_context_set_stroke_width(ctx, selected ? 2 : 1);
    graphics_draw_round_rect(ctx, field, r);
    graphics_context_set_stroke_width(ctx, 1);
  }
  bool draft = s_bar_draft[0] || touch_keyboard_has_draft();
  graphics_context_set_text_color(ctx, draft ? TG_ON_IN : TG_IN_META);
  graphics_draw_text(ctx, s_bar_draft[0] ? s_bar_draft : draft ? "Continue draft" : "Message", font_18(),
                     s_bar_draft[0] ? GRect(field.origin.x + 12, field.origin.y + 2, field.size.w - 16, 44)
                                    : GRect(field.origin.x + 12, field.origin.y + (compose.size.h - 24) / 2,
                                            field.size.w - 16, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  GPoint mc = GPoint(mic.origin.x + mic.size.w / 2, mic.origin.y + mic.size.h / 2);
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, TG_BUBBLE_SHADOW);
  graphics_fill_circle(ctx, GPoint(mc.x, mc.y + 1), r);
#endif
  graphics_context_set_fill_color(ctx, selected ? TG_SELECT_RING : TG_ACCENT);
  graphics_fill_circle(ctx, mc, r);
  if (s_bar_draft[0]) {
    draw_send_icon(ctx, mc, selected ? TG_ON_RING : TG_ON_ACCENT);
  } else {
    draw_mic_icon(ctx, mc, selected ? TG_ON_RING : TG_ON_ACCENT);
  }
}

static GRect compose_rect_for_bounds(GRect bounds) {
  int compose_y = s_compose_bubble_y - s_chat_scroll_offset;
  if (s_compose_floating) {
    // Where the docked bar sits with the chat scrolled to its end, so floating meets docked seamlessly
    int pinned = visible_message_height(bounds) - compose_bar_h() - (ROUND_UI ? 28 : 5);
    compose_y = s_at_newest ? PG_MIN(compose_y, pinned) : pinned;
  }
  if (ROUND_UI) {
    s_round_org = layer_get_frame(s_messages_root).origin;
    return round_clamp(GRect((bounds.size.w - 170) / 2, compose_y, 170, compose_bar_h()), 6);
  }
  return GRect(6, compose_y, bounds.size.w - 12, compose_bar_h());
}

static void draw_chat_background(GContext *ctx, GRect bounds) {
#ifdef PBL_COLOR
  if (s_wallpaper) {
    graphics_draw_bitmap_in_rect(ctx, s_wallpaper, bounds);
    return;
  }
#endif
  graphics_context_set_fill_color(ctx, TG_CHAT_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

static void draw_messages(GContext *ctx, GRect bounds, int visible_h);
static void draw_messages_overlay(GContext *ctx, GRect bounds);

// chat_warp's strip renderer: the messages from region row y0 on.
static void draw_warp_strip(GContext *ctx, int y0, void *data) {
  GRect *bounds = data;
  int saved = s_chat_scroll_offset;
  s_chat_scroll_offset = saved + y0;
  draw_messages(ctx, *bounds, CHAT_WARP_MAX_ROWS);
  s_chat_scroll_offset = saved;
}

static void messages_root_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  if (ROUND_UI) {
    s_round_org = layer_get_frame(layer).origin;
  }
  int visible_h = visible_message_height(bounds);
#if TOUCH_KEYBOARD_AVAILABLE
  // Typing only redraws the keyboard, so skip re-measuring every message.
  if (!s_kb_redraw_only) {
    recalc_message_layout();
  }
  s_kb_redraw_only = false;
#else
  recalc_message_layout();
#endif
  // The keyboard covers the rest, so only the visible part is painted.
  draw_chat_background(ctx, GRect(0, 0, bounds.size.w, visible_h));

  if (ROUND_UI) {
    static ChatWarpMap s_warp;
    GRect frame = layer_get_frame(layer);
    // Each bubble scales as one, so its text keeps its shape
    ChatWarpItem items[MAX_MESSAGES + 1];
    int n = s_message_count;
    if (s_loading_older_messages && !s_at_oldest && n > 0 && n < MAX_MESSAGES + 1) {
      // The spinner scales as one too, so it stays round near the bezel
      items[n].top = older_spinner_y() - OLDER_SPINNER_R - 3 - s_chat_scroll_offset;
      items[n].bottom = items[n].top + OLDER_SPINNER_R * 2 + 7;
      n++;
    }
    for (int i = 0; i < s_message_count; i++) {
      // The selection ring (2px) and shadow (1px) around a bubble scale with it, or they land
      // in rows at the circle's own scale and draw wider than the bubble, as dark lines.
      // So do its day label above and its status line below.
      items[i].top = s_message_y[i] - s_chat_scroll_offset - 2 - (s_day_label[i] ? DAY_LABEL_H : 0);
      items[i].bottom = s_message_y[i] - s_chat_scroll_offset + s_message_h[i] + 2 +
                        reactions_overhang(&s_messages[i]) + (message_shows_status(i) ? STATUS_LINE_H : 0);
    }
    chat_warp_build(&s_warp, visible_h, PBL_DISPLAY_HEIGHT / 2 - frame.origin.y,
                    PBL_DISPLAY_WIDTH / 2, PBL_DISPLAY_WIDTH / 2 - CHAT_WARP_SIDE_INSET + 6 + CHAT_WARP_BEZEL_GAP,
                    items, n);
    s_round_flat = true;
    chat_warp_draw(ctx, &s_warp, GRect(frame.origin.x, frame.origin.y, bounds.size.w, visible_h),
                   PBL_DISPLAY_WIDTH / 2, TG_CHAT_BG, draw_warp_strip, &bounds);
    s_round_flat = false;
  } else {
    draw_messages(ctx, bounds, visible_h);
  }
  draw_messages_overlay(ctx, bounds);
}

// The message bubbles (not the background, composer or keyboard), down to visible_h.
static void draw_messages(GContext *ctx, GRect bounds, int visible_h) {
  if (s_message_count == 0) {
    draw_service_pill(ctx, bounds, bounds.size.h / 2 - 20,
                      s_loading_messages ? "Loading messages..." : "No messages here yet");
  }

  if (s_loading_older_messages && !s_at_oldest && s_message_count > 0) {
    draw_spinner(ctx, GPoint(bounds.size.w / 2, older_spinner_y() - s_chat_scroll_offset));
  }

  // A message is drawn while any of it shows: its reaction pill and status hang below the
  // bubble, its day label above
  int first = 0;
  while (first < s_message_count - 1 &&
         s_message_y[first] + s_message_h[first] + reactions_overhang(&s_messages[first]) + 16 <
             s_chat_scroll_offset) {
    first++;
  }

  for (int i = first; i < s_message_count; i++) {
    Message *m = &s_messages[i];
    MsgBox *box = &s_box[i];
    int y = s_message_y[i] - s_chat_scroll_offset;
    int h = s_message_h[i];
    if (y - (s_day_label[i] ? DAY_LABEL_H : 0) > visible_h) {
      break;
    }
    bool selected = i == s_selected_message;
    GRect bubble = GRect(box->x, y, box->w, h);
    if (s_day_label[i]) {
      char day[20];
      day_label_text(message_time(m), day, sizeof(day));
      graphics_context_set_text_color(ctx, TG_SUBTEXT);
      graphics_draw_text(ctx, day, font_bold14(), GRect(0, y - DAY_LABEL_H, bounds.size.w, 16),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
    if (message_shows_status(i)) {
      int right = box->x + box->w - 4;
      graphics_context_set_text_color(ctx, TG_SUBTEXT);
      graphics_draw_text(ctx, message_status(m), font_14(),
                         GRect(right - 160, y + h - 1 + reactions_overhang(m), 160, 16),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
    }


    if (box->flags & BOX_AVATAR) {
      draw_avatar(ctx, GPoint(message_side_inset(bounds) + GROUP_AVATAR_R - 2, y + h - GROUP_AVATAR_R),
                  GROUP_AVATAR_R, m->sender, m->sender, NULL, TG_CHAT_BG);
    }

    char display_text[MESSAGE_PREVIEW_TEXT + 8];
    copy_cstr(display_text, sizeof(display_text), m->text);
    truncate_cstr_bytes(display_text, sizeof(display_text), MESSAGE_PREVIEW_TEXT, " ...");

    if (box->flags & BOX_EMOJI) {
      if (selected) {
        graphics_context_set_stroke_color(ctx, TG_SELECT_RING);
        graphics_context_set_stroke_width(ctx, 2);
        graphics_draw_round_rect(ctx, GRect(bubble.origin.x - 3, y - 2, bubble.size.w + 6, h + 3), 8);
        graphics_context_set_stroke_width(ctx, 1);
      }
      int emoji_x = m->outgoing ? box->x + box->w - m->text_w : box->x;
      graphics_context_set_text_color(ctx, TG_TEXT);
      graphics_draw_text(ctx, display_text, big_emoji_font(),
                         GRect(emoji_x, y - bubble_text_trim(3), m->text_w + 4, m->text_h + 8),
                         GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
      if (m->meta_w) {
        int pill_w = m->meta_w + 10;
        int pill_x = m->outgoing ? box->x + box->w - pill_w : box->x;
        GRect pill = GRect(pill_x, y + box->meta_y, pill_w, 16);
        graphics_context_set_fill_color(ctx, TG_SERVICE_PILL);
        graphics_fill_rect(ctx, pill, 8, GCornersAll);
        draw_message_meta_at(ctx, m, pill.origin.x + pill_w - 5, pill.origin.y, TG_ON_SERVICE);
      }
      continue;
    }

    draw_bubble(ctx, bubble, m->outgoing, box->flags & BOX_TAIL, selected);
    int left = box->x + BUBBLE_PAD_X;
    int inner_w = box->w - (BUBBLE_PAD_X * 2);

    int meta_right = box->x + box->w - BUBBLE_PAD_X + 1;
    if (ROUND_UI) {
      GRect meta = round_clamp(GRect(0, y + box->meta_y, meta_right, BUBBLE_META_H), BUBBLE_INSET + 3);
      meta_right = meta.origin.x + meta.size.w;
    }

    if (box->flags & BOX_NAME) {
      GRect name = GRect(left, y + BUBBLE_PAD_TOP - 2, inner_w, 16);
      graphics_context_set_text_color(ctx, TG_ON_IN);  // one colour for every name: bold already sets it apart
      graphics_draw_text(ctx, m->sender, font_bold14(), ROUND_UI ? round_clamp(name, BUBBLE_INSET + BUBBLE_PAD_X) : name,
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
    if (message_has_context(m)) {
      draw_message_context(ctx, m, GRect(left, y + box->ctx_y, inner_w, BUBBLE_CTX_H - 2));
    }
    if (m->image_placeholder) {
      draw_message_image(ctx, m, GRect(box->x + (box->w - box->image_w) / 2, y + box->image_y,
                                       box->image_w, box->image_h));
    }
    if (display_text[0] && m->text_h) {
      graphics_context_set_text_color(ctx, m->outgoing ? TG_ON_OUT : TG_ON_IN);
      draw_message_text(ctx, m, display_text, GRect(left, y + box->text_y, box->text_box_w, m->text_h),
                        (box->flags & BOX_META_INLINE) ? meta_right - m->meta_w - 6 : PBL_DISPLAY_WIDTH);
    }
    if (m->pills_h) {
      // On the bubble's own side: right for mine, left for theirs
      int pills_x = m->outgoing ? box->x + box->w - 6 - m->pills_w : box->x + 6;
      layout_pills(ctx, m->reactions, m->pills_w, GPoint(pills_x, y + box->pills_y),
                   m->outgoing, NULL, NULL, NULL);
    }
    if (box->flags & BOX_META_ON_PHOTO) {
      int pill_w = m->meta_w + 10;
      int right = box->x + (box->w + box->image_w) / 2 - 4;
      GRect pill = GRect(right - pill_w, y + box->meta_y, pill_w, 16);
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
      graphics_fill_rect(ctx, pill, 8, GCornersAll);
      draw_message_meta_at(ctx, m, right - 5, pill.origin.y, GColorWhite);
    } else if (m->meta_w) {
      draw_message_meta_at(ctx, m, meta_right, y + box->meta_y, m->outgoing ? TG_OUT_META : TG_IN_META);
    }
  }

}

static void day_pill_timer_callback(void *data) {
  s_day_pill_timer = NULL;
  s_day_pill = false;
  mark_messages_dirty();
}

// The day of the messages at the top, as a pill, while the chat scrolls and a moment after:
// the day's own label is usually far above.
static void draw_day_pill(GContext *ctx, GRect bounds) {
  if (s_chat_scroll_offset != s_day_pill_offset) {
    bool first = s_day_pill_offset < 0;
    s_day_pill_offset = s_chat_scroll_offset;
    if (!first) {
      s_day_pill = true;
      cancel_timer(&s_day_pill_timer);
      s_day_pill_timer = app_timer_register(DAY_PILL_MS, day_pill_timer_callback, NULL);
    }
  }
  if (!s_day_pill || s_message_count == 0) {
    return;
  }
  const int top = 4;
  int i = 0;
  while (i < s_message_count - 1 && s_message_y[i] + s_message_h[i] - s_chat_scroll_offset < top + 22) {
    i++;
  }
  time_t t = message_time(&s_messages[i]);
  // Not when that day's label is itself in view at the top: it would be said twice
  if (!t || (s_day_label[i] && s_message_y[i] - DAY_LABEL_H - s_chat_scroll_offset >= top)) {
    return;
  }
  char day[20];
  day_label_text(t, day, sizeof(day));
  draw_service_pill(ctx, bounds, top, day);
}

// What stays put over the messages: the day, the composer and the keyboard.
static void draw_messages_overlay(GContext *ctx, GRect bounds) {
  draw_day_pill(ctx, bounds);
  GRect compose_rect = compose_rect_for_bounds(bounds);
  if ((s_at_newest || s_compose_floating) && !s_touch_keyboard_open &&
      compose_rect.origin.y < bounds.size.h && compose_rect.origin.y + compose_rect.size.h > 0 &&
      compose_rect.size.w >= COMPOSE_BAR_H * 2) {
    draw_compose_bar(ctx, compose_rect, compose_target_is_selected());
  }

#if TOUCH_KEYBOARD_AVAILABLE
  if (s_touch_keyboard_open) {
    draw_touch_keyboard(ctx, bounds);
  }
#endif
}

static void destroy_chat_view(void) {
  cancel_tap_menu();
  cancel_timer(&s_fling_timer);
  cancel_timer(&s_day_pill_timer);
  s_day_pill = false;
  s_day_pill_offset = -1;
  if (s_chat_menu_animation) {
    animation_unschedule((Animation *)s_chat_menu_animation);
    property_animation_destroy(s_chat_menu_animation);
    s_chat_menu_animation = NULL;
  }
  if (s_messages_animation) {
    animation_unschedule((Animation *)s_messages_animation);
    property_animation_destroy(s_messages_animation);
    s_messages_animation = NULL;
  }
  cancel_timer(&s_chat_scroll_timer);
  if (s_messages_root) {
    layer_destroy(s_messages_root);
    s_messages_root = NULL;
  }
}

static void clear_layer_animation(PropertyAnimation **animation_ref) {
  if (*animation_ref) {
    animation_unschedule((Animation *)*animation_ref);
    property_animation_destroy(*animation_ref);
    *animation_ref = NULL;
  }
}

static void generic_layer_animation_stopped(Animation *animation, bool finished, void *context) {
  PropertyAnimation **animation_ref = (PropertyAnimation **)context;
  if (animation_ref && *animation_ref) {
    property_animation_destroy(*animation_ref);
    *animation_ref = NULL;
  }
}

static void chat_menu_slide_out_stopped(Animation *animation, bool finished, void *context) {
  generic_layer_animation_stopped(animation, finished, &s_chat_menu_animation);
  if (finished && s_view_state == ViewStateChat && s_chat_menu) {
    layer_set_hidden(menu_layer_get_layer(s_chat_menu), true);
  }
}

static void messages_slide_back_stopped(Animation *animation, bool finished, void *context) {
  generic_layer_animation_stopped(animation, finished, &s_messages_animation);
  if (finished && s_view_state == ViewStateChatList && s_messages_root) {
    layer_destroy(s_messages_root);
    s_messages_root = NULL;
  }
}

static void animate_layer_frame(PropertyAnimation **animation_ref, Layer *layer,
                                GRect from_frame, GRect to_frame,
                                AnimationStoppedHandler stopped_handler) {
  clear_layer_animation(animation_ref);
  layer_set_frame(layer, from_frame);
  *animation_ref = property_animation_create_layer_frame(layer, &from_frame, &to_frame);
  if (!*animation_ref) {
    layer_set_frame(layer, to_frame);
    return;
  }
  Animation *animation = (Animation *)*animation_ref;
  animation_set_duration(animation, VIEW_TRANSITION_MS);
  animation_set_curve(animation, AnimationCurveEaseOut);
  animation_set_handlers(animation, (AnimationHandlers) {
    .stopped = stopped_handler ? stopped_handler : generic_layer_animation_stopped
  }, animation_ref);
  animation_schedule(animation);
}

static void show_chat_view(void) {
  const bool instant = s_open_chat_instantly;
  s_open_chat_instantly = false;
  s_chat_view_pending = false;
  s_view_state = ViewStateChat;
  window_set_click_config_provider(s_main_window, click_config_provider);
  destroy_chat_view();
  show_status(s_current_chat_title);

  Layer *window_layer = window_get_root_layer(s_main_window);
  GRect bounds = layer_get_bounds(window_layer);
  int content_y = chat_content_y();
  GRect messages_to = GRect(0, content_y, bounds.size.w, bounds.size.h - content_y);
  GRect messages_from = messages_to;
  messages_from.origin.x = instant ? 0 : bounds.size.w;
  s_messages_root = layer_create(messages_from);
  layer_set_update_proc(s_messages_root, messages_root_update_proc);
  layer_add_child(window_layer, s_messages_root);
  s_chat_scroll_offset = 0;
  s_chat_content_height = 0;
  recalc_message_layout();
  if (s_message_count > 0) {
    s_chat_scroll_offset = clamp_scroll_offset(s_chat_content_height);
    // Opened from a notification: a long newest message starts at its top, to read from there
    const int last = s_message_count - 1;
    const int top = s_message_y[last] - (s_day_label[last] ? DAY_LABEL_H : 0) - (ROUND_UI ? 30 : 4);
    if (instant && top < s_chat_scroll_offset) {
      s_chat_scroll_offset = clamp_scroll_offset(top);
    }
  }
  render_messages();
  if (instant) {
    if (s_chat_menu) {
      layer_set_hidden(menu_layer_get_layer(s_chat_menu), true);
    }
    return;
  }
  animate_layer_frame(&s_messages_animation, s_messages_root, messages_from, messages_to, NULL);
  if (s_chat_menu) {
    Layer *menu_layer = menu_layer_get_layer(s_chat_menu);
    GRect menu_from = layer_get_frame(menu_layer);
    GRect menu_to = menu_from;
    menu_from.origin.x = 0;
    menu_to.origin.x = -bounds.size.w;
    layer_set_hidden(menu_layer, false);
    animate_layer_frame(&s_chat_menu_animation, menu_layer, menu_from, menu_to,
                        chat_menu_slide_out_stopped);
  }
}

static void show_chat_view_timer(void *data) {
  show_chat_view();
}

static void render_chat_list_with_transition(void) {
  int row = s_selected_chat;
  s_view_state = ViewStateChatList;
  s_chats_loading = false;
  s_loading_error = false;
  window_set_click_config_provider(s_main_window, click_config_provider);
  cancel_timer(&s_chat_scroll_timer);
  Layer *window_layer = window_get_root_layer(s_main_window);
  GRect bounds = layer_get_bounds(window_layer);
  if (s_chat_menu) {
    int saved_row = find_chat_index_by_id(s_chat_list_selected_id);
    if (saved_row >= 0) {
      row = saved_row;
    }
    Layer *menu_layer = menu_layer_get_layer(s_chat_menu);
    GRect menu_to = layer_get_frame(menu_layer);
    menu_to.origin.x = 0;
    GRect menu_from = menu_to;
    menu_from.origin.x = -bounds.size.w;
    layer_set_hidden(menu_layer, false);
    menu_layer_reload_data(s_chat_menu);
    select_chat_row(row, false);
    if (fav_focused()) {
      fav_reveal();
    }
    animate_layer_frame(&s_chat_menu_animation, menu_layer, menu_from, menu_to, NULL);
  }
  if (s_messages_root) {
    GRect messages_from = layer_get_frame(s_messages_root);
    messages_from.origin.x = 0;
    GRect messages_to = messages_from;
    messages_to.origin.x = bounds.size.w;
    animate_layer_frame(&s_messages_animation, s_messages_root, messages_from, messages_to,
                        messages_slide_back_stopped);
  }
  show_status("Chat");
}

static void select_chat_row(int row, bool animated) {
  if (!s_chat_menu || s_chats_loading || s_loading_messages || s_chat_count <= 0) {
    return;
  }
  s_selected_chat = PG_MAX(0, PG_MIN(row, s_chat_count - 1));
  copy_cstr(s_chat_list_selected_id, sizeof(s_chat_list_selected_id), s_chats[s_selected_chat].id);
  menu_layer_set_selected_index(s_chat_menu, MenuIndex(0, s_selected_chat), MenuRowAlignCenter, animated);
  // The top of the list, with the favourites' bubbles above the first chat, when both fit
  if (s_selected_chat == 0 &&
      fav_strip_h() + CHAT_ROW_H <= layer_get_frame(menu_layer_get_layer(s_chat_menu)).size.h) {
    scroll_layer_set_content_offset(menu_layer_get_scroll_layer(s_chat_menu), GPointZero, animated);
  }
}

static void reveal_available_chat_rows(void) {
  if (s_view_state == ViewStateChat || s_chat_count <= 0) {
    return;
  }
  s_chats_loading = false;
  s_loading_error = false;
  s_selected_chat = PG_MAX(0, PG_MIN(s_selected_chat, s_chat_count - 1));
  if (s_chat_menu) {
    menu_layer_reload_data(s_chat_menu);
    select_chat_row(s_selected_chat, false);
  }
  if (!s_chat_first_paint_reported &&
      s_chat_count >= PG_MIN(CHAT_FIRST_PAINT_ROWS, s_expected_rows > 0 ? s_expected_rows : CHAT_FIRST_PAINT_ROWS)) {
    s_chat_first_paint_reported = true;
    send_command_with_status("chat_first_paint", NULL, NULL, NULL, NULL, false);
  }
  show_status("Chat");
}

static void remove_chat_at(int row) {
  if (row < 0 || row >= s_chat_count) {
    return;
  }
  clear_chat_slot(&s_chats[row]);
  for (int i = row; i < s_chat_count - 1; i++) {
    s_chats[i] = s_chats[i + 1];
  }
  memset(&s_chats[s_chat_count - 1], 0, sizeof(Chat));
  init_chat_strings(&s_chats[s_chat_count - 1]);
  s_chat_count--;
  if (s_selected_chat > row) {
    s_selected_chat--;
  }
  if (s_chat_count <= 0) {
    s_selected_chat = 0;
  } else if (s_selected_chat >= s_chat_count) {
    s_selected_chat = s_chat_count - 1;
  }
  if (s_chat_menu) {
    menu_layer_reload_data(s_chat_menu);
    select_chat_row(s_selected_chat, false);
  }
}

static void request_chats(void) {
  s_view_state = ViewStateChatList;
  cancel_message_timeout();
  cancel_timer(&s_chat_retry_timer);
  if (s_chat_count == 0) {
    destroy_message_images();
    destroy_chat_avatars();
    s_chat_loading_progress = 0;
  }
  s_expected_rows = 0;
  s_bridge_ready = false;
  s_chats_loading = true;
  s_chat_first_paint_reported = false;
  s_chat_request_attempts = 1;
  show_loading_text("Loading...", false);
  show_status("Chat");
  if (s_chat_menu) {
    menu_layer_reload_data(s_chat_menu);
  }
  send_command_with_status("get_chats", NULL, NULL, NULL, NULL, false);
  if (s_chat_request_attempts < CHAT_COMMAND_MAX_ATTEMPTS) {
    s_chat_retry_timer = app_timer_register(CHAT_COMMAND_WAKE_RETRY_MS, chat_retry_timer_callback, NULL);
  }
  schedule_message_timeout();
}

static void chat_retry_timer_callback(void *data) {
  s_chat_retry_timer = NULL;
  if (!s_chats_loading || s_view_state == ViewStateChat || s_bridge_ready ||
      s_chat_request_attempts >= CHAT_COMMAND_MAX_ATTEMPTS) {
    return;
  }

  s_chat_request_attempts++;
  send_command_with_status("get_chats", NULL, NULL, NULL, NULL, false);
  if (s_chat_request_attempts < CHAT_COMMAND_MAX_ATTEMPTS) {
    s_chat_retry_timer = app_timer_register(CHAT_COMMAND_WAKE_RETRY_MS, chat_retry_timer_callback, NULL);
  }
}

static void startup_wake_timer_callback(void *data) {
  s_startup_wake_timer = NULL;
  if (s_view_state != ViewStateChat && s_chats_loading) {
    request_chats();
  }
}

static void cancel_message_timeout(void) {
  cancel_timer(&s_message_timeout_timer);
}

static void schedule_message_timeout(void) {
  cancel_message_timeout();
  s_message_timeout_timer = app_timer_register(MESSAGE_TRANSFER_TIMEOUT_MS, message_timeout_timer_callback, NULL);
}

static void cancel_message_retry(void) {
  cancel_timer(&s_message_retry_timer);
}

static bool send_pending_message_request(void) {
  if (!s_current_chat_id[0]) {
    return false;
  }
  if (s_loading_messages) {
    return send_command_with_status("get_messages", s_current_chat_id, NULL, NULL, NULL, false);
  }
  if (s_loading_older_messages && s_message_count > 0 && s_messages[0].id[0]) {
    const char *anchor_id = s_older_anchor_id[0] ? s_older_anchor_id : s_messages[0].id;
    return send_command_with_status("get_older_messages", s_current_chat_id, NULL,
                                    s_messages[0].id, anchor_id, false);
  }
  if (s_loading_newer_messages && s_message_count > 0 && s_messages[s_message_count - 1].id[0]) {
    const char *anchor_id = s_newer_anchor_id[0] ? s_newer_anchor_id :
                            s_messages[s_message_count - 1].id;
    return send_command_with_status("get_newer_messages", s_current_chat_id, NULL,
                                    s_messages[s_message_count - 1].id, anchor_id, false);
  }
  return false;
}

// Opened from a notification, the list is hidden until the chat shows; if the chat can't load,
// show the list after all instead of leaving the screen blank.
static void reveal_hidden_chat_list(void) {
  if (!s_open_chat_instantly) {
    return;
  }
  s_open_chat_instantly = false;
  if (s_chat_menu && s_view_state != ViewStateChat) {
    layer_set_hidden(menu_layer_get_layer(s_chat_menu), false);
  }
}

static void message_retry_timer_callback(void *data) {
  s_message_retry_timer = NULL;
  if (!s_loading_messages && !s_loading_older_messages && !s_loading_newer_messages) {
    return;
  }
  if (s_message_request_attempts >= MESSAGE_COMMAND_MAX_ATTEMPTS) {
    bool had_rows = s_message_count > 0;
    s_loading_messages = false;
    s_loading_older_messages = false;
    s_loading_newer_messages = false;
    s_message_transfer_id = 0;
    s_older_anchor_id[0] = '\0';
    s_newer_anchor_id[0] = '\0';
    clear_message_stage();
    reset_message_stream_state();
    show_status(had_rows ? s_current_chat_title : "Messages failed");
    mark_messages_dirty();
    reveal_hidden_chat_list();
    return;
  }
  s_message_request_attempts++;
  if (send_pending_message_request()) {
    schedule_message_timeout();
  } else {
    schedule_message_send_retry();
  }
}

static void schedule_message_send_retry(void) {
  cancel_message_retry();
  s_message_retry_timer = app_timer_register(MESSAGE_COMMAND_WAKE_RETRY_MS,
                                             message_retry_timer_callback, NULL);
}

static void message_timeout_timer_callback(void *data) {
  s_message_timeout_timer = NULL;
  if (s_chats_loading && s_view_state != ViewStateChat) {
    s_chats_loading = false;
    s_loading_error = true;
    show_loading_text("Chats failed", true);
    show_status("Chat");
    if (s_chat_menu) {
      menu_layer_reload_data(s_chat_menu);
    }
    reveal_hidden_chat_list();
    return;
  }
  if (!s_loading_messages && !s_loading_older_messages && !s_loading_newer_messages) {
    return;
  }
  reveal_hidden_chat_list();
  bool had_rows = s_message_count > 0;
  s_loading_messages = false;
  s_loading_older_messages = false;
  s_loading_newer_messages = false;
  s_message_transfer_id = 0;
  s_older_anchor_id[0] = '\0';
  s_newer_anchor_id[0] = '\0';
  clear_message_stage();
  reset_message_stream_state();
  show_status(had_rows ? s_current_chat_title : "Messages failed");
  mark_messages_dirty();
}

// Asks the phone for the active photo. The request says exactly what size to
// draw it at and how many bytes of heap it may use; the phone picks the best
// colour depth (and only shrinks the photo) to fit that budget.
static bool send_active_image_request(void) {
  if (!s_image_message_id[0]) {
    return false;
  }
  Message *message = find_message_by_image_token(s_image_message_id);
  int image_index = message_index_from_ptr(message);
  if (!message || message->image_bitmap || image_index < 0 ||
      !message_image_near_viewport(image_index, IMAGE_KEEP_MARGIN)) {
    clear_active_image_request();
    return false;
  }
  // A restarted request drops whatever arrived before.
  free_image_transfer_buffer();
  s_image_transfer_id = 0;
  s_image_expected_offset = 0;
  s_image_written = 0;
  size_t budget = image_budget_for(image_index);
  if (s_image_budget_cap && budget > s_image_budget_cap) {
    budget = s_image_budget_cap;
  }
  if (budget < IMAGE_MIN_BUDGET) {
    // Not enough memory right now (other photos on screen hold it). Try again
    // when the user scrolls.
    IMAGE_DIAG("PGIMG watch no budget msg=%s heap=%u", message->image_token, image_diag_heap_free());
    clear_active_image_request();
    return false;
  }
  char request_text[32];
  snprintf(request_text, sizeof(request_text), "%u:%u:%d:%d",
           (unsigned)message->image_retry_level, (unsigned)budget,
           s_box[image_index].image_w, s_box[image_index].image_h);
  IMAGE_DIAG("PGIMG watch request msg=%s req=%s heap=%u", message->image_token, request_text,
             image_diag_heap_free());
  if (send_command_with_status("get_image", s_current_chat_id, request_text, NULL, message->image_token, false)) {
    schedule_image_prepare_timeout();
    return true;
  }
  schedule_image_retry();
  return false;
}

// Asks again for the active photo, optionally with a smaller byte budget.
static bool retry_active_image_request(Message *message, const char *detail, unsigned budget_cap) {
  int image_index = message_index_from_ptr(message);
  if (!message || !message->image_token[0] || image_index < 0 ||
      !message_image_near_viewport(image_index, IMAGE_KEEP_MARGIN) ||
      message->image_retry_level >= IMAGE_RETRY_MAX_LEVEL) {
    return false;
  }
  cancel_timer(&s_image_retry_timer);
  reset_image_transfer_state();
  copy_cstr(s_image_message_id, sizeof(s_image_message_id), message->image_token);
  if (budget_cap) {
    s_image_budget_cap = (uint16_t)PG_MIN(budget_cap, 65000);
  }
  message->image_retry_level++;
  message->image_requested = true;
  message->image_failed = false;
  message->image_progress = 0;
  copy_cstr(message->image_error, sizeof(message->image_error),
            detail && detail[0] ? detail : "Retrying");
  set_message_image_progress(message, 12);
  if (!send_active_image_request()) {
    schedule_image_retry();
  }
  mark_messages_dirty();
  return true;
}

// Called whenever the chat scrolls, the selection moves or a transfer ends.
// Photos load one at a time, closest to the screen first. Loaded photos stay
// until their memory is needed for a photo nearer the screen.
static void request_next_image(void) {
  if (!s_messages_root || s_message_count == 0) {
    return;
  }
  release_images_for_heap();
  for (int i = 0; i < s_message_count; i++) {
    // A photo that failed gets another try once the user scrolls back to it.
    if (s_messages[i].image_failed && message_image_distance(i) > IMAGE_KEEP_MARGIN) {
      s_messages[i].image_failed = false;
      s_messages[i].image_error[0] = '\0';
      s_messages[i].image_progress = 0;
      s_messages[i].image_retry_level = 0;
    }
  }

  if (s_page_load_waiting) {
    if (s_loading_older_messages || s_loading_newer_messages) {
      return;  // photos resume once the requested messages are in
    }
    s_page_load_waiting = false;
  }

  if (s_image_message_id[0]) {
    Message *active_message = find_message_by_image_token(s_image_message_id);
    int active_index = message_index_from_ptr(active_message);
    int active_distance = message_image_distance(active_index);
    bool abandon = active_index < 0 || active_distance > IMAGE_KEEP_MARGIN;
    if (!abandon && active_distance > 0 && !s_image_buffer) {
      // Nothing has arrived yet; if another photo is now on screen, switch.
      int best = find_best_image_candidate();
      abandon = best >= 0 && message_image_distance(best) == 0;
    }
    if (!abandon) {
      return;
    }
    IMAGE_DIAG("PGIMG watch active dropped msg=%s distance=%d", s_image_message_id, active_distance);
    clear_active_image_request();
  }

  int image_index = find_best_image_candidate();
  if (image_index < 0) {
    return;
  }

  Message *message = &s_messages[image_index];
  message->image_failed = false;
  copy_cstr(message->image_error, sizeof(message->image_error), "Waiting phone");
  message->image_requested = true;
  set_message_image_progress(message, 5);
  reset_image_transfer_state();
  s_image_budget_cap = 0;
  copy_cstr(s_image_message_id, sizeof(s_image_message_id), message->image_token);
  if (!send_active_image_request() && s_image_message_id[0]) {
    schedule_image_retry();
  }
  mark_messages_dirty();
}

static void image_retry_timer_callback(void *data) {
  s_image_retry_timer = NULL;
  if (!send_active_image_request()) {
    request_next_image();
  }
}

static void loader_timer_callback(void *data) {
  s_loader_timer = NULL;
  if (!s_loading_older_messages || s_view_state != ViewStateChat) {
    return;
  }
  s_loader_frame++;
  mark_messages_dirty();
  s_loader_timer = app_timer_register(OLDER_LOADER_FRAME_MS, loader_timer_callback, NULL);
}

static void start_loader_animation(void) {
  if (!s_loader_timer) {
    s_loader_frame = 0;
    s_loader_timer = app_timer_register(OLDER_LOADER_FRAME_MS, loader_timer_callback, NULL);
  }
}

static void request_older_messages(bool silent) {
  if (s_at_oldest) {
    if (!silent) {
      show_status("No older messages");
    }
    return;
  }
  if (s_message_count <= 0 || !s_messages[0].id[0]) {
    if (!silent) {
      show_status("No older messages");
    }
    return;
  }
  // Photos on screen stay put while the next page loads; the page's messages
  // are staged and matched back to their loaded photos.
  make_room_for_message_page();
  if (!silent) {
    // The phone drops photo transfers when asked for a page; pause photos
    // until the page has arrived so it isn't stuck behind them.
    s_page_load_waiting = true;
    clear_active_image_request();
  }
  if (s_loading_older_messages) {
    if (silent) {
      return;
    }
    cancel_message_timeout();
    cancel_message_retry();
    clear_message_stage();
    reset_message_stream_state();
    s_loading_older_messages = false;
    s_older_anchor_id[0] = '\0';
    s_older_anchor_y = 0;
  }
  recalc_message_layout();
  if (s_selected_message >= 0 && s_selected_message < s_message_count) {
    copy_cstr(s_older_anchor_id, sizeof(s_older_anchor_id), s_messages[s_selected_message].id);
    s_older_anchor_y = s_message_y[s_selected_message];
  } else {
    s_older_anchor_id[0] = '\0';
    s_older_anchor_y = 0;
  }
  s_loading_older_messages = true;
  start_loader_animation();
  s_message_request_attempts = 1;
  if (!silent) {
    show_status("Loading older...");
  }
  const char *anchor_id = (s_selected_message >= 0 && s_selected_message < s_message_count) ?
                          s_messages[s_selected_message].id : s_messages[0].id;
  if (!send_command_with_status("get_older_messages", s_current_chat_id, silent ? "silent" : NULL,
                                s_messages[0].id, anchor_id, !silent)) {
    if (silent) {
      s_loading_older_messages = false;
      s_older_anchor_id[0] = '\0';
      s_older_anchor_y = 0;
    } else {
      schedule_message_send_retry();
    }
  } else {
    schedule_message_timeout();
  }
}

static void request_newer_messages(bool silent) {
  if (s_message_count <= 0 || !s_messages[s_message_count - 1].id[0]) {
    if (!silent) {
      scroll_to_bottom(true);
    }
    return;
  }
  if (s_at_newest) {
    if (!silent) {
      scroll_to_bottom(true);
    }
    return;
  }
  if (s_loading_newer_messages) {
    return;
  }
  make_room_for_message_page();
  if (!silent) {
    s_page_load_waiting = true;
    clear_active_image_request();
  }
  recalc_message_layout();
  if (s_selected_message >= 0 && s_selected_message < s_message_count) {
    copy_cstr(s_newer_anchor_id, sizeof(s_newer_anchor_id), s_messages[s_selected_message].id);
    s_newer_anchor_y = s_message_y[s_selected_message];
  } else {
    s_newer_anchor_id[0] = '\0';
    s_newer_anchor_y = 0;
  }
  s_loading_newer_messages = true;
  s_message_request_attempts = 1;
  if (!silent) {
    show_status("Loading newer...");
  }
  const char *anchor_id = (s_selected_message >= 0 && s_selected_message < s_message_count) ?
                          s_messages[s_selected_message].id : s_messages[s_message_count - 1].id;
  bool sent = send_command_with_status("get_newer_messages", s_current_chat_id, silent ? "silent" : NULL,
                                       s_messages[s_message_count - 1].id, anchor_id, !silent);
  if (!sent) {
    if (silent) {
      s_loading_newer_messages = false;
      s_newer_anchor_id[0] = '\0';
      s_newer_anchor_y = 0;
    } else {
      schedule_message_send_retry();
    }
  } else {
    schedule_message_timeout();
  }
}

static void request_messages(const char *chat_id) {
  s_leave_pending = false;
  cancel_message_timeout();
  cancel_message_retry();
  // Any draft still here belongs to the previous chat (leaving it already saved it):
  // mark it synced so it is never saved into this one.
#if TOUCH_KEYBOARD_AVAILABLE
  s_kb_synced = kb_draft_hash(s_kb_draft, kb_reply_target());
#endif
  reset_touch_keyboard();
  destroy_message_images();
  clear_message_stage();
  s_loading_older_messages = false;
  s_loading_newer_messages = false;
  s_older_anchor_id[0] = '\0';
  s_newer_anchor_id[0] = '\0';
  s_older_anchor_y = 0;
  s_newer_anchor_y = 0;
  s_at_newest = true;
  s_at_oldest = false;
  s_message_scroll_direction = 0;
  s_message_count = 0;
  s_expected_rows = 0;
  s_message_transfer_id = 0;
  reset_message_stream_state();
  s_selected_message = -1;
  s_user_scrolled_messages = false;
  s_compose_floating = false;
  s_compose_float_run = 0;
  cancel_tap_menu();
  s_chat_view_pending = false;
  s_loading_messages = true;
  s_message_request_attempts = 1;
  show_status("Loading messages...");
  if (s_view_state != ViewStateChat || !s_messages_root) {
    s_chat_view_pending = false;
  } else {
    render_messages();
    mark_messages_dirty();
  }
  if (send_command_with_status("get_messages", chat_id, NULL, NULL, NULL, false)) {
    schedule_message_timeout();
  } else {
    schedule_message_send_retry();
  }
}

static void maybe_prefetch_older_messages(void) {
  if (!s_loading_older_messages && !s_at_oldest && s_message_count > 0 &&
      s_selected_message >= 0) {
    const char *anchor_id = s_messages[s_selected_message].id;
    send_command_with_status("prefetch_older_messages", s_current_chat_id, "silent",
                             s_messages[0].id, anchor_id, false);
  }
}

static void maybe_prefetch_newer_messages(void) {
  if (!s_loading_newer_messages && !s_at_newest && s_message_count > 0 &&
      s_selected_message >= 0) {
    const char *anchor_id = s_messages[s_selected_message].id;
    send_command_with_status("prefetch_newer_messages", s_current_chat_id, "silent",
                             s_messages[s_message_count - 1].id, anchor_id, false);
  }
}

static void send_text_message(const char *text, bool as_reply) {
  const char *reply_to = NULL;
  if (!text || !text[0]) {
    show_status("Nothing to send");
    return;
  }
  if (as_reply && s_selected_message >= 0 && s_selected_message < s_message_count) {
    reply_to = s_messages[s_selected_message].id;
  }
  show_status("Sending...");
  if (send_command("send_message", s_current_chat_id, text, reply_to, NULL) && s_at_newest) {
    show_pending_bubble(text);
  }
}

static void send_bar_draft(void) {
  if (s_bar_draft[0]) {
    send_text_message(s_bar_draft, false);
    chat_ui_set_draft("");
  }
}

void chat_ui_set_draft(const char *text) {
  copy_cstr(s_bar_draft, sizeof(s_bar_draft), text ? text : "");
  if (s_view_state == ViewStateChat) {
    recalc_message_layout();  // the bar is taller with a draft
    mark_messages_dirty();
  }
}

static void edit_selected_message(const char *text) {
  if (!s_pending_edit_message_id[0]) {
    show_status("No edit target");
    return;
  }
  show_status("Editing...");
  send_command("edit_message", s_current_chat_id, text, NULL, s_pending_edit_message_id);
  s_pending_edit_message_id[0] = '\0';
}

static bool has_selected_message(void) {
  return s_selected_message >= 0 && s_selected_message < s_message_count;
}

static bool compose_target_is_selected(void) {
  return s_at_newest && s_selected_message == s_message_count;
}

static bool selected_message_is_truncated(void) {
  return has_selected_message() &&
         (int)strlen(s_messages[s_selected_message].text) > MESSAGE_PREVIEW_TEXT;
}

static bool selected_message_has_context(void) {
  return has_selected_message() && message_has_context(&s_messages[s_selected_message]);
}

static bool selected_message_context_is_forward(void) {
  return selected_message_has_context() &&
         strncmp(s_messages[s_selected_message].context, "Fwd from ", 9) == 0;
}

// PebbleOS 4.29.0 added ~1,200 emoji to the system fonts.
static bool emoji_fonts_expanded(void) {
  return true; // built into a firmware that has them
}

// The index-th item of a NUL-separated list the current emoji font can draw (index in range).
static const char *emoji_choice_at(const char *list, const uint32_t *classic, int index) {
  for (int bit = 0;; bit++, list += strlen(list) + 1) {
    if ((emoji_fonts_expanded() || ((classic[bit / 32] >> (bit % 32)) & 1)) && index-- == 0) {
      return list;
    }
  }
}

// The last reaction slot is "Remove".
static int reaction_grid_count(void) {
  return (emoji_fonts_expanded() ? REACTION_CHOICES_COUNT : REACTION_CHOICES_CLASSIC_COUNT) + 1;
}

static bool reaction_grid_is_remove(int index) {
  return index == reaction_grid_count() - 1;
}

static const char *reaction_grid_glyph_at(int index) {
  if (index < 0 || index >= reaction_grid_count()) {
    return "";
  }
  if (reaction_grid_is_remove(index)) {
    return "Remove";
  }
  return emoji_choice_at(REACTION_CHOICES, REACTION_CHOICES_CLASSIC, index);
}

// The phone accepts the emoji itself as the reaction token.
static const char *reaction_grid_token_at(int index) {
  if (index < 0 || index >= reaction_grid_count()) {
    return "";
  }
  return reaction_grid_is_remove(index) ? "remove" : reaction_grid_glyph_at(index);
}

static int emoji_reply_count(void) {
  return emoji_fonts_expanded() ? EMOJI_REPLY_CHOICES_COUNT : EMOJI_REPLY_CHOICES_CLASSIC_COUNT;
}

static const char *emoji_reply_glyph_at(int index) {
  if (index < 0 || index >= emoji_reply_count()) {
    return "";
  }
  return emoji_choice_at(EMOJI_REPLY_CHOICES, EMOJI_REPLY_CHOICES_CLASSIC, index);
}

static void send_selected_reaction(const char *token) {
  if (!has_selected_message()) {
    return;
  }
  show_status((token && strcmp(token, "remove") == 0) ? "Removing..." : "Reacting...");
  send_command("send_reaction", s_current_chat_id, token, NULL, s_messages[s_selected_message].id);
}

// The chat a chat action is about: the open one in a chat, else the list's selected row. -1 if none.
static int action_chat_index(void) {
  if (s_view_state == ViewStateChat) {
    for (int i = 0; i < s_chat_count; i++) {
      if (strcmp(s_chats[i].id, s_current_chat_id) == 0) {
        return i;
      }
    }
    return -1;
  }
  return s_selected_chat >= 0 && s_selected_chat < s_chat_count ? s_selected_chat : -1;
}

static bool chat_is_unread(const Chat *chat) {
  return chat->unread || chat->unread_count > 0;
}

static void send_selected_chat_action(const char *command) {
  int index = action_chat_index();
  if (index < 0) {
    return;
  }
  show_status("Updating...");
  send_command(command, s_chats[index].id, NULL, NULL, NULL);
}

// After archiving or marking unread from inside a chat: back to the list, once the menu has closed.
static char s_chat_to_remove[MAX_ID];

static void leave_chat_after_action(void *data) {
  if (s_view_state == ViewStateChat) {
    main_back_click_handler(NULL, NULL);
  }
  for (int i = 0; s_chat_to_remove[0] && i < s_chat_count; i++) {
    if (strcmp(s_chats[i].id, s_chat_to_remove) == 0) {
      remove_chat_at(i);
      break;
    }
  }
  s_chat_to_remove[0] = '\0';
}

// Message text follows the watch's notification text size (Settings > Notifications > Text Size),
// the size a notification's body is in: Gothic 18, 24 Bold or 28 (Chat's biggest, for Extra Large too).
static uint8_t system_text_size(void) {
  PreferredContentSize size = alerts_preferences_get_notification_content_size();
  if (size == NotificationContentSizeSystem) {
    size = system_theme_get_content_size();
  }
  switch (size) {
    case PreferredContentSizeSmall:
      return 1;
    case PreferredContentSizeMedium:
      return 2;
    default:
      return 3;
  }
}

static void appearance_load(void) {
  Appearance saved;
  if (persist_read_data(APPEARANCE_KEY, &saved, sizeof(saved)) == (int)sizeof(saved) &&
      saved.version == APPEARANCE_VERSION && saved.text_size < 4 && saved.pattern <= PatternLast) {
    s_appearance = saved;
  } else {
    appearance_defaults(&s_appearance);
  }
  s_appearance.text_size = system_text_size();
  s_appearance.dark = system_theme_is_dark_mode();  // Settings > Display, like the system apps
  theme_apply();
}

// Re-themes whatever is on screen after the settings changed.
static void appearance_refresh(void *data) {
  theme_apply();
#ifdef PBL_COLOR
  wallpaper_rebuild();
#endif
  // Cached bubble sizes were measured with the old font. Measuring them again
  // needs a lot of stack (emoji reaction pills), more than the AppMessage and
  // draw callbacks have to spare, so this whole function runs from a timer.
  for (int i = 0; i < MAX_MESSAGES; i++) {
    s_messages[i].layout_key = NULL;
    if (s_message_stage) {
      s_message_stage[i].layout_key = NULL;
    }
  }
  recalc_message_layout();
  window_set_background_color(s_main_window, TG_LIST_BG);
  if (s_chat_menu) {
    menu_layer_set_normal_colors(s_chat_menu, TG_LIST_BG, TG_TEXT);
    layer_mark_dirty(menu_layer_get_layer(s_chat_menu));
  }
  if (s_header_layer) {
    layer_mark_dirty(s_header_layer);
  }
  mark_messages_dirty();
  if (s_action_layer) {
    layer_mark_dirty(s_action_layer);
  }
}

static void dark_mode_changed(bool is_dark) {
  s_appearance.dark = is_dark;
  app_timer_register(0, appearance_refresh, NULL);
}

#ifdef PBL_COLOR
static uint8_t tuple_color(DictionaryIterator *iter, uint32_t key, uint8_t fallback) {
  Tuple *tuple = dict_find(iter, key);
  return tuple ? GColorFromHEX(tuple->value->int32).argb : fallback;
}
#endif

static void appearance_receive(DictionaryIterator *iter) {
  Appearance *a = &s_appearance;
  a->dark = system_theme_is_dark_mode();
  a->pattern = PG_MAX(0, PG_MIN(PatternLast, tuple_int(iter, MESSAGE_KEY_BgPattern, a->pattern)));
  a->text_size = system_text_size();  // the watch's setting, not the phone's
  a->bold = tuple_int(iter, MESSAGE_KEY_TextBold, a->bold) == 1;
  a->photos = tuple_int(iter, MESSAGE_KEY_ShowPhotos, a->photos) == 1;
  a->big_emoji = tuple_int(iter, MESSAGE_KEY_BigEmoji, a->big_emoji) == 1;
#ifdef PBL_COLOR
  a->accent = tuple_color(iter, MESSAGE_KEY_AccentColor, a->accent);
  a->chat_bg = tuple_color(iter, MESSAGE_KEY_ChatBgColor, a->chat_bg);
  a->out_bubble = tuple_color(iter, MESSAGE_KEY_OutBubbleColor, a->out_bubble);
#endif
  persist_write_data(APPEARANCE_KEY, a, sizeof(*a));
  app_timer_register(0, appearance_refresh, NULL);
}

static void inbox_received_callback(DictionaryIterator *iter, void *context) {
  char *type = tuple_cstring(iter, MESSAGE_KEY_Type);
  if (!type) {
    return;
  }
  // Incoming messages need heap; make sure photos have left the reserve free.
  release_images_for_heap();

  if (strcmp(type, "qr") == 0) {
    int modules = tuple_int(iter, MESSAGE_KEY_ImageWidth, 0);
    Tuple *qr_data = dict_find(iter, MESSAGE_KEY_ImageData);
    int qr_bytes = modules > 0 ? (modules * modules + 7) / 8 : 0;
    if (modules <= 0 || modules > QR_MAX_MODULES || !qr_data || qr_data->length < qr_bytes) {
      clear_qr();
    } else {
      memcpy(s_qr_bits, qr_data->value->data, qr_bytes);
      s_qr_modules = modules;
      s_loading_error = false;
      s_offer_qr = false;
    }
    mark_chat_menu_dirty();
    return;
  }

  if (strcmp(type, "status") == 0) {
    char *status = tuple_cstring(iter, MESSAGE_KEY_Status);
    if (status) {
      if (s_chats_loading && s_view_state != ViewStateChat) {
        s_chats_loading = true;
        show_loading_text(status, false);
      }
      if (strcmp(status, "Loading messages...") == 0) {
        cancel_message_retry();
      }
      show_status(status);
    }
    return;
  }

  if (strcmp(type, "settings") == 0) {
    update_canned_replies(tuple_cstring(iter, MESSAGE_KEY_CannedReplies));
    return;
  }

  if (strcmp(type, "features") == 0) {
    int network = tuple_int(iter, MESSAGE_KEY_Network, -1);
    if (network >= 0 && network < CHAT_NETWORKS) {
      copy_cstr(s_network_features[network], sizeof(s_network_features[network]),
                tuple_cstring(iter, MESSAGE_KEY_Text));
      refresh_message_actions();
    }
    return;
  }

  if (strcmp(type, "favorite") == 0) {
    int index = tuple_int(iter, MESSAGE_KEY_Index, -1);
    if (index == 0) {
      s_favorite_count = 0;
    }
    if (index >= 0 && index < MAX_FAVORITES) {
      copy_cstr(s_favorites[index].id, sizeof(s_favorites[index].id),
                tuple_cstring(iter, MESSAGE_KEY_ChatId));
      copy_cstr(s_favorites[index].name, sizeof(s_favorites[index].name),
                tuple_cstring(iter, MESSAGE_KEY_Sender));
      s_favorites[index].network = (uint8_t)tuple_int(iter, MESSAGE_KEY_Network, 0);
      s_favorite_count = PG_MAX(s_favorite_count, index + 1);
    }
    return;
  }

  if (strcmp(type, "favorites_done") == 0) {
    s_favorite_count = PG_MIN(s_favorite_count, tuple_int(iter, MESSAGE_KEY_Count, 0));
    if (s_chat_menu) {
      menu_layer_reload_data(s_chat_menu);  // their bubbles
      if (fav_focused()) {
        fav_reveal();
      }
    }
    return;
  }

  if (strcmp(type, "draft") == 0) {
    touch_keyboard_draft_received(iter);
    return;
  }

  if (strcmp(type, "appearance") == 0) {
    appearance_receive(iter);
    return;
  }

  if (strcmp(type, "error") == 0) {
    char *error = tuple_cstring(iter, MESSAGE_KEY_Error);
    char *offer = tuple_cstring(iter, MESSAGE_KEY_Text);
    clear_qr();
    s_offer_qr = offer && strcmp(offer, "qr") == 0;
    if (error && strncmp(error, "Send failed", 11) == 0) {
      touch_keyboard_send_result(false);
    }
    cancel_message_timeout();
    cancel_message_retry();
    cancel_timer(&s_chat_retry_timer);
    if (s_view_state != ViewStateChat) {
      s_bridge_ready = false;
      s_chats_loading = true;
      show_loading_text(error ? error : "Login failed", true);
      show_status("Chat");
      reveal_hidden_chat_list();
    } else {
      s_loading_messages = false;
      s_loading_older_messages = false;
      s_loading_newer_messages = false;
      s_message_transfer_id = 0;
      show_status(error ? error : "Error");
      mark_messages_dirty();
    }
    return;
  }

  int index = tuple_int(iter, MESSAGE_KEY_Index, 0);
  int count = tuple_int(iter, MESSAGE_KEY_Count, 0);
  s_expected_rows = count;

  if (strcmp(type, "chats_done") == 0) {
    cancel_message_timeout();
    cancel_timer(&s_chat_retry_timer);
    char selected_id[MAX_ID];
    copy_cstr(selected_id, sizeof(selected_id), s_chat_list_selected_id);
    if (!selected_id[0]) {
      copy_cstr(selected_id, sizeof(selected_id), s_chat_refresh_selected_id);
    }
    s_chat_refresh_selected_id[0] = '\0';
    if (!selected_id[0] && s_selected_chat >= 0 && s_selected_chat < s_chat_count) {
      copy_cstr(selected_id, sizeof(selected_id), s_chats[s_selected_chat].id);
    }
    s_bridge_ready = true;
    s_chats_loading = false;
    s_loading_error = false;
    s_offer_qr = false;
    clear_qr();
    int visible_count = PG_MAX(0, PG_MIN(count, MAX_CHATS));
    for (int i = visible_count; i < s_chat_count; i++) {
      clear_chat_slot(&s_chats[i]);
    }
    if (s_chat_count > visible_count) {
      s_chat_count = visible_count;
    }
    int preserved_index = find_chat_index_by_id(selected_id);
    if (preserved_index >= 0) {
      s_selected_chat = preserved_index;
      copy_cstr(s_chat_list_selected_id, sizeof(s_chat_list_selected_id), s_chats[s_selected_chat].id);
    } else if (s_selected_chat >= s_chat_count) {
      s_selected_chat = s_chat_count > 0 ? s_chat_count - 1 : 0;
    }
    // A new message moves its chat up the list. Above the selected row it would land off screen,
    // since the list keeps the selection where it was: select that chat, so the news is seen.
    if (s_view_state == ViewStateChatList && s_chat_seen_count > 0) {
      for (int i = 0; i < s_selected_chat && i < s_chat_count; i++) {
        if (s_chats[i].unread && !chat_row_was_seen(&s_chats[i])) {
          s_selected_chat = i;
          copy_cstr(s_chat_list_selected_id, sizeof(s_chat_list_selected_id), s_chats[i].id);
          break;
        }
      }
    }
    if (s_chat_menu) {
      menu_layer_reload_data(s_chat_menu);
      select_chat_row(s_selected_chat, false);
      if (fav_focused()) {
        fav_reveal();  // the bubble the buttons are on stays in view
      }
    }
    show_status("Chat");
    return;
  }

  if (strcmp(type, "messages_start") == 0) {
    int transfer_id = tuple_int(iter, MESSAGE_KEY_ImageTransferId, 0);
    int mode = tuple_int(iter, MESSAGE_KEY_Index, MESSAGE_MODE_INITIAL);
    bool initial = mode == MESSAGE_MODE_INITIAL;
    bool requested_page = s_loading_older_messages || s_loading_newer_messages;
    if (initial && s_view_state != ViewStateChat && !s_loading_messages) {
      return;
    }
    s_message_transfer_id = transfer_id;
    s_expected_rows = count;
    s_message_stream_mode = mode;
    char *stream_flag = tuple_cstring(iter, MESSAGE_KEY_Text);
    s_message_stream_silent = stream_flag && strcmp(stream_flag, "silent") == 0;
    IMAGE_DIAG("PGIMG watch messages_start mode=%d count=%d transfer=%d initial=%d silent=%d",
               mode, count, transfer_id, initial ? 1 : 0, s_message_stream_silent ? 1 : 0);
    if (initial) {
      clear_message_stage();
      clear_message_rows();
      s_loading_older_messages = false;
      s_loading_newer_messages = false;
      s_older_anchor_id[0] = '\0';
      s_newer_anchor_id[0] = '\0';
      s_at_newest = true;
      s_at_oldest = false;
    } else if (requested_page && !prepare_message_stage()) {
      reset_message_stream_state();
      s_loading_older_messages = false;
      s_loading_newer_messages = false;
      show_status("Memory low");
      return;
    }
    if (initial && !s_message_stream_silent && s_messages_root) {
      mark_messages_dirty();
    }
    return;
  }

  if (strcmp(type, "message_prepend") == 0) {
    char anchor_id[MAX_ID];
    int anchor_y = 0;
    Message *slot;
    if (!message_transfer_matches(iter)) {
      return;
    }
    if (s_view_state != ViewStateChat || !s_messages_root) {
      return;
    }
    anchor_id[0] = '\0';
    recalc_message_layout();
    if (!s_message_stream_silent && s_loading_older_messages && s_older_anchor_id[0]) {
      copy_cstr(anchor_id, sizeof(anchor_id), s_older_anchor_id);
      int anchor_index = find_message_index_by_id(s_older_anchor_id);
      if (anchor_index >= 0) {
        anchor_y = s_message_y[anchor_index];
      }
    } else if (has_selected_message()) {
      copy_cstr(anchor_id, sizeof(anchor_id), s_messages[s_selected_message].id);
      anchor_y = s_message_y[s_selected_message];
    }
    slot = prepend_message_slot();
    populate_message_from_tuple(slot, iter);
    s_expected_rows = count;
    render_after_stream_prepend(anchor_id, anchor_y);
    return;
  }

  if (strcmp(type, "message_append") == 0) {
    char anchor_id[MAX_ID];
    int anchor_y = 0;
    Message *slot;
    bool follow_bottom;
    if (!message_transfer_matches(iter)) {
      return;
    }
    if (s_view_state != ViewStateChat || !s_messages_root) {
      return;
    }
    anchor_id[0] = '\0';
    recalc_message_layout();
    follow_bottom = s_at_newest && compose_target_is_selected();
    if (!s_message_stream_silent && s_loading_newer_messages && s_newer_anchor_id[0]) {
      copy_cstr(anchor_id, sizeof(anchor_id), s_newer_anchor_id);
      int anchor_index = find_message_index_by_id(s_newer_anchor_id);
      if (anchor_index >= 0) {
        anchor_y = s_message_y[anchor_index];
      }
    } else if (has_selected_message()) {
      copy_cstr(anchor_id, sizeof(anchor_id), s_messages[s_selected_message].id);
      anchor_y = s_message_y[s_selected_message];
    }
    char *incoming_text = tuple_cstring(iter, MESSAGE_KEY_Text);
    int pending_index = find_message_index_by_id("pending");
    bool replaces_pending = pending_index >= 0 &&
                            tuple_int(iter, MESSAGE_KEY_IsOutgoing, 0) != 0 &&
                            incoming_text &&
                            text_matches_ignoring_trailing_space(incoming_text,
                                                                 s_messages[pending_index].text);
    if (replaces_pending) {
      // Swap the optimistic bubble for the real message, keeping it in place.
      clear_message_slot(&s_messages[pending_index]);
      memset(&s_messages[pending_index], 0, sizeof(Message));
      init_message_strings(&s_messages[pending_index]);
      slot = &s_messages[pending_index];
    } else {
      slot = append_message_slot();
    }
    populate_message_from_tuple(slot, iter);
    s_expected_rows = count;
    if (!s_message_stream_silent && follow_bottom) {
      scroll_to_bottom(false);
      return;
    }
    render_after_stream_append(anchor_id, anchor_y);
    return;
  }

  if (strcmp(type, "message_update") == 0) {
    char selected_id[MAX_ID];
    char *incoming_id = tuple_cstring(iter, MESSAGE_KEY_MessageId);
    int update_index;
    int selected_y = 0;
    int updated_selected_y = 0;
    if (!incoming_id || s_view_state != ViewStateChat || !s_messages_root) {
      return;
    }
    update_index = find_message_index_by_id(incoming_id);
    if (update_index < 0) {
      return;
    }
    selected_id[0] = '\0';
    if (has_selected_message()) {
      recalc_message_layout();
      copy_cstr(selected_id, sizeof(selected_id), s_messages[s_selected_message].id);
      selected_y = s_message_y[s_selected_message];
    }
    populate_message_from_tuple(&s_messages[update_index], iter);
    if (selected_id[0]) {
      int selected_index = find_message_index_by_id(selected_id);
      if (selected_index >= 0) {
        s_selected_message = selected_index;
        recalc_message_layout();
        updated_selected_y = s_message_y[selected_index];
        set_chat_scroll_offset(s_chat_scroll_offset + (updated_selected_y - selected_y), false);
      } else {
        recalc_message_layout();
      }
    } else {
      recalc_message_layout();
    }
    mark_messages_dirty();
    request_next_image();
    return;
  }

  if (strcmp(type, "messages_done") == 0) {
    if (!message_transfer_matches(iter)) {
      return;
    }
    cancel_message_timeout();
    cancel_message_retry();
    char selected_id[MAX_ID];
    bool loading_initial = s_loading_messages;
    bool loading_older = s_loading_older_messages;
    bool loading_newer = s_loading_newer_messages;
    char *done_flag = tuple_cstring(iter, MESSAGE_KEY_Text);
    bool loading_silent = s_message_stream_silent || (done_flag && strcmp(done_flag, "silent") == 0);
    bool show_pending = s_chat_view_pending;
    bool chat_visible = s_view_state == ViewStateChat && s_messages_root;
    s_loading_messages = false;
    s_message_transfer_id = 0;
    selected_id[0] = '\0';

    if (loading_older && count == 0) {
      s_touch_paging = false;
      s_loading_older_messages = false;
      s_older_anchor_id[0] = '\0';
      s_older_anchor_y = 0;
      s_at_oldest = true;
      clear_message_stage();
      reset_message_stream_state();
      if (!loading_silent) {
        show_status("No older messages");
        mark_messages_dirty();
      }
      return;
    }

    if (loading_newer && count == 0) {
      s_loading_newer_messages = false;
      s_newer_anchor_id[0] = '\0';
      s_newer_anchor_y = 0;
      s_at_newest = true;
      clear_message_stage();
      reset_message_stream_state();
      if (!loading_silent) {
        show_status(s_current_chat_title);
        mark_messages_dirty();
      }
      return;
    }

    char fallback_id[MAX_ID];
    bool staged_load = loading_older || loading_newer;
    bool live_anchor = false;
    int live_anchor_y = 0;
    int live_anchor_scroll_offset = s_chat_scroll_offset;
    fallback_id[0] = '\0';

    if (chat_visible && staged_load && has_selected_message()) {
      recalc_message_layout();
      copy_cstr(selected_id, sizeof(selected_id), s_messages[s_selected_message].id);
      live_anchor_y = s_message_y[s_selected_message];
      live_anchor_scroll_offset = s_chat_scroll_offset;
      live_anchor = true;
    }

    if (loading_older) {
      s_at_newest = false;
      s_at_oldest = false;
      copy_cstr(fallback_id, sizeof(fallback_id), s_older_anchor_id);
    } else if (loading_newer) {
      s_at_oldest = false;
      copy_cstr(fallback_id, sizeof(fallback_id), s_newer_anchor_id);
    } else if (s_user_scrolled_messages && s_selected_message >= 0 && s_selected_message < s_message_count) {
      copy_cstr(selected_id, sizeof(selected_id), s_messages[s_selected_message].id);
    }

    bool reversed_load = staged_load &&
                         ((loading_older && s_message_scroll_direction > 0) ||
                          (loading_newer && s_message_scroll_direction < 0));
    if (reversed_load) {
      int current_direction = s_message_scroll_direction;
      end_page_load();
      if (chat_visible) {
        show_status(s_current_chat_title);
        mark_messages_dirty();
      }
      if (current_direction > 0 && !s_at_newest) {
        request_newer_messages(true);
      } else if (current_direction < 0 && !s_at_oldest) {
        request_older_messages(true);
      }
      return;
    }

    if (staged_load && live_anchor && !message_stage_contains_id(selected_id)) {
      end_page_load();
      if (chat_visible) {
        show_status(s_current_chat_title);
        mark_messages_dirty();
      }
      // A page without the message the user is on is asked again, but not forever: a phone that
      // keeps sending such pages would keep the radio busy and the spinner turning.
      if (++s_page_miss_count > 2) {
        s_page_miss_count = 0;
        s_touch_paging = false;
        return;
      }
      if (loading_older && !s_at_oldest) {
        request_older_messages(true);
      } else if (loading_newer && !s_at_newest) {
        request_newer_messages(true);
      }
      return;
    }

    if (staged_load) {
      s_page_miss_count = 0;
    }
    if (staged_load && !live_anchor && fallback_id[0]) {
      copy_cstr(selected_id, sizeof(selected_id), fallback_id);
    }

    if (staged_load && s_message_stage) {
      commit_message_stage(count);
    } else if (s_message_count > count) {
      s_message_count = count;
    }
    IMAGE_DIAG("PGIMG watch messages_done count=%d staged=%d visible=%d selected=%d rows=%d",
               count, staged_load ? 1 : 0, chat_visible ? 1 : 0,
               s_selected_message, s_message_count);
    if (!staged_load) {
      clear_message_stage();
    }
    int preserved_index = find_message_index_by_id(selected_id);
    if (preserved_index < 0 && staged_load && fallback_id[0]) {
      copy_cstr(selected_id, sizeof(selected_id), fallback_id);
      preserved_index = find_message_index_by_id(selected_id);
    }
    if (preserved_index >= 0) {
      s_selected_message = preserved_index;
    } else if (!s_user_scrolled_messages && !staged_load) {
      s_selected_message = s_at_newest ? s_message_count : (s_message_count > 0 ? s_message_count - 1 : -1);
    } else if (s_message_count > 0 && s_selected_message >= s_message_count) {
      s_selected_message = s_message_count - 1;
    } else if (s_message_count <= 0) {
      s_selected_message = -1;
    }
    s_loading_older_messages = false;
    s_loading_newer_messages = false;
    s_older_anchor_id[0] = '\0';
    s_newer_anchor_id[0] = '\0';
    reset_message_stream_state();
    s_expected_rows = count;
    if (!loading_older && !loading_newer && s_selected_chat >= 0 && s_selected_chat < s_chat_count) {
      s_chats[s_selected_chat].unread = false;
      s_chats[s_selected_chat].unread_count = 0;
    }

    if (chat_visible) {
      recalc_message_layout();
      if (staged_load && preserved_index >= 0) {
        GRect bounds = layer_get_bounds(s_messages_root);
        int margin = 6;
        if (loading_older && s_touch_paging && live_anchor) {
          // A drag fetched them: the message under the finger stays put, so the drag carries on
          s_touch_paging = false;
          set_chat_scroll_offset(live_anchor_scroll_offset + (s_message_y[preserved_index] - live_anchor_y),
                                 false);
          show_status(s_current_chat_title);
          mark_messages_dirty();
          s_older_anchor_y = 0;
          s_newer_anchor_y = 0;
          request_next_image();
          return;
        }
        if (loading_older && !loading_silent && preserved_index > 0) {
          s_selected_message = preserved_index - 1;
          set_chat_scroll_offset(s_message_y[s_selected_message] - margin, false);
          show_status(s_current_chat_title);
          mark_messages_dirty();
          s_older_anchor_y = 0;
          s_newer_anchor_y = 0;
          request_next_image();
          return;
        }
        bool selected_is_tall = s_message_h[preserved_index] > bounds.size.h - (margin * 2);
        bool anchor_top = s_message_scroll_direction < 0 ||
                          (s_message_scroll_direction == 0 && loading_older);
        int target = (selected_is_tall && live_anchor) ?
                     live_anchor_scroll_offset + (s_message_y[preserved_index] - live_anchor_y) :
                     (anchor_top ?
                      s_message_y[preserved_index] - margin :
                      s_message_y[preserved_index] + s_message_h[preserved_index] + margin - bounds.size.h);
        set_chat_scroll_offset(target, false);
      }
      if (!has_selected_message() && !loading_silent) {
        scroll_to_bottom(false);
      }
      show_status(s_current_chat_title);
      mark_messages_dirty();
      s_older_anchor_y = 0;
      s_newer_anchor_y = 0;
      request_next_image();
      return;
    }

    s_older_anchor_y = 0;
    s_newer_anchor_y = 0;
    if (!show_pending && loading_initial) {
      s_chat_view_pending = true;
      app_timer_register(1, show_chat_view_timer, NULL);
    }
    return;
  }

  if (strcmp(type, "chat") == 0 && index >= 0 && index < MAX_CHATS) {
    if (index == 0) {
      s_chat_seen_count = s_chat_count;
      for (int i = 0; i < s_chat_count; i++) {
        s_chat_seen[i] = chat_row_hash(&s_chats[i]);
      }
      s_chat_refresh_selected_id[0] = '\0';
      if (s_chat_menu && s_view_state == ViewStateChatList) {
        MenuIndex selected = menu_layer_get_selected_index(s_chat_menu);
        s_selected_chat = selected.row;
        if (s_selected_chat >= 0 && s_selected_chat < s_chat_count) {
          copy_cstr(s_chat_list_selected_id, sizeof(s_chat_list_selected_id),
                    s_chats[s_selected_chat].id);
        }
      }
      if (s_chat_list_selected_id[0]) {
        copy_cstr(s_chat_refresh_selected_id, sizeof(s_chat_refresh_selected_id),
                  s_chat_list_selected_id);
      } else if (s_selected_chat >= 0 && s_selected_chat < s_chat_count) {
        copy_cstr(s_chat_refresh_selected_id, sizeof(s_chat_refresh_selected_id),
                  s_chats[s_selected_chat].id);
      }
    }
    Chat *chat = &s_chats[index];
    char *incoming_id = tuple_cstring(iter, MESSAGE_KEY_ChatId);
    if (incoming_id && strcmp(chat->id, incoming_id) != 0) {
      preserve_chat_avatar(chat, incoming_id);
    }
    copy_cstr(chat->id, sizeof(chat->id), incoming_id);
    set_chat_strings(chat,
                     tuple_cstring(iter, MESSAGE_KEY_Sender),
                     tuple_cstring(iter, MESSAGE_KEY_Text));
    chat->unread = tuple_int(iter, MESSAGE_KEY_IsUnread, 0) != 0;
    chat->unread_count = tuple_int(iter, MESSAGE_KEY_UnreadCount, chat->unread ? 1 : 0);
    parse_chat_meta(chat, tuple_cstring(iter, MESSAGE_KEY_MessageMeta));
    chat->network = (uint8_t)tuple_int(iter, MESSAGE_KEY_Network, CHAT_NETWORK_BEEPER);
    if (strcmp(chat->id, s_current_chat_id) == 0) {
      s_current_chat_flags = chat->flags;
      s_current_chat_network = chat->network;
      refresh_message_actions();
    }
    bool list_grew = index + 1 > s_chat_count;
    if (list_grew) {
      s_chat_count = index + 1;
    }
    s_bridge_ready = true;
    s_loading_error = false;
    if (list_grew) {
      reveal_available_chat_rows();
    }
    if (s_chat_count >= s_expected_rows) {
      show_status("Chat");
    }
    return;
  }

  if (strcmp(type, "message") == 0 && index >= 0 && index < MAX_MESSAGES) {
    cancel_message_timeout();
    cancel_message_retry();
    bool stream_initial = s_message_stream_mode == MESSAGE_MODE_INITIAL;
    Message *message = stream_initial || !s_message_stage ? &s_messages[index] : &s_message_stage[index];
    populate_message_from_tuple(message, iter);
    if (index + 1 > s_message_stage_count && !stream_initial && s_message_stage) {
      s_message_stage_count = index + 1;
    }
    if (index + 1 > s_message_count && stream_initial) {
      s_message_count = index + 1;
    }
    if (stream_initial && !s_message_stream_silent && s_view_state == ViewStateChat && s_messages_root) {
      render_messages();
      mark_messages_dirty();
    }
    int loaded_count = stream_initial ? s_message_count : s_message_stage_count;
    if (loaded_count < s_expected_rows &&
        (s_loading_messages || s_loading_older_messages || s_loading_newer_messages)) {
      schedule_message_timeout();
    }
    if (loaded_count >= s_expected_rows) {
      if (stream_initial) {
        if (!s_user_scrolled_messages) {
          s_selected_message = s_at_newest ? s_message_count : (s_message_count > 0 ? s_message_count - 1 : -1);
        } else if (s_selected_message < 0 || s_selected_message >= s_message_count) {
          s_selected_message = s_message_count > 0 ? s_message_count - 1 : s_message_count;
        }
        request_next_image();
      }
      if (stream_initial && s_loading_messages && s_view_state != ViewStateChat && !s_chat_view_pending) {
        s_chat_view_pending = true;
        app_timer_register(1, show_chat_view_timer, NULL);
      }
    }
    return;
  }

  if (strcmp(type, "avatar_start") == 0) {
    char *chat_id = tuple_cstring(iter, MESSAGE_KEY_ChatId);
    int image_size = tuple_int(iter, MESSAGE_KEY_ImageSize, 0);
    int transfer_id = tuple_int(iter, MESSAGE_KEY_ImageTransferId, 0);
    if (!chat_id || find_chat_index_by_id(chat_id) < 0 || image_size <= 0 || image_size > MAX_AVATAR_BYTES) {
      reset_avatar_transfer_state();
      return;
    }
    if (!ensure_avatar_transfer_buffer(image_size)) {
      reset_avatar_transfer_state();
      return;
    }
    copy_cstr(s_avatar_chat_id, sizeof(s_avatar_chat_id), chat_id);
    s_avatar_size = image_size;
    s_avatar_received = 0;
    s_avatar_expected_offset = 0;
    s_avatar_transfer_id = transfer_id;
    return;
  }

  if (strcmp(type, "avatar") == 0) {
    char *chat_id = tuple_cstring(iter, MESSAGE_KEY_ChatId);
    int offset = tuple_int(iter, MESSAGE_KEY_Index, -1);
    int transfer_id = tuple_int(iter, MESSAGE_KEY_ImageTransferId, 0);
    Tuple *data = dict_find(iter, MESSAGE_KEY_ImageData);
    int data_len = data ? data->length : 0;
    if (!chat_id || strcmp(chat_id, s_avatar_chat_id) != 0 || transfer_id != s_avatar_transfer_id || !data ||
        !s_avatar_buffer ||
        !transfer_chunk_fits(offset, data_len, s_avatar_expected_offset,
                             s_avatar_size, s_avatar_buffer_capacity)) {
      reset_avatar_transfer_state();
      return;
    }
    memcpy(s_avatar_buffer + offset, data->value->data, data_len);
    s_avatar_received = offset + data_len;
    s_avatar_expected_offset = s_avatar_received;
    return;
  }

  if (strcmp(type, "avatar_done") == 0) {
    char *chat_id = tuple_cstring(iter, MESSAGE_KEY_ChatId);
    int transfer_id = tuple_int(iter, MESSAGE_KEY_ImageTransferId, 0);
    int chat_index = find_chat_index_by_id(chat_id);
    if (chat_index >= 0 && chat_id && strcmp(chat_id, s_avatar_chat_id) == 0 &&
        transfer_id == s_avatar_transfer_id && s_avatar_received == s_avatar_size &&
        s_avatar_buffer) {
      Chat *chat = &s_chats[chat_index];
      destroy_chat_avatar(chat);
      bool png = s_avatar_size > 8 && s_avatar_buffer[0] == 0x89 && s_avatar_buffer[1] == 'P';
      if (png) {
        chat->avatar_bitmap = gbitmap_create_from_png_data(s_avatar_buffer, s_avatar_size);
      } else if (s_avatar_size > 12) {
        // A ready-to-draw bitmap: keep the received bytes as its pixels.
        chat->avatar_bitmap = gbitmap_create_with_data(s_avatar_buffer);
        if (chat->avatar_bitmap) {
          chat->avatar_data = s_avatar_buffer;
          s_avatar_buffer = NULL;
          s_avatar_buffer_capacity = 0;
        }
      }
      if (s_chat_menu) {
        layer_mark_dirty(menu_layer_get_layer(s_chat_menu));
      }
    }
    if (chat_id && strcmp(chat_id, s_avatar_chat_id) == 0) {
      reset_avatar_transfer_state();
    }
    return;
  }

  if (strcmp(type, "image_start") == 0) {
    char *message_id = tuple_cstring(iter, MESSAGE_KEY_MessageId);
    int image_size = tuple_int(iter, MESSAGE_KEY_ImageSize, 0);
    int transfer_id = tuple_int(iter, MESSAGE_KEY_ImageTransferId, 0);
    int image_width = tuple_int(iter, MESSAGE_KEY_ImageWidth, 0);
    int image_height = tuple_int(iter, MESSAGE_KEY_ImageHeight, 0);
    char *image_format = tuple_cstring(iter, MESSAGE_KEY_Text);
    Message *message = find_message_by_image_token(message_id);
    bool is_active_image = message_id && strcmp(message_id, s_image_message_id) == 0;
    IMAGE_DIAG("PGIMG watch image_start msg=%s active=%d transfer=%d bytes=%d dims=%dx%d fmt=%s heap=%u",
               message_id ? message_id : "(null)", is_active_image ? 1 : 0,
               transfer_id, image_size, image_width, image_height,
               image_format && image_format[0] ? image_format : "?", image_diag_heap_free());
    if (!message || !is_active_image) {
      request_next_image();
      return;
    }
    int image_index = message_index_from_ptr(message);
    bool packed = image_format && strcmp(image_format, "pbz") == 0;
    bool known_format = packed || (image_format && strcmp(image_format, "pbi") == 0);
    if (transfer_id <= 0 || image_size <= 12 || image_size > MAX_IMAGE_BYTES || !known_format ||
        image_width <= 0 || image_height <= 0) {
      message->image_requested = false;
      message->image_failed = true;
      set_message_image_error(message, "Photo failed");
      reset_image_transfer_state();
      mark_messages_dirty();
      request_next_image();
      return;
    }
    if (message->image_width == 0 || message->image_height == 0) {
      // The phone did not know this photo's shape up front; size the bubble now.
      message->image_width = (uint16_t)image_width;
      message->image_height = (uint16_t)image_height;
      recalc_message_layout();
      if (image_index == s_selected_message && s_messages_root) {
        GRect bounds = layer_get_bounds(s_messages_root);
        if (message_image_scrolls_tall(image_index, bounds, 6)) {
          set_chat_scroll_offset_quiet(message_image_edge_scroll_target(image_index, bounds, 6,
                                                                         s_message_scroll_direction >= 0));
        }
      }
    }
    free_image_transfer_buffer();
    reset_avatar_transfer_state();
    s_image_buffer = malloc(image_size);
    if (!s_image_buffer) {
      int distance = message_image_distance(image_index);
      while (!s_image_buffer && evict_farthest_image(distance)) {
        s_image_buffer = malloc(image_size);
      }
    }
    if (!s_image_buffer) {
      // The heap is too fragmented or full for this size; ask for a smaller one.
      IMAGE_DIAG("PGIMG watch alloc fail msg=%s size=%d heap=%u", message_id, image_size, image_diag_heap_free());
      if (!retry_active_image_request(message, "Resizing", (unsigned)(image_size * 3) / 4)) {
        message->image_requested = false;
        message->image_failed = true;
        set_message_image_error(message, "Not enough memory");
        reset_image_transfer_state();
        request_next_image();
      }
      mark_messages_dirty();
      return;
    }
    s_image_buffer_capacity = (uint16_t)image_size;
    s_image_size = image_size;
    s_image_received = 0;
    s_image_expected_offset = 0;
    s_image_written = 0;
    s_pack_literal = 0;
    s_pack_repeat = 0;
    s_image_packed = packed;
    s_image_transfer_id = transfer_id;
    copy_cstr(message->image_error, sizeof(message->image_error), "Receiving");
    set_message_image_progress(message, 25);
    schedule_image_transfer_timeout();
    mark_messages_dirty();
    return;
  }

  if (strcmp(type, "image_status") == 0) {
    char *message_id = tuple_cstring(iter, MESSAGE_KEY_MessageId);
    char *detail = tuple_cstring(iter, MESSAGE_KEY_Error);
    Message *message = find_message_by_image_token(message_id);
    bool is_active_image = message_id && strcmp(message_id, s_image_message_id) == 0;
    if (message && is_active_image && message->image_requested && !message->image_failed) {
      copy_cstr(message->image_error, sizeof(message->image_error), detail && detail[0] ? detail : "Preparing");
      set_message_image_progress(message, image_loading_phase_percent(message->image_error));
      if (!s_image_buffer) {
        schedule_image_prepare_timeout();
      }
      mark_messages_dirty();
    }
    return;
  }

  if (strcmp(type, "image") == 0) {
    char *message_id = tuple_cstring(iter, MESSAGE_KEY_MessageId);
    int offset = tuple_int(iter, MESSAGE_KEY_Index, -1);
    int transfer_id = tuple_int(iter, MESSAGE_KEY_ImageTransferId, 0);
    Tuple *data = dict_find(iter, MESSAGE_KEY_ImageData);
    int data_len = data ? data->length : 0;
    if (!message_id || strcmp(message_id, s_image_message_id) != 0 ||
        transfer_id != s_image_transfer_id || !data) {
      return;
    }
    Message *message = find_message_by_image_token(message_id);
    if (!s_image_buffer || offset != s_image_expected_offset || data_len <= 0 ||
        !image_write_chunk(data->value->data, data_len)) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Image transfer gap for %s at %d expected %d",
              message_id, offset, s_image_expected_offset);
      if (!retry_active_image_request(message, "Retrying", 0)) {
        if (message) {
          message->image_requested = false;
          message->image_failed = true;
          set_message_image_error(message, "Photo transfer failed");
        }
        clear_active_image_request();
        request_next_image();
      }
      mark_messages_dirty();
      return;
    }
    s_image_received = offset + data_len;
    s_image_expected_offset = s_image_received;
    if (message) {
      set_message_image_progress(message, 25 + ((progress_percent(s_image_written, s_image_size) * 75) / 100));
    }
    schedule_image_transfer_timeout();
    mark_messages_dirty();
    return;
  }

  if (strcmp(type, "image_done") == 0) {
    char *message_id = tuple_cstring(iter, MESSAGE_KEY_MessageId);
    int transfer_id = tuple_int(iter, MESSAGE_KEY_ImageTransferId, 0);
    Message *message = find_message_by_image_token(message_id);
    bool is_active_image = message_id && strcmp(message_id, s_image_message_id) == 0 &&
                           transfer_id == s_image_transfer_id;
    if (!message || !is_active_image) {
      return;
    }
    bool transfer_complete = s_image_buffer && s_image_written == s_image_size;
    bool retrying_image = false;
    IMAGE_DIAG("PGIMG watch image_done msg=%s complete=%d written=%d/%d heap=%u",
               message_id, transfer_complete ? 1 : 0, s_image_written, s_image_size,
               image_diag_heap_free());
    if (transfer_complete) {
      destroy_message_bitmap(message);
      // The received bytes are a complete Pebble bitmap: wrap them in place.
      message->image_bitmap = gbitmap_create_with_data(s_image_buffer);
      if (message->image_bitmap) {
        message->image_data = s_image_buffer;
        s_image_buffer = NULL;
        s_image_buffer_capacity = 0;
        message->image_failed = false;
        message->image_error[0] = '\0';
        message->image_retry_level = 0;
      } else {
        retrying_image = retry_active_image_request(message, "Retrying", (unsigned)(s_image_size * 3) / 4);
        if (!retrying_image) {
          message->image_failed = true;
          set_message_image_error(message, "Photo failed");
        }
      }
    } else {
      retrying_image = retry_active_image_request(message, "Retrying", 0);
      if (!retrying_image) {
        message->image_failed = message_image_near_viewport(message_index_from_ptr(message), IMAGE_KEEP_MARGIN);
        set_message_image_error(message, "Photo transfer failed");
      }
    }
    if (!retrying_image) {
      message->image_requested = false;
      cancel_timer(&s_image_retry_timer);
      reset_image_transfer_state();
    }
    mark_messages_dirty();
    if (!retrying_image) {
      request_next_image();
    }
    return;
  }

  if (strcmp(type, "image_error") == 0) {
    char *message_id = tuple_cstring(iter, MESSAGE_KEY_MessageId);
    char *detail = tuple_cstring(iter, MESSAGE_KEY_Error);
    int transfer_id = tuple_int(iter, MESSAGE_KEY_ImageTransferId, s_image_transfer_id);
    Message *message = find_message_by_image_token(message_id);
    int image_index = message_index_from_ptr(message);
    bool is_active_image = message_id && strcmp(message_id, s_image_message_id) == 0 &&
                           transfer_id == s_image_transfer_id;
    IMAGE_DIAG("PGIMG watch image_error msg=%s active=%d detail=%s",
               message_id ? message_id : "(null)", is_active_image ? 1 : 0,
               detail && detail[0] ? detail : "");
    if (message && is_active_image) {
      message->image_requested = false;
      message->image_failed = message_image_near_viewport(image_index, IMAGE_KEEP_MARGIN);
      if (message->image_failed) {
        set_message_image_error(message, detail && detail[0] ? detail : "Photo failed");
      } else {
        message->image_error[0] = '\0';
        message->image_progress = 0;
      }
    }
    if (is_active_image) {
      cancel_timer(&s_image_retry_timer);
      reset_image_transfer_state();
    }
    mark_messages_dirty();
    request_next_image();
    return;
  }

  if (strcmp(type, "reacted") == 0) {
    show_status("Reacted");
  }

  if (strcmp(type, "sent") == 0) {
    touch_keyboard_send_result(true);
    show_status("Sent");
    return;
  }

  if (strcmp(type, "edited") == 0) {
    show_status("Edited");
    return;
  }

  if (strcmp(type, "deleted") == 0) {
    char *deleted_id = tuple_cstring(iter, MESSAGE_KEY_MessageId);
    int deleted_index = find_message_index_by_id(deleted_id);
    if (deleted_index >= 0) {
      remove_message_at(deleted_index);
    }
    show_status("Deleted");
    return;
  }

  if (strcmp(type, "message_context") == 0) {
    char *incoming_message_id = tuple_cstring(iter, MESSAGE_KEY_MessageId);
    if (!has_selected_message() || !incoming_message_id ||
        strcmp(s_messages[s_selected_message].id, incoming_message_id) != 0) {
      return;
    }
    copy_cstr(s_full_text_title, sizeof(s_full_text_title), tuple_cstring(iter, MESSAGE_KEY_Sender));
    if (s_full_text_body) {
      copy_cstr(s_full_text_body, MAX_FULL_TEXT, tuple_cstring(iter, MESSAGE_KEY_Text));
    }
    if (s_action_mode == ActionMenuFullText && s_full_text_context && s_action_layer) {
      s_full_text_scroll_offset = 0;
      layer_mark_dirty(s_action_layer);
    }
    return;
  }

  if (strcmp(type, "chat_action_done") == 0) {
    char *action = tuple_cstring(iter, MESSAGE_KEY_Text);
    char *chat_id = tuple_cstring(iter, MESSAGE_KEY_ChatId);
    int chat_index = find_chat_index_by_id(chat_id);
    if (action && (strcmp(action, "archiveChat") == 0 || strcmp(action, "deleteChat") == 0)) {
      remove_chat_at(chat_index >= 0 ? chat_index : s_selected_chat);
      show_status(strcmp(action, "archiveChat") == 0 ? "Archived" : "Deleted");
    } else if (action && strcmp(action, "muteChat") == 0) {
      show_status("Muted");
    } else if (action && strcmp(action, "markUnread") == 0) {
      if (chat_index >= 0) {
        s_chats[chat_index].unread = true;
        s_chats[chat_index].unread_count = 0;
        if (s_chat_menu) {
          menu_layer_reload_data(s_chat_menu);
          select_chat_row(s_selected_chat, false);
        }
      }
      show_status("Marked unread");
    } else {
      show_status("Done");
    }
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  show_status("Message dropped");
}

static void outbox_sent_callback(DictionaryIterator *iter, void *context) {
  if (s_leave_pending) {
    s_leave_pending = !send_command_with_status("leave_chat", s_current_chat_id, NULL, NULL, NULL, false);
  }
}

static void outbox_failed_callback(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  char *command = tuple_cstring(iter, MESSAGE_KEY_Command);
  if (command && strcmp(command, "get_image") == 0) {
    char *message_id = tuple_cstring(iter, MESSAGE_KEY_MessageId);
    Message *message = find_message_by_image_token(tuple_cstring(iter, MESSAGE_KEY_MessageId));
    if (message) {
      message->image_requested = true;
    }
    if (message_id && strcmp(message_id, s_image_message_id) == 0) {
      free_image_transfer_buffer();
      s_image_size = 0;
      s_image_received = 0;
      s_image_expected_offset = 0;
      s_image_transfer_id = 0;
    }
    schedule_image_retry();
    mark_messages_dirty();
    return;
  }
  show_status("Send failed");
}

static void close_action_window(void) {
  if (s_native_action_menu) {
    action_menu_close(s_native_action_menu, true);
    return;
  }
  if (s_action_window) {
    window_stack_remove(s_action_window, true);
    s_action_window = NULL;
  }
}

static int action_item_count(void) {
  switch (s_action_mode) {
    case ActionMenuMain:
      if (!has_selected_message()) {
        return 6 + (touch_keyboard_supported() ? 1 : 0) +  // + Archive Chat, Mark as Unread
               (s_bar_draft[0] ? 2 + (touch_keyboard_supported() ? 1 : 0) : 0);  // the draft's
      }
      return (s_message_actions ? 3 : 2) + 2 +  // + Archive Chat, Mark as Unread
             (s_message_actions && s_messages[s_selected_message].outgoing ?
              (touch_keyboard_supported() ? 2 : 1) : 0) +
             (selected_message_has_context() ? 1 : 0) +
             (selected_message_is_truncated() ? 1 : 0);
    case ActionMenuChat:
      return 2;
    case ActionMenuCanned:
      return canned_reply_count();
    case ActionMenuConfirm:
      return 2;
    case ActionMenuReply:
      return 3 + (touch_keyboard_supported() ? 1 : 0);
    case ActionMenuReactionGrid:
      return reaction_grid_count();
    case ActionMenuEmojiReplyGrid:
      return emoji_reply_count();
    case ActionMenuFullText:
      return 0;
  }
  return 0;
}

static void native_action_perform(ActionMenu *action_menu, const ActionMenuItem *action,
                                  void *context);

#define NATIVE_ACTION_INDEX_NONE 0xffff

static void full_text_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, TG_LIST_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  char title[MAX_SENDER + 10];
  const char *text = "";
  const char *heading = NULL;
  const char *meta = NULL;
  bool outgoing = false;
  GFont full_font = bubble_text_font(s_appearance.text_size);
  int pad = ROUND_UI ? 30 : 10;
  int text_w = bounds.size.w - (pad * 2);
  int header_h = ROUND_UI ? 50 : 30;
  // A message (not a quote or forward) also says when it was sent, on a second header line
  char when[40];
  when[0] = '\0';

  title[0] = '\0';
  if (s_selected_message >= 0 && s_selected_message < s_message_count) {
    Message *m = &s_messages[s_selected_message];
    outgoing = m->outgoing;
    meta = m->meta;
    if (s_full_text_context && s_full_text_title[0]) {
      copy_cstr(title, sizeof(title), s_full_text_title);
      text = s_full_text_body ? s_full_text_body : "";
    } else if (s_full_text_context) {
      // "View Full Message": the full text arrives from the phone into the body.
      copy_cstr(title, sizeof(title), m->outgoing ? "You" : (m->sender[0] ? m->sender : (s_current_chat_title[0] ? s_current_chat_title : "Message")));
      text = s_full_text_body ? s_full_text_body : m->text;
    } else {
      copy_cstr(title, sizeof(title), m->outgoing ? "You" : (m->sender[0] ? m->sender : (s_current_chat_title[0] ? s_current_chat_title : "Message")));
      text = m->text;
    }
    heading = title;
    if (!s_full_text_title[0]) {
      char clock[8];
      message_meta_time(meta, clock, sizeof(clock));
      time_t sent = message_time(m);
      if (sent) {
        day_label_text(sent, when, sizeof(when));
      }
      if (clock[0]) {
        size_t used = strlen(when);
        snprintf(when + used, sizeof(when) - used, "%s%s", used ? ", " : "", clock);
      }
    }
  }
  if (when[0]) {
    header_h += 16;
  }

  GSize text_size = graphics_text_layout_get_content_size(
    text, full_font, GRect(0, 0, text_w, 4000), GTextOverflowModeWordWrap, GTextAlignmentLeft);
  s_full_text_height = header_h + text_size.h + 24 + (ROUND_UI ? 30 : 0);
  int max_scroll = PG_MAX(0, s_full_text_height - bounds.size.h + 8);
  s_full_text_scroll_offset = PG_MIN(s_full_text_scroll_offset, max_scroll);
  int top = -s_full_text_scroll_offset;

  graphics_context_set_text_color(ctx, TG_TEXT);
  graphics_draw_text(ctx, text, full_font, GRect(pad, top + header_h + 4, text_w, text_size.h + 8),
                     GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

  // Header scrolls away with the text, like a message card.
  graphics_context_set_fill_color(ctx, TG_HEADER);
  graphics_fill_rect(ctx, GRect(0, top, bounds.size.w, header_h), 0, GCornerNone);
  if (heading && heading[0]) {
    int avatar_r = 10;
    if (!ROUND_UI) {
      draw_avatar(ctx, GPoint(pad + avatar_r, top + header_h / 2), avatar_r, heading, heading, NULL, TG_HEADER);
    }
    int name_y = top + (ROUND_UI ? 22 : 3);
    int name_x = pad + (ROUND_UI ? 0 : avatar_r * 2 + 6);
    graphics_context_set_text_color(ctx, TG_ON_HEADER);
    graphics_draw_text(ctx, heading, font_bold18(),
                       GRect(name_x, name_y, bounds.size.w - name_x - pad, 22),
                       GTextOverflowModeTrailingEllipsis,
                       ROUND_UI ? GTextAlignmentCenter : GTextAlignmentLeft, NULL);
    if (when[0]) {
      graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(gcolor_equal(TG_ON_HEADER, GColorWhite) ? GColorCeleste : GColorDarkGray,
                                                             TG_ON_HEADER));
      graphics_draw_text(ctx, when, font_14(),
                         GRect(name_x, name_y + 20, bounds.size.w - name_x - pad, 16),
                         GTextOverflowModeTrailingEllipsis,
                         ROUND_UI ? GTextAlignmentCenter : GTextAlignmentLeft, NULL);
    }
  }
  (void)outgoing;

  // Slim scroll indicator.
  if (max_scroll > 0 && !ROUND_UI) {
    int track_h = bounds.size.h - 8;
    int thumb_h = PG_MAX(16, (track_h * bounds.size.h) / s_full_text_height);
    int thumb_y = 4 + ((track_h - thumb_h) * s_full_text_scroll_offset) / max_scroll;
    graphics_context_set_fill_color(ctx, TG_SEPARATOR);
    graphics_fill_rect(ctx, GRect(bounds.size.w - 4, thumb_y, 3, thumb_h), 1, GCornersAll);
  }
}

static void *native_action_data(ActionItem item, int index) {
  uintptr_t packed_index = index < 0 ? NATIVE_ACTION_INDEX_NONE : (uint16_t)index;
  uintptr_t packed = (((uintptr_t)item & 0xffff) << 16) | (packed_index & 0xffff);
  return (void *)packed;
}

static ActionItem native_action_data_item(void *data) {
  return (ActionItem)((((uintptr_t)data) >> 16) & 0xffff);
}

static int native_action_data_index(void *data) {
  uint16_t index = (uint16_t)(((uintptr_t)data) & 0xffff);
  return index == NATIVE_ACTION_INDEX_NONE ? -1 : (int)index;
}

static bool native_add_action(ActionMenuLevel *level, const char *label,
                              ActionItem item, int index) {
  return action_menu_level_add_action(level, label, native_action_perform,
                                      native_action_data(item, index)) != NULL;
}

static void native_add_child(ActionMenuLevel *parent, ActionMenuLevel *child,
                             const char *label) {
  if (!child) {
    return;
  }
  if (!action_menu_level_add_child(parent, child, label)) {
    action_menu_hierarchy_destroy(child, NULL, NULL);
  }
}

static void native_defer_action_mode(ActionMenuMode mode) {
  s_native_deferred_mode = mode;
  s_native_has_deferred_mode = true;
}

static ActionMenuLevel *native_create_canned_level(bool as_reply) {
  int count = canned_reply_count();
  ActionMenuLevel *level = action_menu_level_create((uint16_t)PG_MAX(1, count));
  if (!level) {
    return NULL;
  }
  if (count <= 0) {
    native_add_action(level, "No canned messages", ActionItemConfirmCancel, -1);
    return level;
  }
  for (int i = 0; i < count; i++) {
    native_add_action(level, s_canned[i][0] ? s_canned[i] : "Canned message",
                      as_reply ? ActionItemReplyCanned : ActionItemCanned, i);
  }
  return level;
}

static ActionMenuLevel *native_create_emoji_level(bool as_reply) {
  int count = emoji_reply_count();
  ActionMenuLevel *level = action_menu_level_create((uint16_t)PG_MAX(1, count));
  if (!level) {
    return NULL;
  }
  action_menu_level_set_display_mode(level, ActionMenuLevelDisplayModeThin);
  for (int i = 0; i < count; i++) {
    native_add_action(level, emoji_reply_glyph_at(i),
                      as_reply ? ActionItemReplyEmoji : ActionItemEmoji, i);
  }
  return level;
}

static ActionMenuLevel *native_create_reaction_level(void) {
  int count = reaction_grid_count();
  ActionMenuLevel *level = action_menu_level_create((uint16_t)PG_MAX(1, count));
  if (!level) {
    return NULL;
  }
  action_menu_level_set_display_mode(level, ActionMenuLevelDisplayModeThin);
  for (int i = 0; i < count; i++) {
    native_add_action(level, reaction_grid_glyph_at(i), ActionItemReact, i);
  }
  return level;
}

static ActionMenuLevel *native_create_reply_level(void) {
  ActionMenuLevel *level = action_menu_level_create(touch_keyboard_supported() ? 4 : 3);
  if (!level) {
    return NULL;
  }
  native_add_action(level, "Dictate Reply", ActionItemReplyDictate, -1);
  native_add_child(level, native_create_canned_level(true), "Canned Message");
  if (touch_keyboard_supported()) {
    native_add_action(level, "Emoji", ActionItemReplyEmojiGrid, -1);
  } else {
    native_add_child(level, native_create_emoji_level(true), "Emoji");
  }
  if (touch_keyboard_supported()) {
    native_add_action(level, "Type Reply", ActionItemReplyKeyboard, -1);
  }
  return level;
}

static ActionMenuLevel *native_build_main_level(void) {
  ActionMenuLevel *level = action_menu_level_create((uint16_t)PG_MAX(1, action_item_count()));
  if (!level) {
    return NULL;
  }

  if (!has_selected_message() && s_bar_draft[0]) {
    native_add_action(level, "Send", ActionItemSendDraft, -1);
    native_add_action(level, "Retry Voice", ActionItemRetryDraft, -1);
    if (touch_keyboard_supported()) {
      native_add_action(level, "Edit", ActionItemEditDraft, -1);
    }
  }
  if (!has_selected_message()) {
    native_add_action(level, "Voice", ActionItemCompose, -1);
    native_add_child(level, native_create_canned_level(false), "Canned Message");
    if (touch_keyboard_supported()) {
      native_add_action(level, "Emoji", ActionItemEmojiGrid, -1);
    } else {
      native_add_child(level, native_create_emoji_level(false), "Emoji");
    }
    if (touch_keyboard_supported()) {
      native_add_action(level, "Keyboard", ActionItemKeyboard, -1);
    }
    native_add_action(level, "Go to Bottom", ActionItemGoToBottom, -1);
    if (network_has_feature(s_current_chat_network, "archive")) {
      native_add_action(level, "Archive Chat", ActionItemArchiveChat, -1);
    }
    if (network_has_feature(s_current_chat_network, "mark_unread")) {
      native_add_action(level, "Mark as Unread", ActionItemMarkUnread, -1);
    }
    return level;
  }

  native_add_child(level, native_create_reply_level(), "Reply");
  if (s_message_actions && touch_keyboard_supported()) {
    native_add_action(level, "React", ActionItemReactGrid, -1);
  } else if (s_message_actions) {
    native_add_child(level, native_create_reaction_level(), "React");
  }
  if (selected_message_has_context()) {
    native_add_action(level, selected_message_context_is_forward() ? "View Forward" : "View Quote",
                      ActionItemFullContext, -1);
  }
  if (s_message_actions && s_messages[s_selected_message].outgoing) {
    if (touch_keyboard_supported()) {
      native_add_action(level, "Edit by Keyboard", ActionItemEditKeyboard, -1);
      native_add_action(level, "Edit by Voice", ActionItemEdit, -1);
    } else {
      native_add_action(level, "Edit Message", ActionItemEdit, -1);
    }
  }
  if (selected_message_is_truncated()) {
    native_add_action(level, "View Full Message", ActionItemFullText, -1);
  }
  native_add_action(level, "Go to Bottom", ActionItemGoToBottom, -1);
  if (network_has_feature(s_current_chat_network, "archive")) {
    native_add_action(level, "Archive Chat", ActionItemArchiveChat, -1);
  }
  if (network_has_feature(s_current_chat_network, "mark_unread")) {
    native_add_action(level, "Mark as Unread", ActionItemMarkUnread, -1);
  }
  return level;
}

// Send Message: a favourite, whose chat opens ready to write (an empty one if it has no messages).
static ActionMenuLevel *native_create_favorites_level(void) {
  ActionMenuLevel *level = action_menu_level_create((uint16_t)PG_MAX(1, s_favorite_count));
  if (!level) {
    return NULL;
  }
  if (s_favorite_count == 0) {
    native_add_action(level, "Add favorites in the Pebble app", ActionItemConfirmCancel, -1);
    return level;
  }
  for (int i = 0; i < s_favorite_count; i++) {
    native_add_action(level, s_favorites[i].name[0] ? s_favorites[i].name : "Favorite",
                      ActionItemFavorite, i);
  }
  return level;
}

static ActionMenuLevel *native_build_chat_level(void) {
  ActionMenuLevel *level = action_menu_level_create(5);
  if (!level) {
    return NULL;
  }
  if (s_list_menu) {
    native_add_child(level, native_create_favorites_level(), "Send Message");
    native_add_action(level, "Search", ActionItemSearch, -1);
    if (s_search_query[0]) {
      native_add_action(level, "Show Inbox", ActionItemShowInbox, -1);
    }
    return level;
  }
  int index = action_chat_index();
  uint8_t network = index >= 0 ? s_chats[index].network : CHAT_NETWORK_BEEPER;
  if (network_has_feature(network, "archive")) {
    native_add_action(level, "Archive Chat", ActionItemArchiveChat, -1);
  }
  if (network_has_feature(network, "mark_unread")) {
    native_add_action(level, index >= 0 && chat_is_unread(&s_chats[index]) ? "Mark as Read" : "Mark as Unread",
                      ActionItemMarkUnread, -1);
  }
  if (network == CHAT_NETWORK_SMS || !network_has_feature(network, "archive")) {
    native_add_action(level, "Open Chat", ActionItemOpenChat, -1);  // never an empty menu
  }
  return level;
}

static ActionMenuLevel *native_build_confirm_level(void) {
  ActionMenuLevel *level = action_menu_level_create(2);
  if (!level) {
    return NULL;
  }
  native_add_action(level, s_pending_chat_command[0] ? "Delete" : "Send",
                    ActionItemConfirmSend, -1);
  native_add_action(level, "Cancel", ActionItemConfirmCancel, -1);
  return level;
}

static ActionMenuLevel *native_build_action_level(ActionMenuMode mode) {
  switch (mode) {
    case ActionMenuMain:
      return native_build_main_level();
    case ActionMenuChat:
      return native_build_chat_level();
    case ActionMenuCanned:
      return native_create_canned_level(s_pending_send_as_reply);
    case ActionMenuConfirm:
      return native_build_confirm_level();
    case ActionMenuReply:
      return native_create_reply_level();
    case ActionMenuReactionGrid:
      return native_create_reaction_level();
    case ActionMenuEmojiReplyGrid:
      return native_create_emoji_level(s_pending_send_as_reply);
    case ActionMenuFullText:
      return NULL;
  }
  return NULL;
}

// When the selected message was sent, across the top of its menu: bubbles carry no time.
static TextLayer *s_action_time_layer;
static char s_action_time_text[40];

static void action_menu_show_time(ActionMenu *menu) {
  if (!has_selected_message()) {
    return;
  }
  const Message *m = &s_messages[s_selected_message];
  time_t t = message_time(m);
  if (!t) {
    return;
  }
  char day[20];
  char clock[8];
  day_label_text(t, day, sizeof(day));
  message_meta_time(m->meta, clock, sizeof(clock));
  snprintf(s_action_time_text, sizeof(s_action_time_text), "%s, %s", day, clock);
  // The menu is a window of its own; the line sits above its centred items
  Layer *root = window_get_root_layer((Window *)menu);
  GRect b = layer_get_bounds(root);
  s_action_time_layer = text_layer_create(GRect(0, ROUND_UI ? 30 : 2, b.size.w, 18));
  if (!s_action_time_layer) {
    return;
  }
  text_layer_set_text(s_action_time_layer, s_action_time_text);
  text_layer_set_font(s_action_time_layer, font_bold14());
  text_layer_set_text_alignment(s_action_time_layer, GTextAlignmentCenter);
  text_layer_set_text_color(s_action_time_layer, GColorWhite);
  text_layer_set_background_color(s_action_time_layer, GColorClear);
  layer_add_child(root, text_layer_get_layer(s_action_time_layer));
}

static void native_action_menu_did_close(ActionMenu *menu, const ActionMenuItem *performed_action,
                                         void *context) {
  ActionMenuLevel *root = s_native_action_root;
  int keyboard_mode = s_native_deferred_keyboard;
  s_native_deferred_keyboard = -1;
#if TOUCH_KEYBOARD_AVAILABLE
  int grid_mode = s_native_deferred_grid;
#endif
  s_native_deferred_grid = -1;
  bool start_dictation_after_close = s_native_deferred_dictation;
  bool show_next = s_native_has_deferred_mode;
  ActionMenuMode next_mode = s_native_deferred_mode;

  if (s_action_time_layer) {
    text_layer_destroy(s_action_time_layer);
    s_action_time_layer = NULL;
  }
  s_native_action_menu = NULL;
  s_native_action_root = NULL;
  s_native_deferred_dictation = false;
  s_native_has_deferred_mode = false;

  if (root) {
    action_menu_hierarchy_destroy(root, NULL, NULL);
  }

#if TOUCH_KEYBOARD_AVAILABLE
  if (grid_mode >= 0) {
    open_emoji_grid((EmojiGridMode)grid_mode, s_native_deferred_grid_reply);
    return;
  }
#endif
  if (s_native_deferred_edit_draft) {
    s_native_deferred_edit_draft = false;
#if TOUCH_KEYBOARD_AVAILABLE
    if (s_kb_draft) {
      s_kb_draft[0] = '\0';  // the keyboard starts from the bar's draft
    }
#endif
    open_touch_keyboard(KeyboardModeCompose, NULL, s_bar_draft);
    s_bar_draft[0] = '\0';
    return;
  }
  if (keyboard_mode >= 0) {
    const char *target = s_native_deferred_keyboard_target[0] ? s_native_deferred_keyboard_target : NULL;
    int index = target ? find_message_index_by_id(target) : -1;
    const char *initial = (keyboard_mode == KeyboardModeEdit && index >= 0) ? s_messages[index].text : NULL;
    open_touch_keyboard((KeyboardMode)keyboard_mode, target, initial);
  } else if (start_dictation_after_close) {
    start_dictation();
  } else if (show_next) {
    show_action_window(next_mode);
  }
}

static bool show_native_action_menu(ActionMenuMode mode) {
  close_touch_keyboard();
  if (s_view_state == ViewStateChat) {
    show_status(s_current_chat_title);
  } else {
    show_status("Chat");
  }

  if (s_native_action_menu) {
    action_menu_close(s_native_action_menu, false);
    return false;
  }

  s_action_mode = mode;
  s_action_selected = 0;
  s_native_action_root = native_build_action_level(mode);
  if (!s_native_action_root) {
    return false;
  }

  ActionMenuConfig config = {
    .root_level = s_native_action_root,
    .context = NULL,
    .colors = {
      .background = APP_COLOR,
      .foreground = GColorWhite
    },
    .will_close = NULL,
    .did_close = native_action_menu_did_close,
    .align = ActionMenuAlignCenter
  };
  s_native_action_menu = action_menu_open(&config);
  if (s_native_action_menu) {
    action_menu_show_time(s_native_action_menu);
  }
  if (!s_native_action_menu) {
    action_menu_hierarchy_destroy(s_native_action_root, NULL, NULL);
    s_native_action_root = NULL;
    return false;
  }
  return true;
}

// Once the menu has closed: open a favourite's chat (index >= 0), or chat -1 - index of the list.
// A favourite missing from the list gets a row of its own, so its chat opens empty.
static void open_favorite(void *data) {
  int index = (int)(intptr_t)data;
  if (!s_chat_menu || s_view_state != ViewStateChatList) {
    return;
  }
  int row;
  if (index < 0) {
    row = -1 - index;
  } else {
    row = find_chat_index_by_id(s_favorites[index].id);
    if (row < 0) {
      // A full list gives up its oldest row; the next sync puts it back.
      row = s_chat_count < MAX_CHATS ? s_chat_count++ : MAX_CHATS - 1;
      Chat *chat = &s_chats[row];
      destroy_chat_avatar(chat);
      copy_cstr(chat->id, sizeof(chat->id), s_favorites[index].id);
      set_chat_strings(chat, s_favorites[index].name, "");
      chat->network = s_favorites[index].network;
      chat->unread = false;
      chat->unread_count = 0;
      chat->flags = 0;
      chat->time[0] = '\0';
      chat->receipt = 0;
      menu_layer_reload_data(s_chat_menu);
    }
  }
  if (row < 0 || row >= s_chat_count) {
    return;
  }
  MenuIndex cell = {.section = 0, .row = row};  // opens in place: the list keeps its scroll
  chat_menu_select_callback(s_chat_menu, &cell, NULL);
}

// "View Full Message" for the selected message: from its menu (opens once the menu has closed),
// or straight from a double tap on it.
static void open_full_text(bool from_menu) {
  s_full_text_context = true;
  if (!has_selected_message() || !ensure_full_text_body()) {
    show_status("Memory low");
    return;
  }
  s_full_text_title[0] = '\0';
  copy_cstr(s_full_text_body, MAX_FULL_TEXT, s_messages[s_selected_message].text);
  send_command_with_status("get_message_text", s_current_chat_id, NULL, NULL,
                           s_messages[s_selected_message].id, false);
  s_full_text_scroll_offset = 0;
  if (from_menu) {
    native_defer_action_mode(ActionMenuFullText);
  } else {
    show_action_window(ActionMenuFullText);
  }
}

static void native_action_perform(ActionMenu *action_menu, const ActionMenuItem *action,
                                  void *context) {
  void *data = action_menu_item_get_action_data(action);
  ActionItem item = native_action_data_item(data);
  int index = native_action_data_index(data);

  switch (item) {
    case ActionItemReactGrid:
      s_native_deferred_grid = EmojiGridReact;
      s_native_deferred_grid_reply = false;
      break;
    case ActionItemEmojiGrid:
    case ActionItemReplyEmojiGrid:
      s_native_deferred_grid = EmojiGridReply;
      s_native_deferred_grid_reply = item == ActionItemReplyEmojiGrid;
      break;
    case ActionItemKeyboard:
      s_native_deferred_keyboard = KeyboardModeCompose;
      s_native_deferred_keyboard_target[0] = '\0';
      break;
    case ActionItemReplyKeyboard:
      if (has_selected_message()) {
        s_native_deferred_keyboard = KeyboardModeReply;
        copy_cstr(s_native_deferred_keyboard_target, sizeof(s_native_deferred_keyboard_target),
                  s_messages[s_selected_message].id);
      }
      break;
    case ActionItemEditKeyboard:
      if (has_selected_message() && s_messages[s_selected_message].outgoing) {
        s_native_deferred_keyboard = KeyboardModeEdit;
        copy_cstr(s_native_deferred_keyboard_target, sizeof(s_native_deferred_keyboard_target),
                  s_messages[s_selected_message].id);
      }
      break;
    case ActionItemSendDraft:
      send_bar_draft();
      break;
    case ActionItemRetryDraft:
      s_pending_edit_message_id[0] = '\0';
      s_pending_chat_command[0] = '\0';
      s_search_dictation = false;
      s_dictation_to_draft = true;
      copy_cstr(s_dictation_draft_chat, sizeof(s_dictation_draft_chat), s_current_chat_id);
      s_native_deferred_dictation = true;
      break;
    case ActionItemEditDraft:
      s_native_deferred_edit_draft = true;
      break;
    case ActionItemCompose:
      s_pending_edit_message_id[0] = '\0';
      s_pending_chat_command[0] = '\0';
      s_pending_send_as_reply = false;
      s_search_dictation = false;
      s_native_deferred_dictation = true;
      break;
    case ActionItemCanned:
    case ActionItemReplyCanned:
      if (index >= 0 && index < canned_reply_count()) {
        s_pending_edit_message_id[0] = '\0';
        s_pending_chat_command[0] = '\0';
        s_pending_send_as_reply = item == ActionItemReplyCanned;
        copy_cstr(s_pending_text, MAX_TEXT, s_canned[index]);
        native_defer_action_mode(ActionMenuConfirm);
      }
      break;
    case ActionItemReply:
      native_defer_action_mode(ActionMenuReply);
      break;
    case ActionItemReplyDictate:
      s_pending_edit_message_id[0] = '\0';
      s_pending_chat_command[0] = '\0';
      s_pending_send_as_reply = true;
      s_search_dictation = false;
      s_native_deferred_dictation = true;
      break;
    case ActionItemReplyEmoji:
      if (index >= 0 && index < emoji_reply_count()) {
        send_text_message(emoji_reply_glyph_at(index), true);
        s_pending_send_as_reply = false;
      }
      break;
    case ActionItemEmoji:
      if (index >= 0 && index < emoji_reply_count()) {
        send_text_message(emoji_reply_glyph_at(index), false);
        s_pending_send_as_reply = false;
      }
      break;
    case ActionItemReact:
      if (index >= 0 && index < reaction_grid_count()) {
        send_selected_reaction(reaction_grid_token_at(index));
      }
      break;
    case ActionItemEdit:
      if (has_selected_message() && s_messages[s_selected_message].outgoing) {
        copy_cstr(s_pending_edit_message_id, sizeof(s_pending_edit_message_id),
                  s_messages[s_selected_message].id);
        s_pending_chat_command[0] = '\0';
        s_pending_send_as_reply = false;
        s_search_dictation = false;
      s_native_deferred_dictation = true;
      }
      break;
    case ActionItemFullText:
      open_full_text(true);
      break;
    case ActionItemFullContext:
      s_full_text_context = true;
      if (!has_selected_message() || !ensure_full_text_body()) {
        show_status("Memory low");
        break;
      }
      char body[MAX_CONTEXT_TEXT];
      message_context_strings(&s_messages[s_selected_message], s_full_text_title,
                              sizeof(s_full_text_title), body, sizeof(body));
      copy_cstr(s_full_text_body, MAX_FULL_TEXT, body);
      send_command_with_status("get_context", s_current_chat_id, NULL, NULL,
                               s_messages[s_selected_message].id, false);
      s_full_text_scroll_offset = 0;
      native_defer_action_mode(ActionMenuFullText);
      break;
    case ActionItemGoToBottom:
      go_to_bottom();
      break;
    case ActionItemArchiveChat: {
      int index = action_chat_index();
      if (index >= 0 && phone_ready()) {
        send_selected_chat_action("archive_chat");
        copy_cstr(s_chat_to_remove, sizeof(s_chat_to_remove), s_chats[index].id);
        app_timer_register(0, leave_chat_after_action, NULL);
      }
      break;
    }
    case ActionItemDeleteChat:
      copy_cstr(s_pending_chat_command, sizeof(s_pending_chat_command), "delete_chat");
      if (s_selected_chat >= 0 && s_selected_chat < s_chat_count) {
        snprintf(s_pending_text, MAX_TEXT, "Delete %s?",
                 s_chats[s_selected_chat].title);
      } else {
        copy_cstr(s_pending_text, MAX_TEXT, "Delete chat?");
      }
      native_defer_action_mode(ActionMenuConfirm);
      break;
    case ActionItemMuteChat:
      send_selected_chat_action("mute_chat");
      break;
    case ActionItemMarkUnread: {
      // One item, both ways: an unread chat is marked read, a read one unread
      int index = action_chat_index();
      if (index < 0 || !phone_ready()) {
        break;
      }
      bool was_unread = s_view_state != ViewStateChat && chat_is_unread(&s_chats[index]);
      s_chats[index].unread = !was_unread;
      s_chats[index].unread_count = 0;
      send_selected_chat_action(was_unread ? "mark_read" : "mark_unread");
      if (s_view_state == ViewStateChat) {
        app_timer_register(0, leave_chat_after_action, NULL);  // staying would read it again
      } else if (s_chat_menu) {
        menu_layer_reload_data(s_chat_menu);
        select_chat_row(index, false);
      }
      break;
    }
    case ActionItemConfirmSend:
      if (s_pending_chat_command[0]) {
        send_selected_chat_action(s_pending_chat_command);
        s_pending_chat_command[0] = '\0';
      } else if (s_pending_edit_message_id[0]) {
        edit_selected_message(s_pending_text);
      } else {
        send_text_message(s_pending_text, s_pending_send_as_reply);
      }
      s_pending_send_as_reply = false;
      break;
    case ActionItemConfirmCancel:
      s_pending_edit_message_id[0] = '\0';
      s_pending_chat_command[0] = '\0';
      s_pending_send_as_reply = false;
      break;
    case ActionItemSearch:
      if (phone_ready()) {
        s_search_dictation = true;
        s_native_deferred_dictation = true;
      }
      break;
    case ActionItemShowInbox:
      end_search();
      break;
    case ActionItemFavorite:
      if (index >= 0 && index < s_favorite_count) {
        app_timer_register(0, open_favorite, (void *)(intptr_t)index);
      }
      break;
    case ActionItemOpenChat: {
      int chat = action_chat_index();
      if (chat >= 0) {
        app_timer_register(0, open_favorite, (void *)(intptr_t)(-1 - chat));
      }
      break;
    }
    case ActionItemGoBack:
      break;
  }
}

static void show_action_window(ActionMenuMode mode);

// Back from search results to the inbox.
static void end_search(void) {
  s_search_query[0] = '\0';
  send_command("search_chats", NULL, "", NULL, NULL);
  show_status("Chat");
}

static void show_list_menu(void) {
  s_list_menu = true;
  show_action_window(ActionMenuChat);
  s_list_menu = false;
}

static void show_action_window(ActionMenuMode mode) {
  release_images_for_heap();
  if (mode != ActionMenuFullText) {
    if (!show_native_action_menu(mode)) {
      show_status("Menu unavailable");
    }
    return;
  }

  close_touch_keyboard();
  if (s_view_state == ViewStateChat) {
    show_status(s_current_chat_title);
  } else {
    show_status("Chat");
  }
  s_action_mode = mode;
  s_action_selected = 0;
  s_action_window = window_create();
#if TOUCH_KEYBOARD_AVAILABLE
  if (touch_keyboard_supported()) {
    window_set_touch_bridge_disabled(s_action_window, true);
  }
#endif
  window_set_background_color(s_action_window, GColorWhite);
  window_set_click_config_provider(s_action_window, action_click_config_provider);
  window_set_window_handlers(s_action_window, (WindowHandlers) {
    .unload = action_window_unload
  });

  Layer *window_layer = window_get_root_layer(s_action_window);
  GRect bounds = layer_get_bounds(window_layer);
  s_action_layer = layer_create(bounds);
  layer_set_update_proc(s_action_layer, full_text_layer_update_proc);
  layer_add_child(window_layer, s_action_layer);

  window_stack_push(s_action_window, true);
}

static void dictation_callback(DictationSession *session, DictationSessionStatus status,
                               char *transcription, void *context) {
  if (s_search_dictation) {
    s_search_dictation = false;
    if (status == DictationSessionStatusSuccess && transcription && transcription[0]) {
      copy_cstr(s_search_query, sizeof(s_search_query), transcription);
      // Dictation ends a phrase with a full stop; a name has none
      for (int n = strlen(s_search_query); n > 0 && strchr(".!? ", s_search_query[n - 1]); n--) {
        s_search_query[n - 1] = '\0';
      }
      send_command("search_chats", NULL, s_search_query, NULL, NULL);
      show_status("Searching...");
    }
    return;
  }
  if (s_dictation_to_draft) {
    s_dictation_to_draft = false;
    // Only into the chat it was dictated for: another chat may have opened meanwhile
    if (status == DictationSessionStatusSuccess && transcription && transcription[0] &&
        s_view_state == ViewStateChat && strcmp(s_dictation_draft_chat, s_current_chat_id) == 0) {
      chat_ui_set_draft(transcription);  // Retry: a new draft, sent with the arrow
    }
    return;
  }
  if (status == DictationSessionStatusSuccess && transcription) {
    // Select on the recorder is the send; Back while recording is the cancel
    copy_cstr(s_pending_text, MAX_TEXT, transcription);
    if (s_pending_edit_message_id[0]) {
      edit_selected_message(s_pending_text);
    } else {
      send_text_message(s_pending_text, s_pending_send_as_reply);
    }
    s_pending_send_as_reply = false;
  } else {
    show_status("Dictation failed");
  }
}

static void start_dictation(void) {
  if (!s_dictation_session) {
    s_dictation_session = dictation_session_create(MAX_TEXT - 1, dictation_callback, NULL);
  }
  if (!s_dictation_session ||
      dictation_session_start(s_dictation_session) != DictationSessionStatusSuccess) {
    s_search_dictation = false;  // no callback comes: the next dictation is a message again
    s_dictation_to_draft = false;
    show_status("Dictation failed");
  }
}

// Archive, read state and search are done by the phone; without it, change nothing here
static bool phone_ready(void) {
  if (chat_ds_phone_connected()) {
    return true;
  }
  show_status("Phone not connected");
  return false;
}

static void action_window_unload(Window *window) {
  if (s_action_layer) {
    layer_destroy(s_action_layer);
    s_action_layer = NULL;
  }
  free_full_text_body();
  window_destroy(window);
  if (s_action_window == window) {
    s_action_window = NULL;
  }
}

static void action_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_action_mode == ActionMenuFullText) {
    s_full_text_scroll_offset = PG_MAX(0, s_full_text_scroll_offset - CHAT_SCROLL_DELTA);
    layer_mark_dirty(s_action_layer);
    return;
  }
  if (s_action_selected > 0) {
    s_action_selected--;
    layer_mark_dirty(s_action_layer);
  }
}

static void action_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_action_mode == ActionMenuFullText) {
    if (s_action_layer) {
      GRect bounds = layer_get_bounds(s_action_layer);
      int max_scroll = PG_MAX(0, s_full_text_height - bounds.size.h + 8);
      s_full_text_scroll_offset = PG_MIN(max_scroll, s_full_text_scroll_offset + CHAT_SCROLL_DELTA);
      layer_mark_dirty(s_action_layer);
    }
    return;
  }
  if (s_action_selected < action_item_count() - 1) {
    s_action_selected++;
    layer_mark_dirty(s_action_layer);
  }
}

static void action_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_action_mode == ActionMenuFullText) {
    free_full_text_body();
    close_action_window();
    return;
  }
  if (s_action_mode == ActionMenuCanned || s_action_mode == ActionMenuConfirm ||
      s_action_mode == ActionMenuReply || s_action_mode == ActionMenuReactionGrid ||
      s_action_mode == ActionMenuEmojiReplyGrid || s_action_mode == ActionMenuFullText) {
    if (s_action_mode == ActionMenuFullText) {
      free_full_text_body();
    }
    s_action_mode = ActionMenuMain;
    s_action_selected = 0;
    layer_mark_dirty(s_action_layer);
  } else {
    close_action_window();
  }
}

static void action_click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, REPEAT_SCROLL_MS, action_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, REPEAT_SCROLL_MS, action_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, action_back_click_handler);
}

static void main_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_view_state == ViewStateChat) {
    show_action_window(ActionMenuMain);
  } else if (s_view_state == ViewStateChatList && s_chat_menu) {
    if (s_loading_messages) {
      show_status("Loading messages...");
      return;
    }
    if (fav_focused()) {
      open_favorite((void *)(intptr_t)s_fav_focus);  // Back returns to this bubble
      return;
    }
    MenuIndex index = menu_layer_get_selected_index(s_chat_menu);
    chat_menu_select_callback(s_chat_menu, &index, NULL);
  }
}

static void main_select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (BEEPER_CHAT_ACTIONS && s_view_state == ViewStateChatList && !s_chats_loading &&
      !s_loading_messages && s_chat_count > 0 && s_chat_menu && !fav_focused()) {
    MenuIndex index = menu_layer_get_selected_index(s_chat_menu);
    s_selected_chat = index.row;
    show_action_window(ActionMenuChat);
  }
}

// One button press in a chat. Every press moves the view by at most half a
// screen, so tall messages and photos scroll through smoothly and a press never
// jumps past the part you haven't seen. While the selected message still has
// more above/below the screen, the press scrolls within it; otherwise it
// selects the next message and brings it into view. Returns false at the
// start/end of the loaded messages.
static bool chat_step(int direction, bool repeating) {
  compose_float_track(direction * COMPOSE_FLOAT_TRIGGER);  // one press shows or hides the bar
  GRect bounds = layer_get_bounds(s_messages_root);
  int view_h = visible_message_height(bounds);
  int step = view_h / 2;
  int margin = 6;
  int offset = s_chat_scroll_offset;
  int sel = s_selected_message;
  bool on_compose = sel < 0 || sel >= s_message_count;

  if (!on_compose) {
    // Where the view would sit to show this message's top / bottom edge,
    // limited to how far the chat can actually scroll.
    int top = clamp_scroll_offset(s_message_y[sel] - margin);
    int bottom = clamp_scroll_offset(s_message_y[sel] + s_message_h[sel] + margin - view_h);
    if (direction > 0 && bottom > offset) {
      set_chat_scroll_offset(offset + PG_MIN(step, bottom - offset), !repeating);
      return true;
    }
    if (direction < 0 && top < offset) {
      set_chat_scroll_offset(offset - PG_MIN(step, offset - top), !repeating);
      return true;
    }
  }

  int next = on_compose ? (direction < 0 ? s_message_count - 1 : -1) : sel + direction;
  if (next < 0 || next >= s_message_count) {
    return false;
  }
  s_selected_message = next;
  prepare_selected_image_request();
  int top = s_message_y[next] - margin;
  int bottom = s_message_y[next] + s_message_h[next] + margin;
  bool tall = bottom - top > view_h;
  int target = offset;
  if (direction > 0) {
    // Show the new message from its top; a short one fits completely.
    int wanted = tall ? top : bottom - view_h;
    if (wanted > offset) {
      target = offset + PG_MIN(step, wanted - offset);
    }
  } else {
    // Moving up, a tall message shows its end first, then scrolls up.
    int wanted = tall ? bottom - view_h : top;
    if (wanted < offset) {
      target = offset - PG_MIN(step, offset - wanted);
    }
  }
  set_chat_scroll_offset(target, !repeating);
  return true;
}

static void main_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  bool repeating = click_is_repeating(recognizer);
  if (s_view_state == ViewStateChatList && s_chat_menu) {
    if (s_loading_messages) {
      show_status("Loading messages...");
      return;
    }
#if TOUCH_KEYBOARD_AVAILABLE
    touch_chat_list_follow();  // a throw may have left the highlight off screen
#endif
    // Up from the first chat walks back through the favourites; Up on the first one: the list's menu
    if (fav_focused()) {
      if (s_fav_focus > 0) {
        set_fav_focus(s_fav_focus - 1);
      } else if (!repeating) {
        show_list_menu();
      }
      return;
    }
    if (menu_layer_get_selected_index(s_chat_menu).row == 0 && !repeating && fav_strip_h() > 0) {
      s_top_presses = 0;
      set_fav_focus(s_favorite_count - 1);
      return;
    }
    // Up on the first chat, twice: the list's menu, with Search
    if (menu_layer_get_selected_index(s_chat_menu).row == 0 && !repeating) {
      if (++s_top_presses >= 2) {
        s_top_presses = 0;
        show_list_menu();
      }
      return;
    }
    s_top_presses = 0;
    menu_layer_set_selected_next(s_chat_menu, true, MenuRowAlignCenter, !repeating);
    s_selected_chat = menu_layer_get_selected_index(s_chat_menu).row;
    if (s_selected_chat >= 0 && s_selected_chat < s_chat_count) {
      copy_cstr(s_chat_list_selected_id, sizeof(s_chat_list_selected_id), s_chats[s_selected_chat].id);
    }
    return;
  }
  if (s_view_state != ViewStateChat || !s_messages_root || s_message_count == 0) {
    return;
  }
  s_user_scrolled_messages = true;
  bool reversed_direction = s_message_scroll_direction == 1;
  s_message_scroll_direction = -1;
  finish_pending_chat_scroll();
  recalc_message_layout();
  if (repeating) {
    clear_active_image_request();
  }

  if (!chat_step(-1, repeating)) {
    if (s_loading_older_messages) {
      show_status("Loading older...");
    } else {
      request_older_messages(false);
    }
    return;
  }
  if (!reversed_direction) {
    maybe_prefetch_older_messages();
  }
}

static void main_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  bool repeating = click_is_repeating(recognizer);
  if (s_view_state == ViewStateChatList && s_chat_menu) {
    if (s_loading_messages) {
      show_status("Loading messages...");
      return;
    }
#if TOUCH_KEYBOARD_AVAILABLE
    touch_chat_list_follow();
#endif
    if (fav_focused()) {
      set_fav_focus(s_fav_focus + 1 < s_favorite_count ? s_fav_focus + 1 : -1);
      return;
    }
    menu_layer_set_selected_next(s_chat_menu, false, MenuRowAlignCenter, !repeating);
    s_selected_chat = menu_layer_get_selected_index(s_chat_menu).row;
    if (s_selected_chat >= 0 && s_selected_chat < s_chat_count) {
      copy_cstr(s_chat_list_selected_id, sizeof(s_chat_list_selected_id), s_chats[s_selected_chat].id);
    }
    return;
  }
  if (s_view_state != ViewStateChat || !s_messages_root || s_message_count == 0) {
    return;
  }
  s_user_scrolled_messages = true;
  bool reversed_direction = s_message_scroll_direction == -1;
  s_message_scroll_direction = 1;
  finish_pending_chat_scroll();
  recalc_message_layout();
  if (repeating) {
    clear_active_image_request();
  }

  if (compose_target_is_selected()) {
    scroll_to_bottom(!repeating);
    return;
  }
  if (!chat_step(1, repeating)) {
    if (s_loading_newer_messages) {
      show_status("Loading newer...");
    } else if (s_at_newest) {
      scroll_to_bottom(!repeating);
    } else {
      request_newer_messages(false);
    }
    return;
  }
  if (!reversed_direction) {
    maybe_prefetch_newer_messages();
  }
}

static void main_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_touch_keyboard_open) {
    close_touch_keyboard();
    show_status(s_current_chat_title);
    return;
  }
  if (s_view_state == ViewStateChatList && s_search_query[0]) {
    end_search();  // Back leaves the results first, the app next
    return;
  }
  if (s_view_state == ViewStateChat) {
    cancel_message_timeout();
    cancel_message_retry();
    s_loading_messages = false;
    s_loading_older_messages = false;
    s_loading_newer_messages = false;
    s_message_transfer_id = 0;
    s_chat_view_pending = false;
    clear_message_stage();
    s_bar_draft[0] = '\0';
    reset_touch_keyboard();  // saves the draft first
    s_leave_pending = !send_command_with_status("leave_chat", s_current_chat_id, NULL, NULL, NULL, false);
    render_chat_list_with_transition();
  } else {
    window_stack_pop(true);
  }
}

#if TOUCH_KEYBOARD_AVAILABLE
// ---------------------------------------------------------------------------
// Emoji grid (touch watches). Replaces the PebbleOS grid menu for React and
// Emoji because the system grid only lets a tap pick the first emoji in a row.
// Looks like the system menu (black, blue rail, 3 columns) but every cell is
// tappable. Buttons: Up/Down move, Select picks, Back closes.
// Touch: tap picks, drag scrolls, swipe right closes.
// ---------------------------------------------------------------------------

#define GRID_COLS 3
#define GRID_CELL_H PBL_IF_ROUND_ELSE(50, 46)
#define GRID_RAIL_W PBL_IF_ROUND_ELSE(0, 14)
#define GRID_PAD_TOP PBL_IF_ROUND_ELSE(40, 6)

static Window *s_grid_window;
static Layer *s_grid_layer;
static EmojiGridMode s_grid_mode;
static bool s_grid_as_reply;
static int s_grid_selected;
static int s_grid_scroll;
static int s_grid_pressed = -1;

static int emoji_grid_count(void) {
  return s_grid_mode == EmojiGridReact ? reaction_grid_count() : emoji_reply_count();
}

static const char *emoji_grid_label(int index) {
  return s_grid_mode == EmojiGridReact ? reaction_grid_glyph_at(index) : emoji_reply_glyph_at(index);
}

// "Remove" (last reaction) gets a full-width row of its own.
static bool emoji_grid_is_wide(int index) {
  return s_grid_mode == EmojiGridReact && reaction_grid_is_remove(index);
}

static int emoji_grid_row_of(int index) {
  if (emoji_grid_is_wide(index)) {
    return (index + GRID_COLS - 1) / GRID_COLS;
  }
  return index / GRID_COLS;
}

static int emoji_grid_row_count(void) {
  int count = emoji_grid_count();
  return count > 0 ? emoji_grid_row_of(count - 1) + 1 : 0;
}

static GRect emoji_grid_cell_rect(GRect bounds, int index) {
  int left = GRID_RAIL_W + 2;
  int width = bounds.size.w - left - 2;
#if defined(PBL_ROUND)
  left = 28;
  width = bounds.size.w - 56;
#endif
  int row = emoji_grid_row_of(index);
  int y = GRID_PAD_TOP + row * GRID_CELL_H - s_grid_scroll;
  if (emoji_grid_is_wide(index)) {
    return GRect(left, y, width, GRID_CELL_H);
  }
  int col = index % GRID_COLS;
  int x0 = left + (width * col) / GRID_COLS;
  int x1 = left + (width * (col + 1)) / GRID_COLS;
  return GRect(x0, y, x1 - x0, GRID_CELL_H);
}

static int emoji_grid_max_scroll(GRect bounds) {
  int content = GRID_PAD_TOP * 2 + emoji_grid_row_count() * GRID_CELL_H;
  return PG_MAX(0, content - bounds.size.h);
}

static void emoji_grid_clamp_scroll(GRect bounds) {
  s_grid_scroll = PG_MAX(0, PG_MIN(s_grid_scroll, emoji_grid_max_scroll(bounds)));
}

static void emoji_grid_reveal_selected(void) {
  if (!s_grid_layer) {
    return;
  }
  GRect bounds = layer_get_bounds(s_grid_layer);
  int top = GRID_PAD_TOP + emoji_grid_row_of(s_grid_selected) * GRID_CELL_H;
  int margin = PBL_IF_ROUND_ELSE(bounds.size.h / 3, 4);
  if (top - s_grid_scroll < margin) {
    s_grid_scroll = top - margin;
  } else if (top + GRID_CELL_H - s_grid_scroll > bounds.size.h - margin) {
    s_grid_scroll = top + GRID_CELL_H - bounds.size.h + margin;
  }
  emoji_grid_clamp_scroll(bounds);
}

static int emoji_grid_index_at(GPoint point) {
  if (!s_grid_layer) {
    return -1;
  }
  GRect bounds = layer_get_bounds(s_grid_layer);
  int count = emoji_grid_count();
  for (int i = 0; i < count; i++) {
    GRect cell = emoji_grid_cell_rect(bounds, i);
    if (grect_contains_point(&cell, &point)) {
      return i;
    }
  }
  return -1;
}

static void emoji_grid_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, ACTION_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  if (GRID_RAIL_W > 0) {
    graphics_context_set_fill_color(ctx, APP_COLOR);
    graphics_fill_rect(ctx, GRect(0, 0, GRID_RAIL_W, bounds.size.h), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, GPoint(GRID_RAIL_W / 2, 10), 2);
  }

  int count = emoji_grid_count();
  GFont emoji_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont text_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  for (int i = 0; i < count; i++) {
    GRect cell = emoji_grid_cell_rect(bounds, i);
    if (cell.origin.y + cell.size.h < 0 || cell.origin.y > bounds.size.h) {
      continue;
    }
    bool selected = i == s_grid_selected;
    bool pressed = i == s_grid_pressed;
    GRect face = grect_inset(cell, GEdgeInsets(3));
    if (pressed) {
      graphics_context_set_fill_color(ctx, APP_COLOR);
      graphics_fill_rect(ctx, face, 8, GCornersAll);
    } else if (selected) {
      graphics_context_set_fill_color(ctx, GColorDarkGray);
      graphics_fill_rect(ctx, face, 8, GCornersAll);
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_round_rect(ctx, face, 8);
    }
    bool wide = emoji_grid_is_wide(i);
    graphics_context_set_text_color(ctx, (selected || pressed || wide) ? GColorWhite : ACTION_TEXT);
    graphics_draw_text(ctx, emoji_grid_label(i), wide ? text_font : emoji_font,
                       GRect(cell.origin.x, cell.origin.y + (wide ? 8 : 4), cell.size.w, cell.size.h),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

static void emoji_grid_close(void) {
  if (s_grid_window) {
    window_stack_remove(s_grid_window, true);
  }
}

static void emoji_grid_commit(int index) {
  if (index < 0 || index >= emoji_grid_count()) {
    return;
  }
  EmojiGridMode mode = s_grid_mode;
  bool as_reply = s_grid_as_reply;
  char glyph[16];
  copy_cstr(glyph, sizeof(glyph), mode == EmojiGridReact ? reaction_grid_token_at(index) :
                                                           emoji_reply_glyph_at(index));
  emoji_grid_close();
  if (mode == EmojiGridReact) {
    send_selected_reaction(glyph);
  } else {
    send_text_message(glyph, as_reply);
  }
}

static void emoji_grid_move(int delta) {
  int count = emoji_grid_count();
  if (count <= 0) {
    return;
  }
  s_grid_selected = (s_grid_selected + delta + count) % count;
  emoji_grid_reveal_selected();
  layer_mark_dirty(s_grid_layer);
}

static void emoji_grid_up_handler(ClickRecognizerRef recognizer, void *context) {
  emoji_grid_move(-1);
}

static void emoji_grid_down_handler(ClickRecognizerRef recognizer, void *context) {
  emoji_grid_move(1);
}

static void emoji_grid_select_handler(ClickRecognizerRef recognizer, void *context) {
  emoji_grid_commit(s_grid_selected);
}

static void emoji_grid_click_config(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, REPEAT_SCROLL_MS, emoji_grid_up_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, REPEAT_SCROLL_MS, emoji_grid_down_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, emoji_grid_select_handler);
}

static void emoji_grid_window_unload(Window *window) {
  if (s_grid_layer) {
    layer_destroy(s_grid_layer);
    s_grid_layer = NULL;
  }
  window_destroy(window);
  if (s_grid_window == window) {
    s_grid_window = NULL;
  }
  s_grid_pressed = -1;
}

static void open_emoji_grid(EmojiGridMode mode, bool as_reply) {
  if (s_grid_window) {
    return;
  }
  release_images_for_heap();
  s_grid_mode = mode;
  s_grid_as_reply = as_reply;
  s_grid_selected = 0;
  s_grid_scroll = 0;
  s_grid_pressed = -1;
  s_grid_window = window_create();
  if (!s_grid_window) {
    return;
  }
  window_set_background_color(s_grid_window, ACTION_BG);
  window_set_click_config_provider(s_grid_window, emoji_grid_click_config);
  window_set_window_handlers(s_grid_window, (WindowHandlers) {
    .unload = emoji_grid_window_unload
  });
  window_set_touch_bridge_disabled(s_grid_window, true);
  Layer *root = window_get_root_layer(s_grid_window);
  s_grid_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_grid_layer, emoji_grid_update_proc);
  layer_add_child(root, s_grid_layer);
  window_stack_push(s_grid_window, true);
}

static bool emoji_grid_on_top(void) {
  return s_grid_window && window_stack_get_top_window() == s_grid_window;
}

// Touch hooks called from the gesture dispatcher.
static void emoji_grid_touch_down(GPoint screen) {
  s_grid_pressed = emoji_grid_index_at(screen);
  if (s_grid_layer) {
    layer_mark_dirty(s_grid_layer);
  }
}

static void emoji_grid_touch_drag(int dy) {
  if (!s_grid_layer) {
    return;
  }
  s_grid_pressed = -1;
  s_grid_scroll -= dy;
  emoji_grid_clamp_scroll(layer_get_bounds(s_grid_layer));
  layer_mark_dirty(s_grid_layer);
}

static void emoji_grid_touch_tap(GPoint screen) {
  int index = emoji_grid_index_at(screen);
  s_grid_pressed = -1;
  if (index >= 0) {
    s_grid_selected = index;
    emoji_grid_commit(index);
  } else if (s_grid_layer) {
    layer_mark_dirty(s_grid_layer);
  }
}

static void emoji_grid_touch_cancel(void) {
  if (s_grid_pressed >= 0 && s_grid_layer) {
    s_grid_pressed = -1;
    layer_mark_dirty(s_grid_layer);
  }
}
#endif

#if TOUCH_KEYBOARD_AVAILABLE
// ---------------------------------------------------------------------------
// Touch gestures for the whole app (Pebble Time 2 / Round 2).
//   Chat list:  tap a chat to open it, hold for chat actions, drag to scroll.
//   Chat:       drag to scroll, tap a message for its menu, hold a message to
//               react, tap "New message" to type, swipe right to go back.
//   Full text:  drag to scroll, swipe right to close.
// Buttons keep working exactly as before.
// ---------------------------------------------------------------------------

#define TOUCH_SLOP 8
#define TOUCH_LONG_PRESS_MS 500
#define TOUCH_SWIPE_BACK_MIN 50

typedef struct {
  bool down;
  bool dragging;
  bool horizontal;
  bool long_fired;
  bool keyboard;
  GPoint start;
  GPoint last;
} TouchGesture;

static TouchGesture s_gesture;
static AppTimer *s_long_press_timer;

static void touch_cancel_long_press(void) {
  cancel_timer(&s_long_press_timer);
}

static int touch_abs(int value) {
  return value < 0 ? -value : value;
}

static GPoint touch_point_in_layer(const TouchEvent *event, Layer *layer) {
  GRect frame = layer_get_frame(layer);
  return GPoint(event->x - frame.origin.x, event->y - frame.origin.y);
}

// ---- Chat list ------------------------------------------------------------

static int touch_chat_row_at(GPoint point) {
  if (!s_chat_menu || s_chat_count == 0) {
    return s_chat_menu ? 0 : -1;
  }
  ScrollLayer *scroll = menu_layer_get_scroll_layer(s_chat_menu);
  GPoint offset = scroll_layer_get_content_offset(scroll);
  int content_y = point.y - offset.y - fav_strip_h();
  if (content_y < 0) {
    return -1;
  }
  int row = content_y / CHAT_ROW_H;
  return row < s_chat_count ? row : -1;
}

static void touch_chat_list_select(int row, bool animated) {
  s_fav_focus = -1;
  s_selected_chat = row;
  menu_layer_set_selected_index(s_chat_menu, MenuIndex(0, row), MenuRowAlignNone, animated);
  if (row >= 0 && row < s_chat_count) {
    copy_cstr(s_chat_list_selected_id, sizeof(s_chat_list_selected_id), s_chats[row].id);
  }
}

static void touch_chat_list_tap(GPoint point) {
  if (s_loading_messages) {
    show_status("Loading messages...");
    return;
  }
  int row = touch_chat_row_at(point);
  if (row < 0) {
    // A favourite's bubble or name
    GPoint offset = scroll_layer_get_content_offset(menu_layer_get_scroll_layer(s_chat_menu));
    int width = layer_get_bounds(menu_layer_get_layer(s_chat_menu)).size.w;
    for (int i = 0; s_chat_menu && i < s_favorite_count && fav_strip_h() > 0; i++) {
      GPoint c = fav_center(i, width);
      int dy = point.y - offset.y - c.y;
      if (touch_abs(point.x - c.x) <= CHAT_AVATAR_R + 8 && dy >= -CHAT_AVATAR_R - 4 && dy <= CHAT_AVATAR_R + 18) {
        s_fav_focus = -1;
        open_favorite((void *)(intptr_t)i);
        return;
      }
    }
    return;
  }
  if (s_chat_count > 0) {
    touch_chat_list_select(row, false);
  }
  MenuIndex index = MenuIndex(0, row);
  chat_menu_select_callback(s_chat_menu, &index, NULL);
}

static void touch_chat_list_hold(GPoint point) {
  int row = touch_chat_row_at(point);
  if (row < 0 || s_chat_count == 0 || s_chats_loading || s_loading_messages) {
    return;
  }
  touch_chat_list_select(row, true);
  if (BEEPER_CHAT_ACTIONS) {
    show_action_window(ActionMenuChat);
  }
}

// Keep the highlight on screen: when the selected chat would leave it, move it to the nearest
// chat that is fully visible, without moving the list out from under the finger.
static void touch_chat_list_follow(void) {
  if (s_chat_count == 0) {
    return;
  }
  ScrollLayer *scroll = menu_layer_get_scroll_layer(s_chat_menu);
  GRect frame = layer_get_frame(menu_layer_get_layer(s_chat_menu));
  GPoint offset = scroll_layer_get_content_offset(scroll);
  int top = -offset.y - fav_strip_h();
  int first_full = PG_MAX(0, top + CHAT_ROW_H - 1) / CHAT_ROW_H;
  int last_full = (top + frame.size.h) / CHAT_ROW_H - 1;
  first_full = PG_MAX(0, PG_MIN(s_chat_count - 1, first_full));
  last_full = PG_MAX(first_full, PG_MIN(s_chat_count - 1, last_full));
  int row = menu_layer_get_selected_index(s_chat_menu).row;
  if (row < first_full || row > last_full) {
    touch_chat_list_select(row < first_full ? first_full : last_full, false);
#ifdef CONFIG_TOUCH
    scroll_layer_touch_set_content_offset_overscrolled(scroll, offset.y);  // keeps a rubber band
#else
    scroll_layer_set_content_offset(scroll, offset, false);
#endif
  }
}

static int touch_lift_velocity(void);

#ifdef CONFIG_TOUCH
// Dragging scrolls the list with the finger, pixel for pixel, and pulls a little past either end
// with a rubber band that springs back on lift-off; a throw coasts, like the system's menus.
static int16_t s_list_drag_base;  // the list's offset when the finger came down

static void touch_chat_list_down(void) {
  if (s_chat_menu) {
    ScrollLayer *scroll = menu_layer_get_scroll_layer(s_chat_menu);
    scroll_layer_touch_handle_touchdown(scroll);
    s_list_drag_base = scroll_layer_get_content_offset(scroll).y;
  }
}

static void touch_chat_list_drag(int dy_total) {
  if (s_chat_count == 0) {
    return;
  }
  scroll_layer_touch_handle_pan_update(menu_layer_get_scroll_layer(s_chat_menu),
                                       GPoint(0, s_list_drag_base), GPoint(0, dy_total));
  touch_chat_list_follow();
}

static void touch_chat_list_release(int dy_total) {
  if (s_chat_count == 0) {
    return;
  }
  scroll_layer_touch_handle_snap(menu_layer_get_scroll_layer(s_chat_menu), GPoint(0, s_list_drag_base),
                                 GPoint(0, dy_total), GPoint(0, touch_lift_velocity()));
}
#else
static int s_list_drag_base;

static void touch_chat_list_down(void) {
  if (s_chat_menu) {
    s_list_drag_base = scroll_layer_get_content_offset(menu_layer_get_scroll_layer(s_chat_menu)).y;
  }
}

static void touch_chat_list_drag(int dy_total) {
  if (s_chat_count == 0) {
    return;
  }
  ScrollLayer *scroll = menu_layer_get_scroll_layer(s_chat_menu);
  GRect frame = layer_get_frame(menu_layer_get_layer(s_chat_menu));
  int min_y = PG_MIN(0, frame.size.h - s_chat_count * CHAT_ROW_H);
  int y = s_list_drag_base + dy_total;
  scroll_layer_set_content_offset(scroll, GPoint(0, PG_MAX(min_y, PG_MIN(0, y))), false);
  touch_chat_list_follow();
}

static void touch_chat_list_release(int dy_total) {
}
#endif

// ---- Chat -----------------------------------------------------------------

static int touch_message_at(GPoint point) {
  int content_y = point.y + s_chat_scroll_offset;
  for (int i = 0; i < s_message_count; i++) {
    if (content_y >= s_message_y[i] && content_y < s_message_y[i] + s_message_h[i]) {
      return i;
    }
  }
  return -1;
}

static bool touch_on_compose(GRect bounds, GPoint point) {
  if ((!s_at_newest && !s_compose_floating) || s_touch_keyboard_open) {
    return false;
  }
  GRect compose = compose_rect_for_bounds(bounds);
  return grect_contains_point(&compose, &point);
}

static void tap_menu_timer_callback(void *data) {
  s_tap_menu_timer = NULL;
  int index = find_message_index_by_id(s_tap_menu_id);
  s_tap_menu_id[0] = '\0';
  if (index >= 0 && s_view_state == ViewStateChat && s_selected_message == index &&
      !s_touch_keyboard_open && window_stack_get_top_window() == s_main_window) {
    show_action_window(ActionMenuMain);
  }
}

static void touch_chat_tap(GRect bounds, GPoint point) {
  if (touch_on_compose(bounds, point)) {
    cancel_tap_menu();
    GRect mic = compose_mic_rect(compose_rect_for_bounds(bounds));
    if (s_bar_draft[0]) {
      if (grect_contains_point(&mic, &point)) {
        send_bar_draft();  // the arrow: one tap sends
      } else {
        s_selected_message = s_message_count;
        show_action_window(ActionMenuMain);  // the draft: Send, Retry voice, Edit
      }
      return;
    }
    if (grect_contains_point(&mic, &point)) {
      // Mic button: dictate a new message.
      s_pending_edit_message_id[0] = '\0';
      s_pending_chat_command[0] = '\0';
      s_pending_send_as_reply = false;
      start_dictation();
      return;
    }
    if (touch_keyboard_supported()) {
      open_touch_keyboard(KeyboardModeCompose, NULL, NULL);
    } else {
      s_selected_message = s_message_count;
      show_action_window(ActionMenuMain);
    }
    return;
  }
  int index = touch_message_at(point);
  if (index < 0) {
    return;
  }
  if (s_touch_keyboard_open) {
    close_touch_keyboard();  // Tapping the chat puts the keyboard away (draft is kept).
  }
  // A cut-short message ("...") waits a moment for a second tap: double tap = View Full Message
  if (s_tap_menu_timer && strcmp(s_messages[index].id, s_tap_menu_id) == 0 &&
      s_selected_message == index) {
    cancel_tap_menu();
    open_full_text(false);
    return;
  }
  cancel_tap_menu();
  s_user_scrolled_messages = true;
  s_selected_message = index;
  prepare_selected_image_request();
  mark_messages_dirty();
  if (selected_message_is_truncated()) {
    copy_cstr(s_tap_menu_id, sizeof(s_tap_menu_id), s_messages[index].id);
    s_tap_menu_timer = app_timer_register(TOUCH_DOUBLE_TAP_MS, tap_menu_timer_callback, NULL);
    return;
  }
  show_action_window(ActionMenuMain);
}

static void touch_chat_hold(GRect bounds, GPoint point) {
  int index = touch_message_at(point);
  cancel_tap_menu();
  if (index < 0 || touch_on_compose(bounds, point)) {
    return;
  }
  s_user_scrolled_messages = true;
  s_selected_message = index;
  mark_messages_dirty();
  vibes_short_pulse();
  open_emoji_grid(EmojiGridReact, false);  // Hold a message to react.
}

static void touch_chat_drag(int dy) {
  cancel_tap_menu();
  finish_pending_chat_scroll();
  int before = s_chat_scroll_offset;
  set_chat_scroll_offset_quiet(s_chat_scroll_offset - dy);
  compose_float_track(s_chat_scroll_offset - before);
  s_user_scrolled_messages = true;
  // Within half a screen of the top: fetch older messages now, without waiting for the finger to
  // lift, anchored on the top message on screen so the drag carries on into them
  if (dy > 0 && !s_at_oldest && !s_loading_older_messages && s_message_count > 0 &&
      s_chat_scroll_offset < visible_message_height(layer_get_bounds(s_messages_root)) / 2) {
    int top = 0;
    while (top < s_message_count - 1 && s_message_y[top] + s_message_h[top] <= s_chat_scroll_offset) {
      top++;
    }
    s_selected_message = top;
    s_touch_paging = true;
    request_older_messages(false);
  }
  mark_messages_dirty();
}

static void touch_chat_drag_end(GRect bounds);

// ---- Momentum: a chat let go while moving keeps going and slows down ----
#define FLING_FRAME_MS 30
#define FLING_MIN_V16 (4 * 16)  // slower than 4px a frame at lift-off: no fling
#define FLING_MAX_V16 (108 * 16)  // 3600px/s, the system scroll layer's limit
#define FLING_FRICTION_NUM 57   // speed kept each frame, of 64: the system fling's 240ms decay
static int s_fling_v16;      // px per frame x16, the finger's direction (+ = content moves down)
static uint32_t s_drag_ms;   // when the last drag sample came

static uint32_t touch_now_ms(void) {
  time_t seconds;
  uint16_t ms;
  time_ms(&seconds, &ms);
  return (uint32_t)seconds * 1000 + ms;
}

// The finger's recent path. The throw's speed is measured across FLING_WINDOW_MS of it, not from
// the last sample: a real finger slows or repeats a position just as it lifts.
#define FLING_SAMPLES 8
#define FLING_WINDOW_MS 120
static struct { uint32_t ms; int16_t y; } s_fling_path[FLING_SAMPLES];
static int s_fling_path_n;

static void fling_track(int y) {
  if (s_fling_path_n == FLING_SAMPLES) {
    memmove(s_fling_path, s_fling_path + 1, sizeof(s_fling_path[0]) * --s_fling_path_n);
  }
  s_drag_ms = touch_now_ms();
  s_fling_path[s_fling_path_n].ms = s_drag_ms;
  s_fling_path[s_fling_path_n++].y = y;
}

static void fling_stop(bool settle) {
  cancel_timer(&s_fling_timer);
  s_fling_v16 = 0;
  if (settle && s_view_state == ViewStateChat && s_messages_root) {
    touch_chat_drag_end(layer_get_bounds(s_messages_root));
  }
}

static void fling_timer_callback(void *data) {
  s_fling_timer = NULL;
  if (s_view_state != ViewStateChat || !s_messages_root) {
    s_fling_v16 = 0;
    return;
  }
  int before = s_chat_scroll_offset;
  touch_chat_drag(s_fling_v16 / 16);  // the same path as a finger: older messages load on the way up
  s_fling_v16 = s_fling_v16 * FLING_FRICTION_NUM / 64;
  if (s_chat_scroll_offset == before || touch_abs(s_fling_v16) < 16) {
    fling_stop(true);  // at an end of the chat, or slowed to a stop
    return;
  }
  s_fling_timer = app_timer_register(FLING_FRAME_MS, fling_timer_callback, NULL);
}

// The finger's speed at lift-off in px/s (+ = down), 0 if it paused first: placed, not thrown.
static int touch_lift_velocity(void) {
  const uint32_t now = touch_now_ms();
  if (s_fling_path_n < 2 || now - s_drag_ms > FLING_WINDOW_MS) {
    return 0;
  }
  int from = s_fling_path_n - 2;
  while (from > 0 && now - s_fling_path[from - 1].ms <= FLING_WINDOW_MS) {
    from--;
  }
  const int dt = PG_MAX(10, (int)(now - s_fling_path[from].ms));
  return (s_fling_path[s_fling_path_n - 1].y - s_fling_path[from].y) * 1000 / dt;
}

// The finger left a dragged chat: throw it if it was moving, else settle where it is.
static void fling_start(void) {
  s_fling_v16 = PG_MAX(-FLING_MAX_V16,
                       PG_MIN(FLING_MAX_V16, touch_lift_velocity() * 16 * FLING_FRAME_MS / 1000));
  if (touch_abs(s_fling_v16) < FLING_MIN_V16) {
    fling_stop(true);
    return;
  }
  cancel_timer(&s_fling_timer);
  s_fling_timer = app_timer_register(FLING_FRAME_MS, fling_timer_callback, NULL);
}

// After a drag, point the buttons at what's on screen and load what's needed.
static void touch_chat_drag_end(GRect bounds) {
  int visible_h = visible_message_height(bounds);
  int max_offset = PG_MAX(0, s_chat_content_height - visible_h);
  int center = s_chat_scroll_offset + visible_h / 2;
  int best = -1;
  int best_distance = 0x7fff;
  for (int i = 0; i < s_message_count; i++) {
    int mid = s_message_y[i] + s_message_h[i] / 2;
    int distance = touch_abs(mid - center);
    if (distance < best_distance) {
      best_distance = distance;
      best = i;
    }
  }
  if (s_at_newest && s_chat_scroll_offset >= max_offset - 2) {
    s_selected_message = s_message_count;  // At the bottom: "New message" is selected.
  } else if (best >= 0) {
    s_selected_message = best;
  }
  prepare_selected_image_request();
  request_next_image();
  if (s_chat_scroll_offset <= 0 && !s_at_oldest && !s_loading_older_messages) {
    request_older_messages(false);
  } else if (!s_at_newest && s_chat_scroll_offset >= max_offset - 2 && !s_loading_newer_messages) {
    request_newer_messages(false);
  }
  mark_messages_dirty();
}

// ---- Full text view -------------------------------------------------------

static void touch_full_text_drag(int dy) {
  if (!s_action_layer) {
    return;
  }
  GRect bounds = layer_get_bounds(s_action_layer);
  int max_scroll = PG_MAX(0, s_full_text_height - bounds.size.h + 8);
  s_full_text_scroll_offset = PG_MAX(0, PG_MIN(max_scroll, s_full_text_scroll_offset - dy));
  layer_mark_dirty(s_action_layer);
}

// ---- Dispatcher -----------------------------------------------------------

static void touch_grid_event(const TouchEvent *event, GPoint screen) {
  switch (event->type) {
    case TouchEvent_Touchdown:
      touch_cancel_long_press();
      s_gesture = (TouchGesture){.down = true, .start = screen, .last = screen};
      emoji_grid_touch_down(screen);
      return;
    case TouchEvent_PositionUpdate: {
      if (!s_gesture.down) {
        return;
      }
      int dx = screen.x - s_gesture.start.x;
      int dy_total = screen.y - s_gesture.start.y;
      if (!s_gesture.dragging &&
          (touch_abs(dx) > TOUCH_SLOP || touch_abs(dy_total) > TOUCH_SLOP)) {
        s_gesture.dragging = true;
        s_gesture.horizontal = touch_abs(dx) > touch_abs(dy_total);
        emoji_grid_touch_cancel();
      }
      if (s_gesture.dragging && !s_gesture.horizontal) {
        emoji_grid_touch_drag(screen.y - s_gesture.last.y);
      }
      s_gesture.last = screen;
      return;
    }
    case TouchEvent_Liftoff: {
      TouchGesture gesture = s_gesture;
      s_gesture.down = false;
      if (!gesture.down) {
        return;
      }
      int dx = screen.x - gesture.start.x;
      int dy = screen.y - gesture.start.y;
      if (gesture.dragging && gesture.horizontal && dx > TOUCH_SWIPE_BACK_MIN &&
          touch_abs(dy) < dx / 2) {
        emoji_grid_close();
      } else if (!gesture.dragging) {
        emoji_grid_touch_tap(screen);
      } else {
        emoji_grid_touch_cancel();
      }
      return;
    }
  }
}

static bool touch_full_text_on_top(void) {
  return s_action_window && window_stack_get_top_window() == s_action_window &&
         s_action_mode == ActionMenuFullText;
}

static void touch_long_press_callback(void *context) {
  s_long_press_timer = NULL;
  if (!s_gesture.down || s_gesture.dragging || s_gesture.keyboard) {
    return;
  }
  if (window_stack_get_top_window() != s_main_window) {
    return;
  }
  s_gesture.long_fired = true;
  if (s_view_state == ViewStateChatList && s_chat_menu) {
    touch_chat_list_hold(touch_point_in_layer(&(TouchEvent){.x = s_gesture.start.x,
                                                              .y = s_gesture.start.y},
                                              menu_layer_get_layer(s_chat_menu)));
  } else if (s_view_state == ViewStateChat && s_messages_root) {
    GRect frame = layer_get_frame(s_messages_root);
    GPoint point = GPoint(s_gesture.start.x - frame.origin.x, s_gesture.start.y - frame.origin.y);
    touch_chat_hold(layer_get_bounds(s_messages_root), point);
  }
}

static void touch_handler(const TouchEvent *event, void *context) {
  if (!event || !touch_keyboard_supported()) {
    return;
  }
  // A touch that only woke the screen shouldn't do anything (PebbleOS 4.32+).
  if (event->type == TouchEvent_Touchdown && event->non_navigational &&
      kb_firmware_at_least(4, 32, 0)) {
    s_gesture.down = false;
    return;
  }

  Window *top = window_stack_get_top_window();
  if (emoji_grid_on_top()) {
    touch_grid_event(event, GPoint(event->x, event->y));
    return;
  }
  bool full_text = touch_full_text_on_top();
  if (top != s_main_window && !full_text) {
    // System menus and dictation handle their own input.
    touch_cancel_long_press();
    kb_reset_press();
    s_gesture.down = false;
    return;
  }

  GPoint screen = GPoint(event->x, event->y);

  // The keyboard gets first go at touches inside it.
  if (!full_text && s_view_state == ViewStateChat && s_messages_root) {
    GRect bounds = layer_get_bounds(s_messages_root);
    GPoint point = touch_point_in_layer(event, s_messages_root);
    if ((event->type == TouchEvent_Touchdown || s_gesture.keyboard) &&
        kb_handle_touch(event, bounds, point)) {
      s_gesture.keyboard = event->type != TouchEvent_Liftoff;
      s_gesture.down = false;
      return;
    }
  }

  switch (event->type) {
    case TouchEvent_Touchdown:
      touch_cancel_long_press();
      fling_stop(false);  // a touch catches a moving chat
      s_fling_path_n = 0;
      fling_track(screen.y);
      if (!full_text && s_view_state == ViewStateChatList) {
        touch_chat_list_down();
      }
      s_gesture = (TouchGesture){.down = true, .start = screen, .last = screen};
      s_long_press_timer = app_timer_register(TOUCH_LONG_PRESS_MS, touch_long_press_callback, NULL);
      return;

    case TouchEvent_PositionUpdate: {
      if (!s_gesture.down || s_gesture.long_fired) {
        return;
      }
      int dx = screen.x - s_gesture.start.x;
      int dy_total = screen.y - s_gesture.start.y;
      if (!s_gesture.dragging &&
          (touch_abs(dx) > TOUCH_SLOP || touch_abs(dy_total) > TOUCH_SLOP)) {
        s_gesture.dragging = true;
        s_gesture.horizontal = touch_abs(dx) > touch_abs(dy_total);
        touch_cancel_long_press();
      }
      if (s_gesture.dragging && !s_gesture.horizontal) {
        int dy = screen.y - s_gesture.last.y;
        if (full_text) {
          touch_full_text_drag(dy);
        } else if (s_view_state == ViewStateChat && s_messages_root) {
          fling_track(screen.y);
          touch_chat_drag(dy);
        } else if (s_view_state == ViewStateChatList && s_chat_menu) {
          fling_track(screen.y);
          touch_chat_list_drag(dy_total);
        }
      }
      s_gesture.last = screen;
      return;
    }

    case TouchEvent_Liftoff: {
      touch_cancel_long_press();
      if (!s_gesture.down) {
        return;
      }
      TouchGesture gesture = s_gesture;
      s_gesture.down = false;
      if (gesture.long_fired) {
        return;
      }
      int dx = screen.x - gesture.start.x;
      int dy = screen.y - gesture.start.y;

      // Swipe right: go back.
      if (gesture.dragging && gesture.horizontal && dx > TOUCH_SWIPE_BACK_MIN &&
          touch_abs(dy) < dx / 2) {
        if (full_text) {
          action_back_click_handler(NULL, NULL);
        } else if (s_view_state == ViewStateChat) {
          main_back_click_handler(NULL, NULL);
        }
        return;
      }

      if (gesture.dragging) {
        if (!full_text && s_view_state == ViewStateChat && s_messages_root) {
          fling_start();
        } else if (!full_text && s_view_state == ViewStateChatList && s_chat_menu) {
          touch_chat_list_release(dy);  // springs back from past an end, or coasts
        }
        return;
      }

      // Tap.
      if (full_text) {
        return;
      }
      if (!ROUND_UI && s_view_state == ViewStateChat && gesture.start.y < HEADER_H &&
          gesture.start.x < HEADER_H + 24) {
        main_back_click_handler(NULL, NULL);  // Header back arrow.
        return;
      }
      if (s_view_state == ViewStateChatList && gesture.start.y < HEADER_H) {
        show_list_menu();  // Tap the "Chat" title.
        return;
      }
      if (s_view_state == ViewStateChatList && s_chat_menu) {
        touch_chat_list_tap(touch_point_in_layer(event, menu_layer_get_layer(s_chat_menu)));
      } else if (s_view_state == ViewStateChat && s_messages_root) {
        touch_chat_tap(layer_get_bounds(s_messages_root), touch_point_in_layer(event, s_messages_root));
      }
      return;
    }
  }
}
#endif

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, main_select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, main_select_long_click_handler, NULL);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, REPEAT_SCROLL_MS, main_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, REPEAT_SCROLL_MS, main_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, main_back_click_handler);
}

// While Chat is on screen the phone keeps its sync with Beeper going, so new messages show up
// without leaving and reopening. It stops by itself 45 s after the last of these.
#define APP_OPEN_BEAT_MS 20000
static AppTimer *s_app_open_timer;

static void app_open_beat(void *data) {
  send_command_with_status("app_open", NULL, NULL, NULL, NULL, false);
  s_app_open_timer = app_timer_register(APP_OPEN_BEAT_MS, app_open_beat, NULL);
}

static void main_window_load(Window *window) {
  window_set_background_color(window, TG_LIST_BG);
  window_set_click_config_provider(window, click_config_provider);
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

#ifdef PBL_COLOR
  wallpaper_rebuild();
#endif
  s_header_layer = layer_create(GRect(0, 0, bounds.size.w, HEADER_H));
  layer_set_update_proc(s_header_layer, header_update_proc);
  layer_add_child(window_layer, s_header_layer);
  copy_cstr(s_status_text, sizeof(s_status_text), "Chat");

  int content_y = chat_content_y();
  s_chat_menu = menu_layer_create(GRect(0, content_y, bounds.size.w, bounds.size.h - content_y));
  if (ROUND_UI) {
    menu_layer_set_center_focused(s_chat_menu, false);
  }
  menu_layer_set_callbacks(s_chat_menu, NULL, (MenuLayerCallbacks) {
    .get_num_sections = chat_menu_get_num_sections_callback,
    .get_num_rows = chat_menu_get_num_rows_callback,
    .get_header_height = chat_menu_get_header_height_callback,
    .draw_header = chat_menu_draw_header_callback,
    .draw_row = chat_menu_draw_row_callback,
    .get_cell_height = chat_menu_get_cell_height_callback,
    .select_click = chat_menu_select_callback
  });
  if (chat_ds_has_launch_hint()) {
    // Opened for a chat: the list stays hidden until the chat shows, so it doesn't flash first
    s_open_chat_instantly = true;
    layer_set_hidden(menu_layer_get_layer(s_chat_menu), true);
  }
  menu_layer_set_normal_colors(s_chat_menu, TG_LIST_BG, TG_TEXT);
  layer_add_child(window_layer, menu_layer_get_layer(s_chat_menu));
  s_app_open_timer = app_timer_register(1000, app_open_beat, NULL);
}

static void main_window_unload(Window *window) {
  cancel_timer(&s_app_open_timer);
  light_enable(false);
  cancel_status_clear();
  destroy_chat_view();
  destroy_chat_avatars();
  destroy_message_images();
  if (s_chat_menu) {
    menu_layer_destroy(s_chat_menu);
    s_chat_menu = NULL;
  }
  if (s_header_layer) {
    layer_destroy(s_header_layer);
    s_header_layer = NULL;
  }
#ifdef PBL_COLOR
  if (s_wallpaper) {
    gbitmap_destroy(s_wallpaper);
    s_wallpaper = NULL;
  }
#endif
}

static void main_window_appear(Window *window) {
  light_enable(false);
}

static void init(void) {
  s_chats = calloc(MAX_CHATS, sizeof(Chat));
  s_messages = calloc(MAX_MESSAGES, sizeof(Message));
  s_pending_text = calloc(1, MAX_TEXT);
  s_canned = calloc(MAX_CANNED, CANNED_TEXT_LEN);
  update_canned_replies("Yes|No|On my way|Call you later|Thanks");
#if TOUCH_KEYBOARD_AVAILABLE
  s_kb_draft = calloc(1, TOUCH_KEYBOARD_MAX_TEXT);
#endif
  appearance_load();
  dark_mode_service_subscribe(dark_mode_changed);  // "Auto" switches while Chat is open
  s_view_state = ViewStateChatList;
  s_selected_message = -1;
  s_chats_loading = true;
  light_enable(false);

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_open(APP_INBOX_SIZE, APP_OUTBOX_SIZE);
  s_main_window = window_create();
  window_set_click_config_provider(s_main_window, click_config_provider);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .appear = main_window_appear,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
#if TOUCH_KEYBOARD_AVAILABLE
  if (touch_keyboard_supported()) {
    touch_service_subscribe(touch_handler, NULL);
    // Let PebbleOS's touch navigation drive the pop-up menus (Reply, React,
    // Canned...). Pebblegram's own screens use its own gestures instead, so
    // the system bridge is switched off for those windows.
    app_touch_navigation_enable(true);
    window_set_touch_bridge_disabled(s_main_window, true);
  }
#endif
  s_startup_wake_timer = app_timer_register(PHONE_WAKE_DELAY_MS, startup_wake_timer_callback, NULL);
}

static void deinit(void) {
  dark_mode_service_unsubscribe();
  light_enable(false);
  cancel_timer(&s_startup_wake_timer);
  cancel_timer(&s_chat_retry_timer);
  destroy_chat_avatars();
  destroy_message_images();
  if (s_dictation_session) {
    dictation_session_destroy(s_dictation_session);
  }
  free_image_transfer_buffer();
  free_avatar_transfer_buffer();
  free_full_text_body();
#if TOUCH_KEYBOARD_AVAILABLE
  if (touch_keyboard_supported()) {
    touch_service_unsubscribe();
  }
#endif
  window_destroy(s_main_window);
  // A system app's heap is torn down on exit, but its statics live on: drop the pointers.
  free(s_chats);
  free(s_messages);
  free(s_pending_text);
  free(s_canned);
#if TOUCH_KEYBOARD_AVAILABLE
  free(s_kb_draft);
#endif
}

bool chat_ui_has_avatar(const char *chat_id) {
  const int index = find_chat_index_by_id(chat_id);
  return index >= 0 && s_chats[index].avatar_bitmap;
}

int chat_ui_avatar_size(void) {
  return s_appearance.photos ? CHAT_AVATAR_R * 2 : 0;
}

// Opened from a notification: whatever was open (another chat, its compose or menu windows)
// closes first, so nothing still sends to it
static void return_to_chat_list(void) {
  while (window_stack_get_top_window() && window_stack_get_top_window() != s_main_window) {
    window_stack_pop(false);
  }
  if (s_touch_keyboard_open) {
    close_touch_keyboard();
  }
  if (s_view_state == ViewStateChat) {
    main_back_click_handler(NULL, NULL);
  }
  // A chat still loading (the list shows until its messages come) must not open after all
  cancel_message_timeout();
  cancel_message_retry();
  s_loading_messages = false;
  s_chat_view_pending = false;
  s_message_transfer_id = 0;
  s_current_chat_id[0] = '\0';
  if (s_search_query[0]) {
    end_search();
  }
}

bool chat_ui_open_chat(const char *chat_id) {
  if (s_view_state == ViewStateChat && chat_id && chat_id[0] && strcmp(chat_id, s_current_chat_id) == 0 &&
      window_stack_get_top_window() == s_main_window) {
    return true;  // already showing it
  }
  return_to_chat_list();
  const int index = find_chat_index_by_id(chat_id);
  if (index < 0 || !s_chat_menu) {
    // No such chat: show the list, kept hidden since launch
    s_open_chat_instantly = false;
    if (s_chat_menu) {
      layer_set_hidden(menu_layer_get_layer(s_chat_menu), false);
    }
    return false;
  }
  // Opened from a notification: show the chat straight away, not the list sliding to it
  s_open_chat_instantly = true;
  MenuIndex cell = {.section = 0, .row = index};
  select_chat_row(index, false);
  chat_menu_select_callback(s_chat_menu, &cell, NULL);
  return true;
}

void chat_ui_main(void) {
  init();
  app_event_loop();
  deinit();
}
