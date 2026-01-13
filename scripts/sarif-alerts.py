"""A simple tool to print SARIF results from one or more SARIF files."""

import argparse
import json
from pathlib import Path
from typing import Iterable


def _process_sarif(path: Path) -> Iterable[str]:
    text = path.read_text(encoding="utf-8")
    sarif = json.loads(text)
    for run in sarif.get("runs", []):
        for result in run.get("results", []):
            rule = result.get("ruleId", "<no rule>")
            level = result.get("level", "info")
            baseline = result.get("baselineState", "unchanged")
            message = (result.get("message") or {}).get("text") or ""
            loc = "(unknown location)"
            for location in result.get("locations") or []:
                phys = location.get("physicalLocation") or {}
                uri = phys.get("artifactLocation", {}).get("uri")
                region = phys.get("region") or {}
                line = region.get("startLine")
                if uri:
                    loc = f"{uri}:{line}" if line else uri
                    break
            yield f"{rule} [{level}/{baseline}] {loc} — {message}"


def main(argv=None) -> int:
    """The main entry point of the script.

    Args:
        argv: The command-line arguments.

    Returns:
        The exit code.
    """
    parser = argparse.ArgumentParser(
        description="Print SARIF results from one or more SARIF files"
    )
    parser.add_argument("files", nargs="+", help="Path(s) to SARIF file(s) to inspect")
    args = parser.parse_args(argv)

    total = 0
    for file in args.files:
        path = Path(file)
        if not path.exists():
            print(f"Skipping missing file: {path}", file=sys.stderr)
            continue
        print(f"== {path} ==")
        for line in _process_sarif(path):
            print(line)
            total += 1
    print(f"Total alerts across files: {total}")
    return 0


if __name__ == "__main__":
    import sys

    sys.exit(main())
