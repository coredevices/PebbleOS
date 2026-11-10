# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Read the Docs: download the wasm builds the playground runs into the
playground build directories, and mark them prebuilt for docs/conf.py.

A branch takes them from the native firmware run of its commit, a tag from
the assets of its release. Downloading artifacts needs a GitHub token, any
read-only one, in the PBL_PLAYGROUND_GITHUB_TOKEN environment variable of
the project.

Exits with 2 while the run that makes them has not finished: the Docs
Playground RTD workflow starts the build again once it has. Exits with 3
when there are none, as for releases from before they had them.
"""

import io
import json
import os
import sys
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

REPO = "coredevices/pebbleos"
API = f"https://api.github.com/repos/{REPO}"
NOT_READY = 2
NONE = 3

DEST = Path(__file__).resolve().parents[1] / "_build" / "playground"


class _NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


def _get(url, token, accept="application/vnd.github+json"):
    req = urllib.request.Request(
        url, headers={"Authorization": f"Bearer {token}", "Accept": accept}
    )
    try:
        with urllib.request.build_opener(_NoRedirect).open(req, timeout=60) as r:
            return r.read()
    except urllib.error.HTTPError as e:
        if e.code not in (301, 302, 303, 307, 308):
            raise
        # Downloads redirect to storage, which must not get the token
        with urllib.request.urlopen(e.headers["Location"], timeout=300) as r:
            return r.read()


def _runs(workflow, sha, token):
    url = f"{API}/actions/workflows/{workflow}/runs?head_sha={sha}"
    return json.loads(_get(url, token))["workflow_runs"]


def _extract(board, data):
    outdir = DEST / f"{board}_wasm"
    outdir.mkdir(parents=True, exist_ok=True)
    zipfile.ZipFile(io.BytesIO(data)).extractall(outdir)


def _from_run(sha, token):
    """wasm-<board> artifacts of the native firmware run of sha."""
    runs = _runs("build-native.yml", sha, token)
    for run in runs:
        if run["status"] != "completed":
            continue
        url = f"{run['artifacts_url']}?per_page=100"
        artifacts = json.loads(_get(url, token))["artifacts"]
        artifacts = [a for a in artifacts if a["name"].startswith("wasm-")]
        if not artifacts:
            continue
        if any(a["expired"] for a in artifacts):
            print(f"playground: the wasm builds of {run['html_url']} expired")
            return 1
        for a in artifacts:
            _extract(a["name"][len("wasm-") :], _get(a["archive_download_url"], token))
            print(f"playground: {a['name']} from {run['html_url']}")
        return 0
    if not runs or any(r["status"] != "completed" for r in runs):
        print(f"playground: the native firmware run of {sha} has not finished")
        return NOT_READY
    print(f"playground: no wasm builds in the native firmware run of {sha}")
    return 1


def _from_release(tag, sha, token):
    """wasm_<board>_<tag>.zip assets of the release of tag."""
    if any(r["status"] != "completed" for r in _runs("release.yml", sha, token)):
        print(f"playground: the release run of {tag} has not finished")
        return NOT_READY
    try:
        release = json.loads(_get(f"{API}/releases/tags/{tag}", token))
    except urllib.error.HTTPError as e:
        if e.code != 404:
            raise
        print(f"playground: no release {tag}")
        return NONE
    suffix = f"_{tag}.zip"
    assets = [
        a
        for a in release["assets"]
        if a["name"].startswith("wasm_") and a["name"].endswith(suffix)
    ]
    if not assets:
        print(f"playground: no wasm builds in release {tag}")
        return NONE
    for a in assets:
        data = _get(a["url"], token, accept="application/octet-stream")
        _extract(a["name"][len("wasm_") : -len(suffix)], data)
        print(f"playground: {a['name']} from release {tag}")
    return 0


def main():
    token = os.environ["PBL_PLAYGROUND_GITHUB_TOKEN"]
    sha = os.environ["READTHEDOCS_GIT_COMMIT_HASH"]
    if os.environ["READTHEDOCS_VERSION_TYPE"] == "tag":
        rc = _from_release(os.environ["READTHEDOCS_GIT_IDENTIFIER"], sha, token)
    else:
        rc = _from_run(sha, token)
    if rc == 0:
        (DEST / ".prebuilt").touch()
    return rc


if __name__ == "__main__":
    sys.exit(main())
