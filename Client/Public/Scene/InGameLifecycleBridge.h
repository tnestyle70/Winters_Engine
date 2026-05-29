#pragma once

class CScene_InGame;

class CInGameLifecycleBridge final
{
public:
    static void Shutdown(CScene_InGame& scene);
};
