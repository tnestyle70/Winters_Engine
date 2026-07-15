Session - 07의 영향맵 설계를 실현 트랙(Shared/GameSim, 결정론 CPU)으로 재정착시켜 turretDanger 스칼라를 공간 필드로 일반화한다.

작성일: 2026-07-12 (검증 반영판 — 갱신 위상 불변식·필드 소유권 확정 추가)
성격: 축 설계 문서 (마스터 = 18). `07_STAGE5_INFLUENCE_MAP.md`를 개념 출처로 승계하되 배치와 실행 모델을 교체(supersede)한다.

불변식: Bot AI는 GameCommand 생산자다. 영향맵은 **읽기 전용 공간 요약**이며 gameplay truth가 아니다.

## Goal

"어디가 위험한가 / 어디로 물러나야 하는가 / 어디가 기회인가"를 좌표 함수로 만든다. 현재 유일한 공간 신호인 `turretDanger` 스칼라 1개를, 챔피언/터렛/미니언 위협이 합산된 결정론 필드로 일반화한다.

## Non-goals

- GPU Compute/HLSL 실행 (07의 `Shaders/AI/InfluenceMap.hlsl` 방향 폐기 — 아래 "07 대비 변경" 참조). GPU는 클라 **시각화**에만 허용.
- 전맵 Vision Map 레이어의 독자 구현 (시야는 19 R1의 FOW 게이트와 소스를 공유 — 이중화 금지).
- 미니맵/UI 기능. 오브젝트(드래곤/바론) 기회 레이어는 오브젝트 시스템 존재 여부에 따라 v3 이후.

## 07 대비 변경 (supersede 근거)

| 07 설계 | 본 문서 | 근거 |
|---|---|---|
| Engine측 `CInfluenceMapSystem` + GPU compute | `Shared/GameSim` 결정론 CPU 시스템 | 의사결정 본체는 Shared/GameSim (16 §1.2); GPU float 연산은 결정론 계약과 리플레이/SimLab 재현을 깬다 |
| 140×140 하드코딩 | 그리드 스펙 주입 (codex 스캐폴드의 `GridMapDesc` 아이디어 승계) | 맵/모드 독립 |
| 매 200ms 전체 재계산 or 감쇠 블러 | 절대 틱 위상 고정 재계산부터 시작 (아래 불변식) | 감쇠 블러는 부동소수 누적 드리프트 위험 — 먼저 재계산으로 정확성, 최적화는 측정 후 |
| ImGui 직접 렌더 | 스냅샷 경유 다운샘플 → 클라 오버레이 (21) | 서버 권위 디버그 경로 준수 |

07에서 승계: 5레이어 분류 중 Team/Threat/Opportunity/Terrain (**Vision은 제외** — 19 R1의 FOW 소스와 공유, Non-goals 참조), Map Fusion 가중합, 레이어별 갱신 주기 차등, A* 도달시간 전파 아이디어(v2+).

## 현재 코드 근거 (rg 검증, 2026-07-12)

- 공간 필드 부재: `rg 'Influence|ThreatMap|DangerMap|HeatMap'`은 계획 문서와 로컬 람다(`isThreatInsideDefenseZone`, ChampionAISystem.cpp:3204 근처)만 히트. 코드에 그리드/필드 자료구조 없음.
- `ComputeTurretDanger` (`ChampionAISystem.cpp:1505` 근처): 구조물 선형 순회 → 스칼라 `[0..1.25]` + bool. 유일한 위협 신호.
- 이동 기반: Engine `CNavGrid`(512×512, 0.5u/셀)는 `IWalkableQuery`(`tc.pWalkable`)로만 접근 (직접 include 금지).
- 점수 소비처: `ChampionAIValuation::ValueInput`은 평면 스칼라만 (valuation.h) — 필드 값이 들어갈 확장 지점.

## 설계

### 자료구조와 소유권

`ChampionAIGridMap` — Shared/GameSim 소유, 고정 크기 float 배열 + WorldToCell.

- **소유권 = 팀당 1필드 (총 2개)**. 근거: LoL 규칙상 시야는 팀 공유 **사실**이다 — 팀 필드는 Team Blackboard(보류 영역)의 선차용이 아니라 게임 규칙의 반영. 필드 입력은 "팀 시야로 보이는 적"(사실) + 간이 last-seen 확산까지만. 봇 개체의 해석·예측(19 L1/L2 — 귀속/쿨다운/반응)은 per-bot 소유를 유지한다. (per-bot 필드 10개는 갱신 비용 ~5배로 기각; 봇별 차이는 필드가 아니라 19의 개체 모델에서 만든다.)
- **키프레임 정책**: 필드는 파생 캐시 — 레지스트리 미등록, 복원 후 재계산 (spatial index와 동일 정책, S015 §5). 이것이 성립하려면 아래 갱신 위상 불변식이 필수다.
- CONFIRM_NEEDED: 해상도 — NavGrid 512×512(0.5u)의 1/8 다운스케일(64×64, 4u/셀)을 초안으로 하되, 실제 맵 플레이 영역 치수를 G0 계획서에서 측정 후 확정.

### 갱신 위상 결정론 불변식 (키프레임 복원과의 정합 — 필수)

- 갱신은 **절대 틱 위상**으로만: `tickIndex % kFieldUpdateInterval == 0` (지역 카운터/누산기 금지).
- `kFieldUpdateInterval`은 키프레임 간격 `kKeyframeIntervalTicks=30`의 **약수**여야 한다 (6 | 30 성립 — 정적 검사로 고정). 따라서 모든 키프레임 틱은 갱신 경계이고, 복원 직후 재계산 값 = 무중단 실행이 그 틱에 갖고 있던 값.
- 이 불변식이 깨지면 복원 후 봇이 다른(더 신선하거나 오래된) 필드를 소비해 22 U3의 해시 일치 프로브와 상시 `RunKeyframeRestoreDeterminismProbe`가 영구 FAIL한다. 22 U2의 "복원+1틱 이후 비교" 우회는 6틱 주기 필드에는 적용 불가 — 우회가 아니라 위상 고정으로 푼다.
- 합산 순회는 EntityID 정렬 고정 순서, 전파(블러)는 고정 반복 횟수, `tc.pRng` 불사용(순수 함수).

### 레이어 로드맵

| 버전 | 레이어 | 입력 | 소비처 |
|---|---|---|---|
| v1 | ThreatField | 적 챔피언(DPS·CC 보유·레벨 → 세기, 사거리+이동 반경 → 감쇠), 적 터렛(사거리 하드존 — turretDanger 일반화), 적 웨이브 | ValueInput.retreat 가중, 후퇴 **방향** 선택(현재 안전 앵커 고정 → 최저 위협 방향), 다이브 진입/이탈 경로 평가 |
| v2 | TeamInfluence | 아군/적 챔피언 ± 부호 합산 | 교전 커밋 오라클(19 L2)의 지원 항, 갱 회피 |
| v3 | Opportunity | 웨이브 push 상태, (오브젝트 타이머 — 존재 시) | intent 후보 점수 |

### 갱신과 비용

- 주기: 6틱(0.2s) — decisionInterval과 동기, 위 불변식 준수. `WINTERS_PROFILE_SCOPE`로 갱신 비용 측정, 틱 예산 내 확인 (카운터 cap 32 gotcha 주의). 비용 상한 = 팀 2필드 × 레이어 수.
- 봇별 캐시: 07의 `MapAwarenessComponent` 아이디어 승계 — 자기 주변 셀 값만 결정 시 질의.

## 구현 슬라이스

| 슬라이스 | 내용 | 검증 |
|---|---|---|
| G0 | `ChampionAIGridMap` + WorldToCell + 해상도 확정 + 위상 불변식 정적 검사 | 단위 프로브(경계/변환 왕복) |
| G1 | ThreatField v1 (챔피언+터렛) + ValueInput 배선 | 필드 해시 프로브, "터렛 존에서 retreat 점수 상승" 시나리오, 기존 turretDanger 동작 회귀 없음, **keyframe restore 프로브 PASS 유지** |
| G2 | 후퇴 방향 소비 (최저 위협 방향) | 시나리오: 포위 상황에서 열린 방향으로 후퇴; 인게임 눈 확인 |
| G3 | 히트맵 스냅샷 노출 (21과 공동) | F5에서 히트맵 오버레이 표시 |
| G4 | TeamInfluence/Opportunity | 리그 메트릭 개선 확인 후 채택 |

## Files touched (예정)

`Shared/GameSim/Systems/ChampionAI/` 신규 `{ChampionAIGridMap.h, ChampionAIInfluenceSystem.h/.cpp}`(이름 CONFIRM — 기존 파일 명명과 정합), `ChampionAIValuation.h/.cpp`(ValueInput 확장), `ChampionAISystem.cpp`(소비), `Server/Private/Game/GameRoomTick.cpp`(시스템 호출 위치 — Phase_ServerBotAI 직전), Snapshot.fbs+Builder/Applier(다운샘플 디버그), `Tools/SimLab/main.cpp`(해시 프로브), GameSim.vcxproj 등록.

## Rollback scope

시스템 호출 1줄 + ValueInput 확장 항 제거로 v0 복귀. 필드는 파생 캐시라 키프레임/스냅샷 포맷 영향 없음(디버그 필드 제외).

## Next slice

G0 계획서: 맵 플레이 영역 치수 측정(CONFIRM 해소) + 그리드 스펙 + 위상 불변식 + 변환 프로브.
