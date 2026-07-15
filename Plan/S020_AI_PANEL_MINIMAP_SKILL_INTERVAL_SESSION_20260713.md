Session - AI Debug 패널 스크롤/비대 해소, 미니맵 Z축 전단(top/bottom 압축) 수정, 봇 스킬 5초 간격 게이트를 반영한다.

날짜: 2026-07-13 (Claude 레인). 번호 근거: S017(AIResearch)/S018(Ezreal 투사체)=Codex 완료, S019=Codex 프로파일러 산출물이 선점 → 본 세션 = S020.

불변식: Bot AI는 GameCommand 생산자다. 이번 스킬 게이트는 명령 **생산 억제**만 하며 HP/쿨다운/이동 truth를 건드리지 않는다.

## 0. Codex 레인 동기화 (조사 확정 사항)

- S017 = `Tools/AIResearch/` 신설 (AiEpisodeV1 스키마/브릿지 manifest+SHA 검증/모방 랭킹 NumPy baseline/의사결정 trace 코덱/영향맵·리서치타입 프로브). 신규 헤더 3종: `ChampionAIInfluenceMap.h`(9×9 4레이어, 연구/디버그 전용 — 의사결정 미배선), `ChampionAIPerception.h`(관측 사실 스냅샷 — `ChampionAIContext`의 실체가 됨, `ChampionAISystem.cpp:45` alias), `ChampionAIResearchTypes.h`. `CanChampionAIObserveTarget` 관측 게이트가 EmitSkillCommand에 이미 존재.
- S018 = Ezreal W 프리즈/커서/상점 회귀 (`.md/build/2026-07-13_EZREAL_W_CURSOR_SHOP_REGRESSION_REPORT.md`) — 자동 검증 PASS, **사용자 인게임 게이트 대기로 Codex 중단 상태**. 본 세션 변경은 그 세 레인과 코드 교차 없음(패널/미니맵/ChampionAI 게이트).
- 크로노 브레이크: S015 이후 변화 없음 — Checkpoint/ 미추적+GameRoom 미커밋 유지, 인게임 게이트(S015 RESULT §3) 여전히 대기.

## 1. 반영해야 하는 코드 (본 세션에서 적용 완료)

### 1-1. Client/Public/UI/MinimapPanel.h — 미니맵 Z축 전단 제거

증상: 탑이 아래로, 바텀이 위로 — 세로(Z)만 중앙 압축. 원인: `MinimapProjection` 기본값의 Z 반대각 156.69 vs X 반대각 94.385 → 기저가 비직교·비등배(전단), world-Z 1.66× 부풀림. 2026-06-24 RESULT에서 진단됐던 결함이 S006 X축 재보정 때 Z만 남아 재유입.

기존 코드:

```cpp
Vec2 vWorldAtUv00{ 104.50f, 156.69f };
```

아래로 교체:

```cpp
Vec2 vWorldAtUv00{ 104.50f, 94.385f };
```

(주석 포함. 중심 (104.5, 0)과 X 스팬 보존 → 넥서스 축(가로) 보정은 유지되고 Z만 탈전단.) 전 소비자 자동 정합: 아이콘 `ProjectWorldToMinimapUv`, 클릭 점프 `MinimapUvToWorld`(같은 struct의 해석적 역변환), 카메라 바운즈, FoW 오버레이(`Scene_InGameLifecycle.cpp:298` 런타임 복사).

### 1-2. 봇 스킬 5초 간격 게이트 (Shared/GameSim)

원인 확정: R 남발 = 기본 콤보 플랜 `s_Default[4]=R`의 루프 재시작(완료→클리어→다음 틱 재시작) + Yone R 훅(enemyHp≤50% 매 결정마다). 스킬별 쿨다운(`IsSkillReady`) 외에 AI측 간격 게이트가 없었음. harass `skillRules`에는 R이 원래 없음(전 18 프로필 확인).

설계 (gotchas 2026-07-12 `hard safety -> active commitment -> new utility` 준수): **신규 시전(새 콤보 시작·단발 스킬)만 5초 간격**, 진행 중 커밋(활성 콤보 스텝, 다이브 페이즈, stage-2 재시전)은 예외 — 커밋된 연계는 절대 끊기지 않는다. 콤보 완료 시 `ClearChampionAICombo`가 comboTarget을 비우므로(`ChampionAISystem.cpp:1014-1041` 확인) 다음 사이클 재시작이 시작 게이트에 걸린다 → 버스트 사이 최소 5초.

- `ChampionAIComponent.h`: `fDiveExtraBATimer` 아래 `f32_t fSkillCastCooldownTimer = 0.f;` 추가 (POD 유지 — 키프레임 레지스트리 정합; blob 레이아웃 변경되나 영속 blob 자산 없음).
- `ChampionAISystem.cpp` 4곳:
  - 상수 `kChampionAISkillCastMinInterval = 5.f` (`kChampionAILastSeenMemoryTicks` 아래).
  - `EmitSkillCommand`: `bStage2` 계산 직후 — `!bStage2 && !bCommittedSequence && timer > 0` → `SkillCooldown` 차단 (`bCommittedSequence = comboTarget != NULL || divePhase != None`); push 성공 시 `!bStage2`면 타이머 5초 장전 (커밋 중 시전도 장전 → 버스트 종료 후 간격 보장).
  - `TryEmitAttackChampionCombo`: `bWasActive == false`(신규 시작)이고 타이머>0이면 시작 거부 (`SkillCooldown` 사유) — false 반환이라 봇은 평타/추격으로 계속 싸움.
  - 틱 타이머 블록(`fDiveExtraBATimer` 감쇠 옆): `fSkillCastCooldownTimer` 동일 방식 감쇠 (`tc.fDt`, 결정론).
- 커버 경로: harass/콤보(전 챔피언, Kalista FateCall·LeeSin·Sylas 포함)/Jax 다이브 진입/Yasuo 궁·파밍 Q/Yone R — 전부 `EmitSkillCommand` 초크포인트 경유. 유일한 우회(Yone E2 soul-return 수제 push, `:3430` 근처)는 stage-2 성격이라 의도적으로 비게이트 유지.

### 1-3. Client/Private/UI/AIDebugPanel.cpp — 스크롤 버그 + 컴팩트화

원인: 콘텐츠 ~1340px vs 창 560px + ScrollY 고정 높이 테이블 3개가 마우스휠을 소비 → "스크롤 죽음" 체감. F10(ChampionTuner)/F5(ModelAnimPanel)의 검증된 관용구로 정렬:

- 창 기본 820×720 + `SetNextWindowSizeConstraints(560×320 ~ 1400×1200)`.
- 봇 수집을 테이블 렌더에서 분리(사전 ForEach → `botRows`) → **Bot 콤보 셀렉터 상시 노출** (테이블 안 열어도 선택 가능).
- 11열 전체 테이블은 `CollapsingHeader("All Bots")` (기본 접힘) 뒤로.
- `Selected AI`(판독+Actions+Skills) = 기본 열림 헤더, `Runtime Tuning`(슬라이더 15개, ~360px) / `Decision Trace` / `Server Minions` = 기본 접힘 헤더.
- 접힘 상태 기본 높이 ~300px, 열린 섹션은 창 휠 스크롤이 자연 동작 (한 시점에 ScrollY 테이블 최대 1개 노출).
- `#include <vector>` 추가.

## 2. 검증

검증 명령:

```text
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m /nr:false /v:minimal /clp:ErrorsOnly;Summary
Tools\Bin\Debug\SimLab.exe   (기본 1800틱/seed42 — 전 프로브 + same-seed/seed+1 계약)
git diff --check
```

기대 로그: SimLab 전 프로브 PASS (특히 `RunChampionAIStateGateCommitmentProbe` — 다이브/콤보 커밋 예외로 무회귀; `RunKeyframeRestoreDeterminismProbe` — 컴포넌트 필드 추가 후에도 왕복 일치), 빌드 에러 0.

수동 확인 (사용자 게이트):
- F9: 창이 720px로 열리고 휠 스크롤 동작, Bot 콤보로 선택, 접힘 헤더 4종 확인.
- 미니맵: 탑/바텀 라인 챔피언이 미니맵 대각 코너 방향으로 정상 배치(중앙 압축 해소), 미니맵 클릭 점프 좌표 일치, FoW 오버레이 정합.
- 라인전 관전: 봇 스킬(특히 R) 버스트 사이 ≥5초, 콤보는 내부에서 정상 연계, 다이브 무결.

롤백 범위: MinimapPanel.h 값 1개, ChampionAIComponent.h 필드 1개, ChampionAISystem.cpp 4블록, AIDebugPanel.cpp 렌더 함수 — 전부 이 세션 diff 단위 원복 가능.

## 3. Next slice

- 5초가 라인전 체감상 과하면: 슬롯별 차등(R만 게이트) 또는 15번째 튜닝 노브(F9 슬라이더) 승격 — 16 §4.3 6단계 절차.
- 패널 후속: 21번 문서 D1(score breakdown)/D2(why-not)가 이 컴팩트 구조 위에 얹힘.
- S018 Ezreal/커서/상점 인게임 게이트 + S015 크로노 게이트는 사용자 수동 확인 대기 유지.
