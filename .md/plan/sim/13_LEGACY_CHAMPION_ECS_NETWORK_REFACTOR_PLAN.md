# Legacy Champion 순수 ECS / Server Network 동기화 리팩터 계획

> 날짜: 2026-05-06  
> 선행: `12_BANPICK_ROOM_SYNC_PLAN.md` Phase E 1차 적용  
> 목표: 레거시 `Scene_InGame` 멤버 렌더러 기반 챔피언을 모두 `ChampionDef + CreateECSChampion + 서버 roster/netId/snapshot` 경로로 통합한다.

---

## 0. 현재 기준선

### Pure ECS / 등록 기반으로 바로 생성 가능한 챔피언

```text
Ezreal
Fiora
Jax
Annie
Ashe
Yone
Riven
```

특징:

- `ChampionDef`에 `fbxPath`, shader, texture, spawn scale이 들어 있다.
- `CreateECSChampion(eChampion, eTeam)` 경로로 `ModelRenderer`를 동적 생성한다.
- renderer 소유권은 `m_ChampionRenderers[entity]`가 갖는다.
- BanPick local roster 1차 경로에서 이미 선택 챔피언만 생성 가능하다.

### Legacy / Scene 멤버 렌더러 기반 챔피언

```text
Irelia
Yasuo
Kalista
Garen
Zed
Sylas
Viego
```

현재 문제:

- `Scene_InGame`이 `ModelRenderer m_Irelia`, `m_Yasuo`, `m_Kalista` 같은 멤버를 직접 소유한다.
- `OnEnter()`에서 선택 여부와 무관하게 legacy 모델을 미리 초기화한다.
- `CreateChampionEntity(renderer, transform, champ, team)`가 외부 renderer 포인터를 `RenderComponent`에 꽂는다.
- `ChampionTable.cpp`의 일부 legacy `ChampionDef`는 anim key만 있고 `fbxPath`가 비어 있다.
- 서버는 렌더링 정보를 몰라야 하지만, champion id / team / stat / skill / anim id는 서버와 클라이언트가 공유해야 한다.

---

## 1. 최종 구조

```text
BanPick
  -> server locked roster
  -> GameContext.Roster
  -> Scene_InGame::CreateRosterChampionsFromGameContext()
  -> CreateECSChampion(championId, team)
  -> m_ChampionRenderers[entity] owns renderer
  -> ChampionComponent / Stat / SkillState / NetAnimation / Nav / Vision
```

서버:

```text
LobbySlotState
  -> SpawnChampionForLobbySlot(slot)
  -> ChampionComponent(id/team)
  -> StatComponent(championId)
  -> SkillState / SkillRank / NetAnimation / NetEntityId
  -> Snapshot(championId/team/animId/pos/stat)
```

클라이언트:

```text
Snapshot/Hello/roster
  -> netId -> EntityID bind
  -> championId로 visual ECS entity 생성
  -> 실제 조작/스킬/애니 lookup은 ChampionComponent.id 기준
```

---

## 2. 핵심 원칙

- `Scene_InGame`은 챔피언 종류별 멤버 renderer를 더 이상 소유하지 않는다.
- 모든 챔피언 visual은 `ChampionDef`와 `CreateECSChampion`으로 생성한다.
- 모든 플레이어/봇 생성은 roster slot 또는 snapshot의 `championId/team`에서 출발한다.
- `GameContext.SelectedChampion`은 local fallback/debug 전용이다.
- 서버는 FBX/texture path를 모른다. 서버는 gameplay def만 안다.
- 클라이언트는 snapshot에서 받은 champion id로 visual def를 찾는다.

---

## 3. Phase A. ChampionDef 완성

대상:

```text
Irelia / Yasuo / Kalista / Garen / Zed / Sylas / Viego
```

작업:

1. 각 챔피언에 `*_Registration.cpp`를 만들거나 기존 registration에 통합한다.
2. `ChampionDef` 필드 채우기:
   - `id`
   - `animPrefix`
   - `idleAnimKey`
   - `runAnimKey`
   - `basicAttackKey`
   - `basicAttackRange`
   - `fbxPath`
   - `shaderPath`
   - `defaultTexturePath`
   - `texturePath[]`
   - `spawnScale`
   - `displayName`
3. `CChampionRegistry::Instance().Add(id, cd)`로 등록한다.
4. 기존 `ChampionTable.cpp`의 legacy def는 중복 등록을 피하도록 단계적으로 제거하거나 fallback only로 축소한다.

Gate:

- 각 legacy champion이 `CChampionRegistry::Find(champion)`로 `fbxPath` 포함 def를 반환한다.
- `CreateECSChampion(champion, team)` 단독 호출로 모델이 뜬다.

---

## 4. Phase B. Champion-specific ECS state 부착

`CreateECSChampion` 내부의 state attach switch를 확장한다.

현재 포함:

```text
RivenStateComponent
EzrealStateComponent
FioraStateComponent
JaxStateComponent
AnnieStateComponent
AsheStateComponent
YoneStateComponent
```

추가 대상:

```text
IreliaStateComponent 또는 기존 Irelia blade/status state
YasuoStateComponent
KalistaStateComponent
GarenStateComponent
ZedStateComponent
SylasStateComponent
ViegoStateComponent
```

주의:

- state component는 POD에 가깝게 유지한다.
- renderer 포인터, FX renderer 포인터, Scene 포인터를 state에 넣지 않는다.
- FX/스킬은 hook context로 필요한 runtime 객체를 받는다.

Gate:

- champion id만으로 필요한 state component가 자동 부착된다.
- skill dispatch가 `SelectedChampion` 없이 `ChampionComponent.id`로 작동한다.

---

## 5. Phase C. Scene 멤버 renderer 제거

제거 대상:

```cpp
ModelRenderer m_Irelia;
ModelRenderer m_Yasuo;
ModelRenderer m_Sylas;
ModelRenderer m_Viego;
ModelRenderer m_Kalista;
ModelRenderer m_Garen;
ModelRenderer m_Zed;
```

대체:

```cpp
std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_ChampionRenderers;
```

작업:

1. `OnEnter()`의 legacy model init block 제거.
2. `CreateChampionEntity(ModelRenderer&, CTransform&, ...)` 호출 제거.
3. `CreateChampionEntity_FromBlueprint` 경로를 champion spawn에 사용하지 않는다.
4. 디버그 전시가 필요하면 `LegacyShowcase`가 아니라 roster/debug roster로 생성한다.

Gate:

- `Scene_InGame`에서 champion별 `ModelRenderer m_Xxx` 멤버가 사라진다.
- 선택하지 않은 챔피언이 로딩/생성되지 않는다.
- map renderer와 FX renderer는 유지된다.

---

## 6. Phase D. Skill / FX hook self-register 통합

목표:

- 모든 챔피언이 같은 등록 방식으로 skill def와 hook을 가진다.

구조:

```text
Champion/Xxx/Xxx_Registration.cpp
  -> ChampionDef 등록
  -> SkillDef 등록
  -> GameplayHook 등록
  -> VisualHook 등록
  -> KeepAlive 함수
```

작업:

1. Irelia/Yasuo/Kalista/Garen/Zed/Sylas/Viego의 skill table 의존을 registration으로 이동한다.
2. `SkillTable.cpp`는 legacy fallback 또는 제거 대상으로 축소한다.
3. cast/recovery frame fallback에서 `SelectedChampion` 분기를 제거한다.
4. `SkillHookContext`/`VisualHookContext`에는 caster entity와 champion id를 기준으로 필요한 상태를 조회한다.

Gate:

- 모든 챔피언의 QWER/BA가 `CSkillRegistry`에서 조회된다.
- cast/recovery hook이 `ChampionComponent.id`와 hook id로만 dispatch된다.

---

## 7. Phase E. Server gameplay def 분리

서버는 visual `ChampionDef`를 쓰지 않는다.

서버용 공유 def 후보:

```text
Shared/GameSim/Definitions/ChampionGameplayDef.h
Shared/GameSim/Registries/ChampionGameplayRegistry.h
```

필드:

```cpp
eChampion championId;
f32_t hpMax;
f32_t manaMax;
f32_t moveSpeed;
f32_t attackRange;
f32_t attackDamage;
f32_t armor;
f32_t magicResist;
```

작업:

1. 서버 `SpawnChampionForLobbySlot`에서 champion id로 gameplay def 조회.
2. `StatComponent`, `HealthComponent`, `ChampionComponent`를 def 기반으로 채운다.
3. 서버는 `fbxPath`, texture, animation key를 include하지 않는다.

Gate:

- 서버가 모든 roster champion을 Ezreal 하드코딩 없이 spawn한다.
- server snapshot의 `championId/team/stat`가 client roster와 일치한다.

---

## 8. Phase F. Network identity / visual bind

목표:

- 서버 net id와 클라이언트 visual entity가 같은 champion id를 공유한다.

작업:

1. `Hello`:
   - `yourNetId`
   - `sessionId`
   - `championId`
   - `team`
2. `Snapshot`:
   - entity별 `netId`, `championId`, `team`, `pos`, `yaw`, `animId`
3. `SnapshotApplier::SetOnNewEntityCallback`:
   - `championId/team`으로 `CreateECSChampion` 호출.
   - `EntityIdMap.Bind(netId, entity)`.
4. roster에서 이미 생성한 local entity가 있으면 duplicate spawn하지 않고 net id만 bind한다.

Gate:

- 3 client + bots에서 모든 client의 `netId -> championId` 매핑이 동일하다.
- Fiora/Yone 선택이 Ezreal로 덮이지 않는다.

---

## 9. Phase G. Bot 동기화

작업:

1. BanPick bot slot을 서버 entity로 생성.
2. `BotComponent`, `BlackboardComponent`, difficulty 부착.
3. 서버 snapshot에 bot 포함.
4. 클라이언트는 bot도 동일한 `CreateECSChampion` visual path로 생성.

Gate:

- Strict custom mode에서 host가 추가한 bot만 생성된다.
- Auto fill mode를 켜면 빈 슬롯이 bot으로 채워진다.
- bot champion도 client마다 동일하게 보인다.

---

## 10. 권장 실행 순서

```text
1. Irelia/Yasuo/Kalista/Garen/Zed ChampionDef 완성
2. Scene_InGame legacy renderer init block을 roster path 뒤로 격리
3. 각 legacy champion CreateECSChampion 단독 생성 검증
4. Skill/FX registration self-register로 이동
5. Scene member renderer 제거
6. Server ChampionGameplayDef 추가
7. SpawnChampionForLobbySlot 구현
8. SnapshotApplier duplicate-safe bind 구현
9. 3 client + bot 동기화 검증
```

---

## 11. 리스크

- legacy champion별 animation key가 기존 수동 문자열에 숨어 있다.
- texture slot mapping이 champion마다 다르다.
- Sylas/Viego는 blueprint/special path에 묶여 있어 마지막 단계로 미루는 것이 안전하다.
- 서버 gameplay def와 클라이언트 visual def를 섞으면 DLL/include 경계가 다시 오염된다.
- snapshot 수신 시 roster 선생성 entity와 remote snapshot entity가 중복될 수 있다.

---

## 12. 완료 기준

- `WINTERS_MIN_SCENE` 없이도 BanPick roster가 InGame 생성 대상을 결정한다.
- `Scene_InGame`에 champion별 renderer 멤버가 없다.
- 모든 champion이 `CreateECSChampion(championId, team)`으로 생성된다.
- 서버는 locked roster 기준으로 모든 human/bot champion을 spawn한다.
- Client는 server `championId/team/netId`를 신뢰한다.
- 스킬/애니/FX는 `ChampionComponent.id` 기준으로 동작한다.

