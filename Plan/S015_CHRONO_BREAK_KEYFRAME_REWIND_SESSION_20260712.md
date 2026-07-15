Session - 크로노 브레이크 본체를 반영한다: 비트 정확 월드 키프레임 저장/복원 + 인게임 되감기(RewindSimulationSeconds) + SimLab 골든 게이트 + 적 챔피언 배치(SpawnChampion) + 스킬 오버라이드 대상 확장(targetNet).

작성: Claude 레인 / 2026-07-12 / 선행: S014(시간 제어+명령 저널). 조사: 구현급 에이전트 5종(ECS 내부/POD 전수/비ECS 상태/SimLab/스폰·타겟), 전 앵커 rg 검증.
경계 원칙: **Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변조하지 않는다.** 되감기 후 봇 명령은 저널이 아니라 복원된 상태+RNG에서 결정론적으로 재생성된다 — 이것이 "수치 바꾸고 같은 상황 재실행" 튜닝 루프의 원리다.

---

## 0. 설계 결정 (조사 근거)

- **raw 덤프, 재구축 금지**: ECS dense 배열 순서는 add/swap-remove 이력의 함수이고(`ComponentStore.h` Remove) `ForEach` 순회가 그 순서를 그대로 노출하므로, "정렬 재구축" 복원은 공간 쿼리/타이브레이크를 바꿔 조용히 발산한다. 복원은 sparse/dense/data 3벡터를 바이트 그대로 왕복한다.
- **엔티티 할당자 왕복**: `CEntityManager`의 slots+freeHead+aliveCount를 그대로 복원하면 이후 CreateEntity의 id/generation 할당 순서가 정확히 재현된다(프리 리스트가 slot.nextFree로 연결). EntityID 참조가 복원 전후 전부 유효.
- **명시적 레지스트리 + 완전성 기계 검사**: `m_mapStores`는 type_index 키의 unordered라 직렬화 순서·이름이 불안정하고 익명 네임스페이스 타입(챔피언 dash 6종)은 중앙에서 명명 불가. → stableName 명시 레지스트리 + `Register<T>`의 `static_assert(is_trivially_copyable)`(컴파일 게이트) + Save/Restore 시 월드 전 스토어의 등록 여부 검사(런타임 게이트, 미등록 타입명을 std::cerr로 출력). 익명 6종은 소유 TU에서 자기등록(Annie `s_bRegistered` 관용구).
- **TransformComponent만 커스텀 코덱**: `m_vecChildren`(std::vector) 때문에 memcpy 불가. 서버 sim은 계층 미사용(SetParent 호출 0건 rg 확인) → POD 미러로 저장하고 계층 발견 시 하드 실패(트립와이어).
- **RNG/EntityIdMap**: `DeterministicRng::Get/SetState` 기존 훅 사용. EntityIdMap은 nextNetId를 그대로 왕복(max+1 재유도 금지 — 와드 만료 Unbind가 이미 배포된 id를 지울 수 있음).
- **복원은 틱 경계에서만**: 되감기 요청은 지연(pending) 저장 후 `Tick()` 최상단에서 수행 — 페이즈 중간 복원 금지. 복원 후 **일시정지로 착지**(디자이너가 Step/Resume으로 명시 진행).
- **spatial index는 P1에서 재유도**: per-tick Rebuild가 틱 중간(TurretAI 페이즈)에 일어나 이후 페이즈의 이동을 못 본다 — 엄밀 재현은 셀 직렬화(P2) 필요. SimLab에는 spatial이 없어 골든 게이트는 영향 없음. 확인 필요: 인게임 되감기 직후 1틱의 터렛/유닛AI 공간 쿼리 순서 차이 여부(P2에서 `CSpatialIndex::SaveState/RestoreState`로 봉인 예정).

## 1. 반영해야 하는 코드 (전부 반영 완료 — as-built)

신규 파일 3 + 어댑터 1:
- `Shared/GameSim/Core/Checkpoint/KeyframeComponentRegistry.h` — 레지스트리(헤더 온리), Fnv1a64, Detail::Write/ReadVector, PodSave/PodLoad(absent store는 zero-count).
- `Shared/GameSim/Core/Checkpoint/WorldKeyframe.h` — `SaveWorldKeyframe/RestoreWorldKeyframe` API.
- `Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp` — 컴포넌트 90종 등록(rg AddComponent 전수 census 기반) + Transform 커스텀 코덱 + 포맷(WKF1 v1: header/entityMgr/entityIdMap/stores sorted-by-name) + Save/Restore. **GameSim.vcxproj에 ClCompile 1줄 추가**(filters 파일 없음).
- `Shared/GameSim/Core/Ecs/VisionSystem.h` — Phase 7F 어댑터 1줄(VisibilityComponent 접근).

Engine 헤더 3종 (전부 인라인, DLL 표면 무변경, UpdateLib 자동 동기화):
- `ECS/ComponentStore.h` — `RawSparse/RawDense/RawData/RestoreRaw`.
- `ECS/World.h` — `ForEachStoreBase/GetEntityManager/Checkpoint_TryGetStore/Checkpoint_GetOrCreateStore`.
- `ECS/Entity.h` — `EntitySlot` public 승격 + `RawSlots/RawFreeHead/RawAliveCount/RestoreRaw`.

Shared:
- `ICommandExecutor.h` — `RewindSimulationSeconds = 15`, `SpawnChampion = 16`.
- `EntityIdMap.h` — `ForEachBinding/GetNextNetId/RestoreState` + `<utility>/<vector>`.
- `TurretAISystem.h` — `Get/SetActivationAccum`(시스템 객체의 유일한 sim 상태).
- 챔피언 6 TU(Ashe/Fiora/Jax/Viego/Yasuo/Yone GameSim.cpp) — 익명 dash/ledger 컴포넌트 자기등록.

Server:
- `GameRoom.h` — `RoomKeyframe`(simBytes+waveState+turretAccum+practiceEnabled) 링 + `m_pendingRewindToTick` + `CaptureKeyframeIfDue/PerformPendingRewind` 선언.
- `GameRoomTick.cpp` — Tick 최상단 `PerformPendingRewind()`(pause 게이트보다 먼저), `Phase_BroadcastSnapshot` 뒤 `CaptureKeyframeIfDue(tc)`.
- `GameRoomCommands.cpp` — 캡처(30틱 간격, 링 90 = 90초 창)/복원 구현(복원 순서: world→wave→turretAccum→practice→PracticeSpawned 태그 스캔 재구축→ingress/pending/lagComp/actionSeq 클리어→미래 키프레임 폐기→spatial Rebuild→**paused 착지**), `RewindSimulationSeconds`(1~60s 검증, no-keyframe 거절)·`SpawnChampion`(팀/브레인/id 1..17 검증, Sylas 더미 예약 거절, SpawnMinion 쿼터 블록 재사용, `SpawnChampionForLobbySlot`+위치 재배치+PracticeSpawnedTag) 케이스, `ApplySkillEffectOverride/ClearSkillEffectOverrides`의 targetNet 대상 해석(+챔피언 검증), SetEnabled(false) 오버라이드 전체 스윕. include 3종(RespawnComponent/TurretAISystem/SpatialHashSystem).
- `ServerMinionWaveRuntime.h` — `WaveState` + `Capture/RestoreWaveState`(waypoints/flowfield는 부트타임 파생이라 제외).
- `CommandIngress.h` — `Clear()`. `LagCompensation.h` — `Reset()`.

Client:
- `CommandSerializer.h/.cpp` — `SendPracticeControl(..., targetNet)` 파라미터.
- `ChampionTuner.cpp` — Simulation Time 섹션에 **Chrono Break Rewind 5s/10s/30s**, 신규 **Spawn Champion (Practice)** 섹션(17챔피언 콤보+팀+Dummy/AI Bot+위치), 오버라이드 **Target NetId(0=self)** 입력(F9 패널의 봇 netId로 적 스킬 튜닝).

Schema/SimLab:
- `Command.fbs` — ops 15/16 + flatc 재생성.
- `Tools/SimLab/main.cpp` — `RunKeyframeRestoreDeterminismProbe`(자기완결 본문 공유 램다: 무중단 1..600 실행+300틱 키프레임 → save→restore→save 바이트 동일 검증 → 새 월드 복원 후 301..600 재실행 해시 원소 비교) + main 등록.

## 2. 검증

검증 결과 (2026-07-12 실행):
- flatc 재생성: PASS (ops 15/16 추가만).
- Engine → UpdateLib → GameSim → Server → Client → SimLab Debug x64: **전부 빌드 PASS** (Server include 3종 보정 1회 반복).
- SimLab: `[SimLab][Keyframe] PASS: save@300 -> restore -> replay 301..600 matches uninterrupted run (blob=81027 bytes)` + 기존 프로브 12종 전부 PASS + same-seed/seed-sensitivity 계약 유지, exit 0.
- `git diff --check`: 이상 없음.

미검증 / 확인 필요:
- 인게임 되감기(사용자 게이트): RESULT 문서 §3 체크리스트.
- 되감기 직후 1틱의 spatial 쿼리 순서(§0 마지막 항목) — P2에서 셀 직렬화로 봉인.
- SpawnChampion으로 생성된 비로스터 챔피언의 클라 HUD/미니맵 슬롯 허용 여부(조사 open question) — 인게임 확인.
- 리플레이 저널은 되감기 후 비단조(동일 틱 재기록) — P2에서 타임라인 포크(레코더 분절 저장) 예정.

후속 동기화:
- Engine public header 변경 → `UpdateLib.bat` (Engine 포스트빌드로 자동 수행 확인됨).

다음 슬라이스:
- P2: `CSpatialIndex::SaveState/RestoreState`(비트 정확 봉인) + 서버 전 페이즈(미니언 웨이브/터렛/투사체) 결정론 하네스 + 리플레이 타임라인 포크 + 타임라인 스크럽 UI.
- Codex 조율: T3 트레이스 후보 분해→S013, D1 FX 타임스케일→S012 (S014 §0-3 지침 유지).
