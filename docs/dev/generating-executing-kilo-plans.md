# Generating & Executing Kilo Plans

## Purpose

Explain how to produce a structured execution plan with the **plan** agent and how to run that plan with the **execute** orchestrator. The process supports idempotent re‑runs, explicit model selection, built‑in error handling, and in‑flight re‑planning.

## 1. Generate a plan

1. **Define the goal** – a short, high‑level statement (e.g. "Add a dark‑mode toggle to the web UI").
2. **Invoke the planner** with `kilo run`. The `plan` agent is configured with
   `mode: all`, so it can be selected as a primary agent directly. The prompt is
   passed as a positional argument (there is no `--prompt` option), and
   `--title` optionally names the session (it defaults to a truncated prompt):

   ```bash
   kilo run --agent plan --title "Generate plan for <goal>" "<goal>"
   ```

   Replace `<goal>` with your actual objective.
3. The planner returns JSON following the schema in `agent/plan.md`. Key fields:
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
    // …additional steps
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
* Calls the specified model/sub‑agent (`reason`, `code`, `ask`, `chat`, `coder-qwen`, or `plan`).
* Runs `self_check` after completion.
* Honors `gate` (manual approval), `parallel` (concurrent execution), and retry logic.
* Stops, continues, falls back, or re‑plans based on `on_error`.

Because the plan is marked `idempotent`, you may safely re‑run the whole plan—or only the failing steps—without side‑effects, provided the underlying actions themselves are idempotent (e.g., use upserts, check‑before‑write).

### In‑flight re‑planning

A plan is not frozen once execution begins. When a step declares
`on_error: replan` and fails after exhausting `max_retries`, the orchestrator
suspends execution and re‑invokes the `plan` agent as a sub‑agent to revise the
*remaining* work, using the accumulated outputs and the failure context. This is
what the `mode: all` setting on the `plan` agent enables: the same agent serves
both as the directly‑invocable planner (section 1) and as the re‑planning
sub‑agent dispatched by `execute`.

Re‑planning preserves already‑completed steps verbatim so downstream
dependencies stay satisfied; the planner emits only new or replacement steps
with fresh, non‑colliding `id`s. The orchestrator records each re‑planning event
(the triggering step, the reason, and the new step ids) in its final JSON report
so the revision history is auditable.

For a human‑in‑the‑loop revision, pause at a `gate: true` step, run
`kilo run --agent plan "<revised goal>"` to produce an updated plan, and hand the
result back to a fresh `execute` run.

## 4. Model‑selection policy

* Prefer builtin agent shortcuts for simplicity.
* For performance‑critical or accuracy‑critical tasks, specify an explicit `fnal-litellm` model (e.g. `fnal-litellm/azure/claude-opus-4-8`, `fnal-litellm/azure/gpt_5.3-codex`).
* Only `fnal-litellm` provider models are permitted; the `plan` and `execute` agents reject models from other providers.
* The planner defaults to the free/open‑weight model (`fnal-litellm/openai/gpt-oss-120b`) unless a higher‑capability model is explicitly required.

## 5. Verify with `prek`

After creating or editing the guide, run the pre‑commit hook to ensure compliance:

```bash
prek run --all-files
```

`prek` checks for trailing whitespace, multiple final newlines, and basic markdown linting. This file follows those rules out‑of‑the‑box.

---
*Generated by the Kilo planner and orchestrator.*
