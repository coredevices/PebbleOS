# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from gitlint.rules import CommitRule, RuleViolation


class AuthorNameWords(CommitRule):
    name = "author-name-words"
    id = "UC2"

    def validate(self, commit):
        if not commit.author_name:
            # No commit metadata (e.g. linting piped stdin); nothing to check.
            return

        if len(commit.author_name.split()) < 2:
            return [
                RuleViolation(
                    self.id,
                    f"Author name '{commit.author_name}' must contain at least 2 words",
                    line_nr=1,
                )
            ]
