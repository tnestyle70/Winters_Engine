# 07 스킬 레벨업 및 데미지 증가 계획서

Current sequence

01/08: 미니언 지형 높이 및 맵 콜라이더 투영
-> 02/08: 우클릭 공격 추격
-> 03/08: B 귀환
-> 04/08: 미니맵
-> 05/08: 전장의 안개
-> 06/08: HUD 아틀라스/초상화/스킬 아이콘
-> 07/08 현재: 스킬 레벨업 및 데미지 +5
-> 08/08: 인게임 상점 및 아이템 스탯

Smoke 정책: 자동 smoke 검증은 폐지한다. 레벨업/HUD 동작은 유저가 인게임에서 직접 확인한다.

Bot AI is a GameCommand producer and must not directly mutate gameplay truth.
즉, Bot AI는 `GameCommand` 생산자이며 이동/공격/데미지 같은 gameplay truth를 직접 바꾸면 안 된다.

Goal

챔피언에게 skill point가 있을 때 HUD에서 Q/W/E/R을 업그레이드할 수 있게 한다. 첫 구현에서는 스킬 업그레이드가 성공할 때마다 플레이어의 데미지 수치를 5 올린다. 이후 이 값은 JSON 또는 Lua 기반 챔피언 데이터로 이전한다.

Non-goals

- 최종 스킬별 damage formula는 이 slice에서 완성하지 않는다.
- XP/gold economy tuning 전체는 하지 않는다.
- 네트워크 모드에서 UI가 서버 권위 skill rank를 직접 mutate하지 않는다.

Why this order

06 slice에서 만든 HUD skill button이 실제 gameplay command를 가져야 한다. 임시 `+5 damage`는 즉각적인 피드백을 주면서도 추후 data-driven 전환 경로를 열어둔다.

Current-code evidence

- [SkillRankComponent.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Components/SkillRankComponent.h:5)는 rank 배열과 available point를 저장한다.
- [SkillRankSystem.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/SkillRankSystem.cpp:22)는 champion level에 따라 skill point를 sync할 수 있다.
- [CommandExecutor.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp:1274)는 이미 `HandleLevelSkill`을 가지고 있다.
- [CommandExecutor.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp:1303)는 현재 rank 증가와 point 감소만 처리하고 event/데미지 증가는 없다.
- [UI_Manager.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp:747)는 현재 HUD click 시 skill rank를 local로 직접 올린다.
- [Event.fbs](C:/Users/user/Desktop/Winters/Shared/Schemas/Event.fbs:16)는 이미 `SkillRankUp`을 정의한다.
- [StatComponent.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Components/StatComponent.h:16)는 `baseAd`, `bonusAd`, `ad`를 가지고 있다.

Files touched

- 수정: [UI_Manager.h](C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h)
- 수정: [UI_Manager.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp)
- 수정: [CommandSerializer.h](C:/Users/user/Desktop/Winters/Client/Public/Network/Client/CommandSerializer.h)
- 수정: [CommandSerializer.cpp](C:/Users/user/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp)
- 수정: [CommandExecutor.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp)
- 수정: [ReplicatedEventComponent.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ReplicatedEventComponent.h)
- 수정: [ReplicatedEventSerializer.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ReplicatedEventSerializer.cpp)
- 수정: [EventApplier.cpp](C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp)
- 필요 시 추가: JSON/Lua skill value 이전용 후속 TODO

Insertion/replacement anchors

- `CUI_Manager::DrawChampionHUDOverlay`: 현재 lines 752-759의 local rank mutation을 callback/command request로 교체한다.
- `CCommandSerializer`: `SendLevelSkill(slot)`을 추가한다.
- `CDefaultCommandExecutor::HandleLevelSkill`: rank validation 성공 뒤 임시로 `stat.bonusAd += 5.f; stat.ad = stat.baseAd + stat.bonusAd; stat.bDirty = true;`를 적용한다.
- `ReplicatedEventSerializer`: `SkillRankUp` event에 source entity와 slot/rank를 담아 발행한다.
- `EventApplier`: client에서 rank/stat update를 반영하거나 confirm한다.

Implementation outline

1. HUD click이 `LevelSkill` command를 보낸다.
2. 서버가 레벨 요구치와 available point를 검증한다.
3. 서버가 skill rank를 올린다.
4. 서버가 임시 데미지 증가값 `+5`를 `StatComponent`에 적용한다.
5. 서버가 `SkillRankUp` event를 발행한다.
6. 클라이언트 HUD는 authoritative mode에서 local-only mutation이 아니라 replicated state/event를 통해 갱신된다.

Verification commands and expected results

- 자동 smoke 없음.
- 구현 후 위생 확인: `git diff --check`.
- 유저 수동 검증: skill point가 있는 상태에서 HUD plus를 클릭하면 rank가 오르고 기본 공격/스킬 데미지가 5 증가하는지 확인한다.
- 예상 debug log를 추가한다면: `[SkillRank] up entity=... slot=... rank=... bonusAd=+5`.

Rollback scope

HUD local mutation을 임시 복구하고, `SendLevelSkill`, `HandleLevelSkill`의 stat bonus, `SkillRankUp` event plumbing을 제거한다.

Next slice

HUD command가 서버 권위로 동작하면 08/08 인게임 상점으로 넘어간다.
