import json
import os
import re

def main():
    all_combinations = [
        "gcc/none", "gcc/asan", "gcc/tsan", "gcc/valgrind",
        "clang/none", "clang/asan", "clang/tsan", "clang/valgrind"
    ]
    default_excluded = ["clang/none", "clang/valgrind"]

    user_input = os.getenv("USER_INPUT", "")
    comment_body = os.getenv("COMMENT_BODY", "")
    event_name = os.getenv("GITHUB_EVENT_NAME")

    input_str = ""
    if event_name == "workflow_dispatch":
        input_str = user_input
    elif event_name == "issue_comment":
        match = re.match(r"^@phlexbot build\s*(.*)", comment_body)
        if match:
            input_str = match.group(1)

    tokens = [token for token in re.split(r"[\s,]+", input_str) if token]

    if not tokens:
        final_combinations = [combo for combo in all_combinations if combo not in default_excluded]
    else:
        is_additive = any(token == "all" or token.startswith("+") or token.startswith("-") for token in tokens)

        if is_additive:
            base_set = set(all_combinations if "all" in tokens else [combo for combo in all_combinations if combo not in default_excluded])

            for token in tokens:
                if token.startswith("+"):
                    base_set.add(token[1:])
                elif token.startswith("-"):
                    base_set.discard(token[1:])
            final_combinations = list(base_set)
        else:
            final_combinations = tokens

    matrix = {"include": []}
    for combo in sorted(list(set(final_combinations))):
        compiler, sanitizer = combo.split("/")
        matrix["include"].append({"compiler": compiler, "sanitizer": sanitizer})

    json_matrix = json.dumps(matrix)
    with open(os.environ["GITHUB_OUTPUT"], "a") as f:
        print(f"matrix={json_matrix}", file=f)

if __name__ == "__main__":
    main()
