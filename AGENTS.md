# AGENTS.md

Cross-agent operating rules for Winters. Keep the behavioral core aligned with `CLAUDE.md`; this file is for LLMs, not project inventory.

## Read Order
1. Read this file before coding.
2. Read `.claude/gotchas.md` before starting changes so recurring mistakes are in scope.
3. Read `.md/architecture/WINTERS_CODEBASE_COMPASS.md` when work touches architecture boundaries, LoL DX11, Client/Engine/Shared dependencies, RHI/UI/data pipelines, Elden client/editor direction, collaboration structure, or an unfamiliar module.
4. Read `.md/architecture/WINTERS_IMGUI_TOOL_DESIGN_GUIDE.md` before adding or changing any ImGui tuner, debug panel, inspector, or workflow editor.
5. Read `CLAUDE.md` when working in Claude Code or changing Claude-specific hooks.
6. Read deeper docs only when the task touches that domain.
7. Read `CLAUDE_Legacy.md` when work touches Winters gameplay, networking, Shared/GameSim, server authority, AI, champion skills, animation replication, or FX cues.

## Codebase Compass
- The active architecture compass is `.md/architecture/WINTERS_CODEBASE_COMPASS.md`.
- Keep LoL DX11 client direction, Client/Engine/Shared dependency rules, collaboration conventions, and Elden client/editor direction there instead of expanding this file into project inventory.
- When a cross-cutting architecture rule changes, update the compass. When a repeated mistake appears, update `.claude/gotchas.md`.

## Canonical Authoring Safety
- F4 `Save & Hot Load` writes four canonical sources: `champions.json`, `SkillEffectGameplayDefs.json`, `SpawnObjectGameplayDefs.json`, and `EconomyGameplayDefs.json`. Their current saved values outrank older PLAN/RESULT numbers, test fixtures, generated JSON/C++, and manifests; `ChampionGameplayDefs.json` and `SkillGameplayDefs.json` are generated outputs, not authoring sources.
- A generated-freshness failure requires recooking from current canonical sources, never rolling those sources back. Freeze an F4-editable exact value in a test only when the user explicitly declares that baseline immutable; otherwise verify schema, domain, rank shape, and canonical/generated parity. See `.md/architecture/WINTERS_DATA_ARCHITECTURE.md`.

## Server Authority / GameSim Work
- For server-authoritative gameplay, GameSim, networking, snapshot/event, AI, skill execution, or FX cue work, use `CLAUDE_Legacy.md` as the compact current codebase brief before changing code.
- Preserve the authoritative flow: `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`.
- Gameplay results belong in Shared/Server GameSim. Client champion hooks should be limited to input, weak prediction, interpolation, animation/FX playback, UI, and debug unless the task explicitly targets legacy local-only smoke.
- FX should be driven by server cues and played once through the client visual path.
- For render-quality experiments, do not hide roster, map, minion, snapshot, or champion systems in normal F5 flow as a shortcut. First check the server smoke roster/bot defaults, then choose an explicit temporary lab path if isolation is truly needed.

## NYPC Lab Integration Boundary
- `C:\Users\tnest\Desktop\NYPC\mushroom` is an independent Competition ML Lab for complete turn-based game models, self-play, MCTS, imitation, RL, league evaluation, and submission distillation.
- Do not modify Winters runtime/source code for NYPC Lab work unless the user explicitly requests a Winters integration session.
- NYPC Lab output may inform Winters AI design, but it must first be validated outside Winters with deterministic rules, replay, logs, league reports, and artifact export.
- When integration is explicitly requested, bridge through `.md/plan/ai/14_NYPC_COMPETITION_ML_LAB_BRIDGE.md` and preserve the server-authoritative `Shared/GameSim` direction.

## Document Policy
- Record behavior rules, team decisions, recurring gotchas, and pointers to deeper docs.
- Do not record facts that code, build files, or `rg` can answer directly.
- Prefer stable paths and anchors over copied explanations.
- If this file or a section wants to exceed 100 lines, split details into an imported/reference doc and leave only the rule plus link here.
- Replace stale rules instead of appending around them.
- Keep examples short. Long examples belong in task docs, tests, or skills.

## Plan Document Placement
- When asked for a design guide, implementation guide, handoff plan, or architecture plan, create a dated Markdown file under `.md/plan/YYYY-MM-DD_<TOPIC>.md` unless the user names a more specific plan subfolder.
- Start the document with `Session - ...`, include current code evidence, ownership boundaries, staged direction, and verification/handoff notes.
- Every session that authors, resumes, or applies a dated implementation plan must read and create or update the corresponding `*_PLAN.md` before source edits. Once implementation starts, the same session must create or update the same-name `*_RESULT.md` before handoff; chat-only plans and implementation-only handoffs are not complete.
- Resume the existing plan/result pair for the same slice instead of creating a duplicate, and follow `.md/계획서작성규칙.md` for both files. `2026-07-18_IRELIA_E_UNIT_HIT_MARK_Q_RESET_RECALL_6S_PLAN.md` is the reference example for this mandatory session contract.
- Before source edits, every dated implementation plan must receive a read-only critique from at least one independent sub-agent. The primary agent must review the findings, update the plan with accepted/rejected dispositions and reasons, and only then implement. The pass line is `.md/계획서작성규칙.md` §0: re-critique revisions until no accepted or held (보류) P0/P1 remains. If sub-agent tools are unavailable, mark the critique gate `CONFIRM_NEEDED` and do not claim the plan is reviewed or begin implementation.
- Keep `AGENTS.md` as the rule pointer only; put detailed design guidance in the dated plan document.

## Goal Operating Lens
For goal/plan/priority/retro conversations, apply `.md/process/GOAL_OPERATING_DOCTRINE.md`: enforce the 30% ceiling budget in plans, ask the ceiling-budget question after 3+ consecutive floor-work sessions on one track, require insight-to-output conversion (이해→환전) before a new deep dive, and propose external deadlines for deadline-less goals. When the next problem/task selection is genuinely open — not fixed by the user or a standing handoff — require 2+ candidates scored against the real evaluator's rubric (impact·risk·cost, doctrine §3). Judge user-declared frozen (동결) submissions go/no-go only: fix P0 (factual error, fatal phrasing), route the rest to the post-submission loss log; this never applies to code, dated plans, or build verification. Never cite this lens to lower code verification standards.

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
- Apply Karpathy guardrails on top. The rules doc does not exempt them.
- After drafting, run the mandatory independent sub-agent critique gate from `Plan Document Placement`, record its disposition in the plan, and revise until the `.md/계획서작성규칙.md` §0 pass line (no accepted or held P0/P1 remaining) before implementation.
- For `/plan-rules` or code-preview plans, follow this order: read this file, read `.claude/gotchas.md`, read `.md/계획서작성규칙.md`, then inspect the target h/cpp/vcxproj files before writing code blocks.
- For every `새 파일:` section, include the complete intended file body in a fenced code block; do not replace implementation with `구현 내용`, bullet summaries, omitted functions, or pseudo-code.
- For existing files, use exact existing anchors and explicit `아래에 추가`, `아래로 교체`, or `삭제` blocks; prose-only implementation summaries are not acceptable.
- If a complete code block cannot be produced from known context, mark that file/section as `CONFIRM_NEEDED` and name what must be inspected.

## Slash Commands

- `/review`: review the current dirty change, apply only necessary fixes, run `git diff --check`, and build the relevant target. See `.claude/commands/review.md`.

## Gotchas

Mistake-prevention log: [.claude/gotchas.md](.claude/gotchas.md). Read it before starting work so previous lessons are in scope.

## Progressive Sections

- Team shared rules and coding conventions: add only behavior-changing decisions here; put detailed C++ style in `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md`.
- C++ field names use type prefix first, then domain/skill meaning. Examples: `fEGroundYOffset`, `fECloseSparkSize`, `vEGroundGlowColor`, `vECloseSparkColor`; reserve `e` prefix for enum-like values, not for E-skill float/vector fields.
- Verification/debug logs should use `OutputDebugStringA/W` in Debug paths so client/server diagnostics are visible in the debugger without changing gameplay behavior.
- Gameplay/render debugging pipeline: before tuning symptoms, add or use an inspectable debug UI/overlay, bounded `OutputDebugStringA/W` traces, and visual capture around the authoritative code path; for movement/pathfinding, expose current cell, next cell/waypoint, resolved path, correction direction, and stuck/resolve reason.
- Runtime resources resolve from `Client/Bin/Resource` only; do not add or rely on per-config output `Resource` copies under `Client/Bin/Debug*` or `Client/Bin/Release*`.
- Extreme optimization work must remove duplicate paths before adding new ones. Reuse or tighten the existing Engine/Client pipeline first; if a second renderer, cache, update loop, or data owner is proposed, mark it `CONFIRM_NEEDED` in the plan with the owner and deletion path for the old one.
- Performance plans must name the measured JSON scope/counter, the target frame budget, and the verification command/capture. Do not count instrumentation-only changes as optimization, and do not hide normal F5 roster, map, minion, snapshot, champion, UI, or FX systems to make a number look better.
- Preserve dependency direction under pressure: Engine owns generic runtime/render/resource primitives and must not include LoL Client or Server product code; Shared/GameSim must not include Engine, Client, Renderer, UI, ImGui, or DX types; Server must not depend on Client visuals; Client may consume Engine and Shared but must not create authoritative gameplay truth.
