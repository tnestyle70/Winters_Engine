#pragma once
#include "WintersEngine.h"

class CGameApp final : public IWintersApp
{
public:
    CGameApp() = default;
    ~CGameApp() = default;

    bool OnInit()                    override;
    void OnUpdate(f32_t) override {}      // SceneManager가 처리
    void OnRender()                  override {}      // SceneManager가 처리
    void OnShutdown()                override;
};
