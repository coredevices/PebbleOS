# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import json
import os
import re
import struct
import sys
import unittest
from pathlib import Path

# Allow us to run even if not at the `tools` directory.
root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(0, root_dir)

try:
    import freetype  # noqa: E402

    from font.fontgen import (  # noqa: E402
        FEATURE_OFFSET_16,
        MAX_GLYPHS_EXTENDED,
    )
    from resources.resource_map.resource_generator_font import (  # noqa: E402
        FontResourceGenerator,
    )
    FREETYPE_IMPORT_ERROR = None
except ImportError as e:
    freetype = None
    FEATURE_OFFSET_16 = 0x01
    MAX_GLYPHS_EXTENDED = 255 * 128
    FontResourceGenerator = None
    FREETYPE_IMPORT_ERROR = e


REPO_ROOT = Path(__file__).resolve().parents[2]
RESOURCE_ROOT = REPO_ROOT / "resources"
FONT_PATH = (
    RESOURCE_ROOT
    / "normal/base/lang/zh_CN/NotoSansCJKsc-DemiLight.otf"
)
CODEPOINTS_PATH = (
    RESOURCE_ROOT
    / "normal/base/lang/cjk_notification_codepoints.json"
)
RESOURCE_MAP_PATH = RESOURCE_ROOT / "normal/base/resource_map.json"
SYSTEM_RESOURCE_PATH = REPO_ROOT / "src/fw/resource/system_resource.c"

REQUIRED_TEXT_SAMPLES = {
    "simplified_chinese": (
        "中文通知测试日程会议提醒"
        "天气今天明天地点时间"
    ),
    "traditional_chinese": (
        "繁體中文通知測試日程會議提醒"
        "天氣今天明天地點時間"
    ),
    "japanese": (
        "日本語通知テスト予定会議"
        "リマインダーこんにちはカタカナ"
        "東京大阪京都駅渋谷確認済み"
        "電車遅延"
    ),
    "korean_seed": (
        "한국어알림테스트회의일정"
        "메시지전화오늘내일장소"
        "시간확인삭제저장"
    ),
}

EXPECTED_RANGES = {
    "CJK punctuation": range(0x3000, 0x3040),
    "Hiragana": [
        *range(0x3041, 0x3097),
        *range(0x3099, 0x30A0),
    ],
    "Katakana": range(0x30A0, 0x3100),
    "Katakana phonetic extensions": range(0x31F0, 0x3200),
    "Halfwidth Katakana": range(0xFF61, 0xFFA0),
}


class FakeEnv:
    PLATFORM_NAME = "emery"


class FakePath:
    def relpath(self):
        return "."

    def abspath(self):
        return str(REPO_ROOT)


class FakeResourcePath:
    def relpath(self):
        return "resources"

    def abspath(self):
        return str(RESOURCE_ROOT)


class FakeBuild:
    env = FakeEnv()
    path = FakePath()
    srcnode = FakePath()
    variant = "normal"

    def fatal(self, message):
        raise AssertionError(message)


class FakeResourceBuild(FakeBuild):
    path = FakeResourcePath()


def load_codepoints():
    with open(CODEPOINTS_PATH, "r") as f:
        return [int(cp) for cp in json.load(f)["codepoints"]]


def assert_codepoints_contain(test_case, codepoints, name, text):
    missing = [char for char in text if ord(char) not in codepoints]
    test_case.assertEqual([], missing, f"{name} missing chars: {missing!r}")


def parse_pbf_codepoints(font_data):
    font_md_format = {
        2: "<BBHHBB",
        3: "<BBHHBBBB",
    }
    version = font_data[0]
    if version not in font_md_format:
        raise AssertionError(f"Unexpected font version: {version}")

    font_md_size = struct.calcsize(font_md_format[version])
    header = struct.unpack(font_md_format[version], font_data[:font_md_size])
    if version == 3:
        _, max_height, num_glyphs, _, table_size, cp_bytes, _, features = header
    else:
        _, max_height, num_glyphs, _, table_size, cp_bytes = header
        features = 0

    if cp_bytes not in (2, 4):
        raise AssertionError(f"Unexpected codepoint byte width: {cp_bytes}")

    offset_table_format = "<"
    offset_table_format += "L" if cp_bytes == 4 else "H"
    offset_table_format += "H" if features & FEATURE_OFFSET_16 else "L"
    offset_entry_size = struct.calcsize(offset_table_format)

    hash_entry_format = "<BBH"
    hash_entry_size = struct.calcsize(hash_entry_format)
    hash_table_start = font_md_size
    offset_tables_start = hash_table_start + hash_entry_size * table_size

    indexed_codepoints = set()
    for index in range(table_size):
        offset = hash_table_start + index * hash_entry_size
        _, bucket_size, bucket_offset = struct.unpack(
            hash_entry_format, font_data[offset : offset + hash_entry_size]
        )
        bucket_start = offset_tables_start + bucket_offset
        for bucket_index in range(bucket_size):
            entry_offset = bucket_start + bucket_index * offset_entry_size
            codepoint, _ = struct.unpack(
                offset_table_format,
                font_data[entry_offset : entry_offset + offset_entry_size],
            )
            indexed_codepoints.add(codepoint)

    metadata = {
        "max_height": max_height,
        "num_glyphs": num_glyphs,
        "table_size": table_size,
    }
    return metadata, indexed_codepoints


class TestCjkFontFallback(unittest.TestCase):
    def test_system_fallback_loads_extended_font_resource(self):
        with open(SYSTEM_RESOURCE_PATH, "r") as f:
            system_resource = f.read()

        self.assertRegex(
            system_resource,
            re.compile(
                r"text_resources_init_font\(\s*"
                r"SYSTEM_APP,\s*"
                r"RESOURCE_ID_FONT_FALLBACK_INTERNAL,\s*"
                r"RESOURCE_ID_FONT_FALLBACK_INTERNAL_EXTENDED,",
                re.MULTILINE,
            ),
        )

    def test_resource_map_registers_extended_fallback(self):
        with open(RESOURCE_MAP_PATH, "r") as f:
            media = json.load(f)["media"]

        fallback = next(
            entry
            for entry in media
            if entry.get("name") == "FONT_FALLBACK_INTERNAL_EXTENDED"
        )

        self.assertEqual("font", fallback["type"])
        self.assertEqual(
            "normal/base/lang/zh_CN/NotoSansCJKsc-DemiLight.otf",
            fallback["file"],
        )
        self.assertEqual(
            "normal/base/lang/cjk_notification_codepoints.json",
            fallback["characterList"],
        )
        self.assertEqual(14, fallback["pixelHeight"])
        self.assertTrue(fallback["extended"])
        self.assertTrue(FONT_PATH.exists())
        self.assertTrue(CODEPOINTS_PATH.exists())

    def test_codepoint_list_covers_cjk_notification_text(self):
        codepoints = set(load_codepoints())

        for name, expected_range in EXPECTED_RANGES.items():
            missing = [cp for cp in expected_range if cp not in codepoints]
            self.assertEqual([], missing, f"{name} missing codepoints")

        for name, text in REQUIRED_TEXT_SAMPLES.items():
            assert_codepoints_contain(self, codepoints, name, text)

        self.assertLessEqual(len(codepoints) + 1, MAX_GLYPHS_EXTENDED)

    def test_source_font_supports_every_configured_codepoint(self):
        if FREETYPE_IMPORT_ERROR is not None:
            raise unittest.SkipTest(
                f"freetype is not available: {FREETYPE_IMPORT_ERROR}"
            )

        face = freetype.Face(str(FONT_PATH))
        unsupported = [
            f"U+{cp:04X}"
            for cp in load_codepoints()
            if face.get_char_index(cp) == 0
        ]

        self.assertEqual([], unsupported)

    def test_generated_pbf_indexes_cjk_notification_glyphs(self):
        if FREETYPE_IMPORT_ERROR is not None:
            raise unittest.SkipTest(
                f"font generation is not available: {FREETYPE_IMPORT_ERROR}"
            )

        definition = FontResourceGenerator.font_definition_from_dict(
            FakeEnv,
            {
                "type": "font",
                "name": "FONT_FALLBACK_INTERNAL_EXTENDED",
                "characterList": str(CODEPOINTS_PATH),
                "pixelHeight": 14,
                "extended": True,
            },
            str(FONT_PATH),
        )
        font_data = FontResourceGenerator.build_font_data(
            definition.file, definition
        )
        metadata, indexed_codepoints = parse_pbf_codepoints(font_data)

        self.assertEqual(14, metadata["max_height"])
        self.assertEqual(len(load_codepoints()) + 1, metadata["num_glyphs"])
        self.assertEqual(metadata["num_glyphs"], len(indexed_codepoints))

        for name, text in REQUIRED_TEXT_SAMPLES.items():
            assert_codepoints_contain(self, indexed_codepoints, name, text)

    def test_font_character_list_is_tracked_as_a_generated_source(self):
        if FREETYPE_IMPORT_ERROR is not None:
            raise unittest.SkipTest(
                f"font generation is not available: {FREETYPE_IMPORT_ERROR}"
            )

        bld = FakeBuild()
        current_directory = os.getcwd()
        os.chdir(REPO_ROOT)
        try:
            definitions = FontResourceGenerator.definitions_from_dict(
                bld,
                {
                    "type": "font",
                    "name": "FONT_FALLBACK_INTERNAL_EXTENDED",
                    "file": (
                        "normal/base/lang/zh_CN/"
                        "NotoSansCJKsc-DemiLight.otf"
                    ),
                    "characterList": (
                        "normal/base/lang/"
                        "cjk_notification_codepoints.json"
                    ),
                    "pixelHeight": 14,
                    "extended": True,
                },
                "resources",
            )
        finally:
            os.chdir(current_directory)

        self.assertEqual(1, len(definitions))
        definition = definitions[0]
        self.assertEqual(
            "resources/normal/base/lang/cjk_notification_codepoints.json",
            definition.character_list,
        )
        self.assertIn(definition.character_list, definition.sources)

    def test_font_character_list_accepts_absolute_in_tree_paths(self):
        if FREETYPE_IMPORT_ERROR is not None:
            raise unittest.SkipTest(
                f"font generation is not available: {FREETYPE_IMPORT_ERROR}"
            )

        bld = FakeBuild()
        definitions = FontResourceGenerator.definitions_from_dict(
            bld,
            {
                "type": "font",
                "name": "FONT_FALLBACK_INTERNAL_EXTENDED",
                "file": (
                    "normal/base/lang/zh_CN/"
                    "NotoSansCJKsc-DemiLight.otf"
                ),
                "characterList": str(CODEPOINTS_PATH),
                "pixelHeight": 14,
                "extended": True,
            },
            str(RESOURCE_ROOT),
        )

        self.assertEqual(1, len(definitions))
        definition = definitions[0]
        self.assertEqual(
            "resources/normal/base/lang/cjk_notification_codepoints.json",
            definition.character_list,
        )
        self.assertIn(definition.character_list, definition.sources)

    def test_font_character_list_keeps_open_and_dependency_paths(self):
        if FREETYPE_IMPORT_ERROR is not None:
            raise unittest.SkipTest(
                f"font generation is not available: {FREETYPE_IMPORT_ERROR}"
            )

        bld = FakeResourceBuild()
        definitions = FontResourceGenerator.definitions_from_dict(
            bld,
            {
                "type": "font",
                "name": "FONT_FALLBACK_INTERNAL_EXTENDED",
                "file": (
                    "normal/base/lang/zh_CN/"
                    "NotoSansCJKsc-DemiLight.otf"
                ),
                "characterList": (
                    "normal/base/lang/"
                    "cjk_notification_codepoints.json"
                ),
                "pixelHeight": 14,
                "extended": True,
            },
            ".",
        )

        self.assertEqual(1, len(definitions))
        definition = definitions[0]
        self.assertEqual(
            "resources/normal/base/lang/cjk_notification_codepoints.json",
            definition.character_list,
        )
        self.assertIn(
            "normal/base/lang/cjk_notification_codepoints.json",
            definition.sources,
        )

    def test_font_character_list_rejects_traversal_outside_source(self):
        if FREETYPE_IMPORT_ERROR is not None:
            raise unittest.SkipTest(
                f"font generation is not available: {FREETYPE_IMPORT_ERROR}"
            )

        bld = FakeBuild()
        with self.assertRaisesRegex(
            AssertionError, "escapes resource source path"
        ):
            FontResourceGenerator.definitions_from_dict(
                bld,
                {
                    "type": "font",
                    "name": "FONT_FALLBACK_INTERNAL_EXTENDED",
                    "file": (
                        "normal/base/lang/zh_CN/"
                        "NotoSansCJKsc-DemiLight.otf"
                    ),
                    "characterList": "../outside.json",
                    "pixelHeight": 14,
                    "extended": True,
                },
                "resources",
            )

    def test_pbf_fonts_do_not_track_legacy_character_lists(self):
        if FREETYPE_IMPORT_ERROR is not None:
            raise unittest.SkipTest(
                f"font generation is not available: {FREETYPE_IMPORT_ERROR}"
            )

        bld = FakeBuild()
        definitions = FontResourceGenerator.definitions_from_dict(
            bld,
            {
                "type": "font",
                "name": "FONT_LEGACY_PBF",
                "file": "normal/base/pbf/BITHAM_42_BOLD.pbf",
                "characterList": "normal/base/pbf/nonexistent.json",
            },
            "resources",
        )

        self.assertEqual(1, len(definitions))
        definition = definitions[0]
        self.assertEqual(
            ["resources/normal/base/pbf/BITHAM_42_BOLD.pbf"],
            definition.sources,
        )
        self.assertEqual(
            "normal/base/pbf/nonexistent.json",
            definition.character_list,
        )


if __name__ == "__main__":
    unittest.main()
