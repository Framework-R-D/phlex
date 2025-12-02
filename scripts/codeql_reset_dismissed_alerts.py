#!/usr/bin/env python3
"""Reopen all dismissed CodeQL alerts for a repository.

This script uses the GitHub REST API to locate every CodeQL alert that is
currently in the "dismissed" state and reopens it. Reopening an alert removes
its dismissal so the next CodeQL analysis can triage it as if it were new.

Example:
    GITHUB_TOKEN=ghp_... python3 scripts/codeql_reset_dismissed_alerts.py \
        --owner Framework-R-D --repo phlex

The token must have the ``security_events`` scope (``security_events:write``).
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from typing import Iterator, List, Optional

API_ROOT = "https://api.github.com"
API_VERSION = "2022-11-28"


class GitHubAPIError(RuntimeError):
    """Raised when the GitHub API returns an unexpected response."""


def _token() -> str:
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if not token:
        raise GitHubAPIError("Set GITHUB_TOKEN (or GH_TOKEN) with security_events:write scope.")
    return token


def _request(
    method: str,
    path: str,
    *,
    params: Optional[dict] = None,
    payload: Optional[dict] = None,
) -> dict:
    url = urllib.parse.urljoin(API_ROOT, path)
    if params:
        query = urllib.parse.urlencode(params)
        url = f"{url}?{query}"

    data: Optional[bytes] = None
    headers = {
        "Authorization": f"Bearer {_token()}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": API_VERSION,
        "User-Agent": "phlex-codeql-reset-script",
    }
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"

    request = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(request) as response:
            content = response.read().decode("utf-8")
            if not content:
                return {}
            return json.loads(content)
    except urllib.error.HTTPError as exc:
        message = exc.read().decode("utf-8", errors="replace")
        raise GitHubAPIError(
            f"GitHub API {method} {url} failed with {exc.code}: {message}"
        ) from exc


def _paginate_alerts(owner: str, repo: str) -> Iterator[dict]:
    page = 1
    while True:
        result = _request(
            "GET",
            f"/repos/{owner}/{repo}/code-scanning/alerts",
            params={
                "state": "dismissed",
                "per_page": 100,
                "page": page,
            },
        )
        if not isinstance(result, list):
            raise GitHubAPIError("Unexpected response when listing alerts (expected list).")
        if not result:
            return
        for alert in result:
            yield alert
        page += 1


@dataclass
class Alert:
    """Represents a CodeQL alert."""

    number: int
    html_url: str
    rule_id: str
    dismissed_reason: Optional[str]


def _to_alert(raw: dict) -> Alert:
    try:
        number = int(raw["number"])
    except (KeyError, ValueError) as exc:  # pragma: no cover - defensive
        raise GitHubAPIError(f"Alert object missing 'number': {raw}") from exc
    html_url = str(raw.get("html_url", ""))
    rule = raw.get("rule", {}) or {}
    rule_id = str(rule.get("id", ""))
    dismissed_reason = raw.get("dismissed_reason")
    return Alert(
        number=number,
        html_url=html_url,
        rule_id=rule_id,
        dismissed_reason=str(dismissed_reason) if dismissed_reason else None,
    )


def reopen_alert(owner: str, repo: str, alert: Alert, *, dry_run: bool) -> None:
    """Reopen a dismissed CodeQL alert."""
    if dry_run:
        print(f"DRY RUN: would reopen alert #{alert.number} ({alert.rule_id})")
        return
    _request(
        "PATCH",
        f"/repos/{owner}/{repo}/code-scanning/alerts/{alert.number}",
        payload={"state": "open"},
    )
    print(f"Reopened alert #{alert.number} ({alert.rule_id})")


def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--owner", required=True, help="GitHub organization or user")
    parser.add_argument("--repo", required=True, help="Repository name")
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="List alerts without modifying them",
    )
    return parser.parse_args(argv)


def main(argv: Optional[List[str]] = None) -> int:
    """Main entry point for the script."""
    args = parse_args(argv)
    try:
        alerts = [_to_alert(raw) for raw in _paginate_alerts(args.owner, args.repo)]
    except GitHubAPIError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    if not alerts:
        print("No dismissed CodeQL alerts found.")
        return 0

    print(f"Found {len(alerts)} dismissed alert(s).")
    for alert in alerts:
        reason = f" (reason: {alert.dismissed_reason})" if alert.dismissed_reason else ""
        print(f"- #{alert.number} {alert.rule_id}{reason}")

    for alert in alerts:
        try:
            reopen_alert(args.owner, args.repo, alert, dry_run=args.dry_run)
        except GitHubAPIError as exc:
            print(f"Failed to reopen alert #{alert.number}: {exc}", file=sys.stderr)
            return 1

    if args.dry_run:
        print("Dry run complete; no changes made.")
    else:
        print("All dismissed alerts reopened.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
