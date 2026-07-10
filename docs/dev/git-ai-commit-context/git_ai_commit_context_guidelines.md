# Git‑AI‑Commit Context Guidelines

## Original prompt

Please evaluate the various sources of context to git-ai-commit for relevance/usefulness, and suggest adjustments to context inclusions or prompt wording to minimize context bloat while preserving or enhancing prompt effectiveness with various models and context window sizes.

## Evaluation of current context sources

| Context source | Typical size | Relevance to commit‑message generation | Typical usefulness | Comments (bloat risk) |
| --- | --- | --- | --- | --- |
| **Staged diff (`git diff --staged`)** | 0 – few KB | Highest – gives concrete code changes | essential | Can explode for large refactors; diff‑size‑based truncation is needed |
| **Full file contents of changed files** | 0 – several MB | Medium – helps model see surrounding code, definitions, imports | useful for non‑trivial changes (API tweaks, new types) | Heavy for binary files or huge generated sources |
| **Commit history (last N commits)** | 0 – few KB | Low‑medium – provides project‑wide style, recent terminology | useful for style consistency, detecting reused ticket IDs | Older history quickly becomes irrelevant; limit to “last 5” |
| **Branch name / PR number** | < 100 B | Low – can surface ticket ID, feature name | helpful for including ticket numbers, short context | negligible size |
| **Issue/PR description (GitHub API)** | 0 – few KB | Medium – clarifies intent, scope | valuable when present | Only fetch when `--issue` flag is used |
| **Project‑wide guidelines (README, CONTRIBUTING, `.gitmessage`, `kilo.json`)** | up to tens KB | Low‑medium – informs preferred tone, prefixes (e.g., “feat:”, “fix:”) | useful for enforcing conventions | Most of the file is static; cache and insert only once per session |
| **Language‑specific style configs (`.clang‑format`, `pyproject.toml`, `ruff.toml`)** | < 5 KB | Low – rarely needed for the message itself | seldom used | Can be omitted unless explicitly requested |
| **Recent CI status / test failures** | < 2 KB | Low – may affect urgency wording | occasional | Only useful for “WIP” or “hot‑fix” prompts |
| **User‑defined prompt template** | < 1 KB | High – drives tone, length, ticket format | critical | Fixed size; no bloat |

### Key observations

1. Diff is the single most valuable source.
2. Full file contents add marginal benefit and become a major source of bloat for large files.
3. Static project‑wide docs and style configs contribute almost no incremental information per commit; they can be cached once per session.
4. Branch/issue metadata is tiny but highly effective for ticket IDs.
5. Commit history beyond the last 3–5 entries rarely influences the current message.

---

## Recommendations to shrink context while keeping effectiveness

### 1. Diff‑size gating

- If staged diff > `MAX_DIFF_TOKENS` (≈ 2 k tokens / ~4 KB), truncate to the first `N` hunks (default 10) and add a note: “*diff truncated – see remaining changes in file X*”.
- Provide a `--full-diff` flag for rare cases where the full diff is required.

### 2. Conditional file‑content inclusion

- Include only the *closest* surrounding lines (± 5–10 lines) around each changed hunk, not the whole file.
- Skip binary or generated files (`.pb.cc`, `.pb.h`, `.gen.cpp`, etc.) unless the user explicitly asks.
- Use a file‑type whitelist (`.cpp`, `.hpp`, `.py`, `.jsonnet`, `.cmake`).

### 3. Cache static project context

- Load once per session: `README.md`, `CONTRIBUTING.md`, `kilo.json`, `.gitmessage`.
- Pass a reference token (e.g., `<<PROJECT_GUIDELINES>>`) that the model expands internally via a stored embedding or short summary.

### 4. Limit commit‑history window

- Default to **last 3 commits**; expose a `--history-depth=N` flag.
- Drop merges and revert commits unless they contain the same ticket ID.

### 5. Smart issue/PR pulling

- Only request issue/PR body when a ticket reference is detected in branch name or diff.
- Provide a short excerpt (first 200 chars) plus a “more” flag if the user wants the full description.

### 6. Prompt wording tweaks to reduce token waste

| Current wording (example) | Suggested wording |
| --- | --- |
| “Generate a conventional commit message for the following changes. Include a brief summary, a detailed body, and a footer with any relevant issue numbers.” | **“Write a conventional‑commit message for the diff below. Use a one‑line summary, optional body, and footer if an issue ID is present.”** |
| “Consider the project’s style guidelines, recent commit history, and the current branch name when forming the message.” | **“Follow the project’s conventional‑commit style. Use the branch name for any ticket ID.”** |
| “If the diff is large, you may truncate it but indicate that you omitted sections.” | **“When diff > 2 k tokens, keep the first 10 hunks and add ‘…truncated’.”** |

- Remove filler words (e.g., “please”, “kindly”).
- Use concrete limits instead of vague adjectives.

### 7. Model‑specific context windows

| Model size | Max diff tokens + surrounding lines | History depth |
| --- | --- | --- |
| 4 k tokens (gpt‑3.5‑turbo) | ≤ 1 k diff tokens (≈ 2 KB) | last 2 commits |
| 8 k tokens (claude‑2.1) | ≤ 2 k diff tokens | last 3 commits |
| 16 k tokens (gpt‑4‑32k) | ≤ 4 k diff tokens | last 5 commits |
| 32 k tokens+ (Claude‑3‑Sonnet) | ≤ 6 k diff tokens | last 7 commits |

- Expose a `--max‑tokens` option that defaults based on detected model.

## Concrete adjustment plan for `git‑ai‑commit`

1. Add a “context profile” enum (`tiny`, `standard`, `full`).
2. Implement diff‑truncation routine that keeps first `MAX_HUNKS` (default 10) or until token budget is hit, appending `…diff truncated…`.
3. Cache static docs in `.git‑ai‑commit‑cache.json` on first run; reuse on subsequent invocations.
4. Make issue/PR lookup lazy – only if a pattern like `([A-Z]+‑\d+)` appears.
5. Refine the default prompt template (see wording table) and allow user‑override via `--prompt‑template`.
6. Add CLI flags: `--diff-lines N`, `--history-depth N`, `--include-files PATTERN`, `--exclude‑binary`, `--max‑tokens N`.
7. Document model‑aware defaults in the README and `kilo.json` command config.

### Minimal‑bloat default prompt (example)

```text
Write a conventional‑commit message for the staged changes below.
Use a one‑line summary (≤ 50 chars). Add a body only if the diff
contains multiple logical parts. Include a footer with the ticket
ID if the branch name contains one (e.g. feat/1234‑...).

<DIFF>
```

All other context (project guidelines, recent commits, full issue body) is omitted unless the corresponding flags are supplied.

---

### Functional conformance check (summary)

- The diff remains the primary source; truncation only adds a clear marker, preserving spec compliance.
- Ticket extraction, optional body, and footer handling continue to follow Conventional Commits 1.0.
- Style‑guide caching does not affect the required format.
- Prompt wording explicitly demands a **conventional‑commit** header, which is exactly what the spec requires.
- Therefore the suggested changes maintain full functional conformance while reducing token usage.

---

### Implementation status

The following table records the disposition of each recommendation from this
document as implemented in `scripts/git-ai-commit`.

| # | Recommendation | Disposition | Notes |
| --- | --- | --- | --- |
| 1 | Hunk-aware diff truncation | Implemented | `_truncate_diff` preserves whole file sections and falls back to line-boundary cut; omission marker includes file counts. |
| 2 | Two-tier context caps | Implemented | `_MAX_DIFF_CHARS_DEFAULT = 60 000` (default); `_MAX_DIFF_CHARS_ESCALATED = 400 000` (escalated path). |
| 3 | Rewrite escalation to use raw length | Implemented | `raw_len` computed from untruncated inputs; replaces post-truncation `sum(len(m["content"]))` heuristic. |
| 4 | Binary and generated-file skipping | Implemented (partial) | Binary sections and files matching `_GENERATED_FILE_GLOBS` are replaced with a one-line note. Glob list is deliberately minimal; extend as needed. |
| 5 | Reduce commit-history depth | Implemented | `_recent_log` depth reduced from 10 to 5. No `--history-depth` flag added (kept simple). |
| 6 | Trim `_SYSTEM` prompt filler | Implemented (partial) | Removed one redundant filler sentence; essential Conventional Commits rules and the "message only" constraint preserved. |
| 7 | Reject table-driven model routing | Rejected | Auto-escalation threshold retained as-is; two-tier caps provide the necessary headroom without adding routing complexity. |

### Implementation status (subsequent fixes)

| # | Recommendation | Disposition | Notes |
| --- | --- | --- | --- |
| 8 | Model-aware provider selection | Implemented | `_determine_kilo_provider()` routes open-weight models (qwen/*, google/*, nvidia/*, BAAI/*) to fnal-ow provider (no cost) and Azure models to fnal-azure. Default model changed from azure/claude-haiku-4-5 to qwen/qwen3-coder-next. |

---

*Generated by the Kilo `git‑ai‑commit` context‑optimization work.*

After budget exhaustion fix (2026-07-10): updated default model and added model-aware provider selection to route open-weight models to the unlimited-budget fnal-ow provider, avoiding HTTP 400 budget exceeded errors.
