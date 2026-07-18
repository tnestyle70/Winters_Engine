Session - 서버 권위 아이템 활성 효과와 6슬롯 인벤토리 상호작용
좌표: gameplay/item-runtime · 축: server-authority/data-driven/UI-input
관련: `CLAUDE_Legacy.md`, `.md/architecture/WINTERS_CODEBASE_COMPASS.md`, `Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json`

## 1. 결정과 성공 조건

① 인벤토리는 기존 `InventoryComponent::kMaxSlots == 6`을 유지하고 1~6 키·드래그 교환·아이템 사용을 동일 슬롯 인덱스로 통합한다.
② 갈라진 하늘은 챔피언 대상 기본 공격에만 발동한다. 재사용 5초(150틱), 총 치명타 배율 1.8, 회복 `기본 공격력 100% + 잃은 체력 6%`를 데이터로 정의한다.
③ 존야는 선택 슬롯 사용 시 3초(90틱) 동안 금색·대상 지정 불가·무적·이동/공격/스킬 사용 불가이며, 서버 상태 플래그를 기존 스냅샷으로 복제한다.
④ 수은 장식띠/헤르메스의 시미터는 일반 기절·둔화·무장해제를 제거하되 공중에 뜸과 `ForcedMotionComponent`는 보존한다. 와드는 커서 지점, 칼리스타 계약은 현재 타깃 로직을 슬롯 권위로 재사용한다.
⑤ 성공 기준은 데이터 생성 check, 5초/3초 경계·강제 치명타·회복·무적·정화·슬롯/runtime 교환 SimLab PASS, FlatBuffers/리플레이 계약 PASS, Engine→Server→Client Debug x64 `/m:1` 빌드 PASS다.

이번 세션의 ceiling 예산은 전체 변경의 30% 이하로 제한한다. ceiling은 금색 재질·번호/드래그 강조·미니맵 0.35→0.37 시각 조정이고, 나머지는 권위·데이터·회귀 검증 floor다. 8칸 확장, 헤르메스 이동 속도 액티브, 액티브 쿨다운 HUD 숫자, 상점 신규 구매 UX는 이번 범위에서 제외한다.

데이터 기준은 Riot Data Dragon `16.14.1`의 가격/기본 능력치와 공식 패치의 Lightshield 식을 따른다. 단, 사용자가 지정한 갈라진 하늘 5초와 존야 3초는 명시적 Winters 오버라이드다. 존야/수은 계열 액티브 쿨다운은 각각 120초/90초로 기록하고 서버 슬롯 runtime이 소유한다.

권위 흐름은 고정한다.

```text
Client 1~6/drag intent
  -> GameCommand(UseItem/ReorderItem)
  -> Server GameSim validates inventory slot + ItemDef + cooldown
  -> status/damage/inventory mutation
  -> existing snapshot(gameplayStateFlags, inventory)
  -> Client gold material/HUD redraw
```

## 2. 파일별 구현 프리뷰

### 2.1 아이템 원천 데이터와 생성기

`Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json`의 `"itemId": 6610`, `3157`, `3140`, `3139` 레코드에 아래 필드를 추가한다. `3340`, `3599`는 런타임 전용 비구매 아이템으로 동기화 대상에 포함한다.

```json
"lightshieldStrike": {
  "cooldownSec": 5.0,
  "critDamageMultiplier": 1.8,
  "healBaseAdRatio": 1.0,
  "healMissingHealthRatio": 0.06
}
```

```json
"active": { "kind": "Stasis", "cooldownSec": 120.0, "durationSec": 3.0 }
"active": { "kind": "Cleanse", "cooldownSec": 90.0, "durationSec": 0.0 }
"active": { "kind": "Ward", "cooldownSec": 0.0, "durationSec": 0.0 }
"active": { "kind": "KalistaOathsworn", "cooldownSec": 0.0, "durationSec": 0.0 }
```

`Tools/LoLData/Sync-LoLItemGameplayDefs.py`의 `RUNTIME_TRANSFORM_ITEM_IDS`와 `PRESERVED_GAMEPLAY_FIELDS` 아래를 교체한다.

```python
RUNTIME_ONLY_ITEM_IDS = {3042, 3340, 3599}
PRESERVED_GAMEPLAY_FIELDS = (
    "onHitDamage",
    "spellblade",
    "manaflow",
    "lightshieldStrike",
    "active",
    "maxManaBonusAdRatio",
)
```

`is_supported_shop_item()`은 위 ID를 맵 11 런타임 레코드로 허용하고, `build_document()`는 구매 불가로 출력한다. `Tools/LoLData/Build-LoLDefinitionPack.py`의 `normalize_items_root()`에서 두 객체를 검증해 정규화하고, `emit_cpp()`의 아이템 생성 블록에서 `ItemDef.lightshieldStrike`와 `ItemDef.active`로 출력한다. 생성 결과인 `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`, `Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp`, manifest/parity 파일은 생성기로만 갱신한다.

`Shared/GameSim/Definitions/ItemDef.h`의 `ItemManaflowDef` 아래에 추가한다.

```cpp
enum class eItemActiveKind : u8_t
{
    None = 0,
    Ward,
    Stasis,
    Cleanse,
    KalistaOathsworn,
};

struct ItemLightshieldStrikeDef
{
    bool_t bValid = false;
    f32_t cooldownSec = 0.f;
    f32_t critDamageMultiplier = 0.f;
    f32_t healBaseAdRatio = 0.f;
    f32_t healMissingHealthRatio = 0.f;
};

struct ItemActiveDef
{
    bool_t bValid = false;
    eItemActiveKind kind = eItemActiveKind::None;
    f32_t cooldownSec = 0.f;
    f32_t durationSec = 0.f;
};
```

`ItemDef`의 `ItemManaflowDef manaflow{};` 아래에 추가한다.

```cpp
ItemLightshieldStrikeDef lightshieldStrike{};
ItemActiveDef active{};
```

### 2.2 갈라진 하늘 피해/회복

`Shared/GameSim/Definitions/DamageTypes.h`의 `DamageFlag_ShowCriticalIndicator` 아래에 추가한다.

```cpp
DamageFlag_ForceCrit = 1u << 4,
```

`Shared/GameSim/Components/DamageRequestComponent.h`의 override 필드들에 추가한다. 이 값은 기본 공격 본체만 가리키며 아이템 on-hit/Spellblade는 포함하지 않는다.

```cpp
f32_t critEligibleAmountOverride = 0.f;
f32_t critDamageMultiplierOverride = 0.f;
```

`Shared/GameSim/Systems/Combat/CombatActionSystem.cpp`에서 기본 공격 request를 만들 때 원래 `damage`를 `critEligibleAmountOverride`에도 기록한다. 따라서 이후 `AccumulateBasicAttackItemOnHit()`와 정수 약탈자가 같은 request에 피해를 더해도 갈라진 하늘 강제 치명타는 기본 공격 본체에만 적용된다.

`Shared/GameSim/Systems/Damage/DamagePipeline.cpp`의 `ApplyCritIfNeeded()`를 아래 규칙으로 교체한다.

```cpp
const bool_t bForceCrit = (flags & DamageFlag_ForceCrit) != 0u;
if (!bForceCrit && (flags & DamageFlag_CanCrit) == 0u)
    return amount;
if (req.source == NULL_ENTITY || !world.HasComponent<StatComponent>(req.source))
    return amount;

const auto& sourceStat = world.GetComponent<StatComponent>(req.source);
if (!bForceCrit)
{
    const f32_t chance = std::clamp(sourceStat.critChance, 0.f, 1.f);
    if (chance <= 0.f || !tc.pRng || !tc.pRng->RollChance(chance))
        return amount;
}

outCrit = true;
const f32_t multiplier = req.critDamageMultiplierOverride > 0.f
    ? req.critDamageMultiplierOverride
    : sourceStat.critDamage;
if (bForceCrit && req.critEligibleAmountOverride > 0.f)
{
    const f32_t eligible = std::min(amount, req.critEligibleAmountOverride);
    return amount + eligible * (std::max(1.f, multiplier) - 1.f);
}
return amount * std::max(1.f, multiplier);
```

`Shared/GameSim/Components/ItemRuntimeComponent.h`의 슬롯 상태에 추가한다.

```cpp
u64_t lightshieldReadyTick = 0u;
u64_t activeReadyTick = 0u;
```

`Shared/GameSim/Systems/Item/ItemEffectSystem.h`의 `ItemOnHitResolution`에 갈라진 하늘 슬롯/아이템/회복 정보를 별도 추가하고, `PrepareOnHitDamage()`에서 아래 조건을 만족할 때만 강제 치명타를 설정한다.

```cpp
request.eSourceKind == eDamageSourceKind::BasicAttack &&
world.HasComponent<ChampionComponent>(request.target) &&
tc.tickIndex >= state.lightshieldReadyTick
```

같은 함수의 발동 블록은 아래 의미를 갖는다.

```cpp
request.flags |= DamageFlag_CanCrit |
    DamageFlag_ForceCrit |
    DamageFlag_ShowCriticalIndicator;
request.critDamageMultiplierOverride = item->lightshieldStrike.critDamageMultiplier;
outResolution.lightshieldHeal =
    stat.baseAd * item->lightshieldStrike.healBaseAdRatio +
    missingHealth * item->lightshieldStrike.healMissingHealthRatio;
```

`OnDamageLanded()`는 실제 적용 성공 후에만 authoritative `HealthComponent.fCurrent`를 최대 HP까지 회복하고 `ChampionComponent.hp/maxHp` mirror를 함께 갱신한 뒤 `lightshieldReadyTick = 현재 tick + 150`으로 커밋한다. 정수 약탈자와 갈라진 하늘은 별도 resolution 필드이고, 강제 치명타 eligible 양도 기본 공격 본체로 분리하므로 정수 약탈자/마나무네 피해를 1.8배하지 않는다.

### 2.3 상태·정화·활성 아이템 서버 처리

`Shared/GameSim/Components/GameplayComponents.h`의 상태 ID/플래그 끝에 추가한다.

```cpp
ZhonyaStasis = 12,
inline constexpr u32_t kGameplayStateInvulnerableFlag = 1u << 10;
inline constexpr u32_t kGameplayStateStasisVisualFlag = 1u << 11;
```

`Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.cpp`의 `CanReceiveDamage()`는 `Untargetable | Invulnerable`을 모두 거부하도록 교체한다.

```cpp
constexpr u32_t kDamageBlocked =
    kGameplayStateUntargetableFlag |
    kGameplayStateInvulnerableFlag;
return IsAliveGameplayTarget(world, target) &&
    !HasState(world, target, kDamageBlocked);
```

`Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h`의 `ClearStatusEffects()` 아래에 추가한다.

```cpp
void CleanseCrowdControlEffects(CWorld& world, EntityID entity);
```

구현은 `StatusEffectComponent.active[]`에서 `Airborne` 플래그가 없는 기절·둔화·무장해제 인스턴스만 압축 제거하고, legacy `StunComponent`, `SlowComponent`, `DisarmComponent`만 제거한 뒤 `RebuildGameplayState()`를 호출한다. `ForcedMotionComponent`는 건드리지 않는다.

`Shared/GameSim/Systems/Item/ItemEffectSystem.h/.cpp`에는 활성 쿨다운을 한 소유자로 유지하기 위해 아래 API를 추가한다.

```cpp
static bool_t IsActiveReady(CWorld& world, const TickContext& tc,
    u32_t sourceEntity, u8_t inventorySlot, u16_t itemId);
static void CommitActiveCooldown(CWorld& world, const TickContext& tc,
    u32_t sourceEntity, u8_t inventorySlot, const ItemDef& item);
static void SwapRuntimeSlots(CWorld& world, u32_t sourceEntity,
    u8_t sourceSlot, u8_t targetSlot);
```

`Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h`의 enum 끝에 `ReorderItem = 13`을 append한다. `slot`은 source, `itemId`는 클라이언트가 본 source item ID, `practiceFlags` 하위 8비트는 target slot으로 사용한다. 서버가 source 슬롯의 현재 item ID와 `itemId`를 비교하므로 스냅샷 반영 전에 연속 drag가 들어와도 오래된 intent가 다른 아이템에 적용되지 않는다. FlatBuffers와 v2 replay가 이미 `kind/slot/itemId/practiceFlags`를 저장하므로 ABI 필드 추가 없이 faithful replay를 보존한다.

```cpp
ReorderItem = 13,
```

`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`의 `UseItem` switch 다음에 추가한다.

```cpp
case eCommandKind::ReorderItem:
    result = HandleReorderItem(world, tc, cmd);
    break;
```

`HandleUseItem()`과 `HandleReorderItem()`은 `void`가 아니라 `CommandExecutionResult`를 반환하고 executor switch가 `result = ...`로 받아 authoritative feedback을 남긴다. `cmd.itemId` 우선 분기 대신 `cmd.slot < 6`, 실제 `inventory.itemIds[cmd.slot]`, 전송 itemId 일치, `ItemDef.active`, 해당 슬롯 runtime 쿨다운을 차례로 검증한다. 분기는 다음으로 고정한다.

```cpp
switch (item.active.kind)
{
case eItemActiveKind::Ward:
    // 기존 TryResolveWardPlacement / SpawnWardEntity
    break;
case eItemActiveKind::Stasis:
    bApplied = GameplayStatus::TryApplyStatusEffect(world, cmd.issuerEntity, {
        eStatusEffectId::ZhonyaStasis,
        eStatusStackPolicy::RefreshDuration,
        cmd.issuerEntity,
        0u,
            kGameplayStateUntargetableFlag |
            kGameplayStateInvulnerableFlag |
            kGameplayStateStasisVisualFlag |
            kGameplayStateCannotMoveFlag |
            kGameplayStateCannotAttackFlag |
            kGameplayStateCannotCastFlag,
        item.active.durationSec,
        1.f }, tc);
    break;
case eItemActiveKind::Cleanse:
    GameplayStatus::CleanseCrowdControlEffects(world, cmd.issuerEntity);
    break;
case eItemActiveKind::KalistaOathsworn:
    // 기존 TryBeginOathswornContract, 성공 시 선택 슬롯 소모
    break;
default:
    return CommandExecutionResult::Rejected(
        cmd.sequenceNum, eCommandExecutionReason::InvalidPayload);
}
```

모든 분기는 `bApplied` 하나로 합쳐 와드 배치/칼리스타 계약/존야 `TryApplyStatusEffect`/정화가 실제 성공한 경우에만 쿨다운과 `Accepted`를 커밋한다. 존야 중에는 다른 액티브를 거부하고, 정화는 일반 기절 상태의 `CanCast()==false`를 우회할 수 있지만 죽음·존야는 우회하지 않는다. slot mismatch=`InvalidPayload`, 상태 차단=`StateBlocked`, 쿨다운=`Cooldown`, 성공=`Accepted`로 고정한다.

존야 성공 시 `ClearMoveTarget`, `ClearAttackChase`, `ClearCombatAction`, `CancelRecall`을 실행한다. `StatusEffectSystem.cpp`의 기존 `InterruptActionsForCrowdControl()`은 `kGameplayStateInvulnerableFlag`도 interrupting flag로 받아 skill charge/ActionState/pose까지 기존 공용 경로로 중단한다. SimLab은 상태 해제 뒤 과거 move/attack/cast가 자동 재개되지 않음을 확인한다.

`HandleReorderItem()`은 source=`cmd.slot`, expected item=`cmd.itemId`, target=`static_cast<u8_t>(cmd.practiceFlags & 0xffu)`를 검증한다. `practiceFlags` 상위 비트는 0이어야 하고, source/target은 0..5, source의 현재 item ID는 expected item과 같아야 한다. 성공 시 `InventoryComponent.itemIds`와 `ItemRuntimeComponent.slots`를 함께 `swap`한다. `inventory.count`는 마지막 비어 있지 않은 슬롯 기준으로 재계산하며 스탯은 아이템 집합이 같으므로 dirty 처리하지 않는다.

`Shared/Schemas/Command.fbs`의 enum 끝에 같은 값을 append한다. 전체 schema codegen은 동시 작업 파일을 덮어쓸 수 있으므로 실행 직전 모든 `.fbs`와 생성물 해시를 재확인하고, 안정적이면 기존 옵션으로 `Command.fbs` 하나만 직접 생성한다. 생성물 diff는 `CommandKind` append 외 변화가 없어야 한다.

```fbs
PracticeControl = 12,
ReorderItem = 13
```

### 2.4 Client 1~6 입력·HUD drag/drop·금색 재질

`Client/Public/Network/Client/CommandSerializer.h`의 `SendUseItem()`을 슬롯 인자를 받도록 교체하고 `SendReorderItem()`을 추가한다.

```cpp
void SendUseItem(CClientNetwork& net, u8_t slot, u16_t itemId,
    const Vec3& groundPos, const Vec3& direction = {},
    NetEntityId targetNet = NULL_NET_ENTITY);
void SendReorderItem(CClientNetwork& net, u8_t sourceSlot, u8_t targetSlot,
    u16_t expectedItemId);
```

`Client/Private/Network/Client/CommandSerializer.cpp`는 `UseItem`의 `wire.slot`을 채운다. reorder는 `wire.slot=sourceSlot`, `wire.itemId=expectedItemId`, `wire.practiceFlags=targetSlot`으로 보내며 `GetCommandKindName()`에도 문자열을 추가한다.

`Client/Private/Scene/Scene_InGameInput.cpp`의 고정 `4` 와드와 고정 `6` 칼리스타 블록을 삭제하고, `IsPlayerStunned()` 조기 반환보다 앞에서 1~6 키를 순회한다.

`Shared/GameSim/Spawn/ChampionGameplayAssembly.cpp`는 모든 챔피언의 기본 인벤토리 3번 인덱스(표시 키 4)에 비구매 와드 `3340`을 넣고, 칼리스타만 기존 5번 인덱스에 `3599`를 추가한다. `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`의 리 신 와드 점프는 스킬 W 인덱스를 보내지 않고 인벤토리에서 `3340`의 실제 슬롯을 검색해 `UseItem.slot`으로 보낸다.

`Client/Private/GamePlay/LoLUIContentRegistry.cpp`의 정적 catalog에는 `3340_class_t1_wardingtotem.png`를 `inventory`, `purchasable=false`로 등록한다. PNG 자동 발견이 비구매 와드를 상점 아이템으로 노출하지 않게 하는 최소 변경이다.

```cpp
for (u8_t slot = 0u; slot < InventoryComponent::kMaxSlots; ++slot)
{
    if (!in.IsKeyPressed('1' + slot))
        continue;
    const u16_t itemId = inventory.itemIds[slot];
    if (itemId == 0u)
        continue;
    pCommandSerializer->SendUseItem(
        *pNetworkView, slot, itemId, cursorWorld,
        WintersMath::DirectionXZ(origin, cursorWorld, Vec3{}), hoveredNet);
}
```

클라이언트는 액티브 종류를 판정하지 않고 슬롯/커서/hover intent만 전송하며 서버 `ItemDef.active`가 최종 판정한다.

`Engine/Public/Manager/UI/UI_Manager.h`에서 상점/스킬 콜백 옆에 다음 generic UI API를 추가한다. Engine은 GameSim 아이템 의미를 알지 않는다.

```cpp
void SetInventoryReorderCallback(
    void(*pfn)(void*, u8_t, u8_t, u16_t), void* pUser);
bool_t IsPointerOverActorInventory(f32_t fMouseX, f32_t fMouseY) const;
```

private 상태는 다음만 둔다.

```cpp
void(*m_pfnInventoryReorder)(void*, u8_t, u8_t) = nullptr;
void* m_pInventoryReorderUser = nullptr;
i8_t m_iInventoryDragSource = -1;
i8_t m_iInventoryDragHover = -1;
```

`Engine/Private/Manager/UI/UI_Manager.cpp`는 기존 `kInventorySlotX/Y`를 파일 namespace 공용 layout 상수/rect helper로 승격한다. `DrawActorHUDOverlay()`에서 빈 칸도 6개 모두 테두리와 `1..6` 라벨을 그리고, press→drag source, hover 강조, release→콜백을 처리한다. 클릭이 월드 이동으로 새지 않도록 `IsPointerOverActorInventory()`가 같은 rect helper를 사용한다.

`Engine/Include/GameInstance.h`, `Engine/Private/GameInstance.cpp`에 callback/pointer query 전달자를 추가하고 Engine 빌드의 `UpdateLib.bat`로 `EngineSDK/inc/GameInstance.h`를 동기화한다.

`Client/Private/Scene/Scene_InGameLifecycle.cpp`의 buy/level callback 등록 옆에 reorder callback을 등록하고, 종료 시 nullptr로 해제한다. `Scene_InGameInput.cpp`는 좌클릭 처리 전에 `UI_IsPointerOverActorInventory()`이면 `outSkipGroundMove=true`로 월드 명령을 차단한다.

`Client/Private/Scene/Scene_InGameRender.cpp`의 `ApplyViegoMistMaterialOverride()`를 `ApplyChampionMaterialOverride()`로 교체한다. 금색 override를 실제로 적용한 replicated champion renderer만 별도 집합으로 추적하고, 존야 종료 시 그 renderer만 clear한다. 요네 E 분신처럼 다른 시스템이 소유한 override는 일반 순회가 지우지 않는다.

```cpp
if (world.HasComponent<ReplicatedStateComponent>(entity) &&
    (world.GetComponent<ReplicatedStateComponent>(entity).gameplayStateFlags &
        kGameplayStateStasisVisualFlag) != 0u)
{
    pRenderer->SetMaterialOverrideColor(Vec4{ 1.30f, 0.82f, 0.18f, 1.f }, true);
    return;
}
// 기존 Viego soul/mist 우선순위 보존
pRenderer->ClearMaterialOverrideColor();
```

세 champion render 경로가 모두 이 helper를 호출하므로 상태 종료 후 금색도 동일 경로에서 해제된다.

`Client/Private/UI/MinimapPanel.cpp`의 기본값만 아래로 교체한다.

```cpp
f32_t ViewportHeightRatio = 0.37f;
```

### 2.5 테스트와 회귀 고정

`Tools/SimLab/main.cpp`의 기존 item runtime probe 아래에 `RunActiveItemInventoryProbe()`를 추가하고 `--active-items-only` CLI를 연결한다. 픽스처는 아래 수치 경계를 직접 검증한다.

```cpp
// Sundered Sky: tick 1 proc, tick 150 미발동, tick 151 재발동
// expected damage = basic attack body * 1.8 + non-crit ER/Manamune on-hit
// expected heal = baseAd + missingHealth * 0.06
// Zhonya: 90 ticks state + damage rejection + move/attack/cast rejection
// Zhonya: failed status capacity does not consume cooldown; old actions do not resume
// Cleanse: stun/slow/disarm removed, airborne/forced motion retained
// Reorder: inventory item IDs and ItemRuntimeSlotState cooldown/stack state swap
// Ward/Kalista: selected slot mismatch rejected, selected slot success
```

기존 Kalista SimLab에서 `UseItem` 명령은 실제 보유 슬롯을 `cmd.slot`에 명시하도록 갱신한다. `Tools/Harness/ReplayCommandContractProbe.cpp`는 payload 크기 60을 유지하면서 `ReorderItem`의 source/destination이 `slot/itemId`로 보존되는지 추가 확인한다.

## 3. 예측, 검증, 인계

### 예측

- 갈라진 하늘은 30Hz 기준 발동 후 정확히 150틱 동안 잠기고 151번째 경계 입력에서 재발동하며, 기본 공격 본체만 1.8배하고 정수 약탈자/마나무네 추가 피해는 비치명으로 유지한다.
- 존야는 상태 플래그만으로 기존 snapshot 경로를 타므로 Snapshot schema를 변경하지 않아도 모든 클라이언트에서 같은 3초 상태/금색 재질을 본다.
- reorder가 item runtime까지 함께 바꾸므로 마나무네 스택·정수 약탈자 준비 상태·갈라진 하늘/액티브 쿨다운이 아이템을 따라간다.
- 리 신 ward-hop 명령은 실제 `3340` 보유 슬롯을 보내며, 슬롯 mismatch와 오래된 reorder expected item은 `InvalidPayload`로 거부된다.
- 와드 `3340`은 인벤토리에는 존재하지만 상점에는 구매 가능 항목으로 노출되지 않고, 요네 E/비에고 material override 회귀가 없다.
- 명령 enum append와 기존 `slot/itemId` 재사용으로 FlatBuffers/replay payload ABI와 v2 파일 호환은 유지된다.
- 리스크는 HUD 클릭이 월드 클릭으로 새는 문제와 Claude 동시 편집이다. 공용 rect helper 및 수정 직전/빌드 직전 SHA256 재확인으로 닫는다.

### 검증 명령

```powershell
python Tools/LoLData/Sync-LoLItemGameplayDefs.py --patch 16.14.1 --check
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
Tools\Bin\flatc.exe --cpp --scoped-enums --no-warnings -o Shared\Schemas\Generated\cpp Shared\Schemas\Command.fbs
Tools\Bin\flatc.exe --go --no-warnings -o Shared\Schemas\Generated\go Shared\Schemas\Command.fbs
python Tools/VerifySharedBoundary.py
git diff --check

msbuild Winters.sln /t:Engine /p:Configuration=Debug /p:Platform=x64 /m:1
cmd /c UpdateLib.bat
msbuild Winters.sln /t:Server /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Winters.sln /t:Client /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Winters.sln /t:SimLab /p:Configuration=Debug /p:Platform=x64 /m:1

Tools\SimLab\Bin\Debug\SimLab.exe --active-items-only
Tools\SimLab\Bin\Debug\SimLab.exe --item-runtime-only
Tools\SimLab\Bin\Debug\SimLab.exe
```

실제 산출 경로/target 이름이 솔루션과 다르면 `CONFIRM_NEEDED`: `msbuild Winters.sln /t:...` 실행 전 `msbuild Winters.sln /t:ValidateSolutionConfiguration` 또는 기존 빌드 로그로 정확한 target을 확인한다.

시각 수용 검증은 자동 빌드와 별개로 F5에서 ① 6칸/번호 가시성, ② 빈 칸 포함 drag/drop, ③ 미니맵이 기존 장식 슬롯을 가리는지, ④ 존야 금색 3초/해제 후 원복을 확인한다. 이 턴에 실행 가능한 클라이언트 세션이 없으면 해당 네 항목은 `UNVERIFIED`로 결과 문서에 남기되 GameSim/빌드 PASS를 시각 PASS로 대체 표기하지 않는다.

구현 완료 후 `.md/plan/2026-07-18_ITEM_ACTIVE_INVENTORY_INTERACTION_RESULT.md`에는 규칙대로 예측 대 실측, 판정, 갱신된 트레이드오프만 기록한다. 대상 파일의 수정 전 hash와 마지막 hash를 비교해 외부 세션 변경이 감지되면 자동 덮어쓰기 없이 해당 파일만 재검토한다.
