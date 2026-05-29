#pragma once

#include "Defines.h"
#include "ECS/ISystem.h"
#include "ECS/Entity.h"

#include <memory>
#include <unordered_map>

class ModelRenderer;

class CYoneSoulSpawnSystem final : public ISystem
{
public:
    static std::unique_ptr<CYoneSoulSpawnSystem> Create();

    u32_t GetPhase() const override { return 9u; }
    void Execute(CWorld& world, float fTimeDelta) override;
    const char* GetName() const override { return "YoneSoulSpawnSystem"; }

private:
    void SpawnSoul(CWorld& world, EntityID owner);
    void DespawnSoul(CWorld& world, EntityID owner, bool_t bReturnBody);

    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_mapSoulRenderers{};
};
