#!/usr/bin/env python3
"""Scan public forks of a GitHub repository and summarize divergence.

This is a monitor, not a guarantee that every new fork can be discovered instantly:
GitHub's public forks API is paginated, rate-limited, and only exposes public forks
available to the token. Merge checks are performed in isolated temporary clones.
No fork code is executed and no fork is merged into the upstream repository.
"""
from __future__ import annotations
import json
import os
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
import argparse

API = os.getenv("GH_API_URL", "https://api.github.com").rstrip("/")
TOKEN = os.getenv("GH_TOKEN", "")
REPO = os.getenv("REPOSITORY", "hdkp81/WaterControlESP32")
MAX_FORKS = max(1, min(100, int(os.getenv("MAX_FORKS", "100") or "100")))


def api_get(url: str):
    req = urllib.request.Request(url, headers={
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        **({"Authorization": f"Bearer {TOKEN}"} if TOKEN else {}),
        "User-Agent": "WaterControlESP32-Fork-Monitor",
    })
    with urllib.request.urlopen(req, timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))


def gh(*args: str, cwd: str | None = None, timeout: int = 120):
    result = subprocess.run(["git", *args], cwd=cwd, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=timeout)
    if result.returncode:
        raise RuntimeError(f"git {' '.join(args)} failed: {result.stderr.strip()}")
    return result.stdout.strip()


def scan(output_md: str = "report.md", output_json: str = "report.json"):
    upstream = api_get(f"{API}/repos/{REPO}")
    default_branch = upstream.get("default_branch") or "main"
    forks = []
    page = 1
    while len(forks) < MAX_FORKS:
        per_page = min(100, MAX_FORKS - len(forks))
        url = f"{API}/repos/{REPO}/forks?per_page={per_page}&page={page}&sort=newest"
        batch = api_get(url)
        if not batch:
            break
        forks.extend(batch)
        if len(batch) < per_page:
            break
        page += 1

    rows = []
    import tempfile
    for fork in forks[:MAX_FORKS]:
        full_name = fork.get("full_name", "unknown/unknown")
        branch = fork.get("default_branch") or default_branch
        clone_url = fork.get("clone_url")
        row = {
            "fork": full_name,
            "url": fork.get("html_url"),
            "branch": branch,
            "updated_at": fork.get("updated_at"),
            "archived": fork.get("archived", False),
            "status": "UNKNOWN",
            "ahead_by": None,
            "behind_by": None,
            "changed_files": [],
            "merge_check": "NOT_RUN",
            "notes": "",
        }
        if row["archived"]:
            row["status"] = "ARCHIVED"
            row["notes"] = "Fork is archived; skipped clone/merge check."
            rows.append(row)
            continue
        try:
            with tempfile.TemporaryDirectory(prefix="wqm-fork-") as tmp:
                gh("init", "--quiet", cwd=tmp)
                gh("remote", "add", "upstream", upstream["clone_url"], cwd=tmp)
                gh("remote", "add", "fork", clone_url, cwd=tmp)
                gh("fetch", "--quiet", "--no-tags", "upstream", default_branch, cwd=tmp, timeout=180)
                gh("fetch", "--quiet", "--no-tags", "fork", branch, cwd=tmp, timeout=180)
                base = gh("rev-parse", "FETCH_HEAD", cwd=tmp)
                # The second fetch made FETCH_HEAD the fork head; resolve upstream explicitly.
                base = gh("rev-parse", f"refs/remotes/upstream/{default_branch}", cwd=tmp)
                head = gh("rev-parse", "FETCH_HEAD", cwd=tmp)
                counts = gh("rev-list", "--left-right", "--count", f"{base}...{head}", cwd=tmp).split()
                behind, ahead = map(int, counts)
                row["ahead_by"] = ahead
                row["behind_by"] = behind
                row["status"] = "DIVERGED" if ahead and behind else ("AHEAD" if ahead else ("BEHIND" if behind else "IN_SYNC"))
                if ahead:
                    raw = gh("diff", "--name-status", f"{base}...{head}", cwd=tmp)
                    row["changed_files"] = raw.splitlines()[:200] if raw else []
                # Create a synthetic merge commit in this disposable clone. No remote is pushed.
                merge = subprocess.run(["git", "merge-tree", "--write-tree", base, head], cwd=tmp,
                                       text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
                if merge.returncode == 0:
                    row["merge_check"] = "CLEAN"
                else:
                    output = (merge.stdout + "\n" + merge.stderr).lower()
                    row["merge_check"] = "CONFLICT_OR_CHECK_ERROR"
                    row["notes"] = "merge-tree returned non-zero; inspect artifact/log because this may include a Git/tool error, not only conflicts."
        except (Exception, subprocess.TimeoutExpired, urllib.error.URLError) as exc:
            row["status"] = "CHECK_ERROR"
            row["notes"] = str(exc)[:1000]
        rows.append(row)

    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    report = {
        "repository": REPO,
        "default_branch": default_branch,
        "generated_at": stamp,
        "forks_checked": len(rows),
        "forks_limit": MAX_FORKS,
        "results": rows,
    }
    Path(output_json).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    lines = [
        f"# Fork Monitor — {REPO}", "",
        f"- **Generated:** {stamp}", f"- **Upstream branch:** `{default_branch}`",
        f"- **Forks checked:** {len(rows)} (scan limit {MAX_FORKS})", "",
        "> This report checks public forks returned by GitHub's API. It does not discover private forks or guarantee instant detection. Mergeability is checked in temporary clones; fork code is never executed.", "",
        "| Fork | Status | Ahead | Behind | Merge check | Updated |", "|---|---:|---:|---:|---|---|"
    ]
    for r in rows:
        lines.append(f"| [{r['fork']}]({r.get('url') or ''}) | {r['status']} | {r['ahead_by'] if r['ahead_by'] is not None else '—'} | {r['behind_by'] if r['behind_by'] is not None else '—'} | {r['merge_check']} | {r.get('updated_at') or '—'} |")
    for r in rows:
        if r["changed_files"] or r["notes"]:
            lines.extend(["", f"## {r['fork']}", ""])
            if r["changed_files"]:
                lines.append("**Changed files compared with upstream merge base:**")
                lines.extend([f"- `{x}`" for x in r["changed_files"]])
            if r["notes"]:
                lines.extend(["", f"**Notes:** {r['notes']}"])
    Path(output_md).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Scanned {len(rows)} forks; report.md and report.json created")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="report.md")
    parser.add_argument("--json", dest="json_output", default="report.json")
    args = parser.parse_args()
    try:
        scan(args.output, args.json_output)
    except Exception as exc:
        print(f"FATAL: {exc}", file=sys.stderr)
        raise
