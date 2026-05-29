Session - B 키 recall command를 client/server/shared pipeline에 연결하고 2초 후 respawn 위치로 이동시킨다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/RecallComponent.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

#include <type_traits>

struct RecallComponent
{
    f32_t fRemainingSec = 0.f;
    f32_t fDurationSec = 2.f;
    bool_t bActive = false;
};

static_assert(std::is_trivially_copyable_v<RecallComponent>,
    "RecallComponent must remain POD for deterministic server simulation.");
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/RecallSystem.h

새 파일:

```cpp
#pragma once

class CWorld;
struct TickContext;

class CRecallSystem
{
public:
    static void Execute(CWorld& world, const TickContext& tc);

private:
    CRecallSystem() = delete;
};
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/RecallSystem.cpp

새 파일:

```cpp
#include "Shared/GameSim/Systems/RecallSystem.h"

#include "Shared/GameSim/Components/AttackChaseComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/ICommandExecutor.h"

#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/World.h"

namespace
{
    bool_t IsDead(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<HealthComponent>(entity))
            return false;

        const auto& health = world.GetComponent<HealthComponent>(entity);
        return health.bIsDead || health.fCurrent <= 0.f;
    }

    void ClearMoveTarget(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity) = MoveTargetComponent{};
    }

    void ClearAttackChase(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<AttackChaseComponent>(entity))
            world.RemoveComponent<AttackChaseComponent>(entity);
    }

    void SetIdleAnimation(CWorld& world, EntityID entity, const TickContext& tc)
    {
        auto& anim = world.HasComponent<NetAnimationComponent>(entity)
            ? world.GetComponent<NetAnimationComponent>(entity)
            : world.AddComponent<NetAnimationComponent>(entity, NetAnimationComponent{});

        anim.animId = static_cast<u16_t>(eNetAnimId::Idle);
        anim.animStartTick = tc.tickIndex;
        anim.animPhaseFrame = static_cast<u16_t>(tc.tickIndex & 0xffffu);
        ++anim.actionSeq;
    }
}

void CRecallSystem::Execute(CWorld& world, const TickContext& tc)
{
    const auto entities = DeterministicEntityIterator<RecallComponent>::CollectSorted(world);

    for (EntityID entity : entities)
    {
        if (!world.HasComponent<RecallComponent>(entity))
            continue;

        auto& recall = world.GetComponent<RecallComponent>(entity);
        if (!recall.bActive)
        {
            world.RemoveComponent<RecallComponent>(entity);
            continue;
        }

        if (!world.IsAlive(entity) ||
            !world.HasComponent<RespawnComponent>(entity) ||
            !world.HasComponent<TransformComponent>(entity) ||
            IsDead(world, entity))
        {
            world.RemoveComponent<RecallComponent>(entity);
            continue;
        }

        recall.fRemainingSec -= tc.fDt;
        if (recall.fRemainingSec > 0.f)
            continue;

        const Vec3 spawnPos = world.GetComponent<RespawnComponent>(entity).spawnPos;
        world.GetComponent<TransformComponent>(entity).SetPosition(spawnPos);
        ClearMoveTarget(world, entity);
        ClearAttackChase(world, entity);
        SetIdleAnimation(world, entity, tc);
        world.RemoveComponent<RecallComponent>(entity);
    }
}
```

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ICommandExecutor.h

기존 코드:

```cpp
    BuyItem = 5,
    UseItem = 6,
    Recall = 7,
};
```

아래로 교체:

```cpp
    BuyItem = 5,
    UseItem = 6,
    Recall = 7,
    RecallCancel = 8,
};
```

기존 코드:

```cpp
    void HandleBasicAttack(CWorld&, const TickContext&, const GameCommand&);
    void HandleLevelSkill(CWorld&, const TickContext&, const GameCommand&);
    void HandleBuyItem(CWorld&, const TickContext&, const GameCommand&);
};
```

아래로 교체:

```cpp
    void HandleBasicAttack(CWorld&, const TickContext&, const GameCommand&);
    void HandleLevelSkill(CWorld&, const TickContext&, const GameCommand&);
    void HandleBuyItem(CWorld&, const TickContext&, const GameCommand&);
    void HandleRecall(CWorld&, const TickContext&, const GameCommand&);
    void HandleRecallCancel(CWorld&, const TickContext&, const GameCommand&);
};
```

1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
```

기존 코드:

```cpp
    void ClearAttackChase(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<AttackChaseComponent>(entity))
            world.RemoveComponent<AttackChaseComponent>(entity);
    }
```

아래에 추가:

```cpp
    void CancelRecall(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<RecallComponent>(entity))
            world.RemoveComponent<RecallComponent>(entity);
    }
```

기존 코드:

```cpp
    case eCommandKind::BuyItem:
        HandleBuyItem(world, tc, cmd);
        break;
    default:
        break;
```

아래로 교체:

```cpp
    case eCommandKind::BuyItem:
        HandleBuyItem(world, tc, cmd);
        break;
    case eCommandKind::Recall:
        HandleRecall(world, tc, cmd);
        break;
    case eCommandKind::RecallCancel:
        HandleRecallCancel(world, tc, cmd);
        break;
    default:
        break;
```

`CDefaultCommandExecutor::HandleMove` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    if (!IsFiniteMoveTarget(cmd.groundPos))
    {
        static u32_t s_rejectLogCount = 0;
        if (s_rejectLogCount < 16u)
        {
            char msg[160]{};
            sprintf_s(msg,
                "[Command] move reject reason=invalid-pos issuer=%u seq=%u\n",
                static_cast<u32_t>(cmd.issuerEntity),
                cmd.sequenceNum);
            OutputCommandDebug(msg);
            ++s_rejectLogCount;
        }
        return;
    }
```

아래에 추가:

```cpp
    CancelRecall(world, cmd.issuerEntity);
```

`CDefaultCommandExecutor::HandleCastSkill` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    if (!bStage2 && slot.cooldownRemaining > 0.f)
    {
        LogCastSkill("reject", "cooldown", cmd, champion, slot.cooldownRemaining);
        return;
    }
```

아래에 추가:

```cpp
    CancelRecall(world, cmd.issuerEntity);
```

`CDefaultCommandExecutor::HandleBasicAttack` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    if (!world.HasComponent<TransformComponent>(cmd.issuerEntity) ||
        !world.HasComponent<TransformComponent>(cmd.targetEntity))
    {
        LogBasicAttackReject("missing-transform", cmd);
        return;
    }
```

아래에 추가:

```cpp
    CancelRecall(world, cmd.issuerEntity);
```

아래 기존 코드 바로 아래에 추가:

```cpp
void CDefaultCommandExecutor::HandleBuyItem(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    (void)tc;

    if (cmd.issuerEntity == NULL_ENTITY || cmd.itemId == 0)
        return;

    if (!world.HasComponent<GoldComponent>(cmd.issuerEntity) ||
        !world.HasComponent<InventoryComponent>(cmd.issuerEntity) ||
        !world.HasComponent<StatComponent>(cmd.issuerEntity))
        return;

    const ItemDef* pItem = CItemRegistry::Instance().Find(cmd.itemId);
    if (!pItem)
        return;

    GoldComponent& gold = world.GetComponent<GoldComponent>(cmd.issuerEntity);
    InventoryComponent& inventory = world.GetComponent<InventoryComponent>(cmd.issuerEntity);
    StatComponent& stat = world.GetComponent<StatComponent>(cmd.issuerEntity);

    if (inventory.count >= InventoryComponent::kMaxSlots)
        return;
    if (gold.amount < pItem->price)
        return;

    gold.amount -= pItem->price;
    inventory.itemIds[inventory.count++] = pItem->itemId;
    stat.itemMaskHash ^= static_cast<u32_t>(pItem->itemId) * 16777619u;
    stat.bDirty = true;
}
```

아래에 추가:

```cpp
void CDefaultCommandExecutor::HandleRecall(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    (void)tc;

    if (!world.HasComponent<RespawnComponent>(cmd.issuerEntity) ||
        !world.HasComponent<TransformComponent>(cmd.issuerEntity))
        return;

    if (world.HasComponent<HealthComponent>(cmd.issuerEntity))
    {
        const auto& health = world.GetComponent<HealthComponent>(cmd.issuerEntity);
        if (health.bIsDead || health.fCurrent <= 0.f)
            return;
    }

    ClearMoveTarget(world, cmd.issuerEntity);
    ClearAttackChase(world, cmd.issuerEntity);

    RecallComponent recall{};
    recall.fDurationSec = 2.f;
    recall.fRemainingSec = recall.fDurationSec;
    recall.bActive = true;

    if (world.HasComponent<RecallComponent>(cmd.issuerEntity))
        world.GetComponent<RecallComponent>(cmd.issuerEntity) = recall;
    else
        world.AddComponent<RecallComponent>(cmd.issuerEntity, recall);
}

void CDefaultCommandExecutor::HandleRecallCancel(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    (void)tc;
    CancelRecall(world, cmd.issuerEntity);
}
```

1-6. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/CommandSerializer.h

기존 코드:

```cpp
	void SendBasicAttack(CClientNetwork& net, NetEntityId targetNet,
		const Vec3& groundPos = {}, const Vec3& direction = {});
	void SendBuyItem(CClientNetwork& net, u16_t itemID);
	void SendLevelSkill(CClientNetwork& net, u8_t slot);
```

아래로 교체:

```cpp
	void SendBasicAttack(CClientNetwork& net, NetEntityId targetNet,
		const Vec3& groundPos = {}, const Vec3& direction = {});
	void SendBuyItem(CClientNetwork& net, u16_t itemID);
	void SendRecall(CClientNetwork& net);
	void SendLevelSkill(CClientNetwork& net, u8_t slot);
```

1-7. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp

아래 기존 코드 바로 아래에 추가:

```cpp
void CCommandSerializer::SendBuyItem(CClientNetwork& net, u16_t itemID)
{
    if (itemID == 0)
        return;

    GameCommandWire wire{};
    wire.kind = eCommandKind::BuyItem;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.itemId = itemID;

    //Smoke 안 넣을게
    SendSingle(net, wire);
}
```

아래에 추가:

```cpp
void CCommandSerializer::SendRecall(CClientNetwork& net)
{
    GameCommandWire wire{};
    wire.kind = eCommandKind::Recall;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;

    static u32_t s_recallLogCount = 0;
    if (s_recallLogCount < 32u)
    {
        Winters::DevSmoke::Log(
            "[Command] client send recall sid=%u myNet=%u seq=%u\n",
            net.GetMySessionId(),
            net.GetMyNetEntityId(),
            wire.sequenceNum);
        ++s_recallLogCount;
    }

    SendSingle(net, wire);
}
```

1-8. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameCombatInputBridge.cpp

기존 코드:

```cpp
#include "Network/Client/CommandSerializer.h"
#include "Dev/SmokeLog.h"
```

아래로 교체:

```cpp
#include "Network/Client/ClientNetwork.h"
#include "Network/Client/CommandSerializer.h"
#include "Dev/SmokeLog.h"
```

기존 코드:

```cpp
    if (!bImGuiKbd)
    {
        if (in.IsKeyPressed('Q'))
        {
            ClearNetworkAttackIntent();
            scene.DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::Q));
        }
        if (in.IsKeyPressed('D'))
            scene.TriggerFlash();
```

아래로 교체:

```cpp
    if (!bImGuiKbd)
    {
        if (in.IsKeyPressed('B') && scene.IsNetworkAuthoritativeGameplay())
        {
            CCommandSerializer* pCommandSerializer = scene.GetCommandSerializer();
            CClientNetwork* pNetworkView = scene.GetNetworkView();
            if (pCommandSerializer && pNetworkView && pNetworkView->IsConnected())
            {
                ClearNetworkAttackIntent();
                pCommandSerializer->SendRecall(*pNetworkView);
            }
        }

        if (in.IsKeyPressed('Q'))
        {
            ClearNetworkAttackIntent();
            scene.DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::Q));
        }
        if (in.IsKeyPressed('D'))
            scene.TriggerFlash();
```

1-9. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Systems/MoveSystem.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Systems/MoveSystem.h"
#include "Shared/GameSim/Systems/RecallSystem.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue.h"
```

기존 코드:

```cpp
    CStatSystem::Execute(m_world);
    CBuffSystem::Execute(m_world, tc);
    CSkillCooldownSystem::Execute(m_world, tc);
    CMoveSystem::Execute(m_world, tc);
    CAttackChaseSystem::Execute(m_world, tc, m_pendingExecCommands);
```

아래로 교체:

```cpp
    CStatSystem::Execute(m_world);
    CBuffSystem::Execute(m_world, tc);
    CSkillCooldownSystem::Execute(m_world, tc);
    CRecallSystem::Execute(m_world, tc);
    CMoveSystem::Execute(m_world, tc);
    CAttackChaseSystem::Execute(m_world, tc, m_pendingExecCommands);
```

2. 검증

미검증:
- 계획서만 재작성했으며 코드 반영은 미검증.
- 런타임에서 B 키 recall command가 서버로 전달되고 2초 뒤 snapshot 위치가 우물/respawn 위치로 갱신되는지 미검증.
- 이동/스킬/기본 공격 command가 진행 중 recall을 취소하는지 미검증.

검증 명령:
- git diff --check
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Server
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Client

수동 확인:
- 네트워크 권위 인게임에서 B 키를 누른 뒤 2초 후 플레이어 챔피언이 자기 팀 respawn 위치로 이동하는지 확인.
- recall 대기 중 우클릭 이동, 기본 공격, Q/W/E/R 입력을 보내면 recall이 취소되는지 확인.
- 자동 smoke 검증은 사용하지 않는다.

확인 필요:
- 새로 추가한 `RecallComponent.h`, `RecallSystem.h`, `RecallSystem.cpp`가 Client/Server 빌드 프로젝트에 포함되는지 확인.
