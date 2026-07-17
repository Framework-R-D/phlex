# git-ai-commit context-optimization plan

## Goal

Implement the applicable recommendations from
`git_ai_commit_context_guidelines.md` to maximize commit-message
quality while minimizing unnecessary prompt/context bloat in
`scripts/git-ai-commit`. Treat the report as an idea source, not a spec: the
tool already implements several recommendations and diverges from the report's
generic assumptions (character-based budgets, not tokens; single FNAL
deployment; no full-file inclusion; no doc cache; no issue/PR fetch).

## Authoritative facts (verified)

- Tool: `scripts/git-ai-commit` (Python, git-subcommand convention). This repo
  copy is authoritative; it is symlinked onto PATH in
  `.devcontainer/post-create.sh:57`. The separate personal copy at
  `/home/greenc/scripts/git-ai-commit` is **out of scope**.
- Tests: `scripts/test/test_git_ai_commit.py`, loaded via `SourceFileLoader`.
  Run in CI by `.github/workflows/python-check.yaml:162` and
  `.github/workflows/coverage.yaml:272` (`pytest scripts/test/`).
- Man page: `scripts/man/man1/git-ai-commit.1` (266 lines) — must stay in sync
  with CLI/behavior changes.
- Diff source: `_staged_diff` returns `git diff --cached --stat -p` output, so
  the string begins with a diffstat block, then per-file `diff --git` hunks.
- Current truncation: `_build_messages` (line ~671) slices the raw diff at
  `_MAX_DIFF_CHARS = 60_000` mid-line and appends
  `[diff truncated — too large to show in full]`.
- Current escalation: in `main()` (line ~915), if `backend == "kilo"`, model
  not explicit, not pinned by env, and summed message length >
  `_ESCALATION_THRESHOLD_CHARS = 30_000`, switch model to
  `_ESCALATION_MODEL_KILO = "qwen/qwen3-coder-next"`. Bug: this sums the
  **already-truncated** message content, and truncation runs unconditionally at
  60k regardless of the resolved (escalated) model.
- No full-file content inclusion, no `.git-ai-commit-cache.json`, no GitHub
  issue/PR fetch, no style-config reading exist today.

## Resolved decisions

1. **Scope:** selective/pragmatic. Implement report items that improve message
   quality or reduce genuine bloat; explicitly reject the rest with rationale.
2. **Truncation:** escalate-first, truncate-rarely. When escalation triggers
   (or a high-context model is otherwise in use), raise/disable the char cap so
   the model sees the full diff; on the non-escalated (default Haiku) path keep
   a cap and cut on **hunk boundaries** with a clear marker.
3. **Escalation ordering:** two-tier cap driven by the resolved model.
   `main()` decides escalation from the **raw (pre-truncation)** prompt length,
   then passes a model-appropriate cap into `_build_messages`.
4. **History window:** reduce the default `git log` depth (report item 4).
5. **Prompt wording:** only trims that lose no Conventional Commits conformance
   (report item 6).

## Report item disposition (record in the report doc)

| # | Report recommendation | Disposition | Rationale |
| --- | --- | --- | --- |
| 1 | Diff-size gating, hunk truncation, `--full-diff` | Implement (adapted) | Char-based, no tokenizer dep; hunk-aware cut + escalation-aware cap |
| 2 | Conditional file content; skip binary/generated; whitelist | Partial | Tool never includes full files; add binary/generated-file diff skipping |
| 3 | Cache static docs in `.git-ai-commit-cache.json` | Reject | Per-invocation CLI, no session; cache adds staleness + bloat; AGENTS.md dedup already limits volume |
| 4 | Limit commit-history window (`--history-depth`, default 3) | Implement | Cheap, direct bloat reduction |
| 5 | Smart issue/PR pulling | Reject | Feature absent; adds network + bloat for marginal gain |
| 6 | Prompt-wording tweaks | Partial | Trim only where conformance preserved |
| 7 | Model-window table / `--max-tokens` | Reject (table); tune threshold only | Single FNAL deployment; full table is overkill and bloats config |

## Implementation tasks (ordered)

1. **Add a hunk-aware diff-truncation helper.**
   - New function (e.g. `_truncate_diff(diff: str, max_chars: int) -> str`).
   - Preserve the leading `--stat` block, then include whole `diff --git`
     file sections until the budget is reached; drop remaining files and append
     a clear marker naming how many files/sections were omitted, e.g.
     `\n\n[diff truncated: N of M files omitted to fit context budget]`.
   - If a single file section alone exceeds the budget, fall back to a
     line-boundary cut within that section (never mid-line) plus the marker.
   - Return the diff unchanged when under budget.

2. **Introduce a two-tier character cap.**
   - Replace the single `_MAX_DIFF_CHARS` with two constants, e.g.
     `_MAX_DIFF_CHARS_DEFAULT` (keep ~60_000 for the Haiku path) and
     `_MAX_DIFF_CHARS_ESCALATED` (large ceiling matched to Qwen's practical
     context, or effectively "no truncation" safety net).
   - Keep values as module constants with explanatory comments.

3. **Make escalation decision use raw (pre-truncation) length.**
   - In `main()`, compute the escalation decision from the untruncated diff +
     instructions + context length **before** truncation is applied.
   - Preserve existing guards: `backend == "kilo"`, `not model_explicit`,
     `not _KILO_MODEL_PINNED_BY_ENV`, threshold `_ESCALATION_THRESHOLD_CHARS`.
   - Keep the existing stderr escalation notice.

4. **Pass the resolved cap into `_build_messages`.**
   - Add a `max_diff_chars` parameter to `_build_messages` (default preserves
     current behavior for direct/test callers).
   - `main()` selects `_MAX_DIFF_CHARS_ESCALATED` when the resolved model is the
     escalation model or a user-pinned/explicit high-context model; otherwise
     `_MAX_DIFF_CHARS_DEFAULT`.
   - `_build_messages` calls `_truncate_diff(diff, max_diff_chars)` instead of
     the inline slice.

5. **Skip binary/generated files in the diff (report item 2).**
   - Filter or exclude binary-file sections (git marks these
     `Binary files ... differ`) and known generated patterns
     (`*.pb.cc`, `*.pb.h`, `*.gen.cpp`, etc.) from the diff before prompting,
     replacing each with a one-line note so the model knows a file changed.
   - Keep the exclusion list as a small, commented constant. Confirm no
     conflict with `--stat` (the stat line for skipped files may remain).

6. **Reduce default commit-history depth (report item 4).**
   - Change `_recent_log` from `-10` to a smaller default (recommended `-5`;
     report suggested 3). Update the man page and the docstring reference to
     `git log --oneline -10`.
   - Optional (only if low-risk): add a `--history-depth N` flag threaded into
     `_recent_log`. If added, update argparse, help text, docstring, and man
     page. If omitted, note it as a deliberate simplification.

7. **Tighten `_SYSTEM` wording where conformance is preserved (report item 6).**
   - Remove redundant/filler phrasing only. Do not weaken the Conventional
     Commits v1.0.0 rules, the "respond with only the message" constraint, or
     the subject/body/footer formatting requirements.
   - Keep the change minimal; the existing prompt is already close to the
     report's suggested minimal-bloat form.

8. **Update tests (`scripts/test/test_git_ai_commit.py`).**
   - New tests for `_truncate_diff`: under budget returns unchanged; over budget
     cuts on file/hunk boundary and keeps the `--stat` header; single oversized
     file falls back to line-boundary cut; marker text present.
   - New tests for escalation ordering: a large raw diff triggers escalation
     using pre-truncation length, and the escalated path receives the larger cap
     (assert via `_build_messages` output length / marker absence).
   - New tests for binary/generated-file skipping.
   - Update any existing tests that assumed the old single 60k cap or the old
     inline marker string. Keep `_build_messages` default-arg behavior so
     existing signature-based tests still pass where feasible.

9. **Update documentation.**
   - `scripts/man/man1/git-ai-commit.1`: reflect new truncation behavior,
     reduced history depth, any new flag, and binary/generated skipping.
   - Script module docstring (top of `scripts/git-ai-commit`): sync any option
     or default changes.
   - `git_ai_commit_context_guidelines.md`: append a short
     "Implementation status" section recording the disposition table above
     (done / partial / rejected + rationale) so the report reflects reality.

## Validation

- `pytest scripts/test/ -v` (matches CI in `python-check.yaml` and
  `coverage.yaml`). All existing tests must still pass.
- `ruff` per `pyproject.toml` (99-char limit, double quotes) on the edited
  script and tests.
- Manual dry-run checks (`git ai-commit --dry-run`) on:
  - a small diff (no truncation, default model, no escalation),
  - a synthetic large diff (> escalation threshold) confirming escalation fires
    **and** the full diff is sent (no truncation marker on the escalated path),
  - a diff containing a binary and a generated file (confirm they are skipped
    with a note).
- `PREKCOMMAND=$(command -v prek || command -v pre-commit)`; run
  `$PREKCOMMAND run --all-files` before opening a PR.

## Risks / notes

- The `--stat -p` header interacts with hunk truncation: keep the stat block
  intact so the model still sees the full file list even when hunks are cut.
- Escalation currently measures summed message content; moving the decision to
  raw pre-truncation length must not double-count or regress the existing
  env-pin / explicit-model suppression logic.
- File-formatting rules (global AGENTS.md): exactly one trailing newline, no
  trailing whitespace.
- Implementation requires source edits and running tests — switch to an
  implementation-capable agent to execute this plan.

## Deliberately out of scope

- Token-based budgeting / tokenizer dependency (kept character-based).
- `.git-ai-commit-cache.json` session cache.
- GitHub issue/PR fetching.
- Per-model context-window table / `--max-tokens`.
- Reading language style configs (`.clang-format`, `pyproject.toml`, etc.).
- The personal copy at `/home/greenc/scripts/git-ai-commit`.
