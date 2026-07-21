# Language Pack Development Guide

This guide describes how to develop and build language packs for PebbleOS.

## Overview

PebbleOS supports multiple languages through language packs (.pbl files). Each language pack contains:
- Translated strings (compiled from .po files)
- Font resources (subset to only include required characters)
- Optional images

## Directory Structure

```
resources/normal/base/lang/
├── zh_CN/                    # Language-specific directory
│   ├── lang_map.json         # Resource mapping configuration
│   ├── tintin.po             # Human-readable translation file
│   ├── fireflysung.ttf       # Font file (Chinese)
│   ├── fireflyR14.bdf        # Font file (Chinese)
│   ├── months.json           # Codepoints for month names
│   └── notification_codepoints.json  # Codepoints for notifications
├── en_US/
├── de_DE/
├── ...
└── tintin.pot                # Template file (generated during build)
```

## Commands

### Create/Update Translation Files

```shell
# Create or update translation files for a language
./pbl make_lang --lang=zh_CN
```

### Build Language Pack

```shell
# Build a single language pack
./pbl pack_lang --lang=zh_CN

# Build all language packs
./pbl pack_all_langs
```

### Install Language Pack

```shell
python python_libs/pbl/pbl/commands/install_lang.py build/resources/normal/base/lang/zh_CN/zh_CN.pbl
```

## Building Process

### Step 1: Generate Template

First, build the firmware to generate the translation template:

```shell
./pbl configure --board obelix@pvt
./pbl build
```

This generates `build/src/fw/tintin.pot` - a template file containing all strings that need translation.

### Step 2: Initialize Language

Create the language directory and seed files:

```shell
./pbl make_lang --lang=zh_CN
```

This creates:
- `resources/normal/base/lang/zh_CN/lang_map.json`
- `resources/normal/base/lang/zh_CN/tintin.po` (if not exists)

### Step 3: Translate

Edit the `.po` file using a gettext editor (e.g., Poedit, GTranslator):

```shell
# Open with Poedit
poedit resources/normal/base/lang/zh_CN/tintin.po
```

### Step 4: Build Language Pack

```shell
./pbl pack_lang --lang=zh_CN
```

The build process:
1. Compiles `.po` → `.mo` using `msgfmt`
2. Extracts required codepoints using `generate_codepoint_requirements.py`
3. Generates font subsets using `FontResourceGenerator`
4. Packages everything into a `.pbl` file

### Step 5: Install

```shell
python python_libs/pbl/pbl/commands/install_lang.py build/resources/normal/base/lang/zh_CN/zh_CN.pbl
```

## lang_map.json Reference

The `lang_map.json` file defines the resources included in a language pack:

```json
{
    "strings": {
        "lang": "zh_CN",
        "name": "STRINGS",
        "file": "tintin.po"
    },
    "fonts": [{
        "name": "GOTHIC_24_EXTENDED",
        "file": "fireflysung.ttf",
        "characterList": "notification_codepoints.json",
        "extended": true
    }, {
        "name": "GOTHIC_24_BOLD_EXTENDED",
        "alias": "GOTHIC_24_EXTENDED"
    }],
    "images": []
}
```

### Font Entry Fields

| Field | Description |
|-------|-------------|
| `name` | Resource name (must match firmware resource references) |
| `file` | Font file path (empty string = placeholder) |
| `characterList` | JSON file containing required codepoints |
| `extended` | Whether this is an extended font |
| `alias` | Reference to another font resource (shares data) |

## Adding a New Language

### 1. Prepare Font Files

Add font files to the language directory. Required fonts depend on the language:

- **Latin languages**: Use existing base fonts (no additional files needed)
- **CJK languages**: Add CJK font files (e.g., NotoSansCJK, fireflysung)
- **Arabic/Hebrew**: Add appropriate font files

### 2. Configure lang_map.json

Create or update `lang_map.json` with font definitions:

```json
{
    "strings": {
        "lang": "ja_JP",
        "name": "STRINGS",
        "file": "tintin.po"
    },
    "fonts": [{
        "name": "GOTHIC_24_EXTENDED",
        "file": "NotoSansCJKjp-DemiLight.otf",
        "characterList": "codepoints.json",
        "extended": true
    }],
    "images": []
}
```

### 3. Generate Translation Template

```shell
./pbl make_lang --lang=ja_JP
```

### 4. Translate

Edit `resources/normal/base/lang/ja_JP/tintin.po`

### 5. Build and Test

```shell
./pbl pack_lang --lang=ja_JP
python python_libs/pbl/pbl/commands/install_lang.py build/resources/normal/base/lang/ja_JP/ja_JP.pbl
```

## Tools

### GNU Gettext Tools

| Tool | Purpose |
|------|---------|
| `msginit` | Initialize a new translation file |
| `msgfmt` | Compile .po to .mo |
| `msgmerge` | Merge updated template into existing .po |
| `msgattrib` | Check for untranslated strings |

### Python Tools

| Tool | Purpose |
|------|---------|
| `polib` | Parse and manipulate .po files |
| `generate_codepoint_requirements.py` | Extract codepoints from .po files |

### Font Tools

| Tool | Purpose |
|------|---------|
| `FontResourceGenerator` | Generate font resource data with subsetting |

## Technical Details

### Font Subsetting

Font resources are subset to only include characters used in the translation:

1. `generate_codepoint_requirements.py` extracts all Unicode codepoints from translated strings
2. `FontResourceGenerator` builds font data containing only those codepoints
3. This significantly reduces language pack size

### Translation Format

Translations use GNU gettext format. The workflow:

1. `tintin.pot` - Template generated from source code using `xgettext`
2. `tintin.po` - Per-language translation file (human-readable)
3. `tintin.po.STRINGS.mo` - Compiled binary translation (machine-readable)

### Resource Pack Format

Language packs are `.pbl` files, which are ZIP archives containing:
- Compiled `.mo` translation files
- Font resources (subset)
- Resource pack metadata

## Troubleshooting

### Warning: Untranslated Strings

```
Warning: This PO file contains untranslated strings!
```

Use `msgattrib` to identify untranslated strings:

```shell
msgattrib --untranslated resources/normal/base/lang/zh_CN/tintin.po
```

### Missing tintin.pot

```
Error: could not find tintin.pot. Please run ./pbl build first
```

Run `./pbl build` to generate the template file.

### Font Too Large

If font resources are too large:
- Use `characterList` to limit codepoints
- Use font aliases to share data
- Use smaller font files

## Supported Languages

| Language | Code | Fonts | Status |
|----------|------|-------|--------|
| English (US) | en_US | Base | Default |
| Chinese (Simplified) | zh_CN | CJK | Supported |
| Chinese (Traditional) | en_TW | CJK | Supported |
| German | de_DE | Base | Supported |
| French | fr_FR | Base | Supported |
| Spanish | es_ES | Base | Supported |
| Italian | it_IT | Base | Supported |
| Dutch | nl_NL | Base | Supported |
| Portuguese | pt_PT | Base | Supported |
| Russian | ru_RU | Extended | Supported |
| Ukrainian | uk_UA | Extended | Supported |
| Arabic | ar_SA | Arabic | Supported |
| Hebrew | he_IL | Hebrew | Supported |
| Catalan | ca_ES | Base | Supported |

## References

- [GNU Gettext Manual](https://www.gnu.org/software/gettext/manual/)
- [polib Documentation](https://polib.readthedocs.io/)
- `tools/generate_codepoint_requirements.py`
- `pbl` (language pack commands)
