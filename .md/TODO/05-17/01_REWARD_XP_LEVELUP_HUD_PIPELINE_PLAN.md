Session - 서버 권위 킬/사망/경험치/레벨업을 HUD 진행 UI까지 연결한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ExperienceSystem.h

새 파일:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "WintersTypes.h"

class CWorld;

class CExperienceSystem
{
public:
    static void InitializeChampionExperience(CWorld& world, EntityID entity, u8_t level);
    static void GrantExperience(CWorld& world, EntityID entity, f32_t amount);
    static void GrantKillRewards(CWorld& world, EntityID killer, EntityID victim);

private:
    static f32_t ResolveChampionKillExperience(CWorld& world, EntityID victim);
    static void GrantGold(CWorld& world, EntityID entity, f32_t amount);

    CExperienceSystem() = delete;
};
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ExperienceSystem.cpp

새 파일:

```cpp
#include "Shared/GameSim/Systems/ExperienceSystem.h"

#include "ECS/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/GoldRewardDef.h"
#include "Shared/GameSim/Registries/RewardRegistry.h"
#include "Shared/GameSim/Systems/SkillRankSystem.h"
#include "Shared/GameSim/World.h"

#include <algorithm>

namespace
{
    eMinionRewardKind ResolveMinionRewardKind(u8_t roleType)
    {
        if (roleType == 1)
            return eMinionRewardKind::Ranged;
        if (roleType == 2)
            return eMinionRewardKind::Siege;
        if (roleType == 3)
            return eMinionRewardKind::Super;
        return eMinionRewardKind::Melee;
    }

    u32_t RoundRewardGold(f32_t amount)
    {
        if (amount <= 0.f)
            return 0u;
        return static_cast<u32_t>(amount + 0.5f);
    }
}

void CExperienceSystem::InitializeChampionExperience(CWorld& world, EntityID entity, u8_t level)
{
    if (entity == NULL_ENTITY || !world.IsAlive(entity))
        return;

    ExperienceComponent xp{};
    xp.level = (level > 0) ? level : 1;
    xp.requiredForNextLevel = CRewardRegistry::Instance().GetRequiredExperienceForNextLevel(xp.level);

    if (world.HasComponent<ExperienceComponent>(entity))
        world.GetComponent<ExperienceComponent>(entity) = xp;
    else
        world.AddComponent<ExperienceComponent>(entity, xp);
}

void CExperienceSystem::GrantExperience(CWorld& world, EntityID entity, f32_t amount)
{
    if (entity == NULL_ENTITY ||
        amount <= 0.f ||
        !world.IsAlive(entity) ||
        !world.HasComponent<ExperienceComponent>(entity))
    {
        return;
    }

    auto& xp = world.GetComponent<ExperienceComponent>(entity);
    xp.current += amount;
    xp.total += amount;

    bool_t bLeveled = false;
    while (xp.level < ChampionExperienceCurveDef::kMaxChampionLevel)
    {
        if (xp.requiredForNextLevel <= 0.f)
            xp.requiredForNextLevel =
                CRewardRegistry::Instance().GetRequiredExperienceForNextLevel(xp.level);
        if (xp.requiredForNextLevel <= 0.f || xp.current < xp.requiredForNextLevel)
            break;

        xp.current -= xp.requiredForNextLevel;
        ++xp.level;
        xp.requiredForNextLevel =
            CRewardRegistry::Instance().GetRequiredExperienceForNextLevel(xp.level);
        bLeveled = true;
    }

    if (xp.level >= ChampionExperienceCurveDef::kMaxChampionLevel)
    {
        xp.current = 0.f;
        xp.requiredForNextLevel = 0.f;
    }

    if (world.HasComponent<ChampionComponent>(entity))
        world.GetComponent<ChampionComponent>(entity).level = xp.level;

    if (world.HasComponent<StatComponent>(entity))
    {
        auto& stat = world.GetComponent<StatComponent>(entity);
        if (stat.level != xp.level)
        {
            stat.level = xp.level;
            stat.bDirty = true;
        }
    }

    if (bLeveled && world.HasComponent<SkillRankComponent>(entity))
        CSkillRankSystem::SyncPointsForLevel(world.GetComponent<SkillRankComponent>(entity), xp.level);
}

void CExperienceSystem::GrantKillRewards(CWorld& world, EntityID killer, EntityID victim)
{
    if (killer == NULL_ENTITY ||
        victim == NULL_ENTITY ||
        killer == victim ||
        !world.IsAlive(killer) ||
        !world.IsAlive(victim) ||
        !world.HasComponent<ChampionComponent>(killer))
    {
        return;
    }

    if (world.HasComponent<MinionComponent>(victim))
    {
        const auto& minion = world.GetComponent<MinionComponent>(victim);
        const RewardDef* reward = CRewardRegistry::Instance().FindReward(
            eRewardSourceKind::Minion,
            static_cast<u8_t>(ResolveMinionRewardKind(minion.roleType)));
        if (!reward)
            return;

        GrantGold(world, killer, reward->gold.killerGold);
        GrantExperience(world, killer, reward->experience.nearbyXP);
        return;
    }

    if (world.HasComponent<ChampionComponent>(victim))
    {
        const RewardDef* reward = CRewardRegistry::Instance().FindReward(eRewardSourceKind::Champion);
        if (!reward)
            return;

        GrantGold(world, killer, reward->gold.killerGold);
        GrantExperience(world, killer, ResolveChampionKillExperience(world, victim));
    }
}

f32_t CExperienceSystem::ResolveChampionKillExperience(CWorld& world, EntityID victim)
{
    const RewardDef* reward = CRewardRegistry::Instance().FindReward(eRewardSourceKind::Champion);
    if (!reward)
        return 0.f;

    u8_t victimLevel = 1;
    f32_t nextLevelXp = 0.f;
    if (world.HasComponent<ExperienceComponent>(victim))
    {
        const auto& xp = world.GetComponent<ExperienceComponent>(victim);
        victimLevel = xp.level;
        nextLevelXp = xp.requiredForNextLevel;
    }
    else if (world.HasComponent<ChampionComponent>(victim))
    {
        victimLevel = world.GetComponent<ChampionComponent>(victim).level;
    }

    if (nextLevelXp <= 0.f)
        nextLevelXp = CRewardRegistry::Instance().GetRequiredExperienceForNextLevel(victimLevel);

    return std::max(0.f, nextLevelXp * reward->experience.victimNextLevelXPFactor);
}

void CExperienceSystem::GrantGold(CWorld& world, EntityID entity, f32_t amount)
{
    if (entity == NULL_ENTITY ||
        amount <= 0.f ||
        !world.IsAlive(entity) ||
        !world.HasComponent<GoldComponent>(entity))
    {
        return;
    }

    world.GetComponent<GoldComponent>(entity).amount += RoundRewardGold(amount);
}
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/DamageQueueSystem.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Systems/ExperienceSystem.h"
#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue.h"
```

`CDamageQueueSystem::Execute` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        if (result.finalAmount > 0.f && request.target != NULL_ENTITY)
        {
            ReplicatedEventComponent event{};
            event.kind = eReplicatedEventKind::Damage;
            event.sourceEntity = request.source;
            event.targetEntity = request.target;
            event.amount = result.finalAmount;
            event.damageType = request.type;
            event.bWasCrit = result.bWasCrit;
            event.bKilled = result.bKilled;
            event.skillId = request.skillId;
            event.flags = static_cast<u16_t>(request.flags & 0xffffu);
            event.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(world, event);
        }
```

아래에 추가:

```cpp
        if (result.bKilled &&
            request.source != NULL_ENTITY &&
            request.target != NULL_ENTITY)
        {
            CExperienceSystem::GrantKillRewards(world, request.source, request.target);
        }
```

1-4. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
```

기존 코드:

```cpp
#include "Shared/GameSim/Systems/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/GameplayStateQuery.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Systems/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/ExperienceSystem.h"
#include "Shared/GameSim/Systems/GameplayStateQuery.h"
```

`CGameRoom::SpawnChampionForLobbySlot` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    ExperienceComponent xp{};
    xp.level = stat.level;
    m_world.AddComponent<ExperienceComponent>(entity, xp);
```

아래로 교체:

```cpp
    CExperienceSystem::InitializeChampionExperience(m_world, entity, stat.level);
```

`CGameRoom::SpawnChampionForLobbySlot` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    SkillRankComponent skillRank{};
    skillRank.pointsAvailable = 1;
    m_world.AddComponent<SkillRankComponent>(entity, skillRank);
```

아래에 추가:

```cpp
    GoldComponent gold{};
    gold.amount = 500;
    m_world.AddComponent<GoldComponent>(entity, gold);

    InventoryComponent inventory{};
    m_world.AddComponent<InventoryComponent>(entity, inventory);
```

`CGameRoom::SpawnChampion` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    ExperienceComponent xp{};
    xp.level = stat.level;
    m_world.AddComponent<ExperienceComponent>(entity, xp);
```

아래로 교체:

```cpp
    CExperienceSystem::InitializeChampionExperience(m_world, entity, stat.level);
```

`CGameRoom::SpawnChampion` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    SkillRankComponent skillRank{};
    skillRank.pointsAvailable = 1;
    m_world.AddComponent<SkillRankComponent>(entity, skillRank);
```

아래에 추가:

```cpp
    GoldComponent gold{};
    gold.amount = 500;
    m_world.AddComponent<GoldComponent>(entity, gold);

    InventoryComponent inventory{};
    m_world.AddComponent<InventoryComponent>(entity, inventory);
```

`CGameRoom::Phase_SimulationSystems` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    CDamageQueueSystem::Execute(m_world, tc);
```

아래에 추가:

```cpp
    CStatSystem::Execute(m_world);
```

1-5. C:/Users/user/Desktop/Winters/Shared/Schemas/Snapshot.fbs

기존 코드:

```fbs
    level:ubyte;
    hp:float;
```

아래로 교체:

```fbs
    level:ubyte;
    xpCurrent:float;
    xpRequired:float;
    skillPoints:ubyte;
    hp:float;
```

1-6. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

기존 코드:

```cpp
        u8_t level = 1;
        u32_t buffMask = 0;
```

아래로 교체:

```cpp
        u8_t level = 1;
        f32_t xpCurrent = 0.f;
        f32_t xpRequired = 0.f;
        u8_t skillPoints = 0;
        u32_t buffMask = 0;
```

기존 코드:

```cpp
        if (world.HasComponent<ChampionComponent>(entity))
        {
            const auto& champion = world.GetComponent<ChampionComponent>(entity);
            championId = static_cast<u8_t>(champion.id);
            team = static_cast<u8_t>(champion.team);
```

아래에 추가:

```cpp
        if (world.HasComponent<ExperienceComponent>(entity))
        {
            const auto& xp = world.GetComponent<ExperienceComponent>(entity);
            xpCurrent = xp.current;
            xpRequired = xp.requiredForNextLevel;
            level = xp.level;
        }
```

기존 코드:

```cpp
        std::vector<u8_t> ranks;
        if (world.HasComponent<SkillRankComponent>(entity))
        {
            const auto& skillRank = world.GetComponent<SkillRankComponent>(entity);
            ranks.reserve(SkillRankComponent::kSlotCount);
            for (u8_t i = 0; i < SkillRankComponent::kSlotCount; ++i)
                ranks.push_back(skillRank.ranks[i]);
        }
```

아래로 교체:

```cpp
        std::vector<u8_t> ranks;
        if (world.HasComponent<SkillRankComponent>(entity))
        {
            const auto& skillRank = world.GetComponent<SkillRankComponent>(entity);
            skillPoints = skillRank.pointsAvailable;
            ranks.reserve(SkillRankComponent::kSlotCount);
            for (u8_t i = 0; i < SkillRankComponent::kSlotCount; ++i)
                ranks.push_back(skillRank.ranks[i]);
        }
```

`Shared::Schema::CreateEntitySnapshot` 호출부에서 아래 기존 인자 순서를 교체:

기존 코드:

```cpp
            level,
            hp,
```

아래로 교체:

```cpp
            level,
            xpCurrent,
            xpRequired,
            skillPoints,
            hp,
```

1-7. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

기존 코드:

```cpp
            if (world.HasComponent<ExperienceComponent>(e))
                world.GetComponent<ExperienceComponent>(e).level = es->level();
```

아래로 교체:

```cpp
            if (!world.HasComponent<ExperienceComponent>(e))
                world.AddComponent<ExperienceComponent>(e, ExperienceComponent{});

            auto& xp = world.GetComponent<ExperienceComponent>(e);
            xp.level = es->level();
            xp.current = es->xpCurrent();
            xp.requiredForNextLevel = es->xpRequired();
```

기존 코드:

```cpp
                rank.pointsAvailable = (es->level() > spent)
                    ? static_cast<u8_t>(es->level() - spent)
                    : 0;
```

아래로 교체:

```cpp
                rank.pointsAvailable = es->skillPoints();
```

1-8. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/ChampionHUDPanel.cpp

`DrawElementRHI` 안에서 element color를 결정하는 블록에 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        Vec4 color = WhiteVec();
```

아래에 추가:

```cpp
        if (element.strID == "xp.fill")
            color = Vec4{ 0.55f, 0.18f, 0.95f, 1.f };
```

1-9. C:/Users/user/Desktop/Winters/Client/Bin/Resource/UI/hud_irelia_layout.json

기존 코드:

```json
        {
            "id": "mp.fill",
            "sprite": "bar.mp.fill",
            "bind": "mpRatio",
            "rect": [289.00, 146.00, 315.00, 9.00]
        },
```

아래에 추가:

```json
        {
            "id": "xp.fill",
            "sprite": "bar.mp.fill",
            "bind": "xpRatio",
            "rect": [289.00, 158.00, 315.00, 4.00]
        },
```

1-10. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

기존 코드:

```cpp
    void* m_pSRV_IreliaPortrait = nullptr;
    std::array<void*, 4> m_pSRV_IreliaSkillIcons{};
```

아래로 교체:

```cpp
    eChampion m_eLoadedChampionHudAssets = eChampion::END;
    void* m_pSRV_ChampionPortrait = nullptr;
    void* m_pSRV_ChampionPassiveIcon = nullptr;
    std::array<void*, 4> m_pSRV_ChampionSkillIcons{};
```

1-11. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

기존 코드:

```cpp
    constexpr const wchar_t* kPathIreliaPortrait = L"Resource/Texture/Character/Irelia/irelialoadscreen.png";
    constexpr const wchar_t* kPathIreliaQIcon = L"Resource/Texture/Character/Irelia/icons2d/irelia_q.png";
    constexpr const wchar_t* kPathIreliaWIcon = L"Resource/Texture/Character/Irelia/icons2d/irelia_w.png";
    constexpr const wchar_t* kPathIreliaEIcon = L"Resource/Texture/Character/Irelia/icons2d/irelia_e.png";
    constexpr const wchar_t* kPathIreliaRIcon = L"Resource/Texture/Character/Irelia/icons2d/irelia_r.png";
```

아래로 교체:

```cpp
    struct ChampionHudAssetDef
    {
        eChampion champion = eChampion::END;
        const wchar_t* pPortrait = nullptr;
        const wchar_t* pPassive = nullptr;
        const wchar_t* pSkillIcons[4]{};
    };

    constexpr ChampionHudAssetDef kChampionHudAssets[] =
    {
        {
            eChampion::IRELIA,
            L"Resource/Texture/Character/Irelia/irelialoadscreen.png",
            L"Resource/Texture/Character/Irelia/icons2d/irelia_passive.png",
            {
                L"Resource/Texture/Character/Irelia/icons2d/irelia_q.png",
                L"Resource/Texture/Character/Irelia/icons2d/irelia_w.png",
                L"Resource/Texture/Character/Irelia/icons2d/irelia_e.png",
                L"Resource/Texture/Character/Irelia/icons2d/irelia_r.png",
            },
        },
    };
```

`LoadChampionHUDAssets` 안에서 Irelia 고정 로딩 블록은 아래 동작으로 교체:

```cpp
const ChampionHudAssetDef* ResolveChampionHudAssets(eChampion champion)
{
    for (const ChampionHudAssetDef& def : kChampionHudAssets)
    {
        if (def.champion == champion)
            return &def;
    }
    return &kChampionHudAssets[0];
}
```

```cpp
void CUI_Manager::LoadChampionHUDAssetsForChampion(eChampion champion)
{
    if (m_eLoadedChampionHudAssets == champion)
        return;

    ReleaseSRV(m_pSRV_ChampionPortrait);
    ReleaseSRV(m_pSRV_ChampionPassiveIcon);
    for (void*& pIconSRV : m_pSRV_ChampionSkillIcons)
        ReleaseSRV(pIconSRV);

    const ChampionHudAssetDef* pDef = ResolveChampionHudAssets(champion);
    if (!pDef)
        return;

    m_eLoadedChampionHudAssets = pDef->champion;
    if (pDef->pPortrait && SUCCEEDED(Load_TextureSRV(pDef->pPortrait, &m_pSRV_ChampionPortrait)))
        m_pChampionHudPanel->SetPortraitTexture(m_pSRV_ChampionPortrait);

    if (pDef->pPassive)
        (void)Load_TextureSRV(pDef->pPassive, &m_pSRV_ChampionPassiveIcon);

    for (u32_t i = 0; i < m_pSRV_ChampionSkillIcons.size(); ++i)
    {
        if (pDef->pSkillIcons[i] &&
            SUCCEEDED(Load_TextureSRV(pDef->pSkillIcons[i], &m_pSRV_ChampionSkillIcons[i])))
        {
            m_pChampionHudPanel->SetSkillIconTexture(i, m_pSRV_ChampionSkillIcons[i]);
        }
    }
}
```

`CUI_Manager::DrawChampionHUDOverlay` 시작부에서 아래 코드를 추가:

```cpp
    LoadChampionHUDAssetsForChampion(State.Champion);
```

`CUI_Manager::DrawChampionHUDOverlay`의 Q/W/E/R 루프 안에서 cooldown 숫자 표시 전에 아래 코드를 추가:

```cpp
        if (Rank > 0)
        {
            const std::string StrRankText = std::to_string(static_cast<u32_t>(Rank));
            const ImVec2 RankPos = ToPosition(kSkillSlotTextX[Index], 105.f);
            pDraw->AddText(RankPos, IM_COL32(238, 220, 150, 255), StrRankText.c_str());
        }

        if (bCanLevel)
        {
            const ImVec2 ArrowMin = ToPosition(kSkillSlotTextX[Index] - 12.f, 43.f);
            const ImVec2 ArrowMax = ToPosition(kSkillSlotTextX[Index] + 12.f, 58.f);
            pDraw->AddTriangleFilled(
                ImVec2((ArrowMin.x + ArrowMax.x) * 0.5f, ArrowMin.y),
                ImVec2(ArrowMax.x, ArrowMax.y),
                ImVec2(ArrowMin.x, ArrowMax.y),
                IM_COL32(245, 218, 112, 235));
        }
```

`CUI_Manager::DrawChampionHUDOverlay`에서 스킬 루프 뒤에 아래 코드를 추가:

```cpp
    if (m_pSRV_ChampionPassiveIcon)
    {
        const ImVec2 PassiveMin = ToPosition(303.f, 75.f);
        const ImVec2 PassiveMax = ToPosition(337.f, 109.f);
        pDraw->AddImage(
            static_cast<ImTextureID>(m_pSRV_ChampionPassiveIcon),
            PassiveMin,
            PassiveMax);
    }
```

2. 검증

미검증:
- 코드 미반영.
- `bar.mp.fill`에 보라색 tint를 적용했을 때 XP fill이 의도한 색으로 보이는지 미검증.
- champion별 HUD asset fallback이 Irelia 외 챔피언에서 정상 동작하는지 미검증.
- 미니언 last-hit reward와 챔피언 kill reward가 같은 틱에서 중복 지급되지 않는지 미검증.

검증 명령:
- `.\Shared\Schemas\run_codegen.bat`
- `git diff --check`
- `MSBuild.exe .\Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Server`
- `MSBuild.exe .\Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Client`

확인 필요:
- 새 `ExperienceSystem.h/.cpp`가 Server 빌드 프로젝트에 포함되는지 확인.
- Client 빌드에 `ExperienceSystem.cpp`를 포함할지 확인. 1차 구현은 서버 전용이어도 되지만 shared smoke에서 직접 호출하려면 Client 프로젝트 등록이 필요하다.
- `CreateEntitySnapshot` 인자 순서가 `run_codegen.bat` 이후 생성된 `Snapshot_generated.h`와 정확히 맞는지 확인.
- `CUI_Manager::LoadChampionHUDAssetsForChampion` 선언을 header private 영역에 추가해야 한다.
- 기존 `ReleaseChampionHUDAssets`가 새 `m_pSRV_ChampionPortrait`, `m_pSRV_ChampionPassiveIcon`, `m_pSRV_ChampionSkillIcons` 이름을 해제하도록 같이 교체해야 한다.

후속 동기화:
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.
