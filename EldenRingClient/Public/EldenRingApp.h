#pragma once

#include "WintersEngine.h"

class CEldenRingApp final : public IWintersApp
{
public:
    CEldenRingApp() = default;
    ~CEldenRingApp() override = default;

    bool OnInit() override;
    void OnUpdate(f32_t deltaTime) override;
    void OnRender() override;
    void OnShutdown() override;
};
