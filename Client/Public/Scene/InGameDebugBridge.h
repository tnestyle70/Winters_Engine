#pragma once

#include "Defines.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameContext.h"

class CScene_InGame;
class CWorld;
class CDynamicCamera;
class CFogOfWarRenderer;

struct InGameDebugBridgeDesc
{
    CScene_InGame& scene;
    CWorld& world;
    CFogOfWarRenderer* pFogOfWarRenderer = nullptr;
    CDynamicCamera* pCamera = nullptr;
    eTeam playerTeam = eTeam::Blue;
    bool_t& bLogFrameEvents;
};

class CInGameDebugBridge final
{
public:
    static void Render(InGameDebugBridgeDesc& desc);
};
