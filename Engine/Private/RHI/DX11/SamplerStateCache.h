#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>

// ─────────────────────────────────────────────────────────────────
//  CSamplerStateCache — 자주 쓰이는 샘플러 상태 미리 생성
//
//  Point / Linear / Anisotropic (Wrap / Clamp) 조합 6개를 초기화 시 생성.
//  모델 렌더러는 slot 0~5에 바인딩만 하면 됨.
// ─────────────────────────────────────────────────────────────────

enum class SamplerPreset : uint8_t
{
    PointWrap,
    PointClamp,
    LinearWrap,
    LinearClamp,
    AnisotropicWrap,
    AnisotropicClamp,
    Count
};

class CSamplerStateCache
{
public:
    static CSamplerStateCache& Instance();

    bool Initialize(ID3D11Device* device);
    void Shutdown();

    ID3D11SamplerState* Get(SamplerPreset preset) const;

    // 일괄 바인딩 (PS slot 0~5)
    void BindAllPS(ID3D11DeviceContext* context, UINT startSlot = 0) const;

private:
    CSamplerStateCache() = default;
    ~CSamplerStateCache() = default;
    CSamplerStateCache(const CSamplerStateCache&) = delete;
    CSamplerStateCache& operator=(const CSamplerStateCache&) = delete;

    ID3D11SamplerState* m_pStates[static_cast<size_t>(SamplerPreset::Count)] = {};
};