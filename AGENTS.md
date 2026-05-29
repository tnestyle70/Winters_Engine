# AGENTS.md

Cross-agent operating rules for Winters. Keep the behavioral core aligned with `CLAUDE.md`; this file is for LLMs, not project inventory.

## Read Order
1. Read this file before coding.
2. Read `CLAUDE.md` when working in Claude Code or changing Claude-specific hooks.
3. Read deeper docs only when the task touches that domain.
4. Read `CLAUDE_Legacy.md` when work touches Winters gameplay, networking, Shared/GameSim, server authority, AI, champion skills, animation replication, or FX cues.

## Server Authority / GameSim Work
- For server-authoritative gameplay, GameSim, networking, snapshot/event, AI, skill execution, or FX cue work, use `CLAUDE_Legacy.md` as the compact current codebase brief before changing code.
- Preserve the authoritative flow: `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`.
- Gameplay results belong in Shared/Server GameSim. Client champion hooks should be limited to input, weak prediction, interpolation, animation/FX playback, UI, and debug unless the task explicitly targets legacy local-only smoke.
- FX should be driven by server cues and played once through the client visual path.
- For render-quality experiments, do not hide roster, map, minion, snapshot, or champion systems in normal F5 flow as a shortcut. First check the server smoke roster/bot defaults, then choose an explicit temporary lab path if isolation is truly needed.

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

When the user asks to write or show a plan ("계획 ?�성?�줘", "계획??�?보여�?, "plan 만들?�줘", or equivalent):
- Read `.md/계획서작성규칙.md` first and follow its format/ordering before writing anything.
- Apply Karpathy guardrails on top ??the rules doc does not exempt them.
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
- Runtime resources resolve from `Client/Bin/Resource` only; do not add or rely on per-config output `Resource` copies under `Client/Bin/Debug*` or `Client/Bin/Release*`.
