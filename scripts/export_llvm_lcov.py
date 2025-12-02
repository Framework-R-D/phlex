#!/usr/bin/env python3
"""Export LLVM LCOV."""
from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys

"""Helper to capture llvm-cov export output into an LCOV file."""


def build_parser() -> argparse.ArgumentParser:
    """Build the command-line argument parser."""
    parser = argparse.ArgumentParser(
        description="Run llvm-cov export and write the LCOV data to a file."
    )
    parser.add_argument("output", help="Destination LCOV file path")
    parser.add_argument(
        "llvm_cov",
        help="Path to the llvm-cov executable to invoke",
    )
    parser.add_argument(
        "llvm_cov_args",
        nargs=argparse.REMAINDER,
        help="Additional arguments to pass to llvm-cov export",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    """Main entry point for the script."""
    parser = build_parser()
    args = parser.parse_args(argv)

    if not args.llvm_cov_args:
        parser.error("Missing llvm-cov arguments")

    output_path = pathlib.Path(args.output)
    command = [args.llvm_cov] + args.llvm_cov_args

    output_path.parent.mkdir(parents=True, exist_ok=True)

    try:
        with output_path.open("w", encoding="utf-8") as handle:
            subprocess.run(command, check=True, stdout=handle, text=True)
    except subprocess.CalledProcessError as error:
        return error.returncode

    return 0


if __name__ == "__main__":
    sys.exit(main())
