#pragma once
#include "WintersTypes.h"
#include <atomic>

// 디바이스 드로우 깔때기(DX11Buffer/CDX11FrameCommandList)와 원시 드로우 사이트,
// 상태 바인드 사이트가 공유하는 프레임 통계.
// 드로우마다 프로파일러 AddCounter(전역 mutex + O(n) strcmp)를 잡지 않도록
// relaxed atomic 으로 모으고, CDX11Device::EndFrame 이 프레임당 1회 게이지로 방출·리셋한다.
// Engine DLL 내부 전용 — Client 에서 include 하면 DLL 경계로 인스턴스가 갈라진다.
namespace RenderFrameStats
{
    inline std::atomic<uint64_t> s_uDrawCalls{ 0 };
    inline std::atomic<uint64_t> s_uDrawIndices{ 0 };
    inline std::atomic<uint64_t> s_uBindShader{ 0 };
    inline std::atomic<uint64_t> s_uBindPipeline{ 0 };
    inline std::atomic<uint64_t> s_uBindTexture{ 0 };
    inline std::atomic<uint64_t> s_uBindBlend{ 0 };

    inline void AddDraw(uint64_t indexOrVertexCount)
    {
        s_uDrawCalls.fetch_add(1u, std::memory_order_relaxed);
        s_uDrawIndices.fetch_add(indexOrVertexCount, std::memory_order_relaxed);
    }
    inline void AddBindShader()   { s_uBindShader.fetch_add(1u, std::memory_order_relaxed); }
    inline void AddBindPipeline() { s_uBindPipeline.fetch_add(1u, std::memory_order_relaxed); }
    inline void AddBindTexture()  { s_uBindTexture.fetch_add(1u, std::memory_order_relaxed); }
    inline void AddBindBlend()    { s_uBindBlend.fetch_add(1u, std::memory_order_relaxed); }
}
