# Generating & Executing Kilo Plans

## Purpose

Explain how to produce a structured, machine-readable JSON execution plan with the **genplan** agent and how to run that plan with the **execute** orchestrator. The process supports idempotent re‑runs, explicit model selection, built‑in error handling, and in‑flight re‑planning.

> **Why `genplan`, not `plan`?** `plan` is a **reserved native agent name** in the
> Kilo CLI: it is hard-wired to the built-in Markdown "Plan mode", which researches
> the goal, asks you to "Finalize and save the plan" or "Continue refining", and
> writes a human-readable Markdown plan to `.kilo/plans/*.md`. That native behavior
> overrides a custom agent's system prompt and permissions **by name**, so a custom
> agent named `plan` cannot emit JSON. The JSON planner is therefore named
> `genplan`. Other reserved names to avoid for custom agents: `build`, `code`,
> `ask`, `architect`, `explore`, `general`, `orchestrator`, `debug`, `scout`.

## 1. Generate a plan

1. **Define the goal** – a short, high‑level statement (e.g. "Add a dark‑mode toggle to the web UI").
2. **Invoke the planner** with `kilo run`. The `genplan` agent is configured with
   `mode: all`, so it can be selected as a primary agent directly. The prompt is
   passed as a positional argument (there is no `--prompt` option), and
   `--title` optionally names the session (it defaults to a truncated prompt):

   ```bash
   kilo run --agent genplan --title "Generate plan for <goal>" "<goal>"
   ```

   Replace `<goal>` with your actual objective.
3. The planner returns JSON following the schema in `agent/genplan.md`. Key fields:
   * `goal` – original request
   * `idempotent` – true if the whole plan may be safely re‑run
   * `steps` – ordered array where each step contains:
     * `id` – unique identifier (e.g. `step-1`)
     * `description`
     * `model` – any builtin agent shortcut (`reason`, `ask`, `chat`, `code`, `coder-qwen`) **or** a full `fnal-litellm` provider/model identifier such as `fnal-litellm/openai/gpt-oss-120b` or `fnal-litellm/azure/claude-opus-4-8`. Only `fnal-litellm` provider models are permitted.
     * `input` / `output`
     * `gate`, `parallel`, `retry`, `max_retries`, `on_error`, `self_check`

## 2. Example output

```json
{
  "goal": "Add a dark‑mode toggle to the web UI",
  "idempotent": true,
  "steps": [
    {
      "id": "step-1",
      "description": "Create design spec for the toggle component",
      "model": "reason",
      "input": null,
      "output": "design-spec.md",
      "gate": false,
      "parallel": false,
      "retry": 0,
      "max_retries": 2,
      "on_error": "abort",
      "self_check": "file_exists('design-spec.md')"
    },
    {
      "id": "step-2",
      "description": "Implement the Toggle React component",
      "model": "coder-qwen",
      "input": "design-spec.md",
      "output": "src/components/Toggle.jsx",
      "gate": false,
      "parallel": false,
      "retry": 0,
      "max_retries": 3,
      "on_error": "replan",
      "self_check": "eslint_passes('src/components/Toggle.jsx')"
    }
  ]
}
```

## 3. Execute the plan

Use the orchestrator (the primary `execute` agent) to consume the JSON. As with
the planner, the message is a positional argument and `--title` is optional:

```bash
kilo run --agent execute --title "Execute generated plan" "$(cat plan.json)"
```

The orchestrator:

* Reads each step in order.
* Calls the specified model/sub‑agent (`reason`, `code`, `ask`, `chat`, `coder-qwen`, or `genplan`).
* Runs `self_check` after completion.
* Honors `gate` (manual approval), `parallel` (concurrent execution), and retry logic.
* Stops, continues, falls back, or re‑plans based on `on_error`.

Because the plan is marked `idempotent`, you may safely re‑run the whole plan—or only the failing steps—without side‑effects, provided the underlying actions themselves are idempotent (e.g., use upserts, check‑before‑write).

### In‑flight re‑planning

A plan is not frozen once execution begins. When a step declares
`on_error: replan` and fails after exhausting `max_retries`, the orchestrator
suspends execution and re‑invokes the `genplan` agent as a sub‑agent to revise the
*remaining* work, using the accumulated outputs and the failure context. This is
what the `mode: all` setting on the `genplan` agent enables: the same agent serves
both as the directly‑invocable planner (section 1) and as the re‑planning
sub‑agent dispatched by `execute`.

Re‑planning preserves already‑completed steps verbatim so downstream
dependencies stay satisfied; the planner emits only new or replacement steps
with fresh, non‑colliding `id`s. The orchestrator records each re‑planning event
(the triggering step, the reason, and the new step ids) in its final JSON report
so the revision history is auditable.

For a human‑in‑the‑loop revision, pause at a `gate: true` step, run
`kilo run --agent genplan "<revised goal>"` to produce an updated plan, and hand the
result back to a fresh `execute` run.

## 4. Model‑selection policy

* Prefer builtin agent shortcuts for simplicity.
* For performance‑critical or accuracy‑critical tasks, specify an explicit `fnal-litellm` model (e.g. `fnal-litellm/azure/claude-opus-4-8`, `fnal-litellm/azure/gpt_5.3-codex`).
* Only `fnal-litellm` provider models are permitted; the `genplan` and `execute` agents reject models from other providers.
* The planner defaults to the free/open‑weight model (`fnal-litellm/openai/gpt-oss-120b`) unless a higher‑capability model is explicitly required.

### Planner vs orchestrator model choice

The two agents have different demands, and observed runs justify different
defaults:

* **`genplan` (planning)** decomposes a goal into JSON in essentially one shot.
  The free/open‑weight `fnal-litellm/openai/gpt-oss-120b` handles this well and
  remains the default — planning is a low‑tool‑count, one‑shot task, and plan
  defects are cheap to catch before they do harm (a `gate: true` step invites
  human review, and `on_error: replan` lets the plan self‑correct during
  execution). A run of `genplan` on `fnal-litellm/azure/claude-opus-4-8`
  produced a correct plan but was measurable overkill: it engaged **zero**
  reasoning tokens, confirming the task did not stress a frontier model. Keep
  `gpt-oss-120b` as the planner default and escalate only on observed
  inadequacy.

  Practical complexity ceiling for the default planner:

  * **`gpt-oss-120b` is sufficient** when the goal is backed by a written
    spec/markdown plan to transcribe, or decomposes into roughly ≤ 15 mostly
    linear steps with clear inputs/outputs (the git‑ai‑commit optimization plan
    is a representative example — 11 steps, all delegated to `coder-qwen`).
  * **Consider `fnal-litellm/azure/claude-sonnet-4-6`** for larger DAGs (~15–30
    steps) with non‑trivial cross‑step dependencies, parallelization, or gating
    that the plan itself must reason about.
  * **Reserve `fnal-litellm/azure/claude-opus-4-8`** for the rare case where
    planning is itself hard reasoning — many interacting constraints with **no**
    authoritative spec to transcribe, so the planner must derive the design.
    This did not apply to the git‑ai‑commit plan.
* **`execute` (orchestration)** must dispatch substantial steps to a sub‑agent
  via the `Task` tool, track dependencies and gates, apply trivial edits itself
  only when spawning a sub‑agent would cost more than the edit, and recover when
  a sub‑agent runs short of its own budget. Weaker models fail this in the
  opposite direction: they "help" by implementing whole steps directly, which
  burns the orchestrator's step budget and causes premature step‑limit
  exhaustion. Observed directly here: the same 11‑step plan run headless on
  `gpt-oss-120b` stalled at ~step 6 (it edited files itself instead of
  delegating), whereas a forked continuation on
  `fnal-litellm/azure/claude-sonnet-4-6` completed all steps — delegating the
  test‑ and doc‑authoring, applying the one‑line edits inline, and debugging a
  sub‑agent that had run out of steps. For that reason the orchestrator defaults
  to `claude-sonnet-4-6`. Reserve `fnal-litellm/azure/claude-opus-4-8` for
  unusually large or tangled plans where Sonnet‑level coordination is
  insufficient; it is roughly 5× the per‑token cost and rarely necessary just to
  orchestrate.
* The per‑step `model` fields stay on the cheapest capable option
  (`coder-qwen` for code) regardless of which model runs the orchestrator — the
  orchestrator's model governs coordination, not the implementation work.

## 5. Verify with `prek`

After creating or editing the guide, run the pre‑commit hook to ensure compliance:

```bash
prek run --all-files
```

`prek` checks for trailing whitespace, multiple final newlines, and basic markdown linting. This file follows those rules out‑of‑the‑box.

---

## 6. Resuming an interrupted execution

Orchestration runs can stop before the plan is complete. The most common cause
is **step‑limit exhaustion**: the `execute` agent has a finite per‑session step
budget (`steps:` in `agent/execute.md`), and when it is reached the agent emits
a partial summary and stops. Other causes are an `on_error: abort` step, a
`gate: true` checkpoint awaiting human review, or a manual interrupt. In every
case the orchestrator should leave an auditable resume point naming the last
completed step id and the next pending step id.

There are two ways to resume, and the correct choice depends on the plan's
`idempotent` flag.

### Option A — continue the interrupted session (preferred)

Continuing replays the existing transcript, so the orchestrator already knows
which steps completed regardless of the `idempotent` flag. Use `--continue`
(`-c`) for the **most recent** session, or `--session <id>` to target a
**specific** session by id (list ids with `kilo session`):

```bash
# resume the most recent session
kilo run --agent execute -c "continue executing the remaining plan steps"

# or resume a specific session by id
kilo run --agent execute --session ses_XXXXXXXX "continue executing the remaining plan steps"
```

Add `--fork` to branch a new session from the interrupted one (leaving the
original transcript untouched) when you want to retry without overwriting the
original log. This is the recommended path when the plan is `idempotent: false`,
because re‑running completed steps in a fresh session could repeat
non‑idempotent side effects.

### Option B — start a fresh session

Only safe when the plan is `idempotent: true`, **or** when you first trim the
JSON plan to the not‑yet‑completed steps (and re‑point their `input`
dependencies at artifacts that already exist on disk):

```bash
kilo run --agent execute --title "Resume execution" "$(cat plan-remaining.json)"
```

For an `idempotent: true` plan you may pass the original plan unchanged; the
orchestrator treats steps whose `id` matches a completed step as done via
step‑ID matching. For an `idempotent: false` plan (such as the git‑ai‑commit
optimization plan, where edits are not safe to re‑apply), do **not** re‑submit
the full original plan to a fresh session — either continue the session
(Option A) or hand the orchestrator a plan containing only the remaining steps.

### Resuming across a `gate`

When execution paused at a `gate: true` step, review the gated artifact, then
resume with Option A (`--continue`/`--session`) and instruct the orchestrator to
proceed past the approved gate. If the review requires plan changes, run
`kilo run --agent genplan "<revised goal + completed step ids + failure context>"`
to obtain a revised plan and feed that to a fresh `execute` run.

Both approaches respect `on_error: replan`: if a step fails again, the planner
is invoked to produce a revised plan. For quick iteration `--continue` is most
convenient; a fresh session on a trimmed remaining‑steps plan gives a clean log.

## 7. Interactive vs non‑interactive invocation

`kilo run` has two operating modes, selected by the `--interactive`/`-i` flag.

* **Non‑interactive (default):** `kilo run --agent genplan …` or
  `kilo run --agent execute …` runs headless. The agent receives the full
  prompt, performs its work, and returns a single result. Permission prompts
  block the run unless you pass `--auto` (auto‑approve all permissions) or
  `--dangerously-skip-permissions` (auto‑approve only permissions that are not
  explicitly denied). Use this mode for pipelines and for the normal
  generate‑then‑execute flow.
* **Interactive (`-i`):** switches to the split‑footer TUI, where you converse
  with the agent turn by turn, can steer or correct it, and approve permission
  prompts as they arise. This does **not** insert an automatic pause after every
  reasoning step; it simply keeps you in the loop so you can intervene between
  turns. Use it when the plan touches ambiguous code, when you want to watch a
  large diff before an escalation, or when debugging why the orchestrator made
  an unexpected choice. `--replay` (optionally `--replay-limit N`) reprints prior
  session history when resuming interactively.

### Pros and cons

* Non‑interactive is faster and scriptable but gives no chance to course‑correct
  mid‑run; a bad early decision runs to completion or to the step limit.
* Interactive is slower and needs a human present but catches wrong model
  choices, runaway edits, or context‑size problems before they consume the
  budget.

### Switching modes when problems occur

* If a headless `execute` run stalls, loops, or picks an unexpected model,
  resume it interactively: `kilo run --agent execute -i -c "…"` (or with
  `--session <id>`), watch the next few turns, and steer.
* `--model provider/model` overrides the **agent's own** model for that run
  (e.g. force the orchestrator onto `fnal-litellm/azure/claude-opus-4-8` for a
  hard coordination job). It does not override the per‑step `model` fields in
  the plan — those are honored by the orchestrator when it dispatches each
  sub‑agent. To change a step's model, edit the plan.

You can switch between modes on the fly without changing the underlying plan or
agent definitions.

---
