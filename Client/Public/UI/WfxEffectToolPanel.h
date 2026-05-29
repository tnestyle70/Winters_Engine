#pragma once

class CScene_InGame;

namespace UI
{
    class CWfxEffectToolPanel final
    {
    public:
        static void Render(CScene_InGame* pScene);

    private:
        CWfxEffectToolPanel() = delete;
    };
}
