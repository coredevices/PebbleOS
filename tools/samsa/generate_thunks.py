#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
"""Generate the Samsa thunks of a native build.

Apps call the firmware through the SDK jump table with the 32-bit ARM
calling convention; the native firmware is a 64-bit host program. For each
table entry this emits a thunk that reads the arguments the app passed,
converts them to host values, calls the firmware and converts the result
back. Function pointers an app hands over become host trampolines that call
back into the app.

Types are laid out as the app sees them by parsing the SDK's pebble.h for
the ARM target. A type whose layout is the same on the host (no pointers,
no long) is copied as is; the generated code asserts that it really is.
Anything else is converted field by field.
"""

import argparse
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

import clang.cindex as ci
from generate_native_sdk import parse_c_decl
from pebble_sdk_platform import pebble_platforms

TK = ci.TypeKind

INT_KINDS = {
    TK.BOOL,
    TK.CHAR_U,
    TK.UCHAR,
    TK.CHAR16,
    TK.CHAR32,
    TK.USHORT,
    TK.UINT,
    TK.ULONG,
    TK.ULONGLONG,
    TK.CHAR_S,
    TK.SCHAR,
    TK.WCHAR,
    TK.SHORT,
    TK.INT,
    TK.LONG,
    TK.LONGLONG,
    TK.ENUM,
}
UNSIGNED_KINDS = {
    TK.BOOL,
    TK.CHAR_U,
    TK.UCHAR,
    TK.CHAR16,
    TK.CHAR32,
    TK.USHORT,
    TK.UINT,
    TK.ULONG,
    TK.ULONGLONG,
}
# Typedefs that are 32 bits for the app but may be wider on the host, which
# host code declares by the same name. The SDK makes an app's time_t a long;
# samsa_time_t keeps its name through that.
HOST_WIDE_TYPEDEFS = {
    "samsa_time_t",
    "size_t",
    "ssize_t",
    "ptrdiff_t",
    "intptr_t",
    "uintptr_t",
    "time_t",
    "off_t",
}

# Variadic and otherwise special functions, implemented by hand in samsa_manual.c.
MANUAL = {
    "app_log",
    "snprintf",
    "dict_calc_buffer_size",
    "animation_sequence_create",
    "animation_spawn_create",
}

# Opaque types apps read and write anyway, through a copy of the 32-bit
# firmware's definition, with the hand-written converter showing them that
# layout.
SHADOW_TYPES = {
    "struct GBitmap": "&g_samsa_gbitmap_type",
    "struct AnimationLegacy2": "&g_samsa_animation_legacy2_type",
    "struct PropertyAnimationLegacy2": "&g_samsa_property_animation_legacy2_type",
}

# Converters for samsa_types.c, by typedef: a SamsaType for a structure,
# a trampoline getter for a callback.
EXPORTS = [
    "AnimationLegacy2Implementation",
    "PropertyAnimationLegacy2Implementation",
    "AnimationLegacy2StartedHandler",
    "AnimationLegacy2StoppedHandler",
]

# Opaque types the firmware hands out as integer handles and validates
# itself, so any value the app passes back is safe.
HANDLE_TYPES = {"struct AppTimer", "struct Animation", "struct PropertyAnimation"}

CB_SLOTS = 32

# Entry points pebble.h does not declare: those only pebble_worker.h does,
# and SDK 2 ones the current SDK no longer has, for old apps.
LEGACY_DECLS = """
void worker_event_loop(void);
void worker_launch_app(void);
typedef uint8_t GColor2;
typedef struct TextLayerLegacy2 TextLayerLegacy2;
typedef struct InverterLayer InverterLayer;
typedef struct ActionBarLayerLegacy2 ActionBarLayerLegacy2;
typedef struct AnimationLegacy2 AnimationLegacy2;
typedef struct PropertyAnimationLegacy2 PropertyAnimationLegacy2;
typedef struct GBitmapLegacy2 GBitmapLegacy2;
time_t time__deprecated(time_t *tloc);
struct tm *localtime__deprecated(const time_t *timep);
uint16_t time_ms_deprecated(time_t *t, uint16_t *out_ms);
int persist_read_data__deprecated(const uint32_t key, const size_t buffer_size, void *buffer);
int persist_read_string__deprecated(const uint32_t key, const size_t buffer_size, char *buffer);
int persist_write_data__deprecated(const uint32_t key, const size_t size, const void *data);
bool gcolor_equal__deprecated(GColor8 x, GColor8 y);
TextLayerLegacy2 *text_layer_legacy2_create(GRect frame);
void text_layer_legacy2_destroy(TextLayerLegacy2 *text_layer);
GSize text_layer_legacy2_get_content_size(TextLayerLegacy2 *text_layer);
Layer *text_layer_legacy2_get_layer(TextLayerLegacy2 *text_layer);
const char *text_layer_legacy2_get_text(TextLayerLegacy2 *text_layer);
void text_layer_legacy2_set_background_color_2bit(TextLayerLegacy2 *text_layer, GColor2 color);
void text_layer_legacy2_set_font(TextLayerLegacy2 *text_layer, GFont font);
void text_layer_legacy2_set_overflow_mode(TextLayerLegacy2 *text_layer, GTextOverflowMode mode);
void text_layer_legacy2_set_size(TextLayerLegacy2 *text_layer, const GSize max_size);
void text_layer_legacy2_set_text(TextLayerLegacy2 *text_layer, const char *text);
void text_layer_legacy2_set_text_alignment(TextLayerLegacy2 *text_layer, GTextAlignment alignment);
void text_layer_legacy2_set_text_color_2bit(TextLayerLegacy2 *text_layer, GColor2 color);
void window_set_background_color_2bit(Window *window, GColor2 background_color);
void window_set_fullscreen(Window *window, bool enabled);
bool window_get_fullscreen(const Window *window);
void window_set_status_bar_icon(Window *window, const GBitmap *icon);
void bitmap_layer_set_background_color_2bit(BitmapLayer *bitmap_layer, GColor2 color);
void graphics_context_set_fill_color_2bit(GContext *ctx, GColor2 color);
void graphics_context_set_stroke_color_2bit(GContext *ctx, GColor2 color);
void graphics_context_set_text_color_2bit(GContext *ctx, GColor2 color);
void rot_bitmap_layer_set_corner_clip_color_2bit(RotBitmapLayer *bitmap, GColor2 color);
InverterLayer *inverter_layer_create(GRect frame);
void inverter_layer_destroy(InverterLayer *inverter_layer);
Layer *inverter_layer_get_layer(InverterLayer *inverter_layer);
ActionBarLayerLegacy2 *action_bar_layer_legacy2_create(void);
void action_bar_layer_legacy2_destroy(ActionBarLayerLegacy2 *action_bar);
Layer *action_bar_layer_legacy2_get_layer(ActionBarLayerLegacy2 *action_bar);
void action_bar_layer_legacy2_add_to_window(ActionBarLayerLegacy2 *action_bar, Window *window);
void action_bar_layer_legacy2_remove_from_window(ActionBarLayerLegacy2 *action_bar);
void action_bar_layer_legacy2_set_context(ActionBarLayerLegacy2 *action_bar, void *context);
void action_bar_layer_legacy2_set_icon(ActionBarLayerLegacy2 *action_bar, ButtonId button_id,
                                       const GBitmap *icon);
void action_bar_layer_legacy2_clear_icon(ActionBarLayerLegacy2 *action_bar, ButtonId button_id);
void action_bar_layer_legacy2_set_background_color_2bit(ActionBarLayerLegacy2 *action_bar,
                                                        GColor2 background_color);
void action_bar_layer_legacy2_set_click_config_provider(ActionBarLayerLegacy2 *action_bar,
                                                        ClickConfigProvider click_config_provider);
AnimationLegacy2 *animation_legacy2_create(void);
void animation_legacy2_destroy(AnimationLegacy2 *animation);
void *animation_legacy2_get_context(AnimationLegacy2 *animation);
bool animation_legacy2_is_scheduled(AnimationLegacy2 *animation);
void animation_legacy2_schedule(AnimationLegacy2 *animation);
void animation_legacy2_unschedule(AnimationLegacy2 *animation);
void animation_legacy2_unschedule_all(void);
void animation_legacy2_set_curve(AnimationLegacy2 *animation, AnimationCurve curve);
void animation_legacy2_set_delay(AnimationLegacy2 *animation, uint32_t delay_ms);
void animation_legacy2_set_duration(AnimationLegacy2 *animation, uint32_t duration_ms);
PropertyAnimationLegacy2 *property_animation_legacy2_create_layer_frame(Layer *layer, GRect *from,
                                                                        GRect *to);
void property_animation_legacy2_destroy(PropertyAnimationLegacy2 *property_animation);
MenuLayer *menu_layer_legacy2_create(GRect frame);
GBitmapLegacy2 *gbitmap_create_blank_2bit(GSize size);
void gpath_draw_filled_legacy(GContext *ctx, GPath *path);
GBitmap *graphics_capture_frame_buffer_2bit(GContext *ctx);
typedef struct MenuLayerCallbacksLegacy2 {
  MenuLayerGetNumberOfSectionsCallback get_num_sections;
  MenuLayerGetNumberOfRowsInSectionsCallback get_num_rows;
  MenuLayerGetCellHeightCallback get_cell_height;
  MenuLayerGetHeaderHeightCallback get_header_height;
  MenuLayerDrawRowCallback draw_row;
  MenuLayerDrawHeaderCallback draw_header;
  MenuLayerSelectCallback select_click;
  MenuLayerSelectCallback select_long_click;
  MenuLayerSelectionChangedCallback selection_changed;
  MenuLayerGetSeparatorHeightCallback get_separator_height;
  MenuLayerDrawSeparatorCallback draw_separator;
} MenuLayerCallbacksLegacy2;
typedef MenuLayerCallbacksLegacy2 MenuLayerCallbacksLegacy2__deprecated;
void menu_layer_legacy2_set_callbacks(MenuLayer *menu_layer, void *callback_context,
                                      MenuLayerCallbacksLegacy2 callbacks);
void menu_layer_legacy2_set_callbacks__deprecated(MenuLayer *menu_layer, void *callback_context,
                                                  MenuLayerCallbacksLegacy2__deprecated callbacks);
typedef void (*AnimationLegacy2StartedHandler)(struct AnimationLegacy2 *animation, void *context);
typedef void (*AnimationLegacy2StoppedHandler)(struct AnimationLegacy2 *animation, bool finished,
                                               void *context);
typedef struct AnimationLegacy2Handlers {
  AnimationLegacy2StartedHandler started;
  AnimationLegacy2StoppedHandler stopped;
} AnimationLegacy2Handlers;
void animation_legacy2_set_handlers(struct AnimationLegacy2 *animation,
                                    AnimationLegacy2Handlers callbacks, void *context);
typedef void (*AnimationLegacy2SetupImplementation)(struct AnimationLegacy2 *animation);
typedef void (*AnimationLegacy2UpdateImplementation)(struct AnimationLegacy2 *animation,
                                                     const uint32_t distance_normalized);
typedef void (*AnimationLegacy2TeardownImplementation)(struct AnimationLegacy2 *animation);
typedef struct AnimationLegacy2Implementation {
  AnimationLegacy2SetupImplementation setup;
  AnimationLegacy2UpdateImplementation update;
  AnimationLegacy2TeardownImplementation teardown;
} AnimationLegacy2Implementation;
void animation_legacy2_set_implementation(struct AnimationLegacy2 *animation,
                                          const AnimationLegacy2Implementation *implementation);
void animation_legacy2_set_custom_curve(struct AnimationLegacy2 *animation,
                                        AnimationCurveFunction curve_function);
typedef struct PropertyAnimationLegacy2Implementation {
  AnimationLegacy2Implementation base;
  PropertyAnimationAccessors accessors;
} PropertyAnimationLegacy2Implementation;
struct PropertyAnimationLegacy2 *property_animation_legacy2_create(
    const struct PropertyAnimationLegacy2Implementation *implementation, void *subject,
    void *from_value, void *to_value);
void property_animation_legacy2_update_int16(struct PropertyAnimationLegacy2 *property_animation,
                                             const uint32_t distance_normalized);
void property_animation_legacy2_update_gpoint(struct PropertyAnimationLegacy2 *property_animation,
                                              const uint32_t distance_normalized);
void property_animation_legacy2_update_grect(struct PropertyAnimationLegacy2 *property_animation,
                                             const uint32_t distance_normalized);
"""


class Unsupported(Exception):
    pass


def assign(dst, expr):
    """Assign to a host lvalue, const members included."""
    return f"*(typeof_unqual({dst}) *)&({dst}) = {expr};"


# Pointer arguments that point to arrays, with the argument holding the count.
ARRAY_ARGS = {
    "app_sync_init": (3, 4),
    "app_sync_set": (1, 2),
    "dict_calc_buffer_size_from_tuplets": (0, 1),
    "dict_serialize_tuplets": (2, 3),
    "dict_serialize_tuplets_to_buffer": (0, 1),
    "dict_serialize_tuplets_to_buffer_with_iter": (1, 2),
    "simple_menu_layer_create": (2, 3),
}

# Array arguments the firmware keeps and reads later. They are converted
# again whenever the app changed them, before each SDK call.
RETAINED_ARGS = {"simple_menu_layer_create"}

# Pointer fields to arrays, with the field holding the count.
COUNTED_FIELDS = {("SimpleMenuSection", "items"): "num_items"}


def strip(t):
    while t.kind in (TK.ELABORATED, TK.TYPEDEF, TK.ATTRIBUTED):
        if t.kind == TK.ELABORATED:
            t = t.get_named_type()
        elif t.kind == TK.ATTRIBUTED:
            t = t.get_modified_type()
        else:
            t = t.get_declaration().underlying_typedef_type
    return t


def host_wide(t):
    while True:
        if t.kind == TK.ELABORATED:
            t = t.get_named_type()
        elif t.kind == TK.TYPEDEF:
            if t.get_declaration().spelling in HOST_WIDE_TYPEDEFS:
                return True
            t = t.get_declaration().underlying_typedef_type
        else:
            break
    return t.get_canonical().kind in (TK.LONG, TK.ULONG)


def host_wide_name(t):
    """The typedef a wider host type is declared by, if any."""
    while t.kind in (TK.ELABORATED, TK.TYPEDEF):
        if t.kind == TK.ELABORATED:
            t = t.get_named_type()
            continue
        name = t.get_declaration().spelling
        if name in HOST_WIDE_TYPEDEFS:
            return name
        t = t.get_declaration().underlying_typedef_type
    return None


def int_signed(t):
    c = t.get_canonical()
    if c.kind == TK.ENUM:
        return c.get_declaration().enum_type.get_canonical().kind not in UNSIGNED_KINDS
    return c.kind not in UNSIGNED_KINDS


def is_fn_ptr(t):
    c = t.get_canonical()
    return c.kind == TK.POINTER and c.get_pointee().get_canonical().kind in (
        TK.FUNCTIONPROTO,
        TK.FUNCTIONNOPROTO,
    )


def is_record(t):
    return t.get_canonical().kind == TK.RECORD


def is_complete(t):
    return t.get_canonical().get_size() > 0


def _first_named(rec_type):
    for f in fields(rec_type):
        if f.spelling:
            return f.spelling, 0
        name, off = _first_named(f.type)
        if name:
            return name, off
    return None, 0


class Member:
    """A field, or an anonymous struct or union whose members are the parent's."""

    def __init__(self, spelling, type_, offset):
        self.spelling = spelling
        self.type = type_
        self.offset = offset  # bytes

    def is_bitfield(self):
        return False


def fields(t):
    c = t.get_canonical()
    decl = c.get_declaration()
    children = list(decl.get_children())
    field_types = {
        ch.type.get_canonical().get_declaration().hash
        for ch in children
        if ch.kind == ci.CursorKind.FIELD_DECL
    }
    out = []
    for ch in children:
        if ch.kind == ci.CursorKind.FIELD_DECL:
            if ch.is_bitfield():
                out.append(ch)
            else:
                out.append(Member(ch.spelling, ch.type, ch.get_field_offsetof() // 8))
        elif (
            ch.kind in (ci.CursorKind.STRUCT_DECL, ci.CursorKind.UNION_DECL)
            and ch.hash not in field_types
            and not ch.spelling.replace("struct ", "")
            .replace("union ", "")
            .isidentifier()
        ):
            # An anonymous member: find its offset through a member of its own.
            name, _ = _first_named(ch.type)
            inner = ch.type.get_canonical().get_offset(name) // 8 if name else 0
            outer = c.get_offset(name) // 8 if name else 0
            out.append(Member("", ch.type, outer - inner))
    return out


def is_union(t):
    return t.get_canonical().get_declaration().kind == ci.CursorKind.UNION_DECL


class Gen:
    def __init__(self):
        self._compat = {}
        self.descs = {}  # key -> (index, spelled type, pointee type)
        self.desc_code = []
        self.pools = {}  # key -> (index, fn proto type)
        self.pool_code = []
        self.asserts = {}  # spelled -> code

    # -- layout ---------------------------------------------------------
    def compat(self, t):
        """Whether t has the same layout for the app and the host."""
        c = t.get_canonical()
        if c.kind in INT_KINDS:
            return not host_wide(t)
        if c.kind in (TK.FLOAT, TK.DOUBLE, TK.VOID):
            return True
        if c.kind in (TK.POINTER, TK.FUNCTIONPROTO, TK.FUNCTIONNOPROTO):
            return False
        if c.kind in (TK.CONSTANTARRAY, TK.INCOMPLETEARRAY):
            return self.compat(c.element_type)
        if c.kind == TK.RECORD:
            if not is_complete(c):
                return True  # opaque
            key = c.spelling
            if key not in self._compat:
                self._compat[key] = True  # breaks cycles through pointers
                self._compat[key] = all(self.compat(f.type) for f in fields(c))
            return self._compat[key]
        raise Unsupported(f"type {t.spelling} ({c.kind})")

    def note_assert(self, t):
        """Check on the host that a type copied as is has the app's layout."""
        c = t.get_canonical()
        if c.kind != TK.RECORD or not is_complete(c):
            return
        sp = t.spelling
        if sp in self.asserts or sp.startswith("struct (") or "unnamed" in sp:
            return
        lines = [f'_Static_assert(sizeof({sp}) == {c.get_size()}, "{sp} layout");']
        for f in fields(c):
            if isinstance(f, Member) and f.spelling:
                lines.append(
                    f"_Static_assert(offsetof({sp}, {f.spelling}) == "
                    f'{c.get_offset(f.spelling) // 8}, "{sp}.{f.spelling} offset");'
                )
        self.asserts[sp] = "\n".join(lines)

    # -- conversions ----------------------------------------------------
    def g2h(self, t, src, dst):
        """Code converting app bytes at src into the host lvalue dst."""
        c = t.get_canonical()
        if c.spelling.endswith("struct Tuplet"):
            return self.tuplet(t, src, dst, True)
        if self.compat(t):
            self.note_assert(t)
            return f"memcpy((void *)&({dst}), {src}, {c.get_size()});"
        if c.kind in INT_KINDS:
            rd = "samsa_rd32s" if int_signed(t) else "samsa_rd32"
            return assign(dst, f"{rd}({src})")
        if c.kind == TK.POINTER:
            return assign(
                dst,
                f"(typeof_unqual({dst})){self.ptr_in(t, f'samsa_rd32({src})', False)}",
            )
        if c.kind == TK.CONSTANTARRAY:
            e = c.element_type
            n, es = c.element_count, e.get_canonical().get_size()
            return (
                f"for (unsigned i_ = 0; i_ < {n}; i_++) {{ "
                + self.g2h(e, f"({src}) + i_ * {es}", f"({dst})[i_]")
                + " }"
            )
        if c.kind == TK.RECORD:
            return self.record(t, src, dst, self.g2h, True)
        raise Unsupported(f"g2h {t.spelling}")

    def h2g(self, t, src, dst):
        """Code converting the host lvalue src into app bytes at dst."""
        c = t.get_canonical()
        if c.spelling.endswith("struct Tuplet"):
            return self.tuplet(t, dst, src, False)
        if self.compat(t):
            self.note_assert(t)
            return f"memcpy({dst}, &({src}), {c.get_size()});"
        if c.kind in INT_KINDS:
            return f"samsa_wr32({dst}, (uint32_t)({src}));"
        if c.kind == TK.POINTER:
            return f"samsa_wr32({dst}, {self.ptr_out(t, f'(void *)({src})')});"
        if c.kind == TK.CONSTANTARRAY:
            e = c.element_type
            n, es = c.element_count, e.get_canonical().get_size()
            return (
                f"for (unsigned i_ = 0; i_ < {n}; i_++) {{ "
                + self.h2g(e, f"({src})[i_]", f"({dst}) + i_ * {es}")
                + " }"
            )
        if c.kind == TK.RECORD:
            return self.record(t, dst, src, lambda ft, b, l: self.h2g(ft, l, b), False)
        raise Unsupported(f"h2g {t.spelling}")

    def record(self, t, gbytes, hval, conv, to_host, base=0):
        c = t.get_canonical()
        if is_union(c):
            # A union of data pointers converts like any one of them.
            members = fields(c)
            if members and all(
                m.spelling
                and m.type.get_canonical().kind == TK.POINTER
                and not is_fn_ptr(m.type)
                for m in members
            ):
                m = members[0]
                return conv(m.type, f"({gbytes}) + {base}", f"({hval}).{m.spelling}")
            # A union of callbacks of several signatures, like an animation's
            # setters, gets trampolines that forward raw register values.
            if members and all(m.spelling and is_fn_ptr(m.type) for m in members):
                m = members[0]
                lv, gb = f"({hval}).{m.spelling}", f"({gbytes}) + {base}"
                if to_host:
                    return assign(
                        lv, f"(typeof_unqual({lv}))samsa_cb_raw_get(samsa_rd32({gb}))"
                    )
                return f"samsa_wr32({gb}, samsa_cb_to_guest((void *)({lv})));"
            raise Unsupported(f"union {t.spelling}")
        out = []
        for f in fields(c):
            if f.is_bitfield():
                raise Unsupported(f"bitfield in {t.spelling}")
            off = base + f.offset
            if not f.spelling:
                # Members of an anonymous struct or union are the parent's.
                if self.compat(f.type):
                    raise Unsupported(f"anonymous plain member in {t.spelling}")
                out.append(self.record(f.type, gbytes, hval, conv, to_host, off))
                continue
            count = COUNTED_FIELDS.get((t.spelling.replace("const ", ""), f.spelling))
            if count:
                out.append(self.counted(t, f, count, gbytes, hval, base, to_host))
                continue
            out.append(conv(f.type, f"({gbytes}) + {off}", f"({hval}).{f.spelling}"))
        return " ".join(out)

    def counted(self, t, f, count, gbytes, hval, base, to_host):
        """A pointer field to an array whose length another field holds."""
        c = t.get_canonical()
        elem = f.type.get_canonical().get_pointee()
        desc = self.desc(elem)
        g, lv = f"({gbytes}) + {base + f.offset}", f"({hval}).{f.spelling}"
        n = f"samsa_rd32(({gbytes}) + {base + c.get_offset(count) // 8})"
        if to_host:
            return assign(lv, f"samsa_obj_in_array_soft(samsa_rd32({g}), {n}, {desc})")
        return f"samsa_wr32({g}, samsa_obj_guest_of((void *)({lv}), {desc}));"

    def tuplet(self, t, gbytes, hval, to_host):
        """A Tuplet's union holds a pointer or an integer, as its type says."""
        c = t.get_canonical()
        union = fields(c)[2].type
        bytes_t = next(m.type for m in fields(union) if m.spelling == "bytes")
        int_t = next(m.type for m in fields(union) if m.spelling == "integer")
        g, h = gbytes, hval
        o_type = c.get_offset("type") // 8
        o_key = c.get_offset("key") // 8
        o_data = c.get_offset("bytes") // 8
        o_len = o_data + bytes_t.get_canonical().get_offset("length") // 8
        o_store = c.get_offset("integer") // 8
        o_width = o_store + int_t.get_canonical().get_offset("width") // 8
        is_bytes = "(ty_ == TUPLE_BYTE_ARRAY || ty_ == TUPLE_CSTRING)"
        if to_host:
            return " ".join(
                [
                    f"{{ uint8_t ty_ = ({g})[{o_type}];",
                    assign(f"({h}).type", "ty_"),
                    assign(f"({h}).key", f"samsa_rd32(({g}) + {o_key})"),
                    f"uint16_t n_; if {is_bytes} {{",
                    assign(
                        f"({h}).bytes.data",
                        f"samsa_g2h_ptr(samsa_rd32(({g}) + {o_data}))",
                    ),
                    f"memcpy(&n_, ({g}) + {o_len}, 2);",
                    assign(f"({h}).bytes.length", "n_"),
                    "} else {",
                    assign(f"({h}).integer.storage", f"samsa_rd32(({g}) + {o_store})"),
                    f"memcpy(&n_, ({g}) + {o_width}, 2);",
                    assign(f"({h}).integer.width", "n_"),
                    "} }",
                ]
            )
        return " ".join(
            [
                f"{{ uint8_t ty_ = (uint8_t)({h}).type; ({g})[{o_type}] = ty_;",
                f"samsa_wr32(({g}) + {o_key}, ({h}).key);",
                f"if {is_bytes} {{",
                f"samsa_wr32(({g}) + {o_data}, samsa_h2g(({h}).bytes.data));",
                f"memcpy(({g}) + {o_len}, &({h}).bytes.length, 2);",
                "} else {",
                f"samsa_wr32(({g}) + {o_store}, ({h}).integer.storage);",
                f"memcpy(({g}) + {o_width}, &({h}).integer.width, 2);",
                "} }",
            ]
        )

    # -- pointers -------------------------------------------------------
    def desc(self, pointee):
        """A runtime descriptor for app objects that need converting."""
        key = pointee.get_canonical().spelling.replace("const ", "")
        if key in SHADOW_TYPES:
            return SHADOW_TYPES[key]
        if key in self.descs:
            return f"&s_desc_{self.descs[key]}"
        idx = len(self.descs)
        self.descs[key] = idx
        # typeof_unqual drops the qualifiers, and keeps the firmware's
        # GRect(...)-style macros from expanding.
        sp = f"typeof_unqual({pointee.spelling})"
        g = self.g2h(pointee, "g", "(*h)")
        h = self.h2g(pointee, "(*h)", "g")
        self.desc_code.append(
            f"static void desc_{idx}_g2h(const uint8_t *g, void *hv) {{\n"
            f"  {sp} *h = hv;\n  {g}\n}}\n"
            f"static void desc_{idx}_h2g(const void *hv, uint8_t *g) {{\n"
            f"  const {sp} *h = hv;\n  {h}\n}}\n"
            f"static const SamsaType s_desc_{idx} = {{\n"
            f'  .name = "{sp}",\n'
            f"  .guest_size = {pointee.get_canonical().get_size()},\n"
            f"  .host_size = sizeof({sp}),\n"
            f"  .g2h = desc_{idx}_g2h,\n"
            f"  .h2g = desc_{idx}_h2g,\n"
            f"}};\n"
        )
        return f"&s_desc_{idx}"

    def pointee_plain(self, t):
        p = t.get_canonical().get_pointee()
        if p.get_canonical().spelling.replace("const ", "") in SHADOW_TYPES:
            return False
        return self.compat(p) or not is_complete(p)

    def ptr_in(self, t, gexpr, strict=True):
        """Host pointer for the app pointer gexpr.

        Arguments are checked strictly, so an app passing garbage dies instead
        of the firmware. Pointers inside objects are not: out-parameters hold
        whatever the app left there.
        """
        if is_fn_ptr(t):
            return f"{self.pool(t)}_get({gexpr})"
        pointee = t.get_canonical().get_pointee()
        if not self.pointee_plain(t):
            fn = "samsa_obj_in" if strict else "samsa_obj_in_soft"
            return f"{fn}({gexpr}, {self.desc(t.get_pointee())})"
        if (
            not strict
            or pointee.get_canonical().kind == TK.VOID
            or pointee.get_canonical().spelling.replace("const ", "") in HANDLE_TYPES
        ):
            # void * may carry an int, as contexts do.
            return f"samsa_g2h({gexpr})"
        if not is_complete(pointee):
            # Opaque firmware objects can be handles, like animations.
            return f"samsa_g2h_handle({gexpr})"
        return f"samsa_g2h_ptr({gexpr})"

    def ptr_out(self, t, hexpr):
        """App pointer for the host pointer hexpr."""
        if is_fn_ptr(t):
            return f"samsa_cb_to_guest({hexpr})"
        if self.pointee_plain(t):
            return f"samsa_h2g({hexpr})"
        return f"samsa_obj_out({hexpr}, {self.desc(t.get_pointee())})"

    # -- host types -----------------------------------------------------
    def host_type(self, t):
        """The type to declare a host value of app type t with."""
        c = t.get_canonical()
        if c.kind == TK.VOID:
            return "void"
        if c.kind == TK.BOOL:
            return "bool"
        if c.kind in INT_KINDS:
            if host_wide(t):
                return host_wide_name(t) or (
                    "long" if int_signed(t) else "unsigned long"
                )
            bits = c.get_size() * 8
            return f"{'' if int_signed(t) else 'u'}int{bits}_t"
        if c.kind == TK.FLOAT:
            return "float"
        if c.kind == TK.DOUBLE:
            return "double"
        if c.kind in (TK.POINTER, TK.CONSTANTARRAY, TK.INCOMPLETEARRAY):
            return "void *"
        if c.kind == TK.RECORD:
            return f"typeof_unqual({t.spelling})"
        raise Unsupported(f"host type {t.spelling}")

    # -- reading arguments ----------------------------------------------
    def read_arg(self, t, name, pre, post):
        """Read the app argument of type t into the host variable name."""
        c = t.get_canonical()
        ht = self.host_type(t)
        size = c.get_size()
        if c.kind in (TK.CONSTANTARRAY, TK.INCOMPLETEARRAY):
            pre.append(f"void *{name} = samsa_g2h_ptr(samsa_arg(c, s++));")
        elif c.kind in INT_KINDS or c.kind == TK.FLOAT:
            if size == 8:
                pre.append("s = (s + 1) & ~1u;")
                pre.append(f"{ht} {name} = ({ht})samsa_arg64(c, s); s += 2;")
            elif c.kind == TK.FLOAT:
                pre.append(
                    f"float {name}; {{ uint32_t w_ = samsa_arg(c, s++); memcpy(&{name}, &w_, 4); }}"
                )
            elif c.kind == TK.BOOL:
                pre.append(f"bool {name} = (uint8_t)samsa_arg(c, s++);")
            else:
                cast = f"{'' if int_signed(t) else 'u'}int{size * 8}_t"
                pre.append(f"{ht} {name} = ({cast})samsa_arg(c, s++);")
        elif c.kind == TK.DOUBLE:
            pre.append("s = (s + 1) & ~1u;")
            pre.append(
                f"double {name}; {{ uint64_t w_ = samsa_arg64(c, s); memcpy(&{name}, &w_, 8); }} s += 2;"
            )
        elif c.kind == TK.POINTER:
            w = f"w_{name}"
            pre.append(f"uint32_t {w} = samsa_arg(c, s++);")
            pre.append(f"void *{name} = {self.ptr_in(t, w)};")
            pointee = c.get_pointee()
            if (
                not is_fn_ptr(t)
                and not self.pointee_plain(t)
                and not pointee.is_const_qualified()
            ):
                post.append(
                    f"samsa_obj_sync({w}, {name}, {self.desc(t.get_pointee())});"
                )
        elif c.kind == TK.RECORD:
            words = (size + 3) // 4
            if c.get_align() == 8:
                pre.append("s = (s + 1) & ~1u;")
            b = f"b_{name}"
            pre.append(
                f"uint8_t {b}[{words * 4}]; for (unsigned i_ = 0; i_ < {words}; i_++) "
                f"samsa_wr32({b} + 4 * i_, samsa_arg(c, s++));"
            )
            pre.append(f"{ht} {name};")
            pre.append(self.g2h(t, b, name))
        else:
            raise Unsupported(f"argument {t.spelling}")

    # -- callback pools -------------------------------------------------
    def pool(self, t):
        proto = t.get_canonical().get_pointee().get_canonical()
        key = proto.spelling
        if key in self.pools:
            return f"samsa_cb{self.pools[key]}"
        idx = len(self.pools)
        self.pools[key] = idx
        # Reserve the index before generating, as argument types may recurse.
        self.pool_code.append(None)
        self.pool_code[idx] = self.gen_pool(idx, proto)
        return f"samsa_cb{idx}"

    def gen_pool(self, idx, proto):
        if proto.kind == TK.FUNCTIONNOPROTO:
            args = []
        else:
            args = list(proto.argument_types())
        ret = proto.get_result()
        rc = ret.get_canonical()
        name = f"samsa_cb{idx}"
        params = (
            ", ".join(f"{self.host_type(a)} a{i}" for i, a in enumerate(args)) or "void"
        )
        call_args = ", ".join(f"a{i}" for i in range(len(args)))
        rt = self.host_type(ret)
        body = ["uint32_t w[16]; unsigned n = 0;"]
        after = []
        for i, a in enumerate(args):
            ac = a.get_canonical()
            if ac.kind in INT_KINDS or ac.kind == TK.FLOAT:
                if ac.get_size() == 8:
                    body.append(
                        f"n = (n + 1) & ~1u; w[n++] = (uint32_t)(uint64_t)a{i}; "
                        f"w[n++] = (uint32_t)((uint64_t)a{i} >> 32);"
                    )
                elif ac.kind == TK.FLOAT:
                    body.append(f"memcpy(&w[n++], &a{i}, 4);")
                else:
                    body.append(f"w[n++] = (uint32_t)a{i};")
            elif ac.kind == TK.DOUBLE:
                body.append(f"n = (n + 1) & ~1u; memcpy(&w[n], &a{i}, 8); n += 2;")
            elif ac.kind == TK.POINTER:
                body.append(f"w[n] = {self.ptr_out(a, f'a{i}')};")
                if not is_fn_ptr(a) and not self.pointee_plain(a):
                    after.append(
                        f"samsa_obj_back(w[{i}], a{i}, {self.desc(a.get_pointee())});"
                    )
                body.append("n++;")
            elif ac.kind == TK.RECORD:
                words = (ac.get_size() + 3) // 4
                if ac.get_align() == 8:
                    body.append("n = (n + 1) & ~1u;")
                body.append(
                    f"{{ uint8_t b_[{words * 4}] = {{0}}; {self.h2g(a, f'a{i}', 'b_')} "
                    f"memcpy(&w[n], b_, {words * 4}); n += {words}; }}"
                )
            else:
                raise Unsupported(f"callback argument {a.spelling}")
        if after and any(
            a.get_canonical().kind in (TK.RECORD, TK.DOUBLE)
            or a.get_canonical().get_size() == 8
            for a in args
        ):
            raise Unsupported("callback with converted pointers after wide arguments")
        body.append("uint32_t r = 0;")
        body.append(f"samsa_call_guest(s_{name}[k], w, n, &r);")
        body.extend(after)
        if rc.kind == TK.VOID:
            pass
        elif rc.kind == TK.BOOL:
            body.append("return (uint8_t)r != 0;")
        elif rc.kind in INT_KINDS and rc.get_size() <= 4:
            cast = f"{'' if int_signed(ret) else 'u'}int{rc.get_size() * 8}_t"
            body.append(f"return ({rt})({cast})r;")
        elif rc.kind == TK.FLOAT:
            body.append("float f_; memcpy(&f_, &r, 4); return f_;")
        elif rc.kind == TK.POINTER:
            body.append(f"return {self.ptr_in(ret, 'r')};")
        elif rc.kind == TK.RECORD and rc.get_size() <= 4 and self.compat(ret):
            body.append(f"{rt} v_; memcpy(&v_, &r, {rc.get_size()}); return v_;")
        else:
            raise Unsupported(f"callback result {ret.spelling}")
        slots = " ".join(f"X({k})" for k in range(CB_SLOTS))
        fns = ", ".join(f"(void *){name}_{k}" for k in range(CB_SLOTS))
        ret_kw = "" if rc.kind == TK.VOID else "return "
        return (
            f"// {proto.spelling}\n"
            f"static uint32_t s_{name}[{CB_SLOTS}];\n"
            f"static {rt} {name}_impl(unsigned k{', ' if args else ''}"
            f"{params if args else ''}) {{\n  " + "\n  ".join(body) + "\n}\n"
            f"#define X(k) static {rt} {name}_##k({params}) {{ "
            f"{ret_kw}{name}_impl(k{', ' if args else ''}{call_args}); }}\n"
            f"{slots}\n#undef X\n"
            f"static void *const s_{name}_fns[{CB_SLOTS}] = {{{fns}}};\n"
            f"static SamsaCbPool s_{name}_pool = {{\n"
            f'  "{proto.spelling}", s_{name}, s_{name}_fns, {CB_SLOTS},\n}};\n'
            f"static void *{name}_get(uint32_t g) {{ return samsa_cb_get(&s_{name}_pool, g); }}\n"
        )

    # -- thunks ---------------------------------------------------------
    def thunk(self, idx, fn, impl):
        args = list(fn.type.argument_types())
        ret = fn.result_type
        rc = ret.get_canonical()
        pre, post = ["unsigned s = 0;"], []
        if rc.kind == TK.RECORD and rc.get_size() > 4:
            pre.append("uint32_t r_g = samsa_arg(c, s++);")
        names = []
        array = ARRAY_ARGS.get(fn.spelling)
        for i, a in enumerate(args):
            n = f"a{i}"
            if array and i == array[0]:
                pre.append(f"uint32_t w_{n} = samsa_arg(c, s++);")
            else:
                self.read_arg(a, n, pre, post)
            names.append(n)
        if array:
            p, k = array
            elem = args[p].get_canonical().get_pointee()
            pre.append(
                f"void *a{p} = samsa_obj_in_array{'_retained' if fn.spelling in RETAINED_ARGS else ''}"
                f"(w_a{p}, a{k}, {self.desc(elem)});"
            )
        rt = self.host_type(ret)
        ptypes = ", ".join(self.host_type(a) for a in args) or "void"
        # The firmware's function, declared with the host types, under the
        # symbol it is defined as. A call then matches its definition, which a
        # WebAssembly host checks.
        decl = f'{rt} samsa_impl_{idx}({ptypes}) __asm__(SAMSA_SYMBOL("{impl}"));\n'
        call = f"samsa_impl_{idx}({', '.join(names)})"
        body = list(pre)
        if rc.kind == TK.VOID:
            body.append(call + ";")
            body.extend(post)
            body.append("samsa_return32(c, 0);")
        else:
            body.append(f"{rt} r = {call};")
            body.extend(post)
            if rc.kind in INT_KINDS or rc.kind == TK.FLOAT:
                if rc.get_size() == 8:
                    body.append("samsa_return64(c, (uint64_t)r);")
                elif rc.kind == TK.FLOAT:
                    body.append(
                        "uint32_t w_; memcpy(&w_, &r, 4); samsa_return32(c, w_);"
                    )
                else:
                    cast = f"{'' if int_signed(ret) else 'u'}int{rc.get_size() * 8}_t"
                    body.append(f"samsa_return32(c, (uint32_t)({cast})r);")
            elif rc.kind == TK.DOUBLE:
                body.append("uint64_t w_; memcpy(&w_, &r, 8); samsa_return64(c, w_);")
            elif rc.kind == TK.POINTER:
                body.append(f"samsa_return32(c, {self.ptr_out(ret, 'r')});")
            elif rc.kind == TK.RECORD and rc.get_size() <= 4:
                body.append(f"uint8_t b_[4] = {{0}}; {self.h2g(ret, 'r', 'b_')}")
                body.append("uint32_t w_; memcpy(&w_, b_, 4); samsa_return32(c, w_);")
            elif rc.kind == TK.RECORD:
                body.append(f"uint8_t *b_ = samsa_g2h(r_g); {self.h2g(ret, 'r', 'b_')}")
                body.append("samsa_return32(c, r_g);")
            else:
                raise Unsupported(f"result {ret.spelling}")
        return (
            decl
            + f"static void thunk_{idx}(SamsaCpu *c) {{\n  "
            + "\n  ".join(body)
            + "\n}\n"
        )


def toolchain_includes():
    gcc = os.environ.get("ARM_GCC", "arm-none-eabi-gcc")
    out = []
    for arg in ("-print-file-name=include", "-print-file-name=include-fixed"):
        out.append(subprocess.check_output([gcc, arg], text=True).strip())
    sysroot = subprocess.check_output([gcc, "-print-sysroot"], text=True).strip()
    if not sysroot:
        sysroot = os.path.join(
            os.path.dirname(out[0]), "..", "..", "..", "..", "arm-none-eabi"
        )
    out.append(os.path.join(sysroot, "include"))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--table", required=True, help="pebble.auto.json")
    ap.add_argument(
        "--sdk-include", required=True, help="the platform's SDK include directory"
    )
    ap.add_argument("--platform", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()
    args.sdk_include = os.path.abspath(args.sdk_include)

    parse_c_decl.load_library()
    defines = pebble_platforms[args.platform]["DEFINES"]
    cflags = [
        "-x",
        "c",
        "-std=gnu11",
        "--target=thumbv7m-none-eabi",
        "-mfloat-abi=soft",
        "-fshort-enums",
        "-D_TIME_H_",
        "-Dtime_t=samsa_time_t",
        "-include",
        os.path.join(args.sdk_include, "samsa_prelude.h"),
        "-I",
        args.sdk_include,
    ]
    cflags += [f"-D{d}" for d in defines]
    for inc in toolchain_includes():
        cflags += ["-isystem", inc]
    # Headers an app build generates, and the app's time_t.
    unsaved = [
        (
            os.path.join(args.sdk_include, "samsa_prelude.h"),
            "typedef long samsa_time_t;\n",
        ),
        (os.path.join(args.sdk_include, "message_keys.auto.h"), ""),
        (os.path.join(args.sdk_include, "src", "resource_ids.auto.h"), ""),
    ]
    main_header = os.path.join(args.sdk_include, "samsa_all.h")
    unsaved.append((main_header, '#include "pebble.h"\n' + LEGACY_DECLS))
    tu = ci.Index.create().parse(main_header, args=cflags, unsaved_files=unsaved)
    errors = [d for d in tu.diagnostics if d.severity >= ci.Diagnostic.Error]
    if errors:
        for d in errors[:20]:
            print(d, file=sys.stderr)
        sys.exit(1)

    decls, typedefs = {}, {}
    for cur in tu.cursor.get_children():
        if cur.kind == ci.CursorKind.FUNCTION_DECL:
            decls.setdefault(cur.spelling, cur)
        elif cur.kind == ci.CursorKind.TYPEDEF_DECL:
            typedefs.setdefault(cur.spelling, cur)

    with open(args.table) as f:
        table = json.load(f)
    gen = Gen()
    thunks, entries = [], []
    stats = {"generated": 0, "manual": 0, "missing": 0, "unsupported": 0}
    for idx, e in enumerate(table):
        if e is None:
            entries.append(("NULL", "NULL"))
            continue
        name = e["name"]
        fn = decls.get(name)
        if name in MANUAL:
            thunks.append(f"void samsa_manual_{name}(SamsaCpu *c);\n")
            entries.append((f"samsa_manual_{name}", f'"{name}"'))
            stats["manual"] += 1
            continue
        if fn is None:
            entries.append(("NULL", f'"{name}"'))
            stats["missing"] += 1
            continue
        if fn.type.kind == TK.FUNCTIONPROTO and fn.type.is_function_variadic():
            thunks.append(f"// {name}: variadic\n")
            entries.append(("NULL", f'"{name}"'))
            stats["unsupported"] += 1
            continue
        saved = (
            dict(gen.descs),
            list(gen.desc_code),
            dict(gen.pools),
            list(gen.pool_code),
            dict(gen.asserts),
        )
        try:
            thunks.append(f"// {name}\n" + gen.thunk(idx, fn, e["impl"]))
            entries.append((f"thunk_{idx}", f'"{name}"'))
            stats["generated"] += 1
        except Unsupported as ex:
            gen.descs, gen.desc_code, gen.pools, gen.pool_code, gen.asserts = saved
            thunks.append(f"// {name}: {ex}\n")
            entries.append(("NULL", f'"{name}"'))
            stats["unsupported"] += 1

    exports = []
    for name in EXPORTS:
        t = typedefs[name].type
        if is_fn_ptr(t):
            exports.append(
                f"void *samsa_cb_get_{name}(uint32_t g) {{ return {gen.pool(t)}_get(g); }}"
            )
        else:
            exports.append(
                f"const SamsaType *const g_samsa_type_{name} = {gen.desc(t)};"
            )

    out = [
        "// Generated by tools/samsa/generate_thunks.py. Do not edit.",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "#include <string.h>",
        '#include "all_sdk_headers.h"',
        '#include "samsa_runtime.h"',
        "",
        "typedef pbl_time32_t samsa_time_t;",
        "",
        '#pragma GCC diagnostic ignored "-Wunused-variable"',
        '#pragma GCC diagnostic ignored "-Wunused-but-set-variable"',
        "",
        "",
    ]
    out += gen.asserts.values()
    out.append("")
    # Pools and descriptors refer to each other, so declare the getters first.
    for i in range(len(gen.pools)):
        out.append(f"static void *samsa_cb{i}_get(uint32_t g);")
    out.append("")
    out += gen.desc_code
    out += gen.pool_code
    out += thunks
    out += exports
    out.append("const SamsaThunk g_samsa_thunks[] = {")
    out += [f"  {f}," for f, _ in entries]
    out.append("};")
    out.append("const char *const g_samsa_thunk_names[] = {")
    out += [f"  {n}," for _, n in entries]
    out.append("};")
    out.append(f"const unsigned g_samsa_num_thunks = {len(entries)};")
    out.append("const uint32_t g_samsa_sdk_table[] = {")
    out += [f"  (SAMSA_GUEST_HOOKS + {i * 4}) | 1," for i in range(len(entries))]
    out.append("};")
    out.append("SamsaCbPool *const g_samsa_cb_pools[] = {")
    out += [f"  &s_samsa_cb{i}_pool," for i in range(len(gen.pools))]
    out.append("  NULL,")
    out.append("};")
    with open(args.output, "w") as f:
        f.write("\n".join(out) + "\n")
    print(" ".join(f"{k}={v}" for k, v in stats.items()), file=sys.stderr)


if __name__ == "__main__":
    main()
