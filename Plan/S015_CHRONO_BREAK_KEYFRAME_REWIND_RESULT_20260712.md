Session - 크로노 브레이크(비트 정확 키프레임 되감기) 반영 결과와 기계 증명, 인게임 검증 경계를 고정한다.

## 1. 결론

- **검증 루프의 마지막 고리가 코드로 닫혔다.** 서버 시뮬 월드 전체(컴포넌트 90종 + 엔티티 할당자 + RNG + EntityIdMap)를 틱 경계에서 비트 정확하게 저장/복원하고, F10 패널의 Rewind 5s/10s/30s로 최대 90초 창 안의 과거로 되감는다. 되감기 직후는 일시정지로 착지하며, Step/Resume/트레이스/수치 튜닝과 조합해 "멈춤 → 분해 → 조작 → **같은 상황 재실행** → 관찰" 루프가 한 번의 빌드·실행 안에서 성립한다.
- **기계 증명 확보**: SimLab 골든 프로브가 "틱 300에서 저장(10챔피언 전투 중, 81,027바이트) → 새 월드에 복원 → 301~600틱 재실행"이 무중단 실행과 **틱 단위 해시 완전 일치**함을 증명했다. save→restore→save 재직렬화 바이트 동일성도 함께 검증. 이 프로브는 상시 회귀 게이트로 등록되어, 이후 어떤 컴포넌트 추가/시스템 변경이 결정론·복원 완전성을 깨면 즉시 FAIL로 드러난다.
- 봇 명령은 저널하지 않는다 — 복원된 상태+RNG에서 **재결정**된다. 그래서 되감은 뒤 AI 가중치/스킬 수치를 바꾸면 같은 상황에서 달라진 행동을 즉시 관찰할 수 있다(튜닝 루프의 원리이자 설계 의도).
- 부가 반영: 적 챔피언 배치(SpawnChampion — 17챔피언, Dummy/AI Bot, 임의 위치), 스킬 오버라이드 대상 확장(targetNet — 적 봇 스킬 수치 튜닝 가능), SetEnabled(false) 오버라이드 전체 스윕.

## 2. UE/Unity 대비 입증 포지셔닝

| 능력 | UE5 | Unity | Winters (이번 반영) |
| --- | --- | --- | --- |
| 게임플레이 시뮬 상태의 비트 정확 저장/복원 | 없음 (SaveGame은 수동 직렬화, 비트 정확 아님) | 없음 | **키프레임 81KB, 완전성 기계 검사 내장** |
| 시뮬 되감기 + 재실행 | Rewind Debugger = 애니 트레이스 재생(시뮬 재실행 아님) | 없음 | **서버 권위 30Hz 시뮬을 되감고 봇이 재결정** |
| 되감기의 자동 회귀 증명 | 없음 | 없음 | **SimLab 해시 골든 테스트 상시 게이트** |
| 유사 기술 | 격투게임 롤백(GGPO) — 예측 보정용, 툴 아님 | — | 디자이너 검증 루프로 노출 (F10) |

핵심 서사: "게임 기능의 한계가 엔진의 한계" — 크로노 브레이크는 게임 기능이면서 동시에 엔진의 결정론·직렬화·복제 아키텍처 전체에 대한 증명이다. 어서링 표면(나이아가라류 그래프/유틸리티 에디터)은 이 루프 위에 얹히는 층이다.

## 3. 인게임 검증 절차 (사용자 게이트)

Debug 서버+클라, F10 → Enable Practice Session 후:

**A. 크로노 브레이크**
1. 1분쯤 플레이(미니언 웨이브·전투 발생) → Pause → 상황 확인 → **Rewind 10s**.
2. 기대: 월드가 10초 전 상태로 점프(내 위치/HP/미니언/쿨다운 전부), **일시정지 상태로 착지**, 서버 콘솔에 `[ChronoBreak] rewound to tick N (paused)`.
3. Step 30으로 1초씩 전진하며 과거가 다시 흘러가는 것 확인 → Resume.
4. 같은 지점으로 다시 Rewind → 이번엔 F9에서 봇 튜닝 슬라이더(예: 공격성)를 바꾸고 Resume → **같은 상황에서 봇 행동이 달라지는지** 확인 (루프의 본질 검증).
5. 실패 채증: `[ChronoBreak] rewind aborted/FAILED` 또는 `[Keyframe] unregistered component store: <타입명>` 콘솔 라인 원문.

**B. 적 챔피언 배치**
1. Spawn Champion 섹션: Yone / Red / AI Bot / Use Current 근처 좌표 → Spawn.
2. 기대: 즉시 스폰되어 AI로 행동(Dummy면 제자리), 미니맵/HUD 이상 여부 확인(비로스터 챔피언 허용은 CONFIRM 항목), Clear Practice Spawns로 일괄 제거.

**C. 적 스킬 수치 튜닝**
1. F9에서 적 봇의 netId 확인 → F10 오버라이드 Target NetId에 입력 → BaseDamage 등 수정 → Apply.
2. 기대: 해당 봇의 스킬 데미지가 변경 반영(서버 스냅샷 경유), Clear Server Overrides로 원복.

**D. 회귀**
1. Practice 미사용 일반 매치 정상(키프레임 캡처는 30틱당 1회, 인게임 프레임 저하 체감 없어야 함 — 81KB/초 수준).
2. S014 기능(Pause/Step/TimeScale) 정상 동작 유지.

## 4. 자동 검증 결과 (2026-07-12)

- flatc 재생성 PASS / Engine(+UpdateLib) → GameSim → Server → Client → SimLab Debug x64 **전 빌드 PASS**
- SimLab exit 0: `[SimLab][Keyframe] PASS: save@300 -> restore -> replay 301..600 matches uninterrupted run (blob=81027 bytes)` + 기존 프로브 12종 PASS + same-seed/seed+1 계약 유지
- `git diff --check` 이상 없음
- 빌드 반복 1회: GameRoomCommands.cpp include 3종(RespawnComponent/TurretAISystem/SpatialHashSystem) 누락 보정

## 5. 남은 경계 (P2)

- spatial index는 복원 후 재유도(Rebuild) — 되감기 직후 1틱의 공간 쿼리 순서가 무중단 실행과 다를 수 있음(SimLab 게이트 범위 밖). P2에서 `CSpatialIndex::SaveState/RestoreState`로 봉인.
- 서버 전용 페이즈(미니언 웨이브/터렛/투사체/디페네트레이션)는 결정론 하네스 미보유 — 서버 레벨 골든 테스트 확장 필요.
- 리플레이 저널은 되감기 후 비단조(같은 틱 재기록) — 타임라인 포크(되감기 시 레코더 분절 저장 후 재시작) 예정.
- 복원 실패 시 월드가 부분 복원 상태로 남을 수 있음(현재는 std::cerr 하드 로그) — 더블 버퍼 복원은 P2.
- 전부 미커밋 — 인게임 게이트 통과 후 checkpoint commit (S005 G3 잔여와 함께).
