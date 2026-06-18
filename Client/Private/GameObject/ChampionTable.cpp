#include "GameObject/ChampionDef.h"
#include "GamePlay/ChampionRegistry.h"

static const ChampionDef s_ChampionTable[] =
{
    { eChampion::YASUO,   "yasuo_",   "idle1", "run1", "attack1", 1.5f,
      "Client/Bin/Resource/Texture/Character/Yasuo/yasuo_fixed.wmesh",
      L"Shaders/Mesh3D.hlsl",
      L"Client/Bin/Resource/Texture/Character/Yasuo/yasuo_base_tx_cm.png",
      {
          L"Client/Bin/Resource/Texture/Character/Yasuo/yasuo_base_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Yasuo/yasuo_base_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Yasuo/yasuo_base_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Yasuo/yasuo_base_tx_cm.png"
      },
      { 27.f, 1.f, -6.f },
      0.01f,
      "Yasuo" },


    { eChampion::KALISTA, "kalista_", "idle1", "run", "attack1", 5.5f,
      "Client/Bin/Resource/Texture/Character/Kalista/kalista.wmesh",
      L"Shaders/Mesh3D.hlsl",
      L"Client/Bin/Resource/Texture/Character/Kalista/kalista_base_tx_cm.png",
      {
          L"Client/Bin/Resource/Texture/Character/Kalista/kalista_base_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Kalista/kalista_base_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Kalista/kalistaaltar_base_tx_cm.png"
      },
      { 30.f, 1.f, -6.f },
      0.01f,
      "Kalista" },


      { eChampion::SYLAS, "", "skinned_mesh_sylas_idle", "skinned_mesh_sylas_run", "skinned_mesh_sylas_attack_01", 1.5f,
      "Client/Bin/Resource/Texture/Character/Sylas/sylas.wmesh",
      L"Shaders/Mesh3D.hlsl",
      L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_tx_cm.png",
      {
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_shackles_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_shackles_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_shackles_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_shackles_tx_cm.png"
      },
      { -27.f, 1.f, 6.f },
      0.01f,
      "Sylas" },


    { eChampion::VIEGO, "viego_", "idle1", "run", "attack1", 1.5f,
      "Client/Bin/Resource/Texture/Character/Viego/viego_fixed.wmesh",
      L"Shaders/Mesh3D.hlsl",
      L"Client/Bin/Resource/Texture/Character/Viego/viego_base_body_tx_cm.png",
      {
          nullptr,
          nullptr,
          nullptr,
          L"Client/Bin/Resource/Texture/Character/Viego/viego_base_crown_sword_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Viego/viego_base_crown_sword_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Viego/viego_base_wraith_tx_cm.png"
      },
      { -30.f, 1.f, 6.f },
      0.01f,
      "Viego" },


    { eChampion::GAREN,   "", "garen_2013_idle1", "garen_2013_run", "garen_2013_attack_01", 1.5f,
      "Client/Bin/Resource/Texture/Character/Garen/garen.wmesh",
      L"Shaders/Mesh3D.hlsl",
      L"Client/Bin/Resource/Texture/Character/Garen/garen_base_tx_cm.png",
      {
          nullptr,
          L"Client/Bin/Resource/Texture/Character/Garen/garen_base_tx_cm.png"
      },
      { 33.f, 1.f, -6.f },
      0.01f,
      "Garen" },


    { eChampion::ZED, "", "zed_idle1", "zed_run", "zed_attack1", 1.5f,
      "Client/Bin/Resource/Texture/Character/Zed/zed.wmesh",
      L"Shaders/Mesh3D.hlsl",
      L"Client/Bin/Resource/Texture/Character/Zed/zed_base_tx_cm.png",
      {
          L"Client/Bin/Resource/Texture/Character/Zed/zed_base_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Zed/zed_base_tx_cm.png"
      },
      { 36.f, 1.f, -6.f },
      0.01f,
      "Zed" },


    { eChampion::RIVEN, "riven_", "idle1", "run", "attack1", 1.5f,
      "Client/Bin/Resource/Texture/Character/Riven/riven.wmesh",
      L"Shaders/Mesh3D.hlsl",
      L"Client/Bin/Resource/Texture/Character/Riven/riven_base_tx_cm.png",
      {},
      { 24.f, 1.f, 0.f },
      0.01f,
      "Riven" },
};

const ChampionDef* FindChampionDef(eChampion champ)
{
    for (const auto& c : s_ChampionTable)
    {
        if (c.id == champ)
            return &c;
    }
    return nullptr;
}

const char* GetChampionDisplayName(eChampion champ)
{
    switch (champ)
    {
    case eChampion::IRELIA: return "Irelia";
    case eChampion::YASUO: return "Yasuo";
    case eChampion::KALISTA: return "Kalista";
    case eChampion::SYLAS: return "Sylas";
    case eChampion::VIEGO: return "Viego";
    case eChampion::ANNIE: return "Annie";
    case eChampion::ASHE: return "Ashe";
    case eChampion::FIORA: return "Fiora";
    case eChampion::GAREN: return "Garen";
    case eChampion::RIVEN: return "Riven";
    case eChampion::ZED: return "Zed";
    case eChampion::EZREAL: return "Ezreal";
    case eChampion::YONE: return "Yone";
    case eChampion::JAX: return "Jax";
    case eChampion::MASTERYI: return "MasterYi";
    case eChampion::KINDRED: return "Kindred";
    case eChampion::LEESIN: return "LeeSin";
    default: return "(unnamed)";
    }
}

void RegisterAllLegacy()
{
    for (const auto& cd : s_ChampionTable)
    {
        ChampionDef copy = cd;
        if (!copy.displayName)
            copy.displayName = GetChampionDisplayName(copy.id);
        CChampionRegistry::Instance().Add(copy.id, copy);
    }
}
