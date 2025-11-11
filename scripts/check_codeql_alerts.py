#!/usr/bin/env python3
"""Summarize CodeQL SARIF results for newly introduced or resolved alerts."""

from __future__ import annotations

import argparse
import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence

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

    rule_id: str
    level: str
    message: str
    location: str
    rule_name: Optional[str] = None
    help_uri: Optional[str] = None
    security_severity: Optional[str] = None

    def icon(self) -> str:
        return LEVEL_ICONS.get(self.level, ":grey_question:")

    def level_title(self) -> str:
        return self.level.capitalize()

    def rule_display(self) -> str:
        if self.help_uri:
            return f"[{self.rule_id}]({self.help_uri})"
        return f"`{self.rule_id}`"

    def severity_suffix(self) -> str:
        if self.security_severity:
            return f" ({self.security_severity})"
        return ""


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--sarif",
        required=True,
        type=Path,
        help="Path to the CodeQL SARIF file produced by github/codeql-action/analyze",
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
    return parser.parse_args(argv)


def _load_sarif_file(path: Path) -> Dict[str, Any]:
    try:
        text = path.read_text(encoding="utf-8")
    except FileNotFoundError:
        raise FileNotFoundError(f"SARIF file not found: {path}") from None
    try:
        return json.loads(text)
    except json.JSONDecodeError as exc:
        raise ValueError(f"Failed to parse SARIF JSON: {exc}") from exc


def load_sarif(path: Path) -> Dict[str, Any]:
    if path.is_dir():
        sarif_files = sorted(p for p in path.rglob("*.sarif") if p.is_file())
        if not sarif_files:
            raise FileNotFoundError(f"No SARIF files found under directory: {path}")
        documents = [_load_sarif_file(file_path) for file_path in sarif_files]
        combined_runs: List[Dict[str, Any]] = []
        for document in documents:
            combined_runs.extend(document.get("runs") or [])
        base = documents[0]
        return {
            "version": base.get("version", "2.1.0"),
            "$schema": base.get("$schema"),
            "runs": combined_runs,
        }
    return _load_sarif_file(path)


def severity(level: Optional[str]) -> str:
    if not level:
        return "warning"
    normalized = level.lower()
    if normalized in LEVEL_ORDER:
        return normalized
    return "warning"


def severity_reaches_threshold(level: str, threshold: str) -> bool:
    return LEVEL_ORDER.get(level, 0) >= LEVEL_ORDER.get(threshold, 0)


def sanitize_message(message: Optional[str]) -> str:
    if not message:
        return "(no message provided)"
    flattened = " ".join(message.split())
    if len(flattened) > 220:
        return flattened[:217] + "..."
    return flattened


def extract_message(result: Dict[str, Any]) -> str:
    message = result.get("message") or {}
    text = message.get("markdown") or message.get("text")
    if not text and isinstance(message, dict):
        arguments = message.get("arguments")
        if arguments:
            text = " ".join(str(arg) for arg in arguments)
    return sanitize_message(text)


def extract_location(result: Dict[str, Any]) -> str:
    locations: Iterable[Dict[str, Any]] = result.get("locations") or []
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
    logical_locations: Iterable[Dict[str, Any]] = result.get("logicalLocations") or []
    for logical in logical_locations:
        fq_name = logical.get("fullyQualifiedName") or logical.get("name")
        if fq_name:
            return str(fq_name)
    return "(location unavailable)"


def rule_lookup_map(run: Dict[str, Any]) -> Dict[str, Dict[str, Any]]:
    rules: Dict[str, Dict[str, Any]] = {}
    tool = run.get("tool") or {}
    driver = tool.get("driver") or {}
    for rule in driver.get("rules", []) or []:
        rule_id = rule.get("id")
        if rule_id:
            rules[str(rule_id)] = rule
    return rules


def extract_security_severity(result: Dict[str, Any]) -> Optional[str]:
    props = result.get("properties") or {}
    for key in ("security-severity", "problem.severity", "problemSeverity"):
        value = props.get(key)
        if value:
            return str(value)
    return None


def collect_alerts(
    sarif: Dict[str, Any],
    *,
    min_level: str,
) -> Dict[str, List[Alert]]:
    buckets: Dict[str, List[Alert]] = {"new": [], "absent": []}
    runs: Iterable[Dict[str, Any]] = sarif.get("runs") or []
    for run in runs:
        rules_by_id = rule_lookup_map(run)
        results: Iterable[Dict[str, Any]] = run.get("results") or []
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
                rule_id=rule_id,
                level=level,
                message=extract_message(result),
                location=extract_location(result),
                rule_name=(rule or {}).get("name"),
                help_uri=(rule or {}).get("helpUri"),
                security_severity=extract_security_severity(result),
            )
            buckets[baseline_state].append(alert)
    return buckets


def _format_section(
    alerts: Sequence[Alert],
    *,
    max_results: int,
    bullet_prefix: str,
) -> List[str]:
    lines: List[str] = []
    display = list(alerts[:max_results])
    remaining = max(0, len(alerts) - len(display))
    for alert in display:
        severity_note = alert.severity_suffix()
        lines.append(
            f"- {bullet_prefix} **{alert.level_title()}**{severity_note} {alert.rule_display()} at `{alert.location}` — {alert.message}"
        )
    if remaining:
        lines.append(f"- {bullet_prefix} …and {remaining} more alerts (see Code Scanning for the full list).")
    return lines


def build_comment(
    *,
    new_alerts: Sequence[Alert],
    fixed_alerts: Sequence[Alert],
    repo: Optional[str],
    max_results: int,
    threshold: str,
) -> str:
    lines: List[str] = []
    if new_alerts:
        lines.append(
            f"## ❌ {len(new_alerts)} new CodeQL alert{'s' if len(new_alerts) != 1 else ''} (level ≥ {threshold})"
        )
        lines.extend(_format_section(new_alerts, max_results=max_results, bullet_prefix=":x:"))
        lines.append("")

    if fixed_alerts:
        lines.append(
            f"## ✅ {len(fixed_alerts)} CodeQL alert{'s' if len(fixed_alerts) != 1 else ''} resolved since the previous run"
        )
        lines.extend(_format_section(fixed_alerts, max_results=max_results, bullet_prefix=":white_check_mark:"))
        lines.append("")

    if repo:
        code_scanning_url = f"https://github.com/{repo}/security/code-scanning"
        lines.append(f"Review the [full CodeQL report]({code_scanning_url}) for details.")
    else:
        lines.append("Review the CodeQL report in the Security tab for full details.")
    return "\n".join(lines).strip() + "\n"


def write_summary(
    *,
    new_alerts: Sequence[Alert],
    fixed_alerts: Sequence[Alert],
    max_results: int,
    threshold: str,
) -> None:
    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if not summary_path or (not new_alerts and not fixed_alerts):
        return
    with open(summary_path, "a", encoding="utf-8") as handle:
        handle.write("## CodeQL Alerts\n\n")
        if new_alerts:
            handle.write(
                f"❌ {len(new_alerts)} new alert{'s' if len(new_alerts) != 1 else ''} (level ≥ {threshold}).\n"
            )
            for line in _format_section(new_alerts, max_results=max_results, bullet_prefix=":x:"):
                handle.write(f"{line}\n")
            handle.write("\n")
        if fixed_alerts:
            handle.write(
                f"✅ {len(fixed_alerts)} alert{'s' if len(fixed_alerts) != 1 else ''} resolved since the previous run.\n"
            )
            for line in _format_section(
                fixed_alerts,
                max_results=max_results,
                bullet_prefix=":white_check_mark:",
            ):
                handle.write(f"{line}\n")
            handle.write("\n")


def set_outputs(
    *,
    new_alerts: Sequence[Alert],
    fixed_alerts: Sequence[Alert],
    comment_path: Optional[Path],
) -> None:
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


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    sarif = load_sarif(args.sarif)
    min_level = severity(args.min_level)
    buckets = collect_alerts(sarif, min_level=min_level)
    new_alerts = buckets.get("new", [])
    fixed_alerts = buckets.get("absent", [])

    if new_alerts or fixed_alerts:
        repo = os.environ.get("GITHUB_REPOSITORY")
        comment_body = build_comment(
            new_alerts=new_alerts,
            fixed_alerts=fixed_alerts,
            repo=repo,
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
