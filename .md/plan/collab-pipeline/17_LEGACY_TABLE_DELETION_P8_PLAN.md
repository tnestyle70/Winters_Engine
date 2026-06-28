Session - reader가 0에 도달한 legacy value-owning 경로(ChampionGameDataDB / ChampionStatsRegistry / ChampionRuntimeDefaults 기본값 / SkillTable / ChampionTable)를 의존 순서대로 삭제해 generated pack을 유일한 권위 source로 만든다.

배경(한 줄): 07 DoD-2의 마지막 단계. 삭제는 reader count==0 증명 후에만(07 I4/R5). 아래 카운트는 연구 시점 값이며 작업 직전 rg/audit로 재확인한다. 이 계획서는 "삭제 게이트 체크리스트 + 의존 순서"다.

1. 반영해야 하는 코드 (= 삭제 게이트와 순서)

1-1. 삭제 대상별 현재 reader 카운트 (작업 직전 재확인)

```text
ChampionGameDataDB::            31 readers / 9 files
  - GameplayDefinitionQuery.cpp(10, fallback) / ChampionRuntimeDefaults.cpp(12, wrapper)
  - Client Scene_InGame*(GameplayQuery 205/210, Network 262/1027, LocalSkills 1073, Internal 26)
  - ChampionStatsRegistry.cpp:29 / CommandExecutor.cpp:2400,2408
CChampionStatsRegistry::Instance()  8 readers / 6 files
  - ServerChampionEntityFactory.cpp:117 (★ 서버 fallback, championDef null일 때 crash 위험)
  - StatSystem.cpp:95,125 / SimLab main.cpp:167 / ChampionSpawnService.cpp:202 / LocalSkills 367 / SharedGameSimSmoke 71
FindSkillDef / g_SkillTable        15 readers / 9 files (클라 visual: registrations 3, EventApplier 4, Scene 3, UI 1, table 2)
FindChampionDef / g_ChampionTable   8 readers / 7 files (클라 visual/asset: SpawnService 97, FxPresets, EventApplier 131, RosterSpawner, Loader, LobbyHelpers, Internal)
GetDefaultChampion* (RuntimeDefaults) 11 readers (Zed/Move/CommandExecutor visual yaw 등)
```

1-2. 삭제 의존 순서 (각 단계는 reader 0 증명 후 삭제)

```text
순서 1. ChampionRuntimeDefaults 기본값/wrapper 삭제
  선행: GetDefaultChampionVisualYawOffset 등 visual reader -> P4(13 계획서)로 ClientPublic 이동 후 0.
        나머지 wrapper(ChampionGameDataDB 호출)는 호출처를 GameplayDefinitionQuery로 직접 전환.
순서 2. ChampionGameDataDB:: 삭제
  선행: (a)P4로 visual resolver reader 0, (b)Client Scene_InGame* gameplay reader를 GameplayDefinitionQuery 또는 replicated 결과로 전환,
        (c)GameplayDefinitionQuery 내부 fallback이 ChampionGameDataDB 대신 pack-only가 되도록 P3d 완료.
순서 3. CChampionStatsRegistry 삭제
  선행: ★ServerChampionEntityFactory.cpp:117 fallback 제거 = 모든 챔피언이 GameplayDefinitionPack에 존재(Ezreal/Garen 포함) 보장.
        StatSystem 95/125, SimLab 167, Client 202/367을 pack/def reader로 전환.
순서 4. SkillTable/FindSkillDef, ChampionTable/FindChampionDef 삭제
  선행: 클라 visual/asset reader를 ClientPublic visual pack(P4의 LoLVisualDefinitions)으로 전환.
        champion registration(Garen/Riven/Zed 등), EventApplier, FxPresets가 g_SkillTable/g_ChampionTable 대신 visual pack을 읽게.
순서 5. 잔여 constexpr fallback / 구 generated 삭제 (P3d/P5/P6 fallback 잔재).
```

1-3. 각 삭제 slice의 형태

```text
- 대상 심볼 1개(또는 1 묶음)에 대해: rg로 runtime reader == 0 증명(테스트/계획 문서/주석 제외) ->
  헤더/구현 삭제 -> 프로젝트에서 제거 -> 빌드 -> Verify 파이프라인 -> 리포트.
- 한 번에 다 지우지 않는다. 심볼 단위 slice + 독립 커밋(07 R 롤백 단위).
```

확인 필요:
- ServerChampionEntityFactory.cpp:117 fallback은 "championDef null이면 legacy registry"다. 이걸 지우려면 모든 슬롯 챔피언이 pack에 있어야 한다. Ezreal/Garen이 pack/def에 없으면(12 계획서에서 확인) 먼저 추가.
- SimLab main.cpp:167의 CChampionStatsRegistry 사용은 12 계획서(pDefinitions 주입)와 연동해 pack 경로로 바꾼다.

2. 검증 (07 §6, §8 DoD)

미검증:
- 어떤 것도 아직 삭제 안 함(이 계획서는 게이트 정의)

검증 명령(각 삭제 slice마다):
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1  (reader count 확인)
- rg "ChampionGameDataDB::|CChampionStatsRegistry::Instance|FindSkillDef|g_SkillTable|FindChampionDef|g_ChampionTable|GetDefaultChampion" Client Server Shared
- GameSim/Server/Client/SimLab Debug x64 빌드
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1

통과 기준 (07 DoD-2/DoD-4/DoD-5):
- 각 삭제 전 해당 심볼 runtime reader == 0 (rg 증명).
- 삭제 후 G1 빌드 PASS, G4 SimLab same-seed 해시 불변, G3 audit PASS.
- 최종: generated pack이 유일한 권위 gameplay source. 위 legacy 심볼 전부 부재.

확인 필요:
- 삭제 순서는 P3d(12)/P4(13) 선행 완료에 의존. 그 전에는 순서 2~4 진입 금지(07 I4).
- compatibility-only로 남겨야 하는 것이 있으면(예: 외부 도구가 참조) 삭제 대신 격리 표시.
