# Build Integrity Framework v1 — 빌드 무파손 보장 메커니즘

> **작성일**: 2026-05-04
> **트리거**: Phase B-12 v2 Fiora 박제 후 사용자 메타 피드백 — "관측 신호와 보장 메커니즘이 섞여 있다"
> **핵심 문장**: **read/grep 은 신뢰 신호이고, build/smoke/API 대조/asset 검증이 보장 장치다.**
> **적용 범위**: 모든 신규 계획서 (Champion / FX / RHI / Network / Sim) 의 의무 구조.
> **선행 문서**: `CODEBASE_COMPASS_SYSTEM.md` (탐색 인프라) + `AI_READINESS_RUBRIC.md` (정량 측정) + 본 문서 (보장 게이트).

---

## §0. 한 줄 요약

**계획서 작성 → 코드 박제 → 빌드 → 실행 → 기능 검증 단계마다 5 게이트 (Preflight → Plan Quality → Implementation → Verification → Learning) 통과 의무. 각 게이트는 "도구 호출 로그로 증명 가능한 항목" 만 포함 — Claude 의 자체 주장 ("자동 호출됩니다", "패턴 따랐습니다") 은 증거가 아니라 신호일 뿐. 게이트 통과 신호 = `Bash` / `Grep` / `Read` / `Edit` 도구 로그 + 빌드 산출물 + smoke 실행. 5 게이트 + 7 실패 분류 + Agent Contract = Phase B-12 의 v1→v2 정정 같은 사이클 시간 손실 차단.**

---

## §1. Agent Contract — 자동 로드 가정 금지

### 1.1 잘못된 가정 (제거 대상)

| 가정 | 실측 결과 | 진실 |
|---|---|---|
| "winters-skills/code/SKILL.md 는 자동 로드된다" | Phase B-12 세션 에서 Read 도구 호출 0회 | 시스템 컨텍스트는 **이름 + 1줄 description** 만. SKILL.md 본문은 사용자가 `/code` 슬래시 명시 또는 Skill 도구로 invoke 해야 로드 |
| "5-10분 의무 읽기는 자동 수행된다" | CLAUDE.md L484 텍스트는 컨텍스트 보유, 실제 5 파일 read 호출은 task 발생 후 휴리스틱 의존 | 의무 read 는 **계획서 첫 응답 도구 호출 로그** 에서 검증 가능해야 |
| "Compass System 으로 grep 없이 탐색 가능" | `_MODULE.md` 카운트 0 (Phase A 미진입) | 현재는 Encyclopedia 모델 (grep + Read 기반). Compass 는 설계만 박제 |

### 1.2 도구 로그로 증명할 수 있는 항목 (Agent Contract)

**계획서 작성 응답에서 다음 도구 호출이 로그로 보여야 정상**:

```
[필수]
- Read CLAUDE.md (이미 시스템 강제 로드, 인용 가능 여부로 검증)
- Grep "<feature_keyword>" --include="*.h" --include="*.cpp"   ≥ 5회
- Glob 자원 인벤토리 (Bin/Resource/.../...)                     ≥ 2회
- Read 가장 유사한 기존 패턴 파일                                ≥ 3 파일
- Read SkillDef.h / ChampionDef.h / 관련 Hook context           ≥ 1 파일

[권장]
- Read .md/plan/{도메인}/N_*.md (가장 최근 동일 패턴 계획서)     1 파일
- Bash "Test-Path"  (자원 실존 검증)                            ≥ 1회
- Bash "wc -l" 또는 "grep -c" (수치 검증)                       ≥ 1회
```

**0개 또는 1개 호출 시점에서 계획서 작성 시작 = 자체 추론 신호** — 사용자 검증 질문 1차 트리거.

---

## §2. Preflight Evidence Table — 탐색 결과 박제 의무

계획서 §1 또는 §2 에 다음 표 박제. 미박제 시 사용자 검증 단계 진입 불가.

| 항목 | 결과 | 명령/위치 |
|---|---|---|
| **Read 한 파일** | 18 파일 (예: B-12 v2) | `Read Client/Public/.../Zed/ZedFxPresets.h:1-19` 등 명시 |
| **Grep 패턴** | 16회 | `Grep "eChampion::" Engine/Shared` 등 명시 |
| **발견한 기존 인프라** | 7 챔프 패턴 (Riven/Ezreal 분기) | "ChampionTable 정적 / Registration 자체 모듈 두 패턴 공존" |
| **현재 API 시그니처** | `void OnCastFrame_BA(SkillHookContext& ctx)` | Read SkillHookContext.h L19-29 직인용 |
| **v1 / 중복 파일 존재** | 예: `FioraFxPresets.h` + `Fiora_FxPresets.h` 잔존 | `ls Client/Public/GameObject/Champion/Fiora/` 로 발견 |
| **Hook context 필드** | pWorld / casterEntity / casterTeam / pDef / pCommand / pFxMeshRenderer / pKeyOut / fDeltaTime | Read 시그니처 직인용 (헬싱키 가정 X) |
| **Asset 경로 실존** | 11 PNG / 19 anim / fbx 1 / wmesh 0 (변환 필요) | `Bash ls / Test-Path` 결과 박제 |
| **빌드 가드 위치** | WINTERS_MIN_SCENE 18 hit | `Grep -n "WINTERS_MIN_SCENE" Scene_InGame.cpp` |

**위 표 미박제 시점에서 §3 Plan Quality Gate 진입 불가**.

---

## §3. Plan Quality Gate — 계획서 품질 5 규칙

### 3.1 Placeholder 금지

❌ **금지 패턴**:
```cpp
// TODO: implement this
// (B-13 에서 본격)
/* 동일한 패턴으로 작성 */
```

✅ **허용**:
- `(void)ctx;` — Ezreal Gameplay namespace 처럼 의도된 stub (이유 1줄 주석 의무)
- 기능 분기 명시 (`if (slot == 0) { ... } else if (slot == 1) { /* B-14 polish */ ApplyDamage(...) }`)

### 3.2 Full Code 또는 Diff Hunk 의무

| 파일 크기 | 박제 형식 |
|---|---|
| **신규 ≤ 200 줄** | 전문 (h + cpp 모두) |
| **신규 > 200 줄** | 전문 + 함수별 anchor 표 |
| **수정 ≤ 50 줄 변경** | 수정 전/후 cpp 블록 (전체 함수 또는 logical 단위) |
| **수정 > 50 줄 변경** | Anchor 패턴 + before/after hunk + 삽입 위치 line number |

**Anchor 패턴 예시** (Scene_InGame.cpp 같은 큰 파일):
```
[Anchor] L807: `EntityID fioraEntity = CreateECSChampion(eChampion::FIORA, eTeam::Blue);`

Before (L807-L808):
    EntityID fioraEntity = CreateECSChampion(eChampion::FIORA, eTeam::Blue);
    m_FioraEntity = fioraEntity;

After (L807-L809):
#if !WINTERS_MIN_SCENE
    EntityID fioraEntity = CreateECSChampion(eChampion::FIORA, eTeam::Blue);
    m_FioraEntity = fioraEntity;
#endif
    EntityID jaxEntity = CreateECSChampion(eChampion::JAX, eTeam::Blue);
    m_JaxEntity = jaxEntity;
```

### 3.3 Hook Context 필드 실측 의무

❌ "ctx.pTarget" 같은 가정 필드 사용 → SkillHookContext.h 미실존 시 컴파일 실패.

✅ Read [SkillHookContext.h](Client/Public/GamePlay/SkillHookContext.h:19-29) / [VisualHookRegistry.h](Client/Public/GamePlay/VisualHookRegistry.h:10-18) / [GameplayHookRegistry.h](Shared/GameSim/Systems/GameplayHookRegistry.h:9-19) 결과 직인용 + 사용 필드만 코드에 등장.

### 3.4 Asset Path Test-Path 의무

```bash
# 모든 assetPath 박제 전 1회 실행
ls "Client/Bin/Resource/Texture/Character/Jax/jax.fbx"
ls "Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_q_*.png"
```

미실존 발견 시 → 변환 단계 (D-0) 또는 텍스처 추출 단계 명시. 박제 후 발견은 빌드 후 런타임 실패.

### 3.5 vcxproj / filters 등록 의무

```xml
<!-- ClCompile (cpp) + ClInclude (h) 두 영역 모두 -->
<ClCompile Include="..\Private\GameObject\Champion\Jax\Jax_Registration.cpp" />
<ClInclude Include="..\Public\GameObject\Champion\Jax\Jax_Components.h" />
```

미박제 → cpp 컴파일 단위 미포함 → static initializer 호출 안 됨 → BanPick 미노출 → "자동 작동" 가정 깨짐.

---

## §4. Implementation Gate — 박제 시 의무

### 4.1 최소 수정 우선

원인 확정 전 신규 시스템 신설 금지. v1 → v2 정정 사이클 (B-12) 같은 큰 패치는 사용자 검증 신호 받은 후 진행.

### 4.2 기존 dirty work 보존

**v1 박제분 잔존 시 cleanup 항목으로 분리** — 새 계획서에 1-2 줄 명시:

```markdown
## §X. v1 잔존 cleanup (옵션)

- `ChampionTable.cpp` L18 의 FIORA 정적 entry — Fiora_Registration.cpp 가 자체 등록하므로 중복.
- `Client/Public/GameObject/Champion/Fiora/FioraFxPresets.h` (no underscore) — `Fiora_FxPresets.h` 와 중복. vcxproj 제거 후 파일 삭제.
- Cleanup 시점: B-13 (Jax) 또는 B-15 (FioraState 본격) 합류 시.
```

### 4.3 Public Header / Include / DLL 규칙

CLAUDE.md L880-L920 "Include 컨벤션" + "ComPtr 규칙" 인용 준수:
- 공개 헤더는 flat path 또는 fully qualified
- `Microsoft::WRL::ComPtr<T>` FQN + `#include <wrl/client.h>` 명시
- `bool_t` / `i32_t` 등 namespace alias 는 namespace 밖 사용 금지

### 4.4 Source-of-Truth 전환 보류

신규 enum 도입 시 기존 caller 직접 세팅 필드 (예: `bBillboard`) 즉시 source 만들지 X. 별도 cleanup phase 로 분리.

---

## §5. Verification Gate — 7 실패 분류 + 게이트별 smoke

### 5.1 7 실패 분류

| # | 분류 | 발견 시점 | 신호 |
|---|---|---|---|
| **F1** | Compile error | MSBuild | `error C2065 / C2672 / C7568` 등 |
| **F2** | Link error | MSBuild | `LNK2019 unresolved external` / `LNK1104 file lock` |
| **F3** | Exe/PDB lock | MSBuild | `LNK1104` 의 이미 실행 중 server.exe 같은 케이스 — **컴파일 에러 아님** (구분 필수) |
| **F4** | Shader compile break | 런타임 | `D3DCompileFromFile` `__debugbreak` 진입 |
| **F5** | Runtime immediate exit | 8s smoke | exe 1초 내 종료 (assert / unhandled exception) |
| **F6** | Feature no-op | feature smoke | Cast 했는데 데미지 0, FX 안 보임, 등록은 됐는데 Dispatch 안 됨 |
| **F7** | Network path bypass | 2-client smoke | Server 가 EZREAL 하드코딩 → 다른 챔프 픽도 EZREAL 로 처리됨 같은 우회 |

### 5.2 게이트별 smoke 의무

| 게이트 | 명령 | 통과 조건 |
|---|---|---|
| **G1: Client build** | `MSBuild Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64` | error 0 / warning ≤ baseline |
| **G2: Full solution build** | `MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64` | error 0 — 단 LNK1104 실행 중 lock 은 F3 분류 (별도 처리) |
| **G3: Lock 분리** | `taskkill /F /IM WintersServer.exe /IM WintersGame.exe` 후 G2 재시도 | F2 vs F3 분리 진단 |
| **G4: 8초 실행 smoke** | `start /B WintersGame.exe & timeout /T 8 & taskkill /F /IM WintersGame.exe` | exe 가 8초 동안 살아있음 (1초 내 종료 X) |
| **G5: Feature-specific smoke** | BanPick → 신규 챔프 픽 → cast 1회 → Output 창 `[Jax BA] dmg=` 같은 로그 확인 | 도메인 별 — Jax/Fiora 는 5 슬롯 cast 모두 + FX 1회 이상 |
| **G6: Server/network 경로** | Server 실행 + Client 2 instance + 양쪽에서 cast → `CGameplayHookRegistry::Dispatch` server 측 호출 확인 | Phase 04a v2 D-1 진입 후 — 본 시점 N/A |

### 5.3 빌드 성공 ≠ 기능 완성

**중요한 분리** (Phase B-12 v2 사례):
- Fiora 컴파일 ✅ (Client.vcxproj G1 통과)
- Fiora 8초 smoke ✅ (G4 통과)
- Fiora cast 시 데미지 적용 — Skill ns (`Fiora::OnCastFrame_BA`) 만 동작 ✅
- Fiora **Gameplay** ns 본체 = `(void)ctx;` stub → server 측 권위 시뮬 미구현 ❌ (G6 미통과, 04a v2 D-1 prerequisite)

**계획서는 4 단계 명시 의무**:
- [ ] 등록됨 (Registry::Add 호출)
- [ ] Client visual 동작 (FX/anim)
- [ ] Shared gameplay hook **본체 구현** (단순 `(void)ctx;` stub 이면 명시)
- [ ] Server dispatch 경유함 (04a v2 D-1 합류 시점 별도 검증)
- [ ] 2-client smoke 통과 (04a v2 D-3 시점)

---

## §6. Learning Update — 사이클 종료 의무

### 6.1 박제 후 갱신 4 항목

| 항목 | 위치 | 작성 형식 |
|---|---|---|
| **CLAUDE.md Gotcha 후보** | CLAUDE.md §Gotchas (L1240+) | 1줄: "신규 함정 — `<재현 조건>` → `<증상>` → `<해결>`" |
| **Skill Gotcha 후보** | `winters-skills/code/SKILL.md` §E 사례 | 사례 번호 + 본문 + 교훈 1줄 |
| **Memory 후보** | `memory/feedback_*.md` 신규 | 도메인 사실 (재발 방지) — 구체 사례 + 일반화 |
| **단일 재발 방지 규칙** | 본 framework 의 §5.1 표 | 새 실패 분류 추가 또는 게이트 강화 |

### 6.2 본 framework 의 self-update 트리거

**다음 사이클에서 framework 갱신해야 할 신호**:
- 5 게이트 모두 통과했는데 사용자가 추가 결함 발견 → 게이트 누락
- 7 실패 분류로 못 잡히는 새 실패 모드 → 분류 추가
- Agent Contract 항목 충족했는데 v1→v2 같은 큰 정정 발생 → Contract 항목 부족

---

## §7. 계획서 템플릿 (의무 적용)

모든 신규 계획서는 다음 8 섹션 의무:

```markdown
# Phase X (목표) — vN

## §0. Agent Contract Evidence
- 도구 로그 박제 (Read N 파일 / Grep N회 / Bash N회)

## §1. Preflight Evidence Table
- §2 의 표 박제

## §2. Plan Quality Gate Status
- [ ] Full code or diff hunk
- [ ] No placeholder
- [ ] Hook context fields verified
- [ ] Asset paths Test-Path verified
- [ ] vcxproj/filters registration

## §3. 자원 변환 / 신규 파일 박제 (전문)

## §4. 수정 영역 (Anchor + Before/After hunk)

## §5. Implementation Gate
- 최소 수정 / dirty work 보존 / Source-of-truth 보류

## §6. Verification Gate
- G1 ~ G6 smoke 명령 + 통과 조건

## §7. Learning Update
- 박제 후 갱신 4 항목 후보
```

---

## §8. 본 Framework 의 적용 시점

| 시점 | 적용 |
|---|---|
| **2026-05-04 (현 시점)** | 본 문서 박제 + 10_JAX 계획서 첫 적용 |
| **B-13 Jax 박제 시** | §0~§7 전 섹션 충족 시도 |
| **B-13 완료 후** | 본 framework §6.2 self-update 트리거 평가 |
| **B-14 Ashe / 후속 챔프** | 본 framework 그대로 미러 |
| **04a v2 D-1 합류** | G6 (server/network 경로) 본격 활성 |

---

## §9. 참고 — 본 framework 가 회피하는 사고 사례

### 사례 1: Phase B-12 v1 → v2 정정 (2026-05-04)

- **증상**: v1 계획서 박제 후 사용자 질문 — "Ezreal 서버 동기화 맞아?"
- **원인**: Riven 패턴 (legacy castFrame fallback) 채택 → `castFrameHookId = 0` → GameplayHook Dispatch path skip
- **본 framework 로 회피**: §1.2 Agent Contract 의 "Read SkillDef.h hookId 필드 확인" + §2 Preflight 의 "기존 인프라 — Riven vs Ezreal 분기" + §6 Learning 의 "Skill ns vs Gameplay ns 본체 분리" 박제 → v1 시도 자체 차단
- **소요**: 1 사이클 손실 → framework 적용 후 0 사이클

### 사례 2: Phase B-12 빌드 시 LNK1104 (2026-05-04)

- **증상**: Full solution build 실패
- **원인**: `Server/Bin/Debug/WintersServer.exe` 이미 실행 중 → 파일 잠금
- **본 framework 로 분류**: F3 (Exe/PDB lock) — F2 (link error) 와 분리. G3 게이트 의무 (taskkill 후 재시도) 로 F3 vs F2 진단
- **현재 상태**: Fiora 컴파일 ✅, Server.exe lock 분리 ✅

### 사례 3: Ezreal::Gameplay namespace = (void)ctx; stub (2026-05-04 발견)

- **증상**: Server 가 GameplayHook Dispatch 호출 안 하고 (현재 미구현), Client 만 호출 → Skill ns 만 발화
- **본 framework 로 분류**: F7 (Network path bypass) — Phase 04a v2 D-1 진입 prerequisite
- **명시**: 5.3 의 4 단계 체크리스트로 stub 상태 박제

---

## §10. 단일 핵심 문장 (재인용)

> **read/grep 은 신뢰 신호이고, build/smoke/API 대조/asset 검증이 보장 장치다.**

- **신뢰 신호** (응답 첫 1/3 안에 등장해야): Agent Contract §1.2 의 도구 호출
- **보장 장치** (게이트 통과 의무): G1 ~ G6 + 7 실패 분류 + Learning Update
