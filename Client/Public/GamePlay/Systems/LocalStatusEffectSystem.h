#pragma once

#include "ECS/ISystem.h"
#include "WintersTypes.h"

#include <memory>

class CWorld;

class CLocalStatusEffectSystem final : public ISystem
{
public:
    ~CLocalStatusEffectSystem() override = default;

    static std::unique_ptr<CLocalStatusEffectSystem> Create();
    // Vision(phase 5)과 같은 phase: 접근 집합이 서로 disjoint라 스케줄러가
    // 한 배치로 묶어 병렬 실행한다. 이 시스템의 소비자는 phase 4~5 사이에 없다.
    u32_t GetPhase() const override { return 5; }
    const char* GetName() const override { return "LocalStatusEffectSystem"; }
    void Execute(CWorld& world, f32_t fTimeDelta) override;
    void DescribeAccess(CSystemAccessBuilder& builder) const override;

private:
    CLocalStatusEffectSystem() = default;
};
