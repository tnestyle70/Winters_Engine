#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include <DirectXMath.h>

class CWorld;
class CScene_InGame;
struct ImDrawList;

namespace UI
{
    class CDebugDrawSystem
    {
    public:
        static void Render(CWorld& world, CScene_InGame* pScene, const Mat4& matVP);

    private:
        static void DrawNavGrid(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
        static void DrawPathNavGrid(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
        static void DrawStructures(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
        static void DrawColliders(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
        static void DrawChampions(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
    };
}
