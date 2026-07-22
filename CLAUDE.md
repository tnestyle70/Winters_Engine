# CLAUDE.md

This file is for LLMs, not a project encyclopedia, changelog, or code map. Every rule here must change agent behavior.

## Read Order
1. Read this file before coding.
2. Read `AGENTS.md` for cross-agent behavior; keep the behavioral core in sync.
3. Read `.claude/gotchas.md` before starting changes so recurring mistakes are in scope.
4. Read `.md/architecture/WINTERS_CODEBASE_COMPASS.md` when work touches architecture boundaries, LoL DX11, Client/Engine/Shared dependencies, RHI/UI/data pipelines, Elden client/editor direction, collaboration structure, or an unfamiliar module.
5. Read `.md/architecture/WINTERS_IMGUI_TOOL_DESIGN_GUIDE.md` before adding or changing any ImGui tuner, debug panel, inspector, or workflow editor.
6. Read deeper project docs only when the task actually touches that domain.

## Codebase Compass
- The active architecture compass is `.md/architecture/WINTERS_CODEBASE_COMPASS.md`.
- Keep LoL DX11 client direction, Client/Engine/Shared dependency rules, collaboration conventions, and Elden client/editor direction there instead of expanding this file into project inventory.
- When a cross-cutting architecture rule changes, update the compass. When a repeated mistake appears, update `.claude/gotchas.md`.

## Canonical Authoring Safety
- F4 `Save & Hot Load` writes four canonical sources: `champions.json`, `SkillEffectGameplayDefs.json`, `SpawnObjectGameplayDefs.json`, and `EconomyGameplayDefs.json`. Their current saved values outrank older PLAN/RESULT numbers, test fixtures, generated JSON/C++, and manifests; `ChampionGameplayDefs.json` and `SkillGameplayDefs.json` are generated outputs, not authoring sources.
- A generated-freshness failure requires recooking from current canonical sources, never rolling those sources back. Freeze an F4-editable exact value in a test only when the user explicitly declares that baseline immutable; otherwise verify schema, domain, rank shape, and canonical/generated parity. See `.md/architecture/WINTERS_DATA_ARCHITECTURE.md`.

## Document Policy
- Record behavior rules, team decisions, recurring gotchas, and pointers to deeper docs.
- Do not record facts that code, build files, or `rg` can answer directly.
- Prefer stable paths and anchors over copied explanations.
- If this file or a section wants to exceed 100 lines, split details into an imported/reference doc and leave only the rule plus link here.
- Replace stale rules instead of appending around them.
- Keep examples short. Long examples belong in task docs, tests, or skills.

## Andrej Karpathy Coding Guardrail
Behavioral guidelines to reduce common LLM coding mistakes. Apply before coding unless the task is truly trivial. **Tradeoff:** caution over speed; use judgment for tiny edits.

### 1. Think Before Coding

**Do not assume. Do not hide confusion. Surface tradeoffs.**

Before implementing:
- State assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them; do not choose silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop, name the confusion, and ask.
- Route questions by reachability: inspect code/build facts yourself; ask the user for facts only they hold (intent, taste, priority); reserve `CONFIRM_NEEDED` for what no one in the session can settle.
- For goal-level or ambiguous-scope requests, ask up to 3-5 targeted questions before planning — skip when a handoff/plan doc already supplies the context; under the handoff guard, surface them in the handoff's 확인 필요 section.

### 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No flexibility, configurability, or extension points that were not requested.
- No error handling for impossible scenarios.
- If 200 lines could be 50, rewrite it.

Ask: "Would a senior engineer call this overcomplicated?" If yes, simplify.

### 3. Surgical Changes

**Touch only what is necessary. Clean up only your own mess.**

When editing existing code:
- Do not improve adjacent code, comments, or formatting unless needed for the request.
- Do not refactor things that are not broken.
- Match existing style, even if you would normally do it differently.
- If unrelated dead code appears, mention it; do not delete it.
- Remove imports, variables, and functions only when your change made them unused.

Test: every changed line should trace directly to the user's request.

### 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" -> "Write invalid-input tests, then make them pass."
- "Fix the bug" -> "Write or identify a reproducer, then make it pass."
- "Refactor X" -> "Show tests pass before and after."

For multi-step work, state a brief plan:

```text
1. [Step] -> verify: [check]
2. [Step] -> verify: [check]
3. [Step] -> verify: [check]
```

Strong success criteria allow independent progress. Weak criteria require clarification.

## Plan Authoring

When the user asks to write or show a plan ("계획 작성해줘", "계획서 쭉 보여줘", "plan 만들어줘", or equivalent):
- Read `.md/계획서작성규칙.md` first and follow its format/ordering before writing anything.
- Apply Karpathy guardrails on top — the rules doc does not exempt them.
- Before source edits, send the dated implementation plan to at least one independent sub-agent for read-only critique. Review every finding, revise the plan, and record accepted/rejected dispositions with reasons. The pass line is `.md/계획서작성규칙.md` §0: re-critique revisions until no accepted or held (보류) P0/P1 remains. If sub-agent tools are unavailable, mark the gate `CONFIRM_NEEDED` and do not begin implementation or claim review completion.

## Gotchas Refresh Hook

When the user says "이 실수 다시는 하지 않도록 CLAUDE.md에 반영해줘" or an equivalent request:
- Treat it as permission to edit `.claude/gotchas.md` immediately.
- Extract the reusable failure pattern, not the whole incident.
- Add or update one concise bullet in `.claude/gotchas.md` using: `YYYY-MM-DD - [Area] mistake -> prevention rule/check`.
- Do not add code-derived facts that can be found with `rg`; link to deeper docs for detail.
- Keep `.claude/gotchas.md` focused on prevention rules; split into sub-pages if it grows past ~200 lines.

## Gotchas

Mistake-prevention log lives in [.claude/gotchas.md](.claude/gotchas.md). Imported below so Claude sees current entries every session.

@.claude/gotchas.md

## Goal Operating Lens

For goal/plan/priority/retro conversations, apply `.md/process/GOAL_OPERATING_DOCTRINE.md`: enforce the 30% ceiling budget in plans, ask "천장 예산은요?" after 3+ consecutive floor-work sessions on one track, require 이해→환전 before a new deep dive, and propose external deadlines for deadline-less goals. When the next problem/task selection is genuinely open — not fixed by the user or a standing handoff — require 2+ candidates scored against the real evaluator's rubric (impact·risk·cost, doctrine §3). Judge user-declared frozen (동결) submissions go/no-go only: fix P0 (factual error, fatal phrasing), route the rest to the post-submission loss log; this never applies to code, dated plans, or build verification. Never cite this lens to lower code verification standards.

## Progressive Sections

- Team shared rules and coding conventions: add only behavior-changing decisions here; put detailed C++ style in `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md`.
- Gameplay/render debugging pipeline: before tuning symptoms, add or use an inspectable debug UI/overlay, bounded `OutputDebugStringA/W` traces, and visual capture around the authoritative code path; for movement/pathfinding, expose current cell, next cell/waypoint, resolved path, correction direction, and stuck/resolve reason.
