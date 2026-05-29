# Winters Implementation Handoff Output Rule

Primary authority: `AGENTS.md`.
Compatibility authority for Claude Code: `CLAUDE.md`.

This rule is mandatory whenever a plan, vertical-slice handoff, or implementation code is written in chat before touching the codebase.

## Trigger Words

Use this rule when the user says something like:

```text
plan
handoff
vertical slice
show all code
current phase
direct-apply plan
계획서
코드 전부
쭉 보여줘
다음 단계
현재 단계
가보자
진행하자
직접 반영하기 위해
가보자
진행하자
다음 가자
바로 진행
코드 보여줘
구현 계획서
vertical slice
handoff
계획서 작성
```

## Default Behavior

Do not edit files by default. Output the implementation handoff in chat first.

Edit the codebase only when the user clearly says:

```text
검토해줘
수정해줘
직접 반영해줘
적용해줘
고쳐줘
```

If the wording is ambiguous, say that handoff output mode is active and continue with the template below.

## Required Output Template

작업이 수직 슬라이스 또는 실전 패치 계획서라면 `AGENTS.md`의
`수직 슬라이스 패치 계획서 표준 (필수)` 섹션을 우선 적용한다. 이 형식은
일반 요약 템플릿보다 우선한다. handoff는 정확한 파일, include, helper 함수,
replacement anchor, code block, 예상 로그, 다음 slice 결정이 들어간 복붙 가능한
패치 계획서 형태여야 한다.

Every handoff must start with the current sequence:

```text
Current order:
S7_BasicAttackVerticalSlice
-> S8_EzrealQVerticalSlice
-> S10_BotAIStage1
```

Then output these sections in order:

1. Current slice and its position in the whole sequence.
2. Goal and why this slice comes first.
3. Exact paths for every `.h`, `.cpp`, `.fbs`, `.vcxproj`, `.md`, or asset file.
4. File-by-file insertion or replacement positions.
5. Code blocks to apply.
6. Build/runtime verification commands and expected logs.

## Line Reference Accuracy

- Verify every cited line with `rg -n "<stable anchor>" <file>` or numbered local output before writing it in chat.
- Do not use the function declaration/start line as the replacement line unless the edit really happens there.
- For non-trivial edits, provide both:

```text
Owner/function: <path>:<line>
Replacement anchor: <path>:<line> `<stable code fragment>`
```

- Prefer anchors such as `FxMaterialDesc drawMaterial = fx.material;` over bare line numbers.
- If the user's editor line differs, switch to anchor-based directions immediately and explain the mismatch.

## Planning Documents

Any new plan document must include:

```text
Current sequence
Goal
Why this order
Files touched
Insertion/replacement points
Verification logs
Next slice
```

Direct-apply plans, where the user will manually paste code from chat, must also include:

```text
Non-goals
Current-code evidence with rg-verified anchors
Phase-by-phase completion criteria
Before/after code blocks or full new-file contents
Rollback scope
```

For AI/GameSim/server-authority plans, state the invariant explicitly:

```text
Bot AI is a GameCommand producer. It must not directly mutate gameplay truth
such as HP, cooldowns, damage results, movement truth, or death/respawn state.
```

If a plan mentions JSON/Lua/data migration while the current slice is gameplay
logic stabilization, keep data migration as a later slice instead of mixing it
into the same patch.

Server-authority plans must also reference:

```text
.md/TODO/05-09/ServerAICompletion.md
```

FX/rendering plans must also reference the current FX concept folder:

```text
.md/TODO/05-07/FX개념!
```

## Guard Files

The rule is bound to:

```text
AGENTS.md
CLAUDE.md
.claude/commands/implementation-handoff.md
.claude/hooks/implementation-handoff-context.ps1
.claude/hooks/implementation-handoff-pretool-guard.ps1
.claude/hooks/implementation-handoff-stop-guard.ps1
.claude/settings.json
```

The hook stack is a Claude Code compatibility guard. `AGENTS.md` remains the
cross-agent authoritative rule.
