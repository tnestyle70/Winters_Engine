Session - 하드코딩된 봇 AI 프로필 17종 + 콤보 플랜 7종을 ServerPrivate authoring JSON + generated pack으로 옮기고, GetChampionAIProfile/콤보 조회를 pack reader로 전환한다(결정성 불변).

배경(한 줄): 봇 프로필은 `ChampionAIPolicy.cpp:438-498`의 switch + `MakeXxxProfile()` constexpr(17 프로필+default), 콤보는 `ChampionAIPolicy.cpp:500-643`(6 챔피언 전용 + 1 default = 7) static constexpr 테이블로 박혀 있다. 규칙·게이트는 07.

검토 반영(2026-06-28) — 확정 결정:
- (★게이트 정정) **SimLab frame 해시는 봇 결정을 포함하지 않는다**(`Tools/SimLab/main.cpp:420-438`은 position/health/mana/level/gold/RNG만 해시). 따라서 G4 해시 불변은 "봇 결정 동작의 결과(이동/피해)"만 보장하고 "결정 로직 동일"은 증명하지 못한다. → 본 Phase 게이트에 **별도 봇 시나리오 검증**을 추가한다: 동일 시드 5v5에서 프로필/콤보 적용 전후의 봇 command 로그(slot/target/tick)를 diff해 동일함을 증명. G4 문구에서 "봇 결정이 byte-identical"은 "프로필/콤보 값이 byte-identical + 봇 command 로그 동일"로 교체.
- (I1 확정) `ChampionAIPolicy`는 Shared/GameSim에 남긴다. profile/combo pack은 ServerPrivate이므로 **Shared가 ServerData를 직접 include하지 않는다**. 대신 `TickContext.pDefinitions`(또는 동일 주입 경로)로 pack을 받아 `GameplayDefinitionQuery::ResolveChampionAIProfile(...)` 형태로 조회한다. 호출처 둘(Server `GameRoomSpawn`/`ServerChampionEntityFactory` tail, Shared `ChampionAISystem`) 모두 주입 경로를 쓴다.
- (pack 확정) AI는 **별도 pack**(`ServerData::GetLoLAIProfilePack()`)으로 둔다(GameplayDefinitionPack 구조 변경 회피).
- (콤보 수) "7종" = 6 챔피언 전용(Jax/Fiora/Ashe/Riven/LeeSin/Sylas) + default 1.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Data/LoL/ServerPrivate/AI/ChampionAIProfiles.json (새 파일)

`ChampionAIProfile`(ChampionAIPolicy.h:42-60) 필드를 그대로 표현한다. 값은 현재 constexpr와 byte-identical.

```json
{
  "schemaVersion": 1,
  "profiles": [
    { "championId": "ANNIE", "preferredRange": 6.0, "championScanRange": 9.0,
      "minionScanRange": 12.0, "structureScanRange": 18.0, "leashRange": 16.0,
      "aggression": 0.85, "kiteBias": 1.0, "retreatHpRatio": 0.45, "reengageHpRatio": 0.62,
      "minionPressureWeight": 1.05, "turretRiskWeight": 1.10, "lastHitWeight": 1.05, "siegeWeight": 0.80,
      "skillRules": [ { "slot": "Q", "minRange": 0.0, "score": 1.0 }, { "slot": "W", "minRange": 0.0, "score": 0.8 } ] }
  ],
  "combos": [
    { "championId": "JAX", "steps": [
      { "slot": "Q", "itemId": 0, "minRange": 0.0, "maxRange": 7.0, "selfHpMinRatio": 0.35, "enemyHpMaxRatio": 1.0, "targetMode": "TargetEntity" }
    ] }
  ]
}
```

확인 필요: 17개 프로필 + default, 7개 콤보 + default의 정확한 값은 `ChampionAIPolicy.cpp:6-643`에서 그대로 옮긴다. targetMode/itemId enum 문자열 매핑(TargetEntity/AwayFromTarget/WardBehindTarget/LastOwnWard/SylasHijackTarget/SylasStolenUltimateTarget, kTrinketWardItemId)을 generator에 등록.

1-2. C:/Users/tnest/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

AI 프로필/콤보 정규화·검증·emit을 추가한다. championId 중복 금지, slot 범위, ratio 0~1 범위, build hash 포함. 생성물 예: `Server/Private/Data/Generated/LoLAIProfiles.generated.cpp` + 헤더의 `ServerData::GetLoLAIProfilePack()`.

확인 필요: 기존 generator의 spawn/skill emit 함수를 본떠 추가. AI pack을 `GameplayDefinitionPack`에 합칠지 별도 pack으로 둘지 결정(별도 pack이 의존 단순).

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp

`GetChampionAIProfile(champion)`(438)와 콤보 조회를 pack reader로 바꾼다. constexpr 테이블은 fallback으로 남긴다(reader 0 도달 후 P8에서 삭제).

```text
- GetChampionAIProfile: pack에서 championId로 찾고, 없으면 기존 switch fallback.
- 콤보 조회 함수: 동일 패턴.
```

확인 필요:
- profile은 spawn(GameRoomSpawn.cpp:605-632, 현재 ServerChampionEntityFactory tail이 사용)과 per-tick(ChampionAISystem.cpp:1865-1899, 2947-2950) 양쪽에서 조회된다. 두 경로 모두 pack reader를 쓰게 한다.
- pack은 Server 소유(ServerPrivate). ChampionAIPolicy는 Shared/GameSim에 있다 -> Shared가 ServerData를 직접 include하면 I1 위반. profile pack 접근을 TickContext 같은 주입 경로로 넘기거나, AI policy 자체를 Server로 옮길지 결정(확인 필요, 중요).

1-4. bot skill-rank order

`kLevelOrder`(현재 `ServerChampionEntityFactory.cpp:39-59`, AssignDefaultBotSkillRanks)도 AI 정책 데이터의 일부다. `ChampionAIProfiles.json`의 공용 또는 champion별 `skillRankOrder`로 옮길 후보. 이번 slice가 아니면 P5 후속 slice로 표시.

2. 검증 (07 §6)

미검증:
- AI pack 미생성, reader 전환 미반영

검증 명령:
- python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
- GameSim/Server/Client/SimLab Debug x64 빌드
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1

통과 기준:
- G4 SimLab same-seed 해시 불변: 봇 결정이 byte-identical이어야 한다(프로필/콤보 값 동일). 해시가 바뀌면 어떤 프로필 값이 다른지 추적.
- seed+1 해시 상이(봇 결정이 상태에 반응함 증명).
- G3: AI 프로필/콤보 하드코딩 candidate가 fallback 삭제(P8) 후 0.

확인 필요:
- I1 의존 방향(1-3): Shared AI policy가 ServerPrivate pack을 어떻게 읽을지. 주입 vs 이동 결정이 이 Phase의 핵심 설계점.
- 봇 결정이 SimLab frame hash에 포함되는지(p5 연구: 미확인). 포함 안 되면 hash 불변이 봇 동작 불변을 보장하지 못하므로, 별도 봇 시나리오 해시/로그 비교를 게이트에 추가.
