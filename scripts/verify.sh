#!/bin/bash

set -euo pipefail

usage() {
    printf '%s\n' \
        'Usage: bash scripts/verify.sh (--project NAME | --build-dir DIR) [--tests REGEX] [--verbose]' \
        'Build a configured project, list the selected CTests, and run them fail closed.' \
        'Omit --tests to run the entire suite. Initialize the build environment first.'
}

project=""
build_dir=""
pattern=""
verbose=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --project|--build-dir|--tests)
            if [[ $# -lt 2 || -z "$2" || "$2" == --* ]]; then
                printf 'Missing value for %s\n' "$1" >&2
                exit 2
            fi
            case "$1" in
                --project) project="$2" ;;
                --build-dir) build_dir="$2" ;;
                --tests) pattern="$2" ;;
            esac
            shift 2
            ;;
        --verbose)
            verbose=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown argument: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ -z "$project" && -z "$build_dir" || -n "$project" && -n "$build_dir" ]]; then
    usage >&2
    exit 2
fi

if [[ -n "$project" ]]; then
    build_dir="$(spack mpd info --build "$project")"
    if [[ ! -d "$build_dir" ]]; then
        printf 'MPD build directory does not exist: %s\n' "$build_dir" >&2
        exit 1
    fi
    spack mpd select --project "$project"
    spack mpd build
else
    cmake --build "$build_dir" --parallel
fi

if [[ -n "$project" ]]; then
    test_command=(spack mpd test --)
    selection=()
else
    test_command=(ctest)
    selection=(--test-dir "$build_dir")
fi
if [[ -n "$pattern" ]]; then
    selection+=(-R "$pattern")
fi
"${test_command[@]}" "${selection[@]}" -N
if [[ "$verbose" == 1 ]]; then
    selection+=(-V)
fi
"${test_command[@]}" "${selection[@]}" --parallel --output-on-failure --no-tests=error
