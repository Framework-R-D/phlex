#!/usr/bin/env python3
r"""Reopen all dismissed CodeQL code-scanning alerts for a repository.

PURPOSE
-------
When CodeQL alerts are dismissed in the GitHub Security tab (e.g. marked
"Won't fix" or "Used in tests"), they no longer appear in pull-request checks
or block merges.  Dismissed alerts also accumulate silently — a finding that
was incorrectly dismissed stays hidden from future analysis.

This script reopens every dismissed alert via the GitHub REST API, resetting
them to the "open" state.  After reopening:

* The next CodeQL scan will re-evaluate each alert and either confirm it as
  still present, or mark it as fixed if the underlying code changed.
* Re-opened alerts that are genuinely intentional can be re-dismissed through
  the normal GitHub UI review process.

This is useful before a scheduled audit, after a significant refactor, or
when you want to ensure the alert list reflects only deliberate dismissals.

AUTHENTICATION
--------------
A GitHub personal access token (PAT) or a GitHub App installation token with
the ``security_events`` scope (``security_events:write``) is required.

Set the token in the environment before running::

    export GITHUB_TOKEN=ghp_your_token_here

The script also accepts ``GH_TOKEN`` as a fallback (the name used by the
GitHub CLI, ``gh``).

USAGE
-----
    # Reopen all dismissed alerts (live run)
    GITHUB_TOKEN=ghp_... python3 scripts/codeql_reset_dismissed_alerts.py \\
        --owner Framework-R-D --repo phlex

    # Preview what would be changed without modifying anything
    GITHUB_TOKEN=ghp_... python3 scripts/codeql_reset_dismissed_alerts.py \\
        --owner Framework-R-D --repo phlex --dry-run

    # Using the gh CLI token directly
    GH_TOKEN=$(gh auth token) python3 scripts/codeql_reset_dismissed_alerts.py \\
        --owner Framework-R-D --repo phlex --dry-run

RATE LIMITS
-----------
The script pages through all dismissed alerts in batches of 100, then issues
one PATCH request per alert.  For a repository with hundreds of dismissed
alerts this may approach GitHub's API rate limit (5 000 requests/hour for
authenticated users).  Check your remaining quota with::

    gh api /rate_limit

EXIT CODES
----------
* ``0`` — success (including when no dismissed alerts were found)
* ``1`` — an API error occurred or the token is missing/insufficient
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
from typing import Any, Iterator, List, Optional

API_ROOT = "https://api.github.com"
API_VERSION = "2022-11-28"


class GitHubAPIError(RuntimeError):
    """Raised when the GitHub API returns an unexpected or error response."""


def _token() -> str:
    """Return the GitHub API token from the environment.

    Checks ``GITHUB_TOKEN`` first, then ``GH_TOKEN`` (the name used by the
    GitHub CLI).

    Raises:
        GitHubAPIError: When neither variable is set.
    """
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if not token:
        raise GitHubAPIError(
            "Set GITHUB_TOKEN (or GH_TOKEN) with the security_events:write scope."
        )
    return token


def _request(
    method: str,
    path: str,
    *,
    params: Optional[dict] = None,
    payload: Optional[dict] = None,
) -> Any:
    """Make an authenticated request to the GitHub REST API.

    Args:
        method: HTTP method (``"GET"``, ``"PATCH"``, etc.).
        path: API path relative to ``https://api.github.com``, e.g.
            ``"/repos/owner/repo/code-scanning/alerts"``.
        params: Optional query-string parameters.
        payload: Optional JSON request body (for PATCH/POST).

    Returns:
        Decoded JSON response body.  Returns ``{}`` for empty responses
        (e.g. HTTP 204).

    Raises:
        GitHubAPIError: On any HTTP error response from the API.
    """
    url = urllib.parse.urljoin(API_ROOT, path)
    if params:
        url = f"{url}?{urllib.parse.urlencode(params)}"

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
            return json.loads(content) if content else {}
    except urllib.error.HTTPError as exc:
        message = exc.read().decode("utf-8", errors="replace")
        raise GitHubAPIError(
            f"GitHub API {method} {url} failed with {exc.code}: {message}"
        ) from exc


def _paginate_alerts(owner: str, repo: str) -> Iterator[dict]:
    """Yield every dismissed code-scanning alert for *owner*/*repo*.

    Pages through the API in batches of 100 until the API returns an empty
    page.

    Args:
        owner: GitHub organization or user name.
        repo: Repository name.

    Yields:
        Raw alert dicts as returned by the GitHub API.

    Raises:
        GitHubAPIError: On an unexpected API response or HTTP error.
    """
    page = 1
    while True:
        result = _request(
            "GET",
            f"/repos/{owner}/{repo}/code-scanning/alerts",
            params={"state": "dismissed", "per_page": 100, "page": page},
        )
        if not isinstance(result, list):
            raise GitHubAPIError("Unexpected response when listing alerts (expected a JSON list).")
        if not result:
            return
        for alert in result:
            yield alert
        page += 1


@dataclass
class Alert:
    """A dismissed CodeQL code-scanning alert."""

    number: int
    """Alert number (unique within the repository)."""

    html_url: str
    """URL to the alert in the GitHub Security tab."""

    rule_id: str
    """The CodeQL rule/query that generated this alert (e.g. ``py/sql-injection``)."""

    dismissed_reason: Optional[str]
    """The reason it was dismissed, or ``None`` if not recorded."""


def _to_alert(raw: dict) -> Alert:
    """Convert a raw API alert dict to an :class:`Alert` dataclass.

    Args:
        raw: A single alert object from the GitHub API response.

    Returns:
        A populated :class:`Alert` instance.

    Raises:
        GitHubAPIError: When the ``number`` field is missing or non-integer.
    """
    try:
        number = int(raw["number"])
    except (KeyError, ValueError) as exc:  # pragma: no cover - defensive
        raise GitHubAPIError(f"Alert object missing 'number': {raw}") from exc
    html_url = str(raw.get("html_url", ""))
    rule = raw.get("rule") or {}
    rule_id = str(rule.get("id", ""))
    dismissed_reason = raw.get("dismissed_reason")
    return Alert(
        number=number,
        html_url=html_url,
        rule_id=rule_id,
        dismissed_reason=str(dismissed_reason) if dismissed_reason else None,
    )


def reopen_alert(owner: str, repo: str, alert: Alert, *, dry_run: bool) -> None:
    """Reopen a single dismissed CodeQL alert.

    In dry-run mode the action is logged but no API call is made.

    Args:
        owner: GitHub organization or user name.
        repo: Repository name.
        alert: The alert to reopen.
        dry_run: When ``True``, print what would happen without doing it.

    Raises:
        GitHubAPIError: On an API error in live mode.
    """
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
    """Parse command-line arguments.

    Args:
        argv: Argument list; defaults to ``sys.argv[1:]`` when ``None``.

    Returns:
        Parsed :class:`argparse.Namespace`.
    """
    parser = argparse.ArgumentParser(
        prog="codeql_reset_dismissed_alerts.py",
        description=(
            "Reopen all dismissed CodeQL code-scanning alerts for a GitHub "
            "repository so that the next analysis can re-evaluate them."
        ),
        epilog=(
            "authentication:\n"
            "  Set GITHUB_TOKEN (or GH_TOKEN) to a PAT with the\n"
            "  security_events:write scope before running.\n\n"
            "  GH_TOKEN=$(gh auth token)  # reuse the gh CLI token\n\n"
            "examples:\n"
            "  # Live run — reopen everything\n"
            "  GITHUB_TOKEN=ghp_... %(prog)s --owner Framework-R-D --repo phlex\n\n"
            "  # Preview without changing anything\n"
            "  GITHUB_TOKEN=ghp_... %(prog)s --owner Framework-R-D --repo phlex"
            " --dry-run"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--owner",
        required=True,
        metavar="ORG",
        help="GitHub organization or user that owns the repository.",
    )
    parser.add_argument(
        "--repo",
        required=True,
        metavar="REPO",
        help="Repository name (without the owner prefix).",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help=(
            "List dismissed alerts and show what would be reopened, but make "
            "no changes to the repository."
        ),
    )
    return parser.parse_args(argv)


def main(argv: Optional[List[str]] = None) -> int:
    """Entry point: list dismissed alerts and reopen them.

    Args:
        argv: Argument list; defaults to ``sys.argv[1:]`` when ``None``.

    Returns:
        ``0`` on success, ``1`` on error.
    """
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
        print(f"  #{alert.number}  {alert.rule_id}{reason}")
        print(f"         {alert.html_url}")

    print()
    for alert in alerts:
        try:
            reopen_alert(args.owner, args.repo, alert, dry_run=args.dry_run)
        except GitHubAPIError as exc:
            print(f"Failed to reopen alert #{alert.number}: {exc}", file=sys.stderr)
            return 1

    if args.dry_run:
        print("\nDry run complete; no changes made.")
    else:
        print("\nAll dismissed alerts reopened.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
