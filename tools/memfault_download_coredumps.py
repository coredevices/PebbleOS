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
import email.message
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

DEFAULT_BASE_URL = "https://api.memfault.com"
SYMBOLS_SUBDIR = "symbols"


def request(url, auth, *, binary=False, retries=3, with_headers=False):
    """Perform an authenticated GET, returning the response body."""
    req = urllib.request.Request(url)
    req.add_header("Authorization", f"Basic {auth}")
    req.add_header("Accept", "*/*" if binary else "application/json")

    for attempt in range(retries):
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                body = resp.read()
                return (body, resp.headers) if with_headers else body
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


def parse_content_disposition_filename(headers):
    """Pull a filename out of a Content-Disposition header, if present."""
    cd = headers.get("Content-Disposition") if headers else None
    if not cd:
        return None
    msg = email.message.Message()
    msg["Content-Disposition"] = cd
    return msg.get_filename()


def load_symbol_cache(output_dir):
    """Index already-downloaded symbol files by their Memfault symbol ID."""
    cache = {}
    symbols_dir = os.path.join(output_dir, SYMBOLS_SUBDIR)
    if not os.path.isdir(symbols_dir):
        return cache
    for entry in os.listdir(symbols_dir):
        sid, sep, _ = entry.partition("-")
        if sep and sid.isdigit():
            cache[int(sid)] = os.path.join(symbols_dir, entry)
    return cache


def ensure_symbol_file(api, auth, output_dir, symbol_meta, cache, overwrite):
    """Download the symbol ELF for a coredump if not already cached.

    Returns the local path, or None if no symbol file is available.
    """
    if not symbol_meta or not symbol_meta.get("downloadable"):
        return None
    symbol_id = symbol_meta["id"]
    cached = cache.get(symbol_id)
    if cached and os.path.exists(cached) and not overwrite:
        return cached

    body, headers = request(
        f"{api}/symbols/{symbol_id}/download", auth, binary=True, with_headers=True
    )
    if not body:
        return None

    fname = parse_content_disposition_filename(headers) or f"symbol-{symbol_id}.elf"
    # Prefix with the symbol ID so two distinct symbol files can never collide
    # on disk even if Memfault hands out the same filename for both.
    safe_name = f"{symbol_id}-{fname}"
    symbols_dir = os.path.join(output_dir, SYMBOLS_SUBDIR)
    os.makedirs(symbols_dir, exist_ok=True)
    path = os.path.join(symbols_dir, safe_name)
    with open(path, "wb") as f:
        f.write(body)
    cache[symbol_id] = path
    return path


def link_symbol_next_to_coredump(coredump_path, symbol_path):
    """Drop a symlink beside the coredump pointing at the cached symbol file."""
    base, _ = os.path.splitext(coredump_path)
    link = base + ".symbols.elf"
    target = os.path.relpath(symbol_path, os.path.dirname(link))
    if os.path.lexists(link):
        if os.path.islink(link) and os.readlink(link) == target:
            return link
        os.remove(link)
    os.symlink(target, link)
    return link


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
    parser.add_argument(
        "--no-symbols",
        dest="symbols",
        action="store_false",
        help="Skip downloading the matching symbol (ELF) file for each coredump",
    )
    parser.set_defaults(symbols=True)
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

    symbol_cache = load_symbol_cache(args.output_dir) if args.symbols else {}
    downloaded = skipped = failed = 0
    symbols_downloaded = symbols_reused = symbols_failed = 0
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
        have_coredump = os.path.exists(path) and not args.overwrite
        if have_coredump:
            print(f"{prefix}: already have {name}, skipping")
            skipped += 1
        else:
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

        if args.symbols:
            symbol_meta = coredump.get("symbol_file")
            symbol_id = symbol_meta.get("id") if symbol_meta else None
            already_cached = symbol_id in symbol_cache if symbol_id else False
            try:
                sym_path = ensure_symbol_file(
                    api,
                    auth,
                    args.output_dir,
                    symbol_meta,
                    symbol_cache,
                    args.overwrite,
                )
            except SystemExit as e:
                print(f"{prefix}: symbol download failed: {e}", file=sys.stderr)
                symbols_failed += 1
                sym_path = None

            if sym_path:
                link = link_symbol_next_to_coredump(path, sym_path)
                if already_cached and not args.overwrite:
                    print(
                        f"{prefix}: linked symbols -> {os.path.basename(sym_path)} (cached)"
                    )
                    symbols_reused += 1
                else:
                    print(
                        f"{prefix}: saved symbols {os.path.basename(sym_path)} "
                        f"(linked at {os.path.basename(link)})"
                    )
                    symbols_downloaded += 1
            elif symbol_meta is None:
                # No symbol file attached to this coredump — common for some
                # source types; not an error.
                pass
            elif not symbol_meta.get("downloadable"):
                print(f"{prefix}: symbol file not downloadable, skipping")

    print(f"\nDone. {downloaded} downloaded, {skipped} skipped, {failed} failed.")
    if args.symbols:
        print(
            f"Symbols: {symbols_downloaded} downloaded, "
            f"{symbols_reused} reused, {symbols_failed} failed."
        )
    print(f"Output: {os.path.abspath(args.output_dir)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
