# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import re
import textwrap

_block_re = re.compile(
    r"^(?P<indent>[ \t]*)/\*\*(?![*<])(?P<body>(?:(?!\*/).)*)\*/[ \t]*$",
    flags=re.MULTILINE | re.DOTALL,
)
_line_prefix_re = re.compile(r"^[ \t]*\*(?!/) ?")


def _convert(match):
    indent = match.group("indent")
    lines = match.group("body").split("\n")
    rest = lines[1:]
    if all(_line_prefix_re.match(line) or not line.strip() for line in rest):
        rest = [_line_prefix_re.sub("", line, count=1) for line in rest]
    else:
        rest = textwrap.dedent("\n".join(rest)).split("\n")
    out = [lines[0].strip()] + [line.rstrip() for line in rest]

    while out and not out[0]:
        out.pop(0)
    while out and not out[-1]:
        out.pop()

    return "\n".join(indent + "//!" + (" " + line if line else "") for line in out)


def to_bang_comments(text):
    """Rewrites Javadoc-style doc blocks starting a line as //! line comments, the form the SDK
    generator scans for groups, defines and @internal markers."""
    return _block_re.sub(_convert, text)


def test_to_bang_comments():
    assert to_bang_comments("/** One line. */\nint a;") == "//! One line.\nint a;"
    assert (
        to_bang_comments(
            "  /**\n   * @brief Foo.\n   *\n   * @code{.c}\n   *   bar();\n   * @endcode\n   */"
        )
        == "  //! @brief Foo.\n  //!\n  //! @code{.c}\n  //!   bar();\n  //! @endcode"
    )
    assert to_bang_comments("/** @} */") == "//! @}"
    assert (
        to_bang_comments("/** Foo.\n code {\n   x;\n }\n */")
        == "//! Foo.\n//! code {\n//!   x;\n//! }"
    )
    assert to_bang_comments("int a; /**< Trailing. */") == "int a; /**< Trailing. */"
    assert to_bang_comments("/** a */ int b; /* c */") == "/** a */ int b; /* c */"
    assert to_bang_comments("/* Plain. */") == "/* Plain. */"
