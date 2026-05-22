#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
"""Download all coredumps for a Memfault issue.

For a given issue, this lists every trace, resolves each trace's coredump,
and downloads the coredump into an output directory.

Auth uses HTTP Basic auth and accepts either:
  * an Organization Auth Token (OAT)  -> token as username, empty password
  * a user (personal) API token       -> use --email; email as username,
                                          token as password

Get a token from:
  https://app.memfault.com/organizations/<org>/settings/auth-tokens   (OAT)
  https://app.memfault.com/account/auth-tokens                        (user)

Examples:
  # Organization Auth Token
  export MEMFAULT_OAT=oat_xxxxxxxxxxxx
  ./tools/memfault_download_coredumps.py --issue xxxxxxxx

  # User (personal) API token
  ./tools/memfault_download_coredumps.py --issue xxxxxxxx \\
      --token user_xxxx --email you@example.com
"""

import argparse
import base64
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

DEFAULT_BASE_URL = "https://api.memfault.com"


def request(url, auth, *, binary=False, retries=3):
    """Perform an authenticated GET, returning the response body."""
    req = urllib.request.Request(url)
    req.add_header("Authorization", f"Basic {auth}")
    req.add_header("Accept", "*/*" if binary else "application/json")

    for attempt in range(retries):
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                return resp.read()
        except urllib.error.HTTPError as e:
            # 429 / 5xx are worth retrying; other 4xx are not.
            if e.code != 429 and e.code < 500:
                detail = e.read().decode("utf-8", "replace")[:500]
                raise SystemExit(f"HTTP {e.code} for {url}\n{detail}") from e
            err = e
        except urllib.error.URLError as e:
            err = e
        if attempt == retries - 1:
            raise SystemExit(f"Giving up on {url}: {err}")
        wait = 2**attempt
        print(f"  request failed ({err}), retrying in {wait}s...", file=sys.stderr)
        time.sleep(wait)


def get_json(url, auth):
    return json.loads(request(url, auth))


def iter_traces(api, auth, issue, since, until):
    """Yield every trace summary for an issue, following pagination."""
    page = 1
    while True:
        params = {"issue": issue, "page": page, "per_page": 100}
        if since:
            params["since"] = since
        if until:
            params["until"] = until
        payload = get_json(f"{api}/traces?{urllib.parse.urlencode(params)}", auth)
        yield from payload.get("data", [])
        if page >= payload.get("paging", {}).get("page_count", page):
            break
        page += 1


def main():
    parser = argparse.ArgumentParser(
        description="Download all coredumps for a Memfault issue.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--issue", required=True, help="Issue ID (numeric)")
    parser.add_argument("--org", default="core-devices", help="Organization slug")
    parser.add_argument("--project", default="dev", help="Project slug")
    parser.add_argument(
        "--token",
        default=os.environ.get("MEMFAULT_OAT") or os.environ.get("MEMFAULT_TOKEN"),
        help="Auth token: OAT, or user API token if --email is given "
        "(or set MEMFAULT_OAT / MEMFAULT_TOKEN)",
    )
    parser.add_argument(
        "--email",
        default=os.environ.get("MEMFAULT_EMAIL"),
        help="Email for user (personal) API token auth; omit for an OAT",
    )
    parser.add_argument(
        "--output-dir", default="coredumps", help="Directory to save coredumps into"
    )
    parser.add_argument(
        "--format",
        choices=["elf", "bin"],
        default="elf",
        help="Coredump download format (default: elf)",
    )
    parser.add_argument(
        "--since", help="ISO8601 lower bound, e.g. 2026-03-24T00:00:00Z"
    )
    parser.add_argument("--until", help="ISO8601 upper bound")
    parser.add_argument(
        "--base-url",
        default=DEFAULT_BASE_URL,
        help=f"API base (default: {DEFAULT_BASE_URL})",
    )
    parser.add_argument(
        "--overwrite", action="store_true", help="Re-download files that already exist"
    )
    args = parser.parse_args()

    if not args.token:
        parser.error("no token: pass --token or set MEMFAULT_OAT")

    # With an email, auth is `email:token` (user API token); otherwise it is
    # `token:` with an empty password (Organization Auth Token).
    username, password = (args.email, args.token) if args.email else (args.token, "")
    auth = base64.b64encode(f"{username}:{password}".encode()).decode()

    api = f"{args.base_url}/api/v0/organizations/{args.org}/projects/{args.project}"
    os.makedirs(args.output_dir, exist_ok=True)

    print(f"Listing traces for issue {args.issue}...")
    traces = list(iter_traces(api, auth, args.issue, args.since, args.until))
    print(f"Found {len(traces)} trace(s).")

    downloaded = skipped = failed = 0
    for i, trace in enumerate(traces, 1):
        trace_id = trace["id"]
        prefix = f"[{i}/{len(traces)}] trace {trace_id}"

        detail = get_json(f"{api}/traces/{trace_id}", auth)["data"]
        coredump = detail.get("coredump") or detail.get("elf_coredump")
        if not coredump:
            print(f"{prefix}: no coredump, skipping")
            skipped += 1
            continue

        coredump_id = coredump["id"]
        name = f"coredump-{coredump_id}-trace-{trace_id}.{args.format}"
        path = os.path.join(args.output_dir, name)
        if os.path.exists(path) and not args.overwrite:
            print(f"{prefix}: already have {name}, skipping")
            skipped += 1
            continue

        dl_url = f"{api}/coredumps/{coredump_id}/download"
        if args.format == "elf":
            dl_url += "?format=elf"
        try:
            body = request(dl_url, auth, binary=True)
        except SystemExit as e:
            print(f"{prefix}: download failed: {e}", file=sys.stderr)
            failed += 1
            continue

        if not body:
            print(
                f"{prefix}: empty {args.format} response "
                f"(coredump {coredump_id} may still be processing); skipping",
                file=sys.stderr,
            )
            failed += 1
            continue

        with open(path, "wb") as f:
            f.write(body)
        print(f"{prefix}: saved {name} ({len(body)} bytes)")
        downloaded += 1

    print(
        f"\nDone. {downloaded} downloaded, {skipped} skipped, {failed} failed."
        f"\nOutput: {os.path.abspath(args.output_dir)}"
    )
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
