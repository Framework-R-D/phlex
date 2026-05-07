"""A simple tool to print SARIF results from one or more SARIF files or directories."""

import argparse
import json
import sys
from collections.abc import Iterator
from pathlib import Path

# Levels defined by the SARIF 2.1.0 spec (§3.27.10), ordered by severity.
_LEVEL_ORDER = {"none": 0, "note": 1, "warning": 2, "error": 3}


def _collect_sarif_paths(inputs: list[Path]) -> list[Path]:
    """Expand any directory entries to the .sarif files they contain.

    Files are returned in a stable, sorted order so output is reproducible.
    """
    paths: list[Path] = []
    for p in inputs:
        if p.is_dir():
            found = sorted(p.rglob("*.sarif"), key=str)
            if not found:
                print(f"Warning: no .sarif files found under {p}", file=sys.stderr)
            paths.extend(found)
        else:
            paths.append(p)
    return paths


def _process_sarif(
    path: Path,
    *,
    min_level: str = "none",
    baseline_filter: set[str] | None = None,
    max_message: int = 200,
) -> Iterator[str]:
    """Yield one formatted line per result in the SARIF file at *path*.

    Args:
        path: Path to a single .sarif file.
        min_level: Skip results whose level is below this threshold.
        baseline_filter: If given, only yield results whose baselineState is in
            this set (e.g. ``{"new"}``).  ``None`` means no filter.
        max_message: Truncate the message text to this many characters.

    Yields:
        One human-readable string per matching result.

    Raises:
        OSError: If the file cannot be read.
        ValueError: If the file is not valid JSON or not a SARIF document.
    """
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        raise OSError(f"Cannot read {path}: {exc}") from exc

    try:
        sarif = json.loads(text)
    except json.JSONDecodeError as exc:
        raise ValueError(f"Invalid JSON in {path}: {exc}") from exc

    if not isinstance(sarif, dict):
        raise ValueError(f"Not a SARIF document (expected JSON object): {path}")

    min_order = _LEVEL_ORDER.get(min_level, 0)

    for run in sarif.get("runs") or []:
        for result in run.get("results") or []:
            level = (result.get("level") or "none").lower()
            if _LEVEL_ORDER.get(level, 0) < min_order:
                continue

            baseline = (result.get("baselineState") or "unchanged").lower()
            if baseline_filter is not None and baseline not in baseline_filter:
                continue

            rule = result.get("ruleId") or "<no rule>"
            message = ((result.get("message") or {}).get("text") or "").strip()
            # Collapse whitespace and truncate for terminal readability
            message = " ".join(message.split())
            if len(message) > max_message:
                message = message[: max_message - 1] + "…"

            loc = "(unknown location)"
            for location in result.get("locations") or []:
                phys = location.get("physicalLocation") or {}
                uri = (phys.get("artifactLocation") or {}).get("uri")
                region = phys.get("region") or {}
                line = region.get("startLine")
                if uri:
                    loc = f"{uri}:{line}" if line else uri
                    break

            yield f"{rule} [{level}/{baseline}] {loc} — {message}"


def _positive_int(value: str) -> int:
    """Argparse type for --max-message: reject values less than 1."""
    n = int(value)
    if n < 1:
        raise argparse.ArgumentTypeError(f"N must be at least 1, got {n}")
    return n


def main(argv: list[str] | None = None) -> int:
    """Entry point for the sarif-alerts tool.

    Args:
        argv: Command-line arguments (defaults to sys.argv[1:]).

    Returns:
        0 on success, 1 if any input file could not be processed.
    """
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "files",
        nargs="+",
        metavar="FILE_OR_DIR",
        help="Path(s) to SARIF file(s) or directories containing .sarif files",
    )
    parser.add_argument(
        "--level",
        default="none",
        choices=list(_LEVEL_ORDER),
        metavar="LEVEL",
        help=(
            "Minimum severity level to display "
            f"({', '.join(sorted(_LEVEL_ORDER, key=lambda k: _LEVEL_ORDER[k]))}). "
            "Default: none (show all)."
        ),
    )
    parser.add_argument(
        "--baseline",
        action="append",
        dest="baselines",
        metavar="STATE",
        help=(
            "Only show results with this baselineState "
            "(new, absent, unchanged, updated). "
            "May be repeated. Default: show all states."
        ),
    )
    parser.add_argument(
        "--max-message",
        type=_positive_int,
        default=200,
        metavar="N",
        help="Truncate result messages to N characters (default: 200, minimum: 1).",
    )
    args = parser.parse_args(argv)

    baseline_filter: set[str] | None = (
        {b.lower() for b in args.baselines} if args.baselines else None
    )

    paths = _collect_sarif_paths([Path(f) for f in args.files])

    total = 0
    errors = 0
    for path in paths:
        if not path.exists():
            print(f"Error: file not found: {path}", file=sys.stderr)
            errors += 1
            continue
        print(f"== {path} ==")
        try:
            for line in _process_sarif(
                path,
                min_level=args.level,
                baseline_filter=baseline_filter,
                max_message=args.max_message,
            ):
                print(line)
                total += 1
        except (OSError, ValueError) as exc:
            print(f"Error processing {path}: {exc}", file=sys.stderr)
            errors += 1

    print(f"Total alerts: {total}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
