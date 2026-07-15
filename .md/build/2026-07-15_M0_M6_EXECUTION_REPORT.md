# 2026-07-15 M0~M6 실행 보고 (완료 — 전 게이트 그린)

기준 계획: `.md/plan/2026-07-15_FULL_DATA_DRIVEN_BALANCE_MASTER.md`. owned paths는 ACTIVE_WORK_PACKETS `2026-07-15_data_driven_balance_m0_m6_execution` 패킷 참조.

## 최종 검증 결과 (서버/클라 종료 후 일괄 실행)

- 코드젠: `build_champion_game_data.py` 재생성(레거시 0x710F4FEE — 쿨다운 3초 소성), `Build-LoLDefinitionPack.py` 재생성 **첫 시도 PASS**(팩 0xF78781F7 — 경제/아이템 34종/minionWave 편입) + `--check` PASS, `run_codegen.bat` PASS(op25 + Hello 필드).
- MSBuild Debug x64: GameSim(SharedBoundary PASS) → Server(WintersServer.exe) → Client(WintersGame.exe) → SimLab **전부 에러 0 첫 시도 통과** (Engine은 참조로 재빌드).
- SimLab 1800 42: **PASS, 해시 18110C0D7C01FA27 = 직전 의도 기준선과 완전 일치** — 값 보존 리팩터의 바이트 정확성 증명 (골든 로스터에 Jax 없음, 쿨다운은 타 세션 dev 오버라이드가 동일 3초 강제 중, 시나리오에 레인 미니언/정글 미포함이라 밸런스 변경분은 골든 비간섭).
- 타 세션 `\` 주석 오타는 확인 시점에 이미 `//` 로 수정돼 있었음(무개입).

## M4 — 미니언/웨이브/구조물 (완료)

- SpawnObjectGameplayDefs.json `minionWave` 신설(웨이브 900틱/초기 300/미니언당 10/공성 3주기/성장 2.5%/min·30분 캡/원거리 투사체 5값/시체 1.5s) + 코드젠 emit + **SpawnObject 팩 런타임 오버레이**(`GetActiveLoLSpawnObjectDefinitionPack`, 6번째 소스) — 리로드 후 다음 웨이브/스폰/op24 복원부터 신값.
- 서버 소비 전환: ServerMinionWaveRuntime(캐던스), GameRoomSpawn(성장 공식), GameRoomUnitAI(투사체+시체 1.2→**1.5 단일화**, 의도 변경). ServerMinionTuning 무참조 상수 3종 제거.
- **포탑 DamagePipeline 보류 확정**: 서버 권위 경로는 이미 파이프라인 경유(GameRoomProjectiles.cpp:287-293 → BuildTurretDamageRequest → DamageQueueSystem — 방어력/실드/킬보상 정상). 감사가 지목한 StructureProjectileSystem raw 차감은 클라 오프라인 스모크 전용 — 감사 문서 §4.5 항목은 서버 기준 오탐으로 정정.

## M0 — 런타임 정의 팩 리로드 (완료)

- 신규: `Server/Private/Data/RuntimeGameplayDefinitionOverlay.h/.cpp` (코드젠 팩 복사+JSON 3종 오버레이+원자 스왑, FNV-1a 32 = 코드젠 definition_key 동일, Winters.sln 상향 8단계 경로 규약), `Server/Private/Data/ThirdParty/json.hpp`(nlohmann v3.12 사본 — Client 경계 include 회피).
- `ICommandExecutor.h`: `ReloadGameplayDefinitions = 25`, `Count = 26` + 주석. `Command.fbs` 동일 append + `run_codegen.bat` 재생성 완료(static_assert Count==MAX+1 정합 유지).
- 활성 팩 전환: `GameRoomTick.cpp`(2곳), `GameRoomSpawn.cpp:717`, `GameRoomCommands.cpp`(1311/1994/2642) → `GetActiveLoLGameplayDefinitionPack()`. SimLab/generated/오버레이 base 복사는 소성 팩 유지(골든 결정론).
- `GameRoomCommands.cpp`: op 25 케이스(리로드 실패 시 bounded std::cerr + 활성 팩 불변, 성공 시 전 StatComponent bDirty + rev 로그). `<iostream>` 추가.
- `ChampionTuner.cpp`: "Reload Definitions From JSON (Server)" 버튼 (9키 패널, Clear Server Overrides 아래).
- `Server.vcxproj`: ClCompile/ClInclude 등록.

## M1 — 스킬 JSON 커버리지 (완료)

- `SkillEffectGameplayDefs.json` 신규 8항목(값 = 기존 하드코딩과 동일, 동작 불변): ashe.basic_attack{radius .35, speed 18}, ashe.q{20/4s/4스택/슬로우1.5}, fiora.e{30/5s/2}, jax.r{70/8s/3}, jax.w{45/5s}, kalista.q{70/.6/27}, sylas.r{45s/range10}, yasuo.w{4s/형성.25/렉트1.6+.35/rank/폭.5} + kindred.e에 maxStacks 3 추가. 총 60항목, 정렬 유지, JSON 파싱 검증 PASS.
- 신규 param id 2종: `RectLengthPerRank`, `FormationDelaySec` — SkillAtomData.h + Build-LoLDefinitionPack.py + M0 오버레이 테이블 3곳 동시 반영.
- 챔피언 cpp 전환 7종(병렬 에이전트, 전부 값 보존·자가 재검증 완료):
  - Kalista: Q 생 리터럴(27/0.6/70) → Q resolve(신규 파일 내 헬퍼), R 폴백 드리프트 동기(0.45/2.5/1.0).
  - Yasuo: W 윈드월 5수치 OnW resolve 전환 ((rank-1) 공식 유지; 배치 forward 4.0/drift 0.5는 범위 밖 유지).
  - Ashe: Q 패시브(임계/지속/보너스/슬로우) 소비 시점 resolve + BA 투사체 speed/radius BasicAttack 슬롯 resolve. BA 프로스트 슬로우는 Q 항목이 구동(주의).
  - Fiora: E 윈도우/타수/보너스 OnE resolve.
  - Jax: W/R OnW/OnR 시점 latch + 3타 임계 신규 컴포넌트 필드(ultThirdHitThreshold). **JaxSimComponent 60→64B — WKF1 키프레임 레이아웃 변경 → Jax 포함 크로노 골든은 빌드 세션에서 재생성 필요.**
  - Kindred: E 3타 임계 → MaxStacks resolve.
  - Sylas: R 강탈 45s → resolve; range는 팩 경로가 이미 라이브(champions.json rangeMax) 확인.
- MasterYi Q/W/E: **훅 자체 미구현**(OnR만 존재) — 데이터화 대상 아님 확정.
- 이연: Annie/Sylas/Yasuo 패시브 원자(PassivePolicy — 09 계획 P3a 인프라 필요).

## M5(a) — 쿨다운/마나 오버라이드 데드존 수정 (완료)

- `GameplayDefinitionQuery.cpp` ResolveSkillCooldown/ResolveSkillManaCost 가 world-aware 오버로드 경유로 전환 — practice op 10의 CooldownReductionPerRank/ManaCostPerRank 가 수락-후-무시되던 버그 수정. 일반 플레이 동작 불변(오버라이드 활성 시에만 차이).

## 사운드 감사 (완료 — 별도 보고서)

- `.md/build/2026-07-15_CHAMPION_SOUND_COVERAGE_AUDIT.md`: 17종×6이벤트 102/102 완비, 미배선 없음. BGM 방식 포팅 불필요(이미 동일 SoundManager 경유, PlayBGM은 호출부 0). 반영 1건: `Scene_InGameNetwork.cpp` 카탈로그 미스 bounded 진단 로그.

## M2 — 경제/보상/XP 데이터화 (완료)

- 신규: `Shared/GameSim/Definitions/EconomyGameplayDef.h`(기본값 = 현 cpp 상수, bValid 게이트), `Data/LoL/ServerPrivate/Gameplay/EconomyGameplayDefs.json`.
- `GameplayDefinitionPack.h/.cpp`: `economy` 포인터(맨 뒤 append — aggregate init 보존) + `FindEconomy()`. 코드젠 py: economy 로드/검증/emit/buildHash 포함.
- `RewardRegistry`: `LoadFromEconomyDef()` 신설 + **정글 보상 3종 신설(신규 동작)** — subKind 규약 kJungleRewardSubBaron=0/Epic=1/Small=2 (baron 300g/600xp, epic 150/250, small 35/75). 폴백(LoadDefaultSummonersRift)에도 동일값.
- `ExperienceSystem.cpp`: Jungle 분기 신설(기존 미니언 분기 패턴 + LogRewardMiss). 정글 킬이 이제 골드/XP 지급.
- 소비 전환(economy 있으면 사용, 없으면 기존 상수 폴백 = 값 보존): GoldIncomeSystem(패시브 골드 3값), DamageQueueSystem(어시 윈도우 10s→300틱 정확 일치), CommandExecutor 리콜 2s.
- `ChampionAIValuation.h`: 그림자 상수 4종 → 레지스트리 조회 함수(사용처 0곳 확인 — 선언만 존재했음).
- op25 리로드 + GameRoom 룸 초기화 + SimLab 에 LoadFromEconomyDef 푸시. 오버레이에 EconomyGameplayDefs.json 4번째 소스 편입.
- 보류: RecallSystem.cpp(실소비처 아님 — CommandExecutor 가 스탬프), 클라 리콜 연출 상수(Scene_InGameNetwork:352).

## M6 — 팩 해시/리로드 revision 핸드셰이크 (완료, 축소판)

- `Hello.fbs` append-only 필드 2종(id 7/8): gameplayPackHash/gameplayPackRevision + flatc 재생성(idempotent 확인).
- `GameRoomLobby.cpp`: CreateHello 에 활성 팩 manifest.uBuildHash + 리로드 revision 전달. `SnapshotApplier.h/.cpp`: 저장 + revision>0 시 bounded 8회 가시화 로그(팩 해시는 클라 미컴파일 — 표시용 명시). 구버전 호환(0=생략).
- 이연: 매치 중 리로드 통지 이벤트, Kalista 대시 예측/애니 페이싱 클라 소성값 회수(P4 트랙).

## M5(일부) — 완료/이연 분리

- 완료: 쿨다운/마나 오버라이드 데드존(위), M0 로더에 동시 세션 신규 param `damagePerSpear` 1:1 동기.
- 이연(빌드 세션 필요): CDR 캡 중복 단일화, 랭크 공식 정규화(동작 변경 — 골든 검증 필수), 폴백 상수 삭제 스윕(P3d — 17파일 컴파일 반복 필요), 클라 로컬 데미지 오프라인 게이트 명시화.

## ⚠ 동시 세션 정합 (2026-07-15 병행 작업 감지)

- 다른 세션이 Kalista 버그픽스로 `DamagePerSpear` param 을 enum/py 에 추가 → 본 세션 M0 로더 테이블에 동기 완료. **SimLab 골든은 그 세션에서 18110C0D 로 의도 변경됨**(본 문서 상단의 85A270CA 기준은 그 이전) — 빌드 세션에서 최신 골든 기준으로 재판정.
- **경고**: 그 세션이 CommandExecutor 에 전챔피언 쿨다운 3초 디버그 오버라이드(`return 3.0f`)를 활성 상태로 남김 — 쿨다운 관련 검증 전에 반드시 원복 확인.

## 밸런스 기준선 변경 (사용자 지시 — 의도된 수치 변경 2건)

- **미니언 HP 절반**: SpawnObjectGameplayDefs.json minions 섹션 — melee/ranged/siege役2 450→225, 공성役3 1000→500, 슈퍼役4 1500→750, defaultMinion 450→225. 정글 캠프(타 세션 실측값)/구조물 불가침. 클라 MinionCombatDef.h 헤더 표는 오프라인 경로 전용이라 미변경(M4 이중 진실 제거 대상).
- **QWER 쿨다운 전부 3.0s**: champions.json 17챔피언 × 4슬롯 = 68라인 (바이트 보존 라인 편집, BA 슬롯 0 불가침 검증). 참고: 타 세션이 CommandExecutor 에 동일 취지의 dev 오버라이드(`return 3.0f`, :711)를 이미 활성 — 코드 오버라이드가 살아있는 동안 JSON 튜닝은 가려짐. **그 줄 제거 후부터 JSON 이 기준선**(제거는 해당 세션 소유 — 본 세션 미개입).
- ⚠ 타 세션 파일에서 발견: CommandExecutor.cpp:708 주석이 `//` 가 아니라 `\` 로 시작 — 컴파일 실패 예상 오타. 소유 세션에서 수정 필요.
- 이후 기획자 루프: 리빌드 1회 후 JSON 수정 → 9키 리로드 버튼 → 즉시 반영으로 손보기.

## 진행 중 (에이전트)

- M3 아이템 데이터화: ItemGameplayDefs.json(34종 값 전사)+팩 items 편입+CItemRegistry 인스턴스화+LoadFromItemDefs+리로드/룸/SimLab 푸시. 클라 상점 표시는 컴파일 기본값 유지(라이브 리로드 중 표시 갭 — 문서화).

## 잔여 (후속 세션)

- M5 잔여: CDR 캡 중복 단일화, 랭크 공식 정규화(rank vs rank-1 — 골든 영향 검증 필요), 폴백 상수 삭제 스윕(P3d), 클라 로컬 데미지 오프라인 게이트 명시화.
- 패시브 원자(PassivePolicy — Annie/Sylas/Yasuo), 아이템/미니언 클라 표시 리플리케이션 채널, 매치 중 리로드 통지 이벤트, Kalista 대시 예측 클라 소성값(P4).
- **주의 인계**: 타 세션의 CommandExecutor `return 3.0f`(쿨다운 3초 강제)가 살아있는 동안 JSON 쿨다운 튜닝은 가려짐 — 그 줄 제거 시점부터 champions.json(현재 전 QWER 3.0)이 기준선.
- 인게임 F5 눈검증(사용자 게이트): 9키 → Reload Definitions 버튼 → `[Data] runtime definitions reloaded rev=1` 서버 로그 + 수치 즉시 반영 + 되감기 후 신값 유지.

## 빌드 세션(다음)에서 반드시

1. `Shared/Schemas/run_codegen.bat` (이미 실행됨 — Hello 변경분은 M6 에이전트가 재실행)
2. `python Tools/LoLData/Build-LoLDefinitionPack.py --root .` + `--check` (메인 세션이 마지막에 실행 예정 — 미실행 시 필수)
3. `python Tools/ChampionData/build_champion_game_data.py` 필요 여부 확인(champions.json 무변경이면 불필요)
4. MSBuild Engine/GameSim/Server/Client/SimLab Debug x64
5. SimLab 1800 42 — **골든 해시 85A270CA는 변할 수 있음**: JaxSimComponent 크기 변경(키프레임), 정글 보상 신설(경제), 팩 buildHash 변경. 값 보존 항목들은 해시 불변이어야 하나 신규 동작 2건은 의도 갱신 + 사유 기록.
6. F5 눈검증: JSON 수정→9키 리로드 버튼→즉시 반영→되감기 생존.
