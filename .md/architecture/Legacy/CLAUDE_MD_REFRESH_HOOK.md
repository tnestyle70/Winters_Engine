# CLAUDE.md Auto-Refresh Hook 계획서 v1

> **작성일**: 2026-05-02
> **목적**: CLAUDE.md 를 (1) 매 세션 종료 시 자동, (2) 사용자 명시 요청 시 수동으로 그 세션 대화 내용 바탕으로 refresh 하는 시스템 설계.
> **참조**: Claude Code 공식 hooks docs (https://code.claude.com/docs/en/hooks.md, hooks-guide.md)
> **핵심 결정**: 자동 갱신은 **proposal 파일 분리** (사용자 검토 후 적용), 수동 갱신은 Claude 가 현재 context 로 즉시 수행. 둘 다 **CLAUDE.md 직접 덮어쓰기 금지** — git diff 검토 가능한 형태로 출력.

---

## §0. 한 줄 요약

**Claude Code 의 `SessionEnd` hook (Stop 과 다른 진짜 세션 종료 trigger) + `/refresh-claude-md` slash command 두 entry 로 CLAUDE.md 갱신. SessionEnd 는 stdout context 주입 불가 + transcript 직접 접근 불가 한계 때문에 스크립트가 transcript 파일 (`~/.claude/projects/.../<session-id>.jsonl`) 직접 read → 별도 `CLAUDE.md.refresh-proposal.md` 출력 → 다음 세션 시작 시 사용자가 diff 검토 후 적용. 수동은 Claude 가 현재 context 로 직접 수정 + git diff 사용자 검토. 두 trigger 모두 "갱신 정책 7원칙" (박제 vs 노이즈 분리) 준수.**

---

## §1. 요구 사항

### 1-1. 두 Trigger

| # | Trigger | 시점 | 동작 |
|---|---|---|---|
| **자동** | `SessionEnd` hook | `/clear`, `/logout`, `prompt_input_exit` 등 세션 진짜 종료 | 외부 스크립트 실행 → transcript 파일 read → Claude API 호출로 분석 → `CLAUDE.md.refresh-proposal.md` 출력 |
| **수동** | `/refresh-claude-md` slash command | 사용자가 명시 호출 | Claude 가 현재 세션 context 로 직접 분석 → `CLAUDE.md` 에 Edit 적용 → git diff 사용자 검토 |

### 1-2. 비기능 요구

- **안전성**: 자동 갱신이 사용자 검토 없이 CLAUDE.md 덮어쓰기 X (proposal 파일로 분리)
- **일관성**: 수동/자동 둘 다 동일 갱신 정책 적용
- **추적성**: 모든 갱신은 git diff 로 검토 가능한 형태
- **노이즈 차단**: 일회성 디버깅 / 검토 중 가설 / 임시 코드 박제 X

---

## §2. Claude Code Hook 시스템 사실 정리 (공식)

### 2-1. Hook Event 종류

| Event | 시점 | 본 계획서 사용 |
|---|---|---|
| `SessionStart` | 세션 시작 / resume | (선택) — proposal 파일 존재 확인 알림 |
| `SessionEnd` | 세션 진짜 종료 (`reason` 필드: clear/resume/logout/prompt_input_exit/bypass_permissions_disabled/other) | ✓ 자동 trigger |
| `Stop` | 매 응답 turn 종료 | ✗ 너무 잦음 (사용 X) |
| `UserPromptSubmit` | 사용자가 입력 제출 | ✗ |
| `PreToolUse` / `PostToolUse` | 도구 호출 전/후 | ✗ |

### 2-2. Hook Payload (stdin JSON)

```json
{
  "session_id": "abc123-def456-...",
  "cwd": "/path/to/project",
  "hook_event_name": "SessionEnd",
  "reason": "clear"
}
```

★ **transcript 직접 전달 X** — 스크립트가 session_id 로 transcript 파일 위치 추론 + 직접 read.

### 2-3. Hook 환경 변수

- `CLAUDE_PROJECT_DIR` — 프로젝트 루트 절대 경로
- `CLAUDE_ENV_FILE` — 환경 변수 파일 (모든 Bash 명령 전 source)

### 2-4. Hook 출력 처리 — ★ SessionEnd 의 한계

**일반 hook**: stdout JSON 의 `additionalContext` 가 다음 turn system message 에 주입.

**SessionEnd**: ❌ **stdout 무시** (세션 이미 종료). 즉 hook 안에서 Claude 에게 prompt 주입 불가 → 외부 스크립트가 자체적으로 분석 + 파일 출력만 가능.

★ **본 계획서가 proposal 파일 분리 패턴 채택한 이유** = SessionEnd 의 이 한계 + 자동 갱신의 안전성 우려 둘 다 해결.

### 2-5. 슬래시 커맨드 vs Hook

- **Slash command** (`/X`) — 사용자 명시 trigger, Claude 가 현재 context 안에서 수행 (skill frontmatter 정의)
- **Hook** — 자동 trigger, 외부 스크립트 실행 (Claude context 밖)

본 계획서는 둘 다 사용:
- 자동 = Hook (SessionEnd)
- 수동 = Slash command (/refresh-claude-md)

---

## §3. 설계 — 두 Entry Point

### 3-1. `/refresh-claude-md` Slash Command (수동)

**위치**: `.claude/skills/refresh-claude-md/SKILL.md`

**흐름**:
```
[User] "/refresh-claude-md" 또는 "CLAUDE.md 갱신해"
   ↓
[Claude] SKILL.md 절차 따라 진행
   ├─ 1. 현재 세션 대화에서 박제 후보 추출 (§5 정책 따름)
   ├─ 2. CLAUDE.md Read (현 상태 파악)
   ├─ 3. 박제 위치 결정 (★ 다음 / Gotcha / 미결 / Phase 진행 등)
   ├─ 4. Edit 도구로 CLAUDE.md 직접 수정
   ├─ 5. 사용자에게 diff 요약 + git diff 명령 안내
   └─ 6. 박제하지 않은 후보 (노이즈로 판정) 도 보고
   ↓
[User] git diff 검토 → 만족 시 commit, 아니면 Edit 추가 요청
```

**장점**:
- Claude 가 현재 세션 context 그대로 활용 → transcript 파일 read 불필요
- 즉시 Edit 적용 + git diff 로 사용자 검토 가능
- 외부 도구 의존 0

**단점**:
- 사용자가 명시 호출해야만 동작 (자동 X)

### 3-2. SessionEnd Hook (자동)

**위치**: `.claude/hooks/refresh-claude-md.sh` (실행 스크립트) + `.claude/settings.json` (hook 등록)

**흐름**:
```
[Session 종료 — /clear / /logout / prompt_input_exit]
   ↓
[Claude Code harness] SessionEnd hook 실행
   ↓ (stdin JSON: session_id, cwd, reason)
[refresh-claude-md.sh]
   ├─ 1. session_id 로 transcript 파일 경로 계산
   │     (~/.claude/projects/<project-id>/<session-id>.jsonl)
   ├─ 2. transcript 파일 read — user/assistant 메시지 전체
   ├─ 3. Anthropic API 호출 (claude-haiku-4-5 또는 claude-sonnet-4-6)
   │     prompt: "이 세션의 박제 후보를 §5 정책 따라 추출"
   ├─ 4. 결과를 $CLAUDE_PROJECT_DIR/CLAUDE.md.refresh-proposal.md 에 출력
   │     (CLAUDE.md 직접 수정 X — 사용자 검토 대기)
   └─ 5. (선택) Slack/email/터미널 알림
   ↓
[다음 세션 시작 시 SessionStart hook]
   ├─ proposal 파일 존재 확인
   └─ 존재 시 user 에게 "검토 대기 중" 메시지 출력
```

**장점**:
- 사용자가 잊어도 자동 박제
- 세션마다 누락 없이 후보 수집

**단점**:
- API 비용 (Haiku 기준 세션당 $0.001~0.01 예상)
- 외부 스크립트 의존 (anthropic SDK 설치 필요)
- transcript 파일 경로 = Claude Code 내부 구조 의존 (변경 시 깨짐)

### 3-3. 두 Entry 의 책임 분담

| 상황 | 어느 쪽 사용 |
|---|---|
| 세션 중간에 큰 결정 박제 직후 | 수동 `/refresh-claude-md` |
| 세션 끝에 잊고 나감 | 자동 SessionEnd 가 proposal 생성 |
| 검토 후 정식 박제 | 다음 세션 시작 시 proposal 검토 → 수동 적용 |
| 일회성 디버깅 세션 | 자동 proposal 무시 (적용 안 함) |

---

## §4. Transcript 접근 전략

### 4-1. 수동 (Slash command) — Claude 의 현재 context 활용

Claude Code 가 slash command 호출 시 현재 세션의 모든 user/assistant 메시지를 이미 context 에 보유. 별도 transcript 파일 read 불필요. SKILL.md 의 절차에서 "이 세션에서 우리가 결정한 것" 을 직접 추출.

### 4-2. 자동 (Hook) — Transcript 파일 read

**파일 위치 패턴** (Claude Code 의 일반 구조):
```
~/.claude/projects/<project-id>/<session-id>.jsonl
```

`<project-id>` 는 cwd 의 hash 또는 인코딩 (정확한 알고리즘은 docs 미공개 — Phase A 검증 필요).
`<session-id>` 는 hook payload 의 `session_id` 필드.

**스크립트가 추론하는 방법** (Phase A 검증 항목):
1. cwd 기반: `~/.claude/projects/<encoded-cwd>/<session_id>.jsonl`
2. 글로브: `~/.claude/projects/*/<session_id>.jsonl` 로 단일 매치 찾기 (안전)

★ **Phase A 1일차 작업**: `find ~/.claude/projects -name "<session_id>.jsonl"` 로 실제 위치 패턴 확인 후 스크립트에 박제.

### 4-3. Transcript JSON Lines 형식

각 줄이 1개 메시지:
```json
{"type": "user", "message": {"content": "..."}}
{"type": "assistant", "message": {"content": [...]}, "tool_uses": [...]}
{"type": "tool_result", ...}
```

스크립트가 line-by-line read → user/assistant 메시지 추출 → API prompt 구성.

---

## §5. Refresh 정책 — 무엇을 박제하고 무엇을 안 박제하는가

### 5-1. 박제 후보 (✓)

| 카테고리 | 예시 | CLAUDE.md 위치 |
|---|---|---|
| **새 결정** | "Get_WorkerSlot 옵션 A 채택 (현행 thread 기반 유지)" | §보안/안티치트 또는 §Phase 진행 |
| **새 Gotcha** | "CJobCounter 에 wait list 박제 금지 — stack lifetime + mutex contention" | §Gotchas |
| **완료된 Phase** | "Phase 5-A 완료 — Chase-Lev hybrid + help-stealing" | §현재 진행 / §Phase 로드맵 |
| **새 계획서 위치** | ".md/plan/engine/FIBER_JOB_SYSTEM_v2.md (v2.1, Codex 6건)" | §문서 인덱스 |
| **다음 진입 명령** | "Phase 5-B M0a stress 진입 명령" | §★ 다음 |
| **새 컨벤션** | "fiber 컨텍스트 안에서 Get_WorkerSlot 캐시 금지" | §코딩 컨벤션 |

### 5-2. 노이즈 (✗ 박제 X)

| 카테고리 | 이유 |
|---|---|
| 일회성 디버깅 | "이 함수에서 break 걸어보니 X 였음" — 이미 fix 됐으면 박제 가치 0 |
| 검토 중 가설 | "이거 X 일 수도 있는데 확인 필요" — 결정 안 된 정보는 노이즈 |
| 코드 인용 그대로 | grep 결과 / 파일 내용 그대로 — CLAUDE.md 비대화 |
| 사용자의 짧은 chitchat | "ok" / "그래" / "다음" 류 |
| 수정된 임시 결정 | 세션 중간에 "X 로 가자" → 끝에 "Y 로 변경" — 최종만 박제 |

### 5-3. 7 원칙 (자동/수동 모두 적용)

1. **결정의 결정성** — 검토 중 가설 X, 확정된 결정만
2. **재방문 가치** — 다음 세션에서 또 필요할 정보만
3. **위치 정확성** — 박제 위치 (Gotcha / Phase / 컨벤션 / 인덱스) 명시
4. **간결성** — 1-3 줄로 압축 (긴 설명은 별도 .md 파일)
5. **링크화** — 새 계획서 박제 시 경로 링크
6. **Stale 검증** — 기존 박제와 모순 없는지 (있으면 사용자에게 confirm 요청)
7. **수정 vs 추가 구분** — 기존 박제 갱신은 Edit, 새 정보는 Append

---

## §6. 안전장치

### 6-1. 자동 갱신의 안전 (Proposal 분리)

★ **CLAUDE.md 직접 덮어쓰기 절대 금지**. SessionEnd hook 은 항상 proposal 파일로 출력:

```
$CLAUDE_PROJECT_DIR/CLAUDE.md.refresh-proposal.md
```

이 파일에는:
- 추출된 박제 후보 (§5-1 카테고리별 분류)
- 각 후보의 박제 위치 제안 (CLAUDE.md 의 어느 섹션)
- 노이즈로 판정한 후보 (참고용, 사용자가 동의 안 하면 박제 가능)
- diff 형식 — `--- CLAUDE.md` / `+++ CLAUDE.md.proposed` 의 patch

다음 세션 시작 시 사용자가:
1. proposal 파일 read
2. 동의하는 후보만 CLAUDE.md 에 직접 적용 (수동 또는 `/apply-refresh-proposal` 같은 follow-up command)
3. proposal 파일 삭제 또는 archive

### 6-2. 수동 갱신의 안전 (Git Diff 검토)

`/refresh-claude-md` 가 Claude 에게 직접 Edit 적용을 허용하지만:
- Claude 가 작업 끝에 **반드시 git diff 명령 안내**
- 사용자가 검토 후 commit (자동 commit 금지)
- 만족 못 하면 Edit 추가 요청

### 6-3. CLAUDE.md 자체의 백업

자동/수동 둘 다 적용 전 백업:
```
.claude/backups/CLAUDE.md.<timestamp>
```

비상 복구용. 7일 보관 후 자동 삭제 (cron 또는 hook).

### 6-4. Refresh 무한 루프 방지

수동 호출 시: Claude 가 Edit 후 다시 `/refresh-claude-md` 호출 X (skill 안에서 재귀 차단).
자동 호출 시: SessionEnd 가 새 세션을 시작하지 않으므로 자연 차단.

---

## §7. 파일 명세 (3 신규)

### 7-1. `.claude/skills/refresh-claude-md/SKILL.md` (수동 trigger)

```markdown
---
name: refresh-claude-md
description: |
  Refresh CLAUDE.md based on the current session's discussion.
  Use when the user explicitly says "CLAUDE.md 갱신", "refresh
  claude.md", "박제해줘", "/refresh-claude-md", or after a major
  decision/gotcha/phase-completion that should persist to next session.
  Apply 7-rule policy (decision-finality, re-visit value, location
  accuracy, brevity, link-ization, stale check, edit-vs-append).
---

# /refresh-claude-md

CLAUDE.md 를 현재 세션 대화 바탕으로 갱신하는 절차.

## 1. 박제 후보 추출 (현 세션 context 활용)

현재 대화에서 다음 카테고리별 후보 식별:
- 새 결정 (확정된 것만 — 검토 중 가설 제외)
- 새 Gotcha (재현 가능한 함정 + 회피 방법)
- 완료된 Phase
- 새 계획서 / 가이드 / 문서 위치
- 다음 진입 명령 (★ 다음 섹션)
- 새 컨벤션

노이즈 (박제 X):
- 일회성 디버깅
- 검토 중 가설
- 사용자 짧은 chitchat
- 세션 중간에 수정된 결정 (최종만 박제)

## 2. CLAUDE.md 현 상태 파악

- `CLAUDE.md` Read
- 박제 위치 후보 섹션 식별:
  - Gotchas (가장 흔한 추가)
  - ★ 다음 (Phase 진행)
  - 문서 인덱스 (새 계획서 링크)
  - 코딩 컨벤션 (새 규칙)
  - Phase 로드맵 표 (완료 표시)

## 3. 사용자 confirm

5+ 후보가 있으면 사용자에게 짧은 표로 보고 후 적용 여부 confirm:
- "다음 X 개 박제 후보 발견. 적용할까요?"
- 표에 카테고리 / 1줄 요약 / 박제 위치 표시
- 사용자가 일부만 선택 가능

## 4. Edit 적용

Edit 도구로 CLAUDE.md 수정. 각 후보마다:
- 박제 위치의 정확한 텍스트 매칭
- 1-3 줄로 압축
- 새 계획서는 markdown 링크

## 5. 사용자 검토 안내

작업 끝에 안내:
- "git diff CLAUDE.md 로 검토 후 commit 하세요"
- 박제하지 않은 후보 (노이즈 판정) 도 짧게 보고

## 6. 금지

- 자동 git commit X (사용자 책임)
- 재귀 호출 X (이 skill 안에서 다시 /refresh-claude-md 호출 금지)
- 박제 후보 모호하면 사용자 confirm 우선

## 7. 7 원칙 (.md/architecture/CLAUDE_MD_REFRESH_HOOK.md §5-3 박제)

1. 결정의 결정성
2. 재방문 가치
3. 위치 정확성
4. 간결성 (1-3 줄)
5. 링크화
6. Stale 검증
7. 수정 vs 추가 구분
```

### 7-2. `.claude/hooks/refresh-claude-md.sh` (자동 trigger)

```bash
#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────
# Claude Code SessionEnd hook — CLAUDE.md refresh proposal 생성
#
# 등록: .claude/settings.json 의 hooks.SessionEnd 에 명시
# 실행: 세션 진짜 종료 시 (clear/logout/prompt_input_exit/other)
# 입력: stdin JSON { session_id, cwd, hook_event_name, reason }
# 출력: $CLAUDE_PROJECT_DIR/CLAUDE.md.refresh-proposal.md
#
# ★ CLAUDE.md 직접 수정 X — proposal 파일로만 출력 (사용자 검토 대기)
# ─────────────────────────────────────────────────────────────

set -euo pipefail

# ── 1. stdin payload 파싱 ──────────────────────────────────
PAYLOAD=$(cat)
SESSION_ID=$(echo "$PAYLOAD" | jq -r '.session_id // empty')
REASON=$(echo "$PAYLOAD" | jq -r '.reason // empty')

# Skip if reason indicates non-meaningful end
if [[ "$REASON" == "bypass_permissions_disabled" ]]; then
    exit 0
fi

if [[ -z "$SESSION_ID" || -z "$CLAUDE_PROJECT_DIR" ]]; then
    echo "[refresh-claude-md] missing session_id or CLAUDE_PROJECT_DIR" >&2
    exit 0  # SessionEnd 는 exit code 무시되지만 명시적 0
fi

# ── 2. transcript 파일 위치 추론 ───────────────────────────
# 패턴: ~/.claude/projects/<project-id>/<session-id>.jsonl
# project-id 는 cwd 인코딩 — 글로브로 안전하게 매치
TRANSCRIPT_PATH=$(find "$HOME/.claude/projects" -name "${SESSION_ID}.jsonl" 2>/dev/null | head -1)

if [[ -z "$TRANSCRIPT_PATH" ]]; then
    echo "[refresh-claude-md] transcript not found for session $SESSION_ID" >&2
    exit 0
fi

# ── 3. 환경 검증 ───────────────────────────────────────────
if ! command -v anthropic-claude-cli >/dev/null 2>&1; then
    # anthropic SDK CLI 또는 curl + ANTHROPIC_API_KEY 사용
    if [[ -z "${ANTHROPIC_API_KEY:-}" ]]; then
        echo "[refresh-claude-md] ANTHROPIC_API_KEY missing" >&2
        exit 0
    fi
fi

# ── 4. proposal 파일 경로 ──────────────────────────────────
PROPOSAL_PATH="$CLAUDE_PROJECT_DIR/CLAUDE.md.refresh-proposal.md"
TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
TMP_PROPOSAL="$PROPOSAL_PATH.tmp"

# ── 5. CLAUDE.md 백업 (proposal 적용 전 사용자 복구용) ────
BACKUP_DIR="$CLAUDE_PROJECT_DIR/.claude/backups"
mkdir -p "$BACKUP_DIR"
cp "$CLAUDE_PROJECT_DIR/CLAUDE.md" "$BACKUP_DIR/CLAUDE.md.${TIMESTAMP}"

# ── 6. Anthropic API 호출 ──────────────────────────────────
# transcript 의 user/assistant 메시지만 추출
TRANSCRIPT_TEXT=$(jq -r '
    select(.type == "user" or .type == "assistant") |
    "[" + .type + "] " + (
        if .type == "user" then
            (.message.content // "" | tostring)
        else
            (.message.content[]? | select(.type == "text") | .text // "")
        end
    )
' "$TRANSCRIPT_PATH" 2>/dev/null | head -c 100000)  # 100KB 한도

# CLAUDE.md 전문 (위치 매핑용)
CLAUDE_MD_CONTENT=$(cat "$CLAUDE_PROJECT_DIR/CLAUDE.md")

# Prompt 구성
read -r -d '' PROMPT <<EOF || true
당신은 Winters Engine 프로젝트의 CLAUDE.md 갱신 보조자입니다.
다음 세션 transcript 와 현 CLAUDE.md 를 비교해 박제 후보를 추출하세요.

## 박제 정책 (.md/architecture/CLAUDE_MD_REFRESH_HOOK.md §5)

### 박제 (✓)
- 새 결정 (확정된 것만)
- 새 Gotcha (재현 가능 + 회피)
- 완료된 Phase
- 새 계획서 / 문서 위치
- 다음 진입 명령
- 새 컨벤션

### 노이즈 (✗)
- 일회성 디버깅
- 검토 중 가설
- 사용자 짧은 chitchat
- 세션 중간에 수정된 결정 (최종만)

## 출력 형식

\`\`\`markdown
# CLAUDE.md Refresh Proposal — ${TIMESTAMP}

## 박제 후보

### 1. {카테고리} — {1줄 요약}
- **위치**: CLAUDE.md 의 §{섹션}
- **내용** (1-3 줄):
  > {박제할 텍스트}
- **이유**: {왜 박제 가치 있는지}

### 2. ...

## 노이즈 (참고 — 박제 X)
- {후보} — 이유: {왜 노이즈}

## Diff Patch (사용자가 적용 시)
\`\`\`diff
--- CLAUDE.md
+++ CLAUDE.md.proposed
@@ ... @@
{patch}
\`\`\`
\`\`\`

## Transcript

${TRANSCRIPT_TEXT}

## 현 CLAUDE.md

${CLAUDE_MD_CONTENT}
EOF

# Anthropic API 호출 (claude-haiku-4-5, 비용 최소화)
RESPONSE=$(curl -sS -X POST "https://api.anthropic.com/v1/messages" \
    -H "x-api-key: $ANTHROPIC_API_KEY" \
    -H "anthropic-version: 2023-06-01" \
    -H "content-type: application/json" \
    -d "$(jq -n \
        --arg model "claude-haiku-4-5-20251001" \
        --arg prompt "$PROMPT" \
        '{
            model: $model,
            max_tokens: 4096,
            messages: [{role: "user", content: $prompt}]
        }')" \
    2>&1)

# 결과 추출
PROPOSAL_TEXT=$(echo "$RESPONSE" | jq -r '.content[0].text // empty' 2>/dev/null || echo "")

if [[ -z "$PROPOSAL_TEXT" ]]; then
    echo "[refresh-claude-md] API call failed or empty response" >&2
    echo "[refresh-claude-md] Response: $RESPONSE" >&2
    exit 0
fi

# ── 7. proposal 파일 출력 ──────────────────────────────────
{
    echo "<!-- Auto-generated by .claude/hooks/refresh-claude-md.sh -->"
    echo "<!-- Session: $SESSION_ID, Reason: $REASON, Timestamp: $TIMESTAMP -->"
    echo "<!-- 사용자: 이 파일을 검토 후 동의하는 항목만 CLAUDE.md 에 적용하세요. -->"
    echo "<!-- 적용 후 이 파일은 삭제 또는 .claude/backups/ 로 archive. -->"
    echo ""
    echo "$PROPOSAL_TEXT"
} > "$TMP_PROPOSAL"

mv "$TMP_PROPOSAL" "$PROPOSAL_PATH"

echo "[refresh-claude-md] proposal generated: $PROPOSAL_PATH"
exit 0
```

### 7-3. `.claude/settings.json` 패치 (hook 등록)

기존 `settings.json` 에 추가:

```json
{
  "hooks": {
    "SessionEnd": [
      {
        "matcher": "clear|logout|prompt_input_exit|other",
        "hooks": [
          {
            "type": "command",
            "command": "\"$CLAUDE_PROJECT_DIR\"/.claude/hooks/refresh-claude-md.sh"
          }
        ]
      }
    ],
    "SessionStart": [
      {
        "matcher": ".*",
        "hooks": [
          {
            "type": "command",
            "command": "test -f \"$CLAUDE_PROJECT_DIR/CLAUDE.md.refresh-proposal.md\" && echo '[NOTICE] CLAUDE.md.refresh-proposal.md 존재 — 검토 후 적용 필요' || true"
          }
        ]
      }
    ]
  }
}
```

★ **`bypass_permissions_disabled` matcher 제외** — 권한 비활성 종료는 의미 없음.
★ **SessionStart hook** — proposal 파일 존재 시 사용자에게 알림 (검토 잊음 방지).

---

## §8. 적용 단계

### Phase A — 수동 trigger 만 도입 (3일)

| Day | 작업 | 산출물 |
|---|---|---|
| 1 | `/refresh-claude-md` skill 작성 + 테스트 | `.claude/skills/refresh-claude-md/SKILL.md` |
| 2 | 5 회 실제 세션에서 사용 — 7 원칙 / 카테고리 / 박제 위치 정확성 검증 | (회귀 노트) |
| 3 | SKILL.md 보강 — 자주 놓치는 패턴 추가 | (수정) |

**합격 기준**:
- 세션 끝 사용자가 `/refresh-claude-md` 호출 → 박제 후보 정확히 추출
- 노이즈 판정 정확 (false positive < 20%)
- git diff 검토로 사용자가 모든 변경 명확히 파악

### Phase B — 자동 trigger 추가 (3일)

| Day | 작업 | 산출물 |
|---|---|---|
| 1 | transcript 파일 위치 패턴 검증 — `find ~/.claude/projects -name "<id>.jsonl"` | (확인) |
| 2 | `refresh-claude-md.sh` 스크립트 작성 + 로컬 테스트 (jq, curl, ANTHROPIC_API_KEY) | `.claude/hooks/refresh-claude-md.sh` |
| 3 | settings.json hook 등록 + 5 회 실제 종료 테스트 | settings.json patch |

**합격 기준**:
- SessionEnd 가 reason 별로 정확 발동 (clear / logout 등)
- proposal 파일 정상 생성 + 형식 일관
- API 비용 < $0.01 / 세션
- 다음 세션 시작 시 SessionStart hook 의 알림 정상

### Phase C — 안정화 + 자동 적용 옵션 (선택, 1주)

| 작업 | 산출물 |
|---|---|
| `/apply-refresh-proposal` skill — proposal 파일 읽고 사용자 confirm 후 적용 | `.claude/skills/apply-refresh-proposal/SKILL.md` |
| backup 자동 정리 cron 또는 hook | `.claude/hooks/cleanup-backups.sh` |
| Phase A/B 회고 — 정확도 / 노이즈 / 사용자 만족도 측정 | (보고서) |

---

## §9. 측정 + 회귀

### 9-1. 정량 지표

| 지표 | 목표 | 측정 방법 |
|---|---|---|
| 박제 정확도 (사용자 동의 비율) | > 80% | 후보 N 개 중 사용자가 적용한 비율 |
| 노이즈 false positive | < 20% | 박제 후보 중 사용자가 노이즈로 분류한 비율 |
| 박제 누락 (사용자가 후속 추가) | < 10% | 다음 세션에서 사용자가 수동 추가한 항목 |
| API 비용 (자동) | < $0.01 / 세션 | Anthropic 대시보드 |
| Hook 실패율 | < 5% | hook stderr 로그 |
| Proposal 검토 lag | < 1 일 | proposal 생성 → 사용자 적용 시간 |

### 9-2. 정성 지표

- 사용자가 "박제 잊어서 다음 세션에서 다시 설명" 횟수 감소
- CLAUDE.md 가 stale 정보로 오염되지 않음
- 새 세션 진입 시 CLAUDE.md 만으로 컨텍스트 복구 가능

### 9-3. 안티패턴 회귀 (체크리스트)

- ❌ Hook 이 CLAUDE.md 직접 수정 (proposal 파일 우회)
- ❌ proposal 파일이 누적 (사용자 검토 안 하고 방치)
- ❌ Slash command 호출 후 사용자 검토 없이 commit
- ❌ 노이즈 박제 빈발 (7 원칙 중 #1, #2 위반)
- ❌ Refresh 무한 루프 (skill 안에서 재호출)

---

## §10. 한 줄 요약

**Claude Code 의 SessionEnd hook (Stop 과 다른 진짜 세션 종료) + `/refresh-claude-md` slash command 두 entry. SessionEnd 의 한계 (stdout 무시 + transcript 직접 접근 X) 때문에 외부 스크립트가 transcript 파일 직접 read → Claude API 호출 → `CLAUDE.md.refresh-proposal.md` 출력 → 다음 세션 시작 시 사용자 검토 후 적용. 수동은 Claude 가 현재 context 로 직접 Edit + git diff 사용자 검토. 둘 다 7 원칙 (결정성/재방문 가치/위치/간결/링크화/stale/edit-vs-append) 준수. 안전: CLAUDE.md 직접 덮어쓰기 절대 금지 + 백업 7일 + 무한 루프 차단. Phase A 수동 (3일) → Phase B 자동 (3일) → Phase C 안정화 (1주). 합계 ~2주.**
