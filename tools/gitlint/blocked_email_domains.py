# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from pathlib import Path

from gitlint.rules import CommitRule, RuleViolation

BLOCKLIST_FILE = Path(__file__).with_name("blocked_email_domains.txt")


def _load_blocked_domains():
    domains = set()
    for line in BLOCKLIST_FILE.read_text().splitlines():
        line = line.split("#", 1)[0].strip().lower()
        if line:
            domains.add(line.lstrip("@"))
    return domains


class BlockedEmailDomains(CommitRule):
    name = "blocked-email-domains"
    id = "UC3"

    def validate(self, commit):
        if not commit.author_email:
            # No commit metadata (e.g. linting piped stdin); nothing to check.
            return

        domain = commit.author_email.rsplit("@", 1)[-1].lower()
        if domain in _load_blocked_domains():
            return [
                RuleViolation(
                    self.id,
                    f"Author email domain '{domain}' is blocked",
                    line_nr=1,
                )
            ]
