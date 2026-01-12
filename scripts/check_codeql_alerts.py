#!/usr/bin/env python3
"""Summarize CodeQL SARIF results for newly introduced or resolved alerts."""

import argparse
import collections.abc
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


class GitHubAPIError(RuntimeError):
    """Raised when the GitHub API returns an unexpected response."""


def _api_request(
    method: str,
    path: str,
    *,
    params: dict | None = None,
    payload: dict | None = None,
) -> Any:
    API_ROOT = "https://api.github.com"
    API_VERSION = "2022-11-28"
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if not token:
        raise GitHubAPIError("Set GITHUB_TOKEN (or GH_TOKEN) with appropriate scopes.")

    url = urllib.parse.urljoin(API_ROOT, path)
    if params:
        url = f"{url}?{urllib.parse.urlencode(params)}"

    data: bytes | None = None
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": API_VERSION,
        "User-Agent": "phlex-codeql-alerts-helper",
    }
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"

    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        _debug(f"GitHub API request: {method} {url}")
        with urllib.request.urlopen(req) as resp:
            content = resp.read().decode("utf-8")
            _debug(f"GitHub API response: {method} {url} (len={len(content)})")
            if not content:
                return None
            return json.loads(content)
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        _debug(f"GitHub API HTTPError {exc.code} for {url}: {body[:200]}")
        raise GitHubAPIError(f"GitHub API {method} {url} failed with {exc.code}: {body}") from exc


LEVEL_ORDER = {"none": 0, "note": 1, "warning": 2, "error": 3}
LEVEL_ICONS = {
    "error": ":x:",
    "warning": ":warning:",
    "note": ":information_source:",
    "none": ":grey_question:",
}


@dataclass
class Alert:
    """Represents a CodeQL alert extracted from SARIF."""

    number: int | None
    html_url: str | None
    rule_id: str
    level: str
    message: str
    location: str
    rule_name: str | None = None
    help_uri: str | None = None
    security_severity: str | None = None
    dismissed_reason: str | None = None
    analysis_key: str | None = None

    def icon(self) -> str:
        """Returns an icon for the alert's level."""
        return LEVEL_ICONS.get(self.level, ":grey_question:")

    def level_title(self) -> str:
        """Returns a title-cased version of the alert's level."""
        return self.level.capitalize()

    def rule_display(self) -> str:
        """Returns a formatted string for the rule ID, including a link if available."""
        if self.help_uri:
            return f"[{self.rule_id}]({self.help_uri})"
        return f"`{self.rule_id}`"

    def severity_suffix(self) -> str:
        """Returns a string indicating the security severity, if available."""
        if self.security_severity:
            return f" ({self.security_severity})"
        return ""


@dataclass
class APIAlertComparison:
    """Holds the results of comparing alerts between refs via the API."""

    new_alerts: list[Alert]
    fixed_alerts: list[Alert]
    matched_alerts: list[Alert]
    new_vs_prev: list[Alert]
    fixed_vs_prev: list[Alert]
    new_vs_base: list[Alert]
    fixed_vs_base: list[Alert]
    base_sha: str | None
    prev_commit_ref: str | None


def parse_args(argv: collections.abc.Sequence[str] | None = None) -> argparse.Namespace:
    """Parses command-line arguments.

    Args:
        argv: The command-line arguments to parse.

    Returns:
        The parsed arguments.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--sarif",
        required=True,
        type=Path,
        help="Path to the CodeQL SARIF file produced by github/codeql-action/analyze",
    )
    parser.add_argument(
        "--owner",
        required=False,
        help="GitHub owner (organization or user). If omitted, read from GITHUB_REPOSITORY",
    )
    parser.add_argument(
        "--repo",
        required=False,
        help="Repository name. If omitted, read from GITHUB_REPOSITORY",
    )
    parser.add_argument(
        "--ref",
        required=False,
        help="Optional Git ref to compare (e.g. refs/pull/104/merge). "
        "When provided the script will query the Code Scanning API "
        "instead of relying only on SARIF baselineState",
    )
    parser.add_argument(
        "--min-level",
        default="warning",
        choices=list(LEVEL_ORDER.keys()),
        help="Lowest SARIF result.level to treat as actionable (default: warning)",
    )
    parser.add_argument(
        "--max-results",
        type=int,
        default=20,
        help="Maximum number of alerts to include in the generated comment",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Enable debug output to stderr (prints API query info)",
    )
    parser.add_argument(
        "--log-path",
        type=Path,
        default=Path(os.environ.get("RUNNER_TEMP", "/tmp")) / "codeql-alerts.log",
        help="Path to write the persistent debug log file.",
    )
    return parser.parse_args(argv)


# Global debug flag set in main()
DEBUG = False


def _debug(msg: str) -> None:
    # Always write debug records to a file for later inspection. When --debug
    # is active, also echo to stderr for immediate visibility.
    try:
        _log(msg)
    except Exception:
        # Never fail the whole run due to logging problems
        pass
    if DEBUG:
        print(msg, file=sys.stderr)


_LOG_PATH: Path | None = None


def _log(msg: str) -> None:
    """Append a timestamped message to the persistent log file.

    This is intentionally lightweight and best-effort: logging failures are
    swallowed to avoid affecting the main script flow.
    """
    if not _LOG_PATH:
        return
    try:
        _LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        # Use timezone-aware UTC timestamp to avoid deprecation warnings.
        ts = datetime.now(timezone.utc).isoformat()
        with open(_LOG_PATH, "a", encoding="utf-8") as fh:
            fh.write(f"{ts} {msg}\n")
    except Exception:
        # swallow logging errors
        return


def _init_log(log_path: Path) -> None:
    """Ensure the persistent log file exists and is truncated for a fresh run.

    This makes it straightforward to upload the single log file as a CI artifact
    for a short lifetime after the run completes.
    """
    global _LOG_PATH
    _LOG_PATH = log_path
    try:
        _LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        ts = datetime.now(timezone.utc).isoformat()
        with open(_LOG_PATH, "w", encoding="utf-8") as fh:
            fh.write(f"{ts} CodeQL alerts helper log (truncated for new run)\n")
    except Exception:
        # Best-effort only; don't fail the run due to logging setup
        return


def _paginate_alerts_api(
    owner: str, repo: str, *, state: str = "open", ref: str | None = None
) -> collections.abc.Iterator[dict]:
    """Paginate Code Scanning alerts from the GitHub API for given repo and ref."""
    page = 1
    while True:
        params = {"state": state, "per_page": 100, "page": page}
        if ref:
            params["ref"] = ref
        result = _api_request("GET", f"/repos/{owner}/{repo}/code-scanning/alerts", params=params)
        if not isinstance(result, list):
            raise GitHubAPIError("Unexpected response when listing alerts (expected list).")
        if not result:
            return
        for alert in result:
            yield alert
        page += 1


def _format_physical_location(phys: dict[str, Any]) -> str | None:
    """Return a formatted location string `path[:line[:col]]`.

    Input: SARIF physicalLocation dict, or None.
    """
    if not phys:
        return None
    artifact = phys.get("artifactLocation", {})
    uri = artifact.get("uri") or artifact.get("uriBaseId")
    if not uri:
        return None
    uri = str(uri)
    region = phys.get("region") or {}
    start_line = region.get("startLine")
    start_column = region.get("startColumn")
    if start_line is not None:
        loc = f"{uri}:{start_line}"
        if start_column is not None:
            loc = f"{loc}:{start_column}"
        return loc
    return uri


def _to_alert_api(raw: dict) -> Alert:
    """Convert a raw code-scanning alert object from the API into Alert dataclass."""
    rule = raw.get("rule") or {}
    rule_id = str(rule.get("id") or "(rule id unavailable)")
    instance = raw.get("most_recent_instance") or {}
    loc = instance.get("location") or {}
    location = "(location unavailable)"
    if loc:
        phys = loc.get("physicalLocation") or {}
        formatted = _format_physical_location(phys)
        if formatted:
            location = formatted
    else:
        # If the API instance has no physical location, try to locate using other instances
        # (some alerts expose locations in 'instances' or other fields)
        other_instances = raw.get("instances") or []
        for inst in other_instances:
            inst_loc = inst.get("location") or {}
            phys = inst_loc.get("physicalLocation") or {}
            formatted = _format_physical_location(phys)
            if formatted:
                location = formatted
                break

    # The API doesn't always include a textual message; use rule name if present
    rule_name = rule.get("name")
    help_uri = rule.get("helpUri")
    # dismissed reason may be present on alert
    dismissed_reason = raw.get("dismissed_reason") or raw.get("dismissedReason")

    # Try to extract a security severity from various possible locations
    security_severity = None
    # rule properties
    rule_props = rule.get("properties") or {}
    for key in ("security-severity", "securitySeverity", "problem.severity", "problemSeverity"):
        if rule_props.get(key):
            security_severity = str(rule_props.get(key))
            break
    # instance properties fallback
    if not security_severity:
        inst_props = instance.get("properties") or {}
        for key in (
            "security-severity",
            "securitySeverity",
            "problem.severity",
            "problemSeverity",
        ):
            if inst_props.get(key):
                security_severity = str(inst_props.get(key))
                break
    alert = Alert(
        number=int(raw["number"]) if "number" in raw and raw["number"] is not None else None,
        html_url=raw.get("html_url"),
        rule_id=rule_id,
        level=(raw.get("severity") or "warning"),
        message=str(raw.get("message", {}).get("text") or rule_name or "(no message)"),
        location=location,
        rule_name=rule_name,
        help_uri=help_uri,
        security_severity=security_severity,
        dismissed_reason=(str(dismissed_reason) if dismissed_reason is not None else None),
        analysis_key=(
            instance.get("analysis_key") or raw.get("analysis_key") or raw.get("fingerprint")
        ),
    )
    # If location couldn't be determined, log the raw API alert for inspection
    if location == "(location unavailable)":
        try:
            snippet = {
                "number": raw.get("number"),
                "rule": raw.get("rule"),
                "most_recent_instance": raw.get("most_recent_instance"),
                "instances": raw.get("instances"),
            }
            _log(f"Unknown API alert location: {json.dumps(snippet, default=str)[:4000]}")
        except (TypeError, OSError) as exc:
            # Best-effort logging failed; print a short message in debug mode.
            if DEBUG:
                print(f"Failed to write API unknown-location snippet: {exc}", file=sys.stderr)
    return alert


def _load_sarif_file(path: Path) -> dict[str, Any]:
    try:
        text = path.read_text(encoding="utf-8")
    except FileNotFoundError:
        raise FileNotFoundError(f"SARIF file not found: {path}") from None
    try:
        return json.loads(text)
    except json.JSONDecodeError as exc:
        raise ValueError(f"Failed to parse SARIF JSON: {exc}") from exc


def load_sarif(path: Path) -> dict[str, Any]:
    """Loads a SARIF file, handling single files and directories.

    Args:
        path: The path to the SARIF file or directory.

    Returns:
        The loaded SARIF data.
    """
    if path.is_dir():
        sarif_files = sorted(p for p in path.rglob("*.sarif") if p.is_file())
        if not sarif_files:
            raise FileNotFoundError(f"No SARIF files found under directory: {path}")
        documents = [_load_sarif_file(file_path) for file_path in sarif_files]
        combined_runs: list[dict[str, Any]] = []
        for document in documents:
            combined_runs.extend(document.get("runs") or [])
        base = documents[0]
        return {
            "version": base.get("version", "2.1.0"),
            "$schema": base.get("$schema"),
            "runs": combined_runs,
        }
    return _load_sarif_file(path)


def severity(level: str | None) -> str:
    """Normalizes a severity level string.

    Args:
        level: The severity level string.

    Returns:
        The normalized severity level.
    """
    if not level:
        return "warning"
    normalized = level.lower()
    if normalized in LEVEL_ORDER:
        return normalized
    return "warning"


def severity_reaches_threshold(level: str, threshold: str) -> bool:
    """Checks if a severity level meets a threshold.

    Args:
        level: The severity level to check.
        threshold: The threshold to compare against.

    Returns:
        True if the severity level meets the threshold, False otherwise.
    """
    return LEVEL_ORDER.get(level, 0) >= LEVEL_ORDER.get(threshold, 0)


def sanitize_message(message: str | None) -> str:
    """Sanitizes and truncates a message string.

    Args:
        message: The message to sanitize.

    Returns:
        The sanitized message.
    """
    if not message:
        return "(no message provided)"
    flattened = " ".join(message.split())
    if len(flattened) > 220:
        return flattened[:217] + "..."
    return flattened


def extract_message(result: dict[str, Any]) -> str:
    """Extracts the message from a SARIF result.

    Args:
        result: The SARIF result.

    Returns:
        The extracted message.
    """
    message = result.get("message") or {}
    text = message.get("markdown") or message.get("text")
    if not text and isinstance(message, dict):
        arguments = message.get("arguments")
        if arguments:
            text = " ".join(str(arg) for arg in arguments)
    return sanitize_message(text)


def extract_location(result: dict[str, Any]) -> str:
    """Extracts the location from a SARIF result.

    Args:
        result: The SARIF result.

    Returns:
        The extracted location.
    """
    locations: collections.abc.Iterable[dict[str, Any]] = result.get("locations") or []
    for location in locations:
        phys = location.get("physicalLocation") or {}
        artifact = phys.get("artifactLocation") or {}
        uri = artifact.get("uri") or artifact.get("uriBaseId")
        if not uri:
            continue
        uri = str(uri)
        region = phys.get("region") or {}
        start_line = region.get("startLine")
        start_column = region.get("startColumn")
        if start_line is not None:
            location_str = f"{uri}:{start_line}"
            if start_column is not None:
                location_str = f"{location_str}:{start_column}"
        else:
            location_str = uri
        return location_str
    # Try relatedLocations as a fallback
    related_locations: collections.abc.Iterable[dict[str, Any]] = (
        result.get("relatedLocations") or []
    )
    for location in related_locations:
        phys = location.get("physicalLocation") or {}
        artifact = phys.get("artifactLocation") or {}
        uri = artifact.get("uri") or artifact.get("uriBaseId")
        if not uri:
            continue
        uri = str(uri)
        region = phys.get("region") or {}
        start_line = region.get("startLine")
        start_column = region.get("startColumn")
        if start_line is not None:
            location_str = f"{uri}:{start_line}"
            if start_column is not None:
                location_str = f"{location_str}:{start_column}"
        else:
            location_str = uri
        return location_str

    logical_locations: collections.abc.Iterable[dict[str, Any]] = (
        result.get("logicalLocations") or []
    )
    for logical in logical_locations:
        fq_name = logical.get("fullyQualifiedName") or logical.get("name")
        if fq_name:
            return str(fq_name)
    # Try codeFlows/threadFlows locations
    code_flows = result.get("codeFlows") or []
    for cf in code_flows:
        for tf in cf.get("threadFlows") or []:
            for loc_entry in tf.get("locations") or []:
                loc = loc_entry.get("location") or {}
                phys = loc.get("physicalLocation") or {}
                artifact = phys.get("artifactLocation") or {}
                uri = artifact.get("uri") or artifact.get("uriBaseId")
                if not uri:
                    continue
                uri = str(uri)
                region = phys.get("region") or {}
                start_line = region.get("startLine")
                start_column = region.get("startColumn")
                if start_line is not None:
                    location_str = f"{uri}:{start_line}"
                    if start_column is not None:
                        location_str = f"{location_str}:{start_column}"
                else:
                    location_str = uri
                return location_str
    return "(location unavailable)"


def rule_lookup_map(run: dict[str, Any]) -> dict[str, dict[str, Any]]:
    """Creates a map of rule IDs to rule objects.

    Args:
        run: The SARIF run object.

    Returns:
        A dictionary mapping rule IDs to rule objects.
    """
    rules: dict[str, dict[str, Any]] = {}
    tool = run.get("tool") or {}
    driver = tool.get("driver") or {}
    for rule in driver.get("rules", []) or []:
        rule_id = rule.get("id")
        if rule_id:
            rules[str(rule_id)] = rule
    return rules


def extract_security_severity(result: dict[str, Any]) -> str | None:
    """Extracts the security severity from a SARIF result.

    Args:
        result: The SARIF result.

    Returns:
        The security severity, or None if not found.
    """
    props = result.get("properties") or {}
    for key in ("security-severity", "problem.severity", "problemSeverity"):
        value = props.get(key)
        if value:
            return str(value)
    return None


def collect_alerts(
    sarif: dict[str, Any],
    *,
    min_level: str,
) -> dict[str, list[Alert]]:
    """Collects new and absent alerts from a SARIF report.

    Args:
        sarif: The SARIF report.
        min_level: The minimum severity level to include.

    Returns:
        A dictionary of new and absent alerts.
    """
    buckets: dict[str, list[Alert]] = {"new": [], "absent": []}
    runs: collections.abc.Iterable[dict[str, Any]] = sarif.get("runs") or []
    for run in runs:
        rules_by_id = rule_lookup_map(run)
        results: collections.abc.Iterable[dict[str, Any]] = run.get("results") or []
        for result in results:
            baseline_state = (result.get("baselineState") or "").lower()
            if baseline_state not in {"new", "absent"}:
                continue
            level = severity(result.get("level"))
            if baseline_state == "new" and not severity_reaches_threshold(level, min_level):
                continue
            rule_id = str(result.get("ruleId") or "(rule id unavailable)")
            rule = rules_by_id.get(rule_id)
            alert = Alert(
                number=None,
                html_url=None,
                rule_id=rule_id,
                level=level,
                message=extract_message(result),
                location=extract_location(result),
                rule_name=(rule or {}).get("name"),
                help_uri=(rule or {}).get("helpUri"),
                security_severity=extract_security_severity(result),
            )
            # If we couldn't determine a physical location, log SARIF snippet for later analysis
            if alert.location == "(location unavailable)":
                try:
                    snippet = {
                        "ruleId": rule_id,
                        "level": level,
                        "message": alert.message,
                        "locations": result.get("locations"),
                        "relatedLocations": result.get("relatedLocations"),
                    }
                    _log(
                        f"Unknown SARIF location for result: "
                        f"{json.dumps(snippet, default=str)[:4000]}"
                    )
                except (TypeError, OSError) as exc:
                    if DEBUG:
                        print(
                            f"Failed to write SARIF unknown-location snippet: {exc}",
                            file=sys.stderr,
                        )
            buckets[baseline_state].append(alert)
    return buckets


def _format_section(
    alerts: collections.abc.Sequence[Alert],
    *,
    max_results: int,
    bullet_prefix: str,
) -> list[str]:
    lines: list[str] = []
    display = list(alerts[:max_results])
    remaining = max(0, len(alerts) - len(display))
    for alert in display:
        severity_note = alert.severity_suffix()
        # Prefer to show an alert number link when available
        if alert.number and alert.html_url:
            prefix = f"[# {alert.number}]({alert.html_url}) "
        elif alert.number:
            prefix = f"# {alert.number} "
        else:
            prefix = ""
        dismissed_note = (
            f" (dismissed: {alert.dismissed_reason})" if alert.dismissed_reason else ""
        )

        lines.append(
            f"- {bullet_prefix} **{alert.level_title()}**{severity_note} {prefix}"
            f"{alert.rule_display()}{dismissed_note} at `{alert.location}` — {alert.message}"
        )
    if remaining:
        lines.append(
            f"- {bullet_prefix} …and {remaining} more alerts "
            "(see Code Scanning for the full list)."
        )
    return lines


def build_comment(
    *,
    new_alerts: collections.abc.Sequence[Alert],
    fixed_alerts: collections.abc.Sequence[Alert],
    repo: str | None,
    max_results: int,
    threshold: str,
) -> str:
    """Builds a comment body for a pull request.

    Args:
        new_alerts: A list of new alerts.
        fixed_alerts: A list of fixed alerts.
        repo: The repository name.
        max_results: The maximum number of results to include.
        threshold: The severity threshold.

    Returns:
        The formatted comment body.
    """
    lines: list[str] = []

    def _highest_severity(alerts: collections.abc.Sequence[Alert]) -> str | None:
        if not alerts:
            return None
        # map level to order and pick highest
        best = max(alerts, key=lambda a: LEVEL_ORDER.get(a.level, 0))
        return best.level_title()

    highest_new = _highest_severity(new_alerts)
    if new_alerts:
        sev_note = f" — Highest severity: {highest_new}" if highest_new else ""
        lines.append(
            f"## ❌ {len(new_alerts)} new CodeQL alert"
            f"{'s' if len(new_alerts) != 1 else ''} (level ≥ {threshold}){sev_note}"
        )
        lines.extend(_format_section(new_alerts, max_results=max_results, bullet_prefix=":x:"))
        lines.append("")

    if fixed_alerts:
        lines.append(
            f"## ✅ {len(fixed_alerts)} CodeQL alert"
            f"{'s' if len(fixed_alerts) != 1 else ''} resolved since the previous run"
        )
        lines.extend(
            _format_section(
                fixed_alerts, max_results=max_results, bullet_prefix=":white_check_mark:"
            )
        )
        lines.append("")

    if repo:
        code_scanning_url = f"https://github.com/{repo}/security/code-scanning"
        lines.append(f"Review the [full CodeQL report]({code_scanning_url}) for details.")
    else:
        lines.append("Review the CodeQL report in the Security tab for full details.")
    return "\n".join(lines).strip() + "\n"


def highest_severity_level_title(alerts: collections.abc.Sequence[Alert]) -> str | None:
    """Finds the highest severity level in a list of alerts.

    Args:
        alerts: A list of alerts.

    Returns:
        The title of the highest severity level, or None if the list is empty.
    """
    if not alerts:
        return None
    best = max(alerts, key=lambda a: LEVEL_ORDER.get(a.level, 0))
    return best.level_title()


def write_summary(
    *,
    new_alerts: collections.abc.Sequence[Alert],
    fixed_alerts: collections.abc.Sequence[Alert],
    max_results: int,
    threshold: str,
) -> None:
    """Writes a summary of the alerts to the GitHub step summary.

    Args:
        new_alerts: A list of new alerts.
        fixed_alerts: A list of fixed alerts.
        max_results: The maximum number of results to include.
        threshold: The severity threshold.
    """
    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if not summary_path or (not new_alerts and not fixed_alerts):
        return
    with open(summary_path, "a", encoding="utf-8") as handle:
        handle.write("## CodeQL Alerts\n\n")
        if new_alerts:
            handle.write(
                f"❌ {len(new_alerts)} new alert{'s' if len(new_alerts) != 1 else ''} "
                "(level ≥ {threshold}).\n"
            )
            for line in _format_section(new_alerts, max_results=max_results, bullet_prefix=":x:"):
                handle.write(f"{line}\n")
            handle.write("\n")
        if fixed_alerts:
            handle.write(
                f"✅ {len(fixed_alerts)} alert{'s' if len(fixed_alerts) != 1 else ''} "
                "resolved since the previous run.\n"
            )
            for line in _format_section(
                fixed_alerts,
                max_results=max_results,
                bullet_prefix=":white_check_mark:",
            ):
                handle.write(f"{line}\n")
            handle.write("\n")


def _print_summary(
    *,
    new_alerts: collections.abc.Sequence[Alert],
    fixed_alerts: collections.abc.Sequence[Alert],
    matched_alerts: collections.abc.Sequence[Alert],
    matched_available: bool,
) -> None:
    """Print a concise summary of new, fixed, and matched alerts to stdout."""
    highest_new = highest_severity_level_title(new_alerts)
    highest_fixed = highest_severity_level_title(fixed_alerts)
    highest_matched = highest_severity_level_title(matched_alerts) if matched_alerts else None

    print("CodeQL Summary:")
    print(
        f"- New (unfiltered): {len(new_alerts)}"
        f"{f' (Highest: {highest_new})' if highest_new else ''}"
    )
    print(
        f"- Fixed (unfiltered): {len(fixed_alerts)}"
        f"{f' (Highest: {highest_fixed})' if highest_fixed else ''}"
    )
    if matched_available:
        print(
            f"- Matched (preexisting): {len(matched_alerts)}"
            f"{f' (Highest: {highest_matched})' if highest_matched else ''}"
        )
    else:
        print("- Matched (preexisting): N/A (no API comparison)")
    print("")


def set_outputs(
    *,
    new_alerts: collections.abc.Sequence[Alert],
    fixed_alerts: collections.abc.Sequence[Alert],
    comment_path: Path | None,
) -> None:
    """Sets the GitHub action outputs.

    Args:
        new_alerts: A list of new alerts.
        fixed_alerts: A list of fixed alerts.
        comment_path: The path to the comment file.
    """
    output_path = os.environ.get("GITHUB_OUTPUT")
    if not output_path:
        return
    with open(output_path, "a", encoding="utf-8") as handle:
        handle.write(f"new_alerts={'true' if new_alerts else 'false'}\n")
        handle.write(f"alert_count={len(new_alerts)}\n")
        handle.write(f"fixed_alerts={'true' if fixed_alerts else 'false'}\n")
        handle.write(f"fixed_count={len(fixed_alerts)}\n")
        if comment_path:
            handle.write(f"comment_path={comment_path}\n")
        else:
            handle.write("comment_path=\n")
        # Expose the persistent log file path so workflows can upload it as
        # a short-lived artifact for debugging if desired.
        if _LOG_PATH:
            handle.write(f"log_path={_LOG_PATH}\n")
        else:
            handle.write("log_path=\n")


def _compare_alerts_via_api(owner: str, repo: str, ref: str) -> APIAlertComparison:
    """Compare alerts between a ref and the main branch using the GitHub API."""
    # Fetch alerts for the PR merge ref (fixed) and for the repo default state (open)
    pr_alerts_raw = list(_paginate_alerts_api(owner, repo, state="open", ref=ref))
    _debug(f"Fetched {len(pr_alerts_raw)} alerts for ref={ref}")
    main_alerts_raw = list(_paginate_alerts_api(owner, repo, state="open", ref=None))
    _debug(f"Fetched {len(main_alerts_raw)} alerts for repo (main)")

    # Also fetch alerts at the PR base (branch point) when possible
    base_ref: str | None = None
    prev_commit_ref: str | None = None
    base_sha: str | None = None
    if ref.startswith("refs/pull/"):
        try:
            pr_num = int(ref.split("/")[2])
            pr_info = _api_request("GET", f"/repos/{owner}/{repo}/pulls/{pr_num}")
            base_ref = pr_info.get("base", {}).get("ref")
            base_sha = pr_info.get("base", {}).get("sha")
            # Determine previous commit on PR if available
            commits = _api_request("GET", f"/repos/{owner}/{repo}/pulls/{pr_num}/commits") or []
            if isinstance(commits, list) and len(commits) >= 2:
                prev_commit_ref = commits[-2].get("sha")
        except (ValueError, GitHubAPIError, IndexError, KeyError, TypeError) as exc:
            # Malformed ref, API error, or unexpected response structure — treat as unavailable
            _debug(f"Could not determine PR base/previous commit for ref {ref}: {exc}")
            base_ref = None
            prev_commit_ref = None

        base_alerts_raw: list[dict] = []
        if base_ref or base_sha:
            # prefer base SHA if available
            base_target = base_sha or base_ref
            base_alerts_raw = list(
                _paginate_alerts_api(owner, repo, state="open", ref=base_target)
            )
            _debug(f"Fetched {len(base_alerts_raw)} alerts for base {base_target}")

    prev_alerts_raw: list[dict] = []
    if prev_commit_ref:
        prev_alerts_raw = list(
            _paginate_alerts_api(owner, repo, state="open", ref=prev_commit_ref)
        )
        _debug(f"Fetched {len(prev_alerts_raw)} alerts for prev commit {prev_commit_ref}")

    def alert_key(a: Alert) -> tuple[str, str]:
        # Prefer analysis_key/fingerprint when available; otherwise use rule+location
        if a.analysis_key:
            return ("ak", str(a.analysis_key))
        return ("rl", f"{a.rule_id}::{a.location or '(location unavailable)'}")

    pr_alerts: dict[tuple[str, str], Alert] = {}
    for raw in pr_alerts_raw:
        alert_obj = _to_alert_api(raw)
        pr_alerts[alert_key(alert_obj)] = alert_obj

    main_alerts: dict[tuple[str, str], Alert] = {}
    for raw in main_alerts_raw:
        alert_obj = _to_alert_api(raw)
        main_alerts[alert_key(alert_obj)] = alert_obj

    # Alerts present in main but not in PR are 'fixed' (resolved vs main)
    pr_keys = set(pr_alerts)
    main_keys = set(main_alerts)

    fixed_ids = main_keys - pr_keys
    # Alerts present in PR but not in main are 'new' (introduced by PR)
    new_ids = pr_keys - main_keys

    # Matching statistics
    pr_total = len(pr_keys)
    main_total = len(main_keys)
    pr_ak_count = sum(1 for k in pr_keys if k[0] == "ak")
    main_ak_count = sum(1 for k in main_keys if k[0] == "ak")
    pr_rl_count = pr_total - pr_ak_count
    main_rl_count = main_total - main_ak_count

    matched_keys = pr_keys & main_keys
    matched_by_ak = sum(1 for k in matched_keys if k[0] == "ak")
    matched_by_rl = sum(1 for k in matched_keys if k[0] == "rl")

    _debug(f"PR alerts: total={pr_total}, ak={pr_ak_count}, rl={pr_rl_count}")
    _debug(f"Main alerts: total={main_total}, ak={main_ak_count}, rl={main_rl_count}")
    _debug(f"Matched: by_ak={matched_by_ak}, by_rl={matched_by_rl}")

    # Build comparisons against previous PR commit and branch point if available
    prev_alerts: dict[tuple[str, str], Alert] = {}
    for raw in prev_alerts_raw or []:
        alert_obj = _to_alert_api(raw)
        prev_alerts[alert_key(alert_obj)] = alert_obj

    base_alerts: dict[tuple[str, str], Alert] = {}
    for raw in base_alerts_raw or []:
        alert_obj = _to_alert_api(raw)
        base_alerts[alert_key(alert_obj)] = alert_obj

    # Changes vs previous commit (if present)
    new_vs_prev: list[Alert] = []
    fixed_vs_prev: list[Alert] = []
    if prev_alerts:
        new_vs_prev_ids = set(pr_alerts) - set(prev_alerts)
        fixed_vs_prev_ids = set(prev_alerts) - set(pr_alerts)
        new_vs_prev = [pr_alerts[rid] for rid in sorted(new_vs_prev_ids)]
        fixed_vs_prev = [prev_alerts[rid] for rid in sorted(fixed_vs_prev_ids)]

    # Changes vs branch point (base)
    new_vs_base: list[Alert] = []
    fixed_vs_base: list[Alert] = []
    if base_alerts:
        new_vs_base_ids = set(pr_alerts) - set(base_alerts)
        fixed_vs_base_ids = set(base_alerts) - set(pr_alerts)
        new_vs_base = [pr_alerts[rid] for rid in sorted(new_vs_base_ids)]
        fixed_vs_base = [base_alerts[rid] for rid in sorted(fixed_vs_base_ids)]

    return APIAlertComparison(
        new_alerts=[pr_alerts[rid] for rid in sorted(new_ids)],
        fixed_alerts=[main_alerts[rid] for rid in sorted(fixed_ids)],
        matched_alerts=[pr_alerts[rid] for rid in sorted(pr_keys & main_keys)],
        new_vs_prev=new_vs_prev,
        fixed_vs_prev=fixed_vs_prev,
        new_vs_base=new_vs_base,
        fixed_vs_base=fixed_vs_base,
        base_sha=base_sha,
        prev_commit_ref=prev_commit_ref,
    )


def _build_multi_section_comment(
    api_comp: APIAlertComparison,
    max_results: int,
) -> str:
    """Build a detailed PR comment when multiple comparisons have been made."""
    lines: list[str] = []
    # Matching summary (always include in PR comment)
    if api_comp.new_vs_base or api_comp.fixed_vs_base:
        lines.append(
            f"{len(api_comp.fixed_vs_base)} fixed, {len(api_comp.new_vs_base)} "
            "new since branch point ("
            f"{api_comp.base_sha[:7] if api_comp.base_sha else 'unknown'})"
        )
    if api_comp.new_vs_prev or api_comp.fixed_vs_prev:
        lines.append(
            f"{len(api_comp.fixed_vs_prev)} fixed, {len(api_comp.new_vs_prev)} "
            "new since previous report on PR ("
            f"{api_comp.prev_commit_ref[:7] if api_comp.prev_commit_ref else 'unknown'})"
        )
    lines.append("")

    # Add previous-commit comparison if available
    if api_comp.new_vs_prev:
        lines.append(
            f"## ❌ {len(api_comp.new_vs_prev)} new CodeQL alert"
            f"{'s' if len(api_comp.new_vs_prev) != 1 else ''} "
            "since the previous PR commit"
        )
        lines.extend(
            _format_section(api_comp.new_vs_prev, max_results=max_results, bullet_prefix=":x:")
        )
        lines.append("")
    if api_comp.fixed_vs_prev:
        lines.append(
            f"## ✅ {len(api_comp.fixed_vs_prev)} CodeQL alert"
            f"{'s' if len(api_comp.fixed_vs_prev) != 1 else ''} "
            "resolved since the previous PR commit"
        )
        lines.extend(
            _format_section(
                api_comp.fixed_vs_prev, max_results=max_results, bullet_prefix=":white_check_mark:"
            )
        )
        lines.append("")

    # Add branch-point comparison if available
    if api_comp.new_vs_base:
        lines.append(
            f"## ❌ {len(api_comp.new_vs_base)} new CodeQL alert"
            f"{'s' if len(api_comp.new_vs_base) != 1 else ''} since the branch point"
        )
        lines.extend(
            _format_section(api_comp.new_vs_base, max_results=max_results, bullet_prefix=":x:")
        )
        lines.append("")
    if api_comp.fixed_vs_base:
        lines.append(
            f"## ✅ {len(api_comp.fixed_vs_base)} CodeQL alert"
            f"{'s' if len(api_comp.fixed_vs_base) != 1 else ''} resolved since the branch point"
        )
        lines.extend(
            _format_section(
                api_comp.fixed_vs_base, max_results=max_results, bullet_prefix=":white_check_mark:"
            )
        )
        lines.append("")

    repo_str = os.environ.get("GITHUB_REPOSITORY")
    if repo_str:
        code_scanning_url = f"https://github.com/{repo_str}/security/code-scanning"
        lines.append(f"Review the [full CodeQL report]({code_scanning_url}) for details.")
    else:
        lines.append("Review the CodeQL report in the Security tab for full details.")

    return "\n".join(line for line in lines if line).strip() + "\n"


def main(argv: collections.abc.Sequence[str] | None = None) -> int:
    """The main entry point of the script.

    Args:
        argv: The command-line arguments.

    Returns:
        The exit code.
    """
    args = parse_args(argv)
    # set global debug flag
    global DEBUG
    DEBUG = getattr(args, "debug", False)
    # Recreate/truncate the persistent debug log for this run so CI can
    # upload the single artifact if desired.
    _init_log(args.log_path)
    sarif = load_sarif(args.sarif)
    min_level = severity(args.min_level)

    # First try SARIF baselineState if present
    buckets = collect_alerts(sarif, min_level=min_level)
    new_alerts = buckets.get("new", [])
    fixed_alerts = buckets.get("absent", [])
    _debug(f"SARIF baseline results: new={len(new_alerts)}, fixed={len(fixed_alerts)}")

    # Initialize API comparison variables
    api_comp = None
    # If user supplied a ref and we found no SARIF baseline info, query the API
    # to compare alerts for the given ref against the repository state.
    if args.ref and not (new_alerts or fixed_alerts):
        _debug("No SARIF baselineState results found; switching to API comparison mode")
        # Determine owner/repo
        owner = args.owner
        repo = args.repo
        if not owner or not repo:
            repo_full = os.environ.get("GITHUB_REPOSITORY")
            if not repo_full:
                print(
                    "GITHUB_REPOSITORY not set; please provide --owner and --repo", file=sys.stderr
                )
                return 2
            owner, repo = repo_full.split("/", 1)
        try:
            api_comp = _compare_alerts_via_api(owner, repo, args.ref)
            new_alerts = api_comp.new_alerts
            fixed_alerts = api_comp.fixed_alerts
        except GitHubAPIError as exc:
            print(f"GitHub API error: {exc}", file=sys.stderr)
            return 2

    # Compute unfiltered new/fixed and matched summaries for stdout
    # If API-mode was used, pr_alerts/main_alerts dicts are available;
    # otherwise fall back to SARIF unfiltered buckets
    if api_comp is not None:
        new_all = api_comp.new_alerts
        fixed_all = api_comp.fixed_alerts
        matched_all = api_comp.matched_alerts
        matched_available = True
    else:
        # SARIF-only mode: re-collect without threshold to get unfiltered counts
        try:
            buckets_all = collect_alerts(sarif, min_level="none")
            new_all = buckets_all.get("new", [])
            fixed_all = buckets_all.get("absent", [])
        except (ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
            # If re-collection fails (malformed SARIF or unexpected structure),
            # fall back to thresholded lists
            _debug(
                f"Re-collecting SARIF without threshold failed: {exc}; "
                "falling back to thresholded lists"
            )
            new_all = new_alerts
            fixed_all = fixed_alerts
        matched_all = []
        matched_available = False

    # Print summary to stdout (always)
    _print_summary(
        new_alerts=new_all,
        fixed_alerts=fixed_all,
        matched_alerts=matched_all,
        matched_available=matched_available,
    )

    if (
        new_alerts
        or fixed_alerts
        or (
            api_comp
            and (
                api_comp.new_vs_prev
                or api_comp.fixed_vs_prev
                or api_comp.new_vs_base
                or api_comp.fixed_vs_base
            )
        )
    ):
        repo_str = os.environ.get("GITHUB_REPOSITORY")
        comment_body = ""
        if api_comp:
            comment_body = _build_multi_section_comment(api_comp, args.max_results)
        else:
            comment_body = build_comment(
                new_alerts=new_alerts,
                fixed_alerts=fixed_alerts,
                repo=repo_str,
                max_results=args.max_results,
                threshold=min_level,
            )

        comment_path = Path(os.environ.get("RUNNER_TEMP", ".")) / "codeql-alerts.md"
        comment_path.parent.mkdir(parents=True, exist_ok=True)
        comment_path.write_text(comment_body, encoding="utf-8")

        write_summary(
            new_alerts=new_alerts,
            fixed_alerts=fixed_alerts,
            max_results=args.max_results,
            threshold=min_level,
        )
        set_outputs(new_alerts=new_alerts, fixed_alerts=fixed_alerts, comment_path=comment_path)
        print(comment_body)
        return 0

    print("No new or resolved CodeQL alerts past the configured threshold.")
    set_outputs(new_alerts=[], fixed_alerts=[], comment_path=None)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:  # pragma: no cover
        print(f"Error: {exc}", file=sys.stderr)
        sys.exit(2)
