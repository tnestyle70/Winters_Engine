# 2026-07-18 Data-Driven 스킬 구조 인수인계서 (W 스킬 미해결 + 완전 구조 설계 착수용)

> **상태 정정(2026-07-18 후속 세션):** 이 문서의 “W 미해결” 상태와
> Viego W lock 0.7초 단독 원인 판정은 후속 자동 검사로 대체됐다. 최신 코드 반영,
> 17×85/13-stage SimLab, Debug/Release 빌드, 남은 activation schema 부채는
> `2026-07-18_IRELIA_VIEGO_EZREAL_STAGE_INPUT_REGRESSION_RECOVERY_RESULT.md`를 우선한다.

```text
Session - Data-Driven 회귀 전수 정리 + W 스킬 미해결 판정 프로토콜 + 완전 구조 설계 착수점 인계
좌표: 축 C7 권위와 정합성 · C8 검증이 병목
관련: 2026-07-18_IRELIA_W_STAGE_BOOKKEEPING_HYPOTHESIS_FIX_PLAN.md,
      2026-07-18_ITEM_SHOP_DATA_DRIVEN_REGRESSION_FIX_PLAN.md,
      2026-07-17_DATA_DRIVEN_100_PERCENT_AND_MAIN_MENU_PROFILE_PLAN.md
```

## Current sequence

```text
(끝난 것) 상점 회귀 수정 → targetMode 오버레이 제외 수정 → 전수 타이밍 감사
-> (현재 미해결) IRELIA W hold/release 이동락 + 런타임 스테이지 북키핑
-> (다음 세션) 이 인계서로 W 원인 판정 → 완전 구조 설계 → 전 챔피언 재검증
```

## Goal / Non-goals / Why this order

- **Goal**: 다음 세션이 (1) W 미해결 버그를 BP/trace 없이 로그로 판정하고, (2) Data-Driven 스킬 구조의 "행동 계약" 구멍을 메우는 완전 구조를 설계할 수 있도록, 현재까지의 확정 사실·미해결 지점·판정 도구를 한 문서에 고정한다.
- **Non-goals**: 이번 인계서는 코드를 바꾸지 않는다(빌드 검증만). W 수정 자체는 판정 후.
- **Why this order**: W는 근본 원인이 아직 3갈래(H1 아톰 / H3~H5 런타임 / H6 서버)로 열려 있고, 정적 분석은 소진됐다. 계측이 자백해야 진행 가능하므로 판정 도구를 먼저 인계한다.

## 1. 결정 기록 (왜 Data-Driven이 깨졌나 — 구조 판정)

```text
① 문제: 스킬의 "홀드/2타" 같은 행동은 값이 아니라 [입력 관례 + stageCount 신호 +
   스테이지 머신 + targetMode 계약]의 합성이다. 100% 데이터 이관은 이 중 "값"만 옮기고
   나머지 계약을 반쪽만 데이터화했다. 그 반쪽 경계에서 회귀가 전부 터졌다(실측: E 역주행,
   W 이동락, 아이템 0원, targetMode 15+ 변질).
② 순진한 해법의 실패: SkillTable 전면 복원 = 100% 플랜 검증기(:1625 kItems/ResetToDefaults
   부재)를 깨고 150챔피언 스케일 목표를 되돌린다. 반대로 계속 값-취급 = 계약이 다시 깨진다.
③ 메커니즘: 값(cooldown/mana/range/lock/window)은 팩 권위 유지, 계약(targetMode·홀드
   스테이지 의미)은 스키마가 표현 가능해질 때까지 코드 권위. 경계를 감사로 상시 감시.
④ 대조: LoL 실물·업계 표준도 "표시/튜닝은 데이터, 행동 계약은 코드/스크립트". targetMode를
   Contextual로 뭉갠 저작이 원죄이지 데이터화 자체가 원죄가 아니다.
⑤ 대가: targetMode·홀드가 당분간 코드 권위로 남아 "100% 데이터"가 아니다. champions.json의
   Conditional 오저작은 서버측에 잔존(클라는 오버레이 제외로 무해화). 스키마에 per-stage
   targetMode + 홀드 활성화 타입이 생기면 재이관 가능 — 그게 "완전 구조"의 정의.
```

## 2. 확정 사실 (rg 실증, 다음 세션 신뢰 가능)

### 2-1. 이미 적용된 수정 (working tree, 미커밋)
- `Client/Private/GamePlay/SkillRegistry.cpp:51-64` `ApplyAuthoredGameplayData`: **targetMode 병합 제거**(주석으로 경계 명시) + `def.stageCount = max(def.stageCount, skill.stageCount)`. → E 역주행/타겟 변질 15건 해소 경로.
- `Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json`: 칼리스타 R stage2(cast4/rec10) 복원.
- 팩 재생성 해시 `0x8C5D9212` (클라·서버·매니페스트 동기).

### 2-2. Codex가 방금 추가한 것 — 리뷰 결과
- `Client/Private/GamePlay/SkillRegistry.cpp:227` `AuditDataDrivenContracts()` + `ChampionModuleBootstrap.cpp:41` 호출(전 챔피언 등록 직후 = 올바른 위치).
- **하는 일**: 부팅 1회, 등록 아톰 vs `champions.json` 생성 테이블(17챔프×85슬롯) numeric/target/stage 파리티 비교 → `[DataContract]` 로그 + PASS/FAIL. 2스테이지 스킬은 `[DataContract][TwoStage]` 상세 라인.
- **평가**: 방향 정확 — 지난 세션 제안한 "아톰 파리티 덤프"와 동일. 단 **3가지 갭**:
  - **갭A (치명)**: `#if !defined(_DEBUG) return true`(:229) — **Release 무효**. 사용자는 Release 3클라로 검증 중 → 이 로그 안 나옴. W 판정에 쓰려면 Debug 클라 1개를 띄워 출력창을 읽어야 함.
  - **갭B (이번 W버그 못 잡음)**: 정적 아톰만 검증. W 릴리스 실패의 실제 원인 층(런타임 스테이지 북키핑·입력 에지)은 비계측. 아톰이 정상이면 PASS를 찍으면서 W 버그는 생존.
  - **갭C (근원 은폐)**: targetMode가 Conditional이면 mismatch에서 제외(:314-317)하고 `conditionalSchemaLoss` 카운트로만 남김. 우리 오버레이-제외 수정과는 정합하나, 감사가 "PASS"라 말해도 champions.json 오저작은 그대로.

## 3. 미해결: IRELIA W hold/release 이동락 — 판정 프로토콜

### 3-1. 확정된 정상 경로 (rg 실증)
- 입력: `Scene_InGameInput.cpp:493`(프레스=1타)·`:500`(릴리스=2타, 조건 `IsKeyReleased && (s_bWReleasePending || HasPendingSkillStage)`).
- 프레스 북키핑: `Scene_InGameLocalSkills.cpp:2179` `if (gameData.stage.stageCount == 2) { currentStage=1; stageWindow=...; }`.
- 감쇠: 클라 `Scene_InGame.cpp:1256-1260`(`stageWindow -= dt`), 서버 `SkillCooldownSystem.cpp:208`.
- 서버 2타 판정: `CommandExecutor.cpp:2343` `bStage2 = requestedStage2 && currentStage==1 && stageWindow>0 && IsSkillTwoStage`.
- 등록 저작: `Irelia_Registration.cpp:85` `s.stageCount = 2`(worktree 확인). 세 원천(등록·champions.json·비주얼팩) 모두 W=2 정합. 중복 등록/자동 루프 0건.

### 3-2. 사용자 최신 관측
"`if (m_bNetworkAuthoritativeGameplay)` 블록에 들어가 return true 하는데, 릴리스 분기(`Input.cpp:500`)에 안 들어온다."
→ 논리적 귀결: 릴리스 조건 OR 양쪽 false = **프레스에서 `currentStage=1`이 안 섰다**. 그런데 서버는 W1 락(5s)을 걸었으니 커맨드는 나감.

### 3-3. 미확정 갈림길 (다음 세션 1순위 — 아직 관측 안 됨)
`Scene_InGameLocalSkills.cpp:2181` `slotState.currentStage = 1;` 줄이 실행되는가?
- **판정 자동화**: Debug 클라 1개 실행 → InGame 진입 → 출력창에서 `[DataContract][TwoStage] champ=1 slot=2`(IRELIA=1, LoLMatchContext.h:8) 라인 확인.
  - 라인 있고 stageCount=2 → **아톰 정상, H1 기각** → 원인은 런타임(H3 dt 스파이크 / H4 입력 에지 유실 / H5 스테일 pending). 가설·수정 코드는 `..._HYPOTHESIS_FIX_PLAN.md` V3a/V4/V5.
  - 라인 없음/stageCount=1 → **H1 확정** → `Add`에 조건부 BP(champ==IRELIA&&slot==2) 첫 호출 콜스택.
- **주의**: 갭A 때문에 이 로그는 **Debug 전용**. Release로는 안 나온다.

### 3-4. 남은 계측 부채 (Codex 감사가 안 덮은 절반)
런타임 게이트 4곳이 전부 무음 리턴이라 장님 상태. 다음 세션이 넣어야 할 bounded trace:
```text
[WGate] press dispatched=? stageCount=? cd=?     (LocalSkills 프레스 경로)
[WGate] bookkeeping set stage=1 window=4.0        (:2181 직후)
[WGate] window expired dt=? remaining=?           (Scene_InGame.cpp:1258)
[WGate] release edge=? pending=? sWRP=?           (Input.cpp:500)
```
+ 서버측: `[Command] cast-skill reject reason=...`는 이미 존재(CommandExecutor) — Debug 서버 콘솔에서 W 릴리스 직후 reason 읽기.

## 4. 별건 확정 버그 (targetMode와 무관, 데이터 저작)

- **VIEGO W**: `SkillGameplayDefs.json:6784` + `champions.json` viego.w `lockSeconds[0]` = HEAD 4.0(홀드 윈도우) → 0.7. 홀드가 0.7초에 끊김. **수정 = 두 JSON 모두 4.0 복원 + 생성기 2종(build_champion_game_data.py + Build-LoLDefinitionPack.py) 재실행 + 재빌드**(이중 코드젠 gotcha). W 판정 세션과 같은 빌드 사이클로.

## 5. 완전 구조 설계 착수점 (다음 세션 본론)

행동 계약을 데이터로 안전하게 옮기려면 스키마에 3가지가 필요하다 — 이게 "완전 구조":
1. **per-stage targetMode**: 현 스키마는 스킬당 1개 shape만. 이렐 W(방향)·E(지면)처럼 스테이지별 상이한 타겟을 표현 못 해 Conditional로 뭉개짐.
2. **홀드/차지 활성화 타입**: 현재 "홀드"는 stageCount==2 + 입력 관례의 암묵 합성. Jax W 예외가 `ShouldSuppressCastActionState`로 코드 하드코딩된 게 증거. 활성화 정책(즉발/홀드/차지/토글)을 명시 enum으로.
3. **fail-loud 계약**: 팩 미스가 현재 0값 무음(쿨0 스팸·락0). 누락은 로그+거부로.
- 이관 순서(재발 방지): 소비자 전수 목록 → HEAD 값 전량 물질화 → 기계 파리티(Codex 감사를 Release에도, 갭A 수정) → fail-loud → 단일 생성 파이프라인 → 원천 삭제.

## Files touched (이 인계서 세션)

- 신규: `.md/plan/2026-07-18_DATA_DRIVEN_SKILL_STRUCTURE_HANDOFF.md` (본 문서, 코드 무변경)
- 빌드 검증만: `Client/Include/Client.vcxproj` (Codex 감사 컴파일 확인 — 소스 무수정)
- 코드 반영 없음. Codex working-tree 편집(SkillRegistry 감사 등)은 Codex 소관, 본 세션 미개입.

## Verification (예측 우선)

```text
예측:
- Codex 감사(AuditDataDrivenContracts)는 Debug 컴파일 통과한다(순수 읽기+OutputDebugString).
- Debug 클라 1개 실행 시 [DataContract][Client] PASS 또는 numeric/target mismatch 라인이
  부팅 로그에 뜬다. IRELIA slot2 TwoStage 라인 유무가 W H1을 판정한다.
- 이 인계서는 코드 무변경이라 SimLab 골든·런타임 불변.
- Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다(W/서버 수정 시 유지).

검증 명령:
- msbuild Client.vcxproj Debug /m:1 (flatc 충돌 시 1회 재시도 — 알려진 gotcha).
- (다음 세션) Debug 클라 실행 → InGame 진입 → DebugView/VS 출력창에서 [DataContract]/[WGate] grep.

미검증:
- W 원인 3갈래(H1/H3~H5/H6) — Debug 클라 실행+로그 판독 대기(다음 세션).
- Codex 감사의 실제 런타임 출력 — 헤드리스로 InGame 진입 불가, 사용자 실행 필요.

확인 필요:
- Codex 세션이 SkillRegistry/Input/LocalSkills를 계속 편집 중 — 다음 세션 착수 시 앵커 재확인.
- 갭A(감사 Release 무효) 수정 여부 = 사용자 결정(Release로 볼지 Debug로 볼지).
```

## Next slice (다음 세션 진입점)

1. Debug 클라 1개 실행 → `[DataContract]` 로그로 W H1 판정(아톰 정상/붕괴).
2. 아톰 정상이면 §3-4 [WGate] trace 4종 반영 → W 1회 재현 → H3/H4/H5 확정 → `..._HYPOTHESIS_FIX_PLAN.md` V3a/V4/V5 중 해당 버전 적용.
3. VIEGO W 데이터(§4) 같은 사이클에 수정.
4. 완전 구조(§5) 설계 문서 착수 — per-stage targetMode + 활성화 타입 enum + fail-loud.
```
