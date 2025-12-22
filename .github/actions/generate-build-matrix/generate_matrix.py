"""Generates a build matrix for CI based on the trigger event and user input."""

import json
import os
import re


def get_default_combinations(event_name, all_combinations):
    """Gets the default build combinations based on the GitHub event type."""
    if event_name in ("push", "pull_request", "pull_request_target"):
        return ["gcc/none"]
    elif event_name == "issue_comment":
        return ["gcc/none", "clang/none"]
    elif event_name == "workflow_dispatch":
        return all_combinations
    else:
        # Default to a minimal safe configuration for unknown events
        return ["gcc/none"]


def main():
    """Generates and outputs the build matrix based on environment variables."""
    all_combinations = [
        "gcc/none",
        "gcc/asan",
        "gcc/tsan",
        "gcc/valgrind",
        "clang/none",
        "clang/asan",
        "clang/tsan",
        "clang/valgrind",
    ]
    user_input = os.getenv("USER_INPUT", "")
    comment_body = os.getenv("COMMENT_BODY", "")
    event_name = os.getenv("GITHUB_EVENT_NAME")

    default_combinations = get_default_combinations(event_name, all_combinations)

    input_str = user_input
    if event_name == "issue_comment":
        match = re.match(r"^@phlexbot build\s*(.*)", comment_body)
        if match:
            input_str = match.group(1).strip()

    tokens = [token for token in re.split(r"[\s,]+", input_str) if token]

    if not tokens:
        # Case 3: No input. Use the trigger-specific default.
        final_combinations = default_combinations
    else:
        # Check for explicit (non-modifier) combinations
        explicit_combos = {
            t for t in tokens if not (t.startswith("+") or t.startswith("-") or t == "all")
        }

        if explicit_combos:
            # Case 1: Explicit list. This forms the base set, ignoring defaults.
            base_set = explicit_combos
        else:
            # Case 2: Only modifiers. Determine base set from 'all' or defaults.
            if "all" in tokens:
                base_set = set(all_combinations)
            else:
                base_set = set(default_combinations)

        # Apply modifiers to the determined base set
        for token in tokens:
            if token.startswith("+"):
                base_set.add(token[1:])
            elif token.startswith("-"):
                base_set.discard(token[1:])

        final_combinations = list(base_set)

    matrix = {"include": []}
    for combo in sorted(set(final_combinations)):
        compiler, sanitizer = combo.split("/")
        matrix["include"].append({"compiler": compiler, "sanitizer": sanitizer})

    json_matrix = json.dumps(matrix)
    with open(os.environ["GITHUB_OUTPUT"], "a") as f:
        print(f"matrix={json_matrix}", file=f)


if __name__ == "__main__":
    main()
