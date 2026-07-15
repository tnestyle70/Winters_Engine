#pragma once

#include "WintersTypes.h"

namespace Engine
{
    class CGameInstance;
}

namespace Client
{
    void RegisterLoLUIContent(Engine::CGameInstance& GameInstance);

    // 인게임 상점 배치 편집 (Practice Tool ImGui 전용).
    // 항목 순서/노출만 클라 표현을 바꾸며, 구매 검증·스탯은 서버 권위 그대로다.
    struct LoLShopEditorEntryView
    {
        u16_t iItemId = 0u;
        const char* pDisplayName = nullptr;
        bool_t bEnabled = true;
    };

    u32_t GetLoLShopEditorEntryCount();
    LoLShopEditorEntryView GetLoLShopEditorEntry(u32_t Index);
    void SetLoLShopEditorEntryEnabled(u32_t Index, bool_t bEnabled);
    bool_t MoveLoLShopEditorEntry(u32_t Index, bool_t bMoveUp);
    void ReapplyLoLShopItems(Engine::CGameInstance& GameInstance);
}
