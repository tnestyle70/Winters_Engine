#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "AI/MCTSPlanner.h"
#include "ECS/Entity.h"

#include <memory>
#include <string>
#include <vector>

class CWorld;

NS_BEGIN(Engine)

class WINTERS_ENGINE CRLBridge
{
public:
    static std::unique_ptr<CRLBridge> Create();
    ~CRLBridge();

    CRLBridge(const CRLBridge&) = delete;
    CRLBridge& operator=(const CRLBridge&) = delete;

    bool LoadModel(const std::string& onnxPath);
    bool IsLoaded() const { return m_bLoaded; }

    static constexpr u32_t STATE_DIM = 24;
    static constexpr u32_t ACTION_DIM = static_cast<u32_t>(eMCTSAction::END);

    void EncodeState(CWorld& world, EntityID self, std::vector<f32_t>& out) const;
    bool Infer(const std::vector<f32_t>& state, std::vector<f32_t>& outLogits);
    i32_t BestAction(const std::vector<f32_t>& logits) const;

private:
    CRLBridge() = default;

    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
    bool_t m_bLoaded = false;
};

NS_END
