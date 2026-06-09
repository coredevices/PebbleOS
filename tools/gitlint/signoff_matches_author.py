# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from gitlint.rules import CommitRule, RuleViolation

SIGNOFF_PREFIX = "signed-off-by:"


class SignoffMatchesAuthor(CommitRule):
    name = "signoff-matches-author"
    id = "UC1"

    def validate(self, commit):
        if not commit.author_name and not commit.author_email:
            # No commit metadata (e.g. linting piped stdin); nothing to check.
            return

        expected = f"{commit.author_name} <{commit.author_email}>"
        signoffs = [
            line.strip()[len(SIGNOFF_PREFIX) :].strip()
            for line in commit.message.body
            if line.strip().lower().startswith(SIGNOFF_PREFIX)
        ]

        if not signoffs:
            # Absence is handled by contrib-body-requires-signed-off-by.
            return

        if expected not in signoffs:
            return [
                RuleViolation(
                    self.id,
                    f"Signed-off-by does not match commit author '{expected}'",
                    line_nr=1,
                )
            ]
