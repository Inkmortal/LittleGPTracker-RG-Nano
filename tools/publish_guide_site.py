#!/usr/bin/env python3
"""Build the guide site and publish it to GitHub Pages (gh-pages branch).

    python tools/publish_guide_site.py --gh-user Inkmortal

--gh-user picks which logged-in `gh` account's token pushes, without
changing the active gh account. Pages is enabled on first publish.
"""

from __future__ import annotations

import argparse
import base64
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
REPO = "Inkmortal/LittleGPTracker-RG-Nano"
NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)

sys.path.insert(0, str(TOOLS))
import build_guide_site  # noqa: E402


def run(cmd: list[str], cwd: Path | None = None, check: bool = True, env: dict | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, cwd=cwd, check=check, capture_output=True, text=True, creationflags=NO_WINDOW, env=env)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--gh-user", default="", help="gh account whose token pushes")
    parser.add_argument("--repo", default=REPO)
    args = parser.parse_args()

    token = run(["gh", "auth", "token", "-u", args.gh_user]).stdout.strip() if args.gh_user else ""
    auth: list[str] = []
    if token:
        basic = base64.b64encode(f"x-access-token:{token}".encode()).decode()
        auth = ["-c", "credential.helper=", "-c", f"http.extraheader=Authorization: Basic {basic}"]

    work = Path(tempfile.mkdtemp(prefix="rgnano-guide-"))
    try:
        build_guide_site.build(work)
        name = run(["git", "-C", str(ROOT), "config", "user.name"]).stdout.strip()
        email = run(["git", "-C", str(ROOT), "config", "user.email"]).stdout.strip()
        commit = run(["git", "-C", str(ROOT), "rev-parse", "--short", "HEAD"]).stdout.strip()
        run(["git", "init", "-q", "-b", "gh-pages"], cwd=work)
        run(["git", "add", "-A"], cwd=work)
        run(["git", "-c", f"user.name={name}", "-c", f"user.email={email}", "commit", "-q", "-m",
             f"Guide site from {commit}"], cwd=work)
        remote = f"https://github.com/{args.repo}.git"
        push = run(["git", *auth, "push", "-q", "--force", remote, "gh-pages:gh-pages"], cwd=work, check=False)
        if push.returncode != 0:
            print(push.stderr, file=sys.stderr)
            return 1
    finally:
        shutil.rmtree(work, ignore_errors=True)

    env = dict(**__import__("os").environ)
    if token:
        env["GH_TOKEN"] = token
    pages = run(["gh", "api", f"repos/{args.repo}/pages"], check=False, env=env)
    if pages.returncode != 0:
        payload = json.dumps({"source": {"branch": "gh-pages", "path": "/"}})
        create = subprocess.run(["gh", "api", "-X", "POST", f"repos/{args.repo}/pages", "--input", "-"],
                                input=payload, capture_output=True, text=True, env=env, creationflags=NO_WINDOW)
        # Pushing gh-pages can auto-enable Pages; "already enabled" is fine
        if create.returncode != 0 and "already enabled" not in (create.stderr + create.stdout):
            print(create.stderr, file=sys.stderr)
            return 1
        pages = run(["gh", "api", f"repos/{args.repo}/pages"], env=env)
    url = json.loads(pages.stdout).get("html_url", "")
    print(f"Published guide: {url}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
