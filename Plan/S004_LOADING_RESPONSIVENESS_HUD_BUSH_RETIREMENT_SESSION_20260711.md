Session - 로딩 창의 입력 응답성과 안전한 CPU 병렬 처리를 복구하고, 누락된 인게임 HUD를 실제 리소스 경로로 연결하며, 실패한 교차 PNG 부쉬를 normal F5에서 폐기한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

기존 코드:

```cpp
    constexpr const wchar_t* kPathUnitBlueHPBar = L"Resource/Texture/UI/UnitBlueHPBar.png";
    constexpr const wchar_t* kPathUnitRedHPBar = L"Resource/Texture/UI/UnitRedHPBar.png";
    constexpr const wchar_t* kPathStructureBlueHPBar = L"Resource/Texture/UI/StructureHpBarBlue.png";
    constexpr const wchar_t* kPathStructureRedHPBar = L"Resource/Texture/UI/StructureHpBarRed.png";
```

아래로 교체:

```cpp
    constexpr const wchar_t* kPathUnitBlueHPBar = L"Resource/Texture/UI/MinionBlueHPBar.png";
    constexpr const wchar_t* kPathUnitRedHPBar = L"Resource/Texture/UI/MinionRedHPBar.png";
    constexpr const wchar_t* kPathStructureBlueHPBar = L"Resource/Texture/UI/TurretHpBarBlue.png";
    constexpr const wchar_t* kPathStructureRedHPBar = L"Resource/Texture/UI/TurretHpBarRed.png";
```

기존 코드:

```cpp
    constexpr const wchar_t* kPathActorHUDDefault = L"Resource/Texture/UI/ActorHUD_Default.png";
    constexpr const wchar_t* kPathHUDHit = L"Resource/Texture/UI/HUD/ActorHitFlash.png";
    constexpr const wchar_t* kPathHUDStun = L"Resource/Texture/UI/HUD/ActorStatusFlash.png";
    constexpr const wchar_t* kPathSkillRankPip = L"Resource/Texture/UI/HUD/SkillRankPip.png";
    constexpr const wchar_t* kPathSkillUpgrade = L"Resource/Texture/UI/SkillUpgrade.png";
    constexpr const wchar_t* kPathHUDManifest = L"Resource/UI/hud_atlas_manifest.json";
    constexpr const wchar_t* kPathHUDManifestFallback = L"UI/hud_atlas_manifest.json";
    constexpr const wchar_t* kPathHUDLayout = L"Resource/UI/actor_hud_layout.json";
    constexpr const wchar_t* kPathHUDLayoutFallback = L"UI/actor_hud_layout.json";
    constexpr const wchar_t* kPathInGameShopReference = L"Resource/Texture/UI/?�점1.png";
```

아래로 교체:

```cpp
    constexpr const wchar_t* kPathActorHUDDefault = L"Resource/Texture/UI/HUD_Irelia_2.png";
    constexpr const wchar_t* kPathHUDHit = L"Resource/Texture/UI/HUD/lol_ingame_hit.png";
    constexpr const wchar_t* kPathHUDStun = L"Resource/Texture/UI/HUD/lol_ingame_stun.png";
    constexpr const wchar_t* kPathSkillRankPip = L"Resource/Texture/Character/Irelia/particles/defaultcoloroverlifetime.png";
    constexpr const wchar_t* kPathSkillUpgrade = L"Resource/Texture/UI/SkillUpgrade.png";
    constexpr const wchar_t* kPathHUDManifest = L"Resource/UI/hud_atlas_manifest.json";
    constexpr const wchar_t* kPathHUDManifestFallback = L"UI/hud_atlas_manifest.json";
    constexpr const wchar_t* kPathHUDLayout = L"Resource/UI/hud_irelia_layout.json";
    constexpr const wchar_t* kPathHUDLayoutFallback = L"Client/Bin/Resource/UI/hud_irelia_layout.json";
    constexpr const wchar_t* kPathInGameShopReference = L"Resource/Texture/UI/상점1.png";
```

1-2. C:/Users/user/Desktop/Winters/Data/Stage1.dat

기존 바이너리:

```text
WSTG v5 / S30 / J12 / W27 / B64 / 21540 bytes
SHA-256 14AF613D8D13C6ADB0409C00C913C4902A75F1D9F8E8F5CF9EB27208C13F11FE
```

아래로 교체:

```text
HEAD의 검증된 WSTG v4 / S30 / J12 / W27 / B0 / 5408 bytes
SHA-256 6494A5637D2D24BD962C11ACBD1E495E7DDC5319E2216126C0AF86A369731A28
```

1-3. C:/Users/user/Desktop/Winters/Tools/cook_map11_brush_volumes.py

기존 코드 전체를 아래로 교체:

```python
# Map11 centered brush authoring CSV -> research-only canonical WBRUSH v1.
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CSV = ROOT / "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/manifest/map11_brush_volumes.csv"
OUT = ROOT / "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/map11_brush_volumes.wbrush"

WBRUSH_MAGIC = 0x48534257  # 'WBSH'
WBRUSH_VERSION = 1
MAP11_STAGE_CENTER_X = 104.50


def read_entries():
    entries = []
    for line in CSV.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [part.strip() for part in line.split(",")]
        if len(parts) != 4:
            raise SystemExit(f"bad row: {line!r}")
        bush_id = int(parts[0])
        local_x, local_z, radius = (float(parts[1]), float(parts[2]), float(parts[3]))
        if bush_id <= 0 or radius <= 0.0:
            raise SystemExit(f"bad brush record: {line!r}")
        entries.append((bush_id, local_x + MAP11_STAGE_CENTER_X, local_z, radius))
    if not entries:
        raise SystemExit("no Map11 brush records")
    return entries


def main():
    entries = read_entries()
    payload = struct.pack("<IIII", WBRUSH_MAGIC, WBRUSH_VERSION, len(entries), 0)
    for bush_id, world_x, world_z, radius in entries:
        payload += struct.pack("<Ifff", bush_id, world_x, world_z, radius)
    OUT.write_bytes(payload)
    print(f"cooked {len(entries)} research brush volumes -> {OUT} ({len(payload)} bytes)")


if __name__ == "__main__":
    main()
```

1-4. C:/Users/user/Desktop/Winters/Tools/build_map11_bush_cluster.py

삭제할 범위:
파일 전체를 삭제한다. crossed-card OBJ/WMesh를 다시 생성하는 실행 경로를 남기지 않는다.

1-5. C:/Users/user/Desktop/Winters/Client/Public/Scene/Loader.h

기존 코드:

```cpp
class CJobSystem;
class CJobCounter;
```

아래에 추가:

```cpp
namespace Engine
{
    class CMapSurfaceSampler;
}
```

기존 코드:

```cpp
	void PrepareMainThreadInGameLoad();
	void SetProgress(f32_t fValue);
```

아래로 교체:

```cpp
	void PrepareMainThreadInGameLoad();
	void StartInGameCpuLoad();
	void RunInGameCpuLoad();
	bool_t AreInGameCpuLoadsComplete() const;
	void TryFinishInGameLoad();
	void SetProgress(f32_t fValue);
```

기존 코드:

```cpp
	std::unique_ptr<CJobCounter> m_pCounter{};
	MatchContext m_LoadContext{};

	bool_t m_bMainThreadLoad = false;
	std::vector<LoadStep> m_LoadSteps{};
	u32_t m_iNextLoadStep = 0;
```

아래로 교체:

```cpp
	std::unique_ptr<CJobCounter> m_pCounter{};
	std::unique_ptr<Engine::CMapSurfaceSampler> m_pPreparedMapSurfaceSampler{};
	MatchContext m_LoadContext{};
	std::atomic_bool m_bCancelCpuLoad{ false };

	bool_t m_bMainThreadLoad = false;
	std::vector<LoadStep> m_LoadSteps{};
	u32_t m_iNextLoadStep = 0;
```

1-6. C:/Users/user/Desktop/Winters/Client/Private/Scene/Loader.cpp

기존 코드:

```cpp
#include "Scene/InGameRosterSpawner.h"
#include "Scene/LobbyRosterHelpers.h"
```

아래에 추가:

```cpp
#include "Scene/Scene_InGame.h"
#include "Manager/Navigation/MapSurfaceSampler.h"
#include "Core/CTransform.h"
#include "WintersPaths.h"
```

삭제할 코드:

```cpp
    constexpr const char* kSummonersRiftMapModelPath =
        "Texture/MAP/output/sr_base_flip.wmesh";
```

`CLoader::Create`의 InGame 분기를:

기존 코드:

```cpp
    if (eNextSceneID == eSceneID::InGame)
    {
        Register_Blueprints_InGame();
        pInstance->PrepareMainThreadInGameLoad();
        return pInstance;
    }
```

아래로 교체:

```cpp
    if (eNextSceneID == eSceneID::InGame)
    {
        Register_Blueprints_InGame();
        pInstance->PrepareMainThreadInGameLoad();
        pInstance->StartInGameCpuLoad();
        return pInstance;
    }
```

`CLoader::~CLoader` 시작 부분에 아래 기존 코드 바로 위에 추가:

기존 코드:

```cpp
    if (m_pCounter && !m_pCounter->IsComplete())
```

아래에 추가:

```cpp
    m_bCancelCpuLoad.store(true, std::memory_order_release);
```

`CLoader::TickMainThreadLoad`의 본문을 아래로 교체:

```cpp
void CLoader::TickMainThreadLoad()
{
    if (!m_bMainThreadLoad || m_bFinished.load())
        return;

    if (m_iNextLoadStep < m_LoadSteps.size())
    {
        const LoadStep& step = m_LoadSteps[m_iNextLoadStep++];
        switch (step.eType)
        {
        case eLoadStepType::FxDirectory:
            CFxCuePlayer::PreloadDirectory(step.strTexturePath.c_str());
            break;
        case eLoadStepType::Model:
            PreloadModel(step.strModelPath.c_str(), 0.f);
            break;
        case eLoadStepType::Texture:
            PreloadTexture(step.strTexturePath.c_str(), 0.f);
            break;
        default:
            break;
        }
        SetProgress(step.fProgressAfter);
    }

    TryFinishInGameLoad();
}
```

`CLoader::Build_NextScene`의 본문을 아래로 교체:

```cpp
unique_ptr<IScene> CLoader::Build_NextScene()
{
    if (!m_pFactory)
        return nullptr;

    unique_ptr<IScene> scene = m_pFactory();
    if (m_eNextSceneID == eSceneID::InGame && m_pPreparedMapSurfaceSampler)
    {
        if (CScene_InGame* pInGame = dynamic_cast<CScene_InGame*>(scene.get()))
            pInGame->AdoptPreparedMapSurfaceSampler(std::move(m_pPreparedMapSurfaceSampler));
    }
    return scene;
}
```

`CLoader::PrepareMainThreadInGameLoad`의 map path 대입 코드를:

기존 코드:

```cpp
    mapStep.strModelPath = kSummonersRiftMapModelPath;
```

아래로 교체:

```cpp
    mapStep.strModelPath = CScene_InGame::GetSelectedMapMeshPath();
```

`CLoader::PrepareMainThreadInGameLoad` 바로 아래에 추가:

```cpp
void CLoader::StartInGameCpuLoad()
{
    m_pCounter = std::make_unique<CJobCounter>();
    CLoader* pRaw = this;
    if (CJobSystem* pJS = CGameInstance::Get()->Get_JobSystem())
        pJS->Submit([pRaw]() { pRaw->RunInGameCpuLoad(); }, m_pCounter.get());
    else
        RunInGameCpuLoad();
}

void CLoader::RunInGameCpuLoad()
{
    wchar_t resolvedPath[MAX_PATH]{};
    const wchar_t* pSurfacePath = CScene_InGame::GetSelectedMapSurfacePath();
    if (!WintersResolveContentPath(pSurfacePath, resolvedPath, MAX_PATH))
        return;

    CTransform mapTransform{};
    CScene_InGame::ConfigureDefaultMapTransform(mapTransform);
    auto sampler = std::make_unique<Engine::CMapSurfaceSampler>();
    if (!sampler->LoadFromWMesh(
            resolvedPath,
            mapTransform.GetWorldMatrix(),
            &m_bCancelCpuLoad))
    {
        return;
    }

    if (!m_bCancelCpuLoad.load(std::memory_order_acquire))
        m_pPreparedMapSurfaceSampler = std::move(sampler);
}

bool_t CLoader::AreInGameCpuLoadsComplete() const
{
    return !m_pCounter || m_pCounter->IsComplete();
}

void CLoader::TryFinishInGameLoad()
{
    if (m_iNextLoadStep < m_LoadSteps.size() || !AreInGameCpuLoadsComplete())
        return;

    SetProgress(1.f);
    m_bFinished.store(true, std::memory_order_release);
}
```

1-7. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

`class CScene_InGame final : public IScene`의 public 영역에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    void RebuildMapWalkableNavGridForDebug();
```

아래에 추가:

```cpp
    static const char* GetSelectedMapMeshPath();
    static const wchar_t* GetSelectedMapSurfacePath();
    static void ConfigureDefaultMapTransform(CTransform& transform);
    void AdoptPreparedMapSurfaceSampler(
        unique_ptr<Engine::CMapSurfaceSampler> sampler);
```

1-8. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLifecycle.cpp

`SendServerInGameReady` 바로 위에 추가:

```cpp
const char* CScene_InGame::GetSelectedMapMeshPath()
{
    return SelectMapMeshPath();
}

const wchar_t* CScene_InGame::GetSelectedMapSurfacePath()
{
    return SelectMapSurfacePath();
}

void CScene_InGame::ConfigureDefaultMapTransform(CTransform& transform)
{
    transform.SetPosition(0.f, 0.f, 0.f);
    transform.SetScale({ -0.01f, 0.01f, 0.01f });
    transform.SetRotation({ 0.f, DirectX::XMConvertToRadians(-135.f), 0.f });
}
```

`CScene_InGame::OnEnter`의 map 초기화 블록에서:

기존 코드:

```cpp
        bMapInit = m_Map.Initialize(SelectMapMeshPath(), L"Shaders/Mesh3D.hlsl");
```

아래로 교체:

```cpp
        bMapInit = m_Map.Initialize(GetSelectedMapMeshPath(), L"Shaders/Mesh3D.hlsl");
```

기존 코드:

```cpp
    m_MapTransform.SetPosition(0.f, 0.f, 0.f);
    m_MapTransform.SetScale({ -0.01f, 0.01f, 0.01f });
    m_MapTransform.SetRotation(m_vMapRotation);
    InitializeMapSurfaceSampler(bMapInit, SelectMapSurfacePath());
```

아래로 교체:

```cpp
    ConfigureDefaultMapTransform(m_MapTransform);
    m_vMapRotation = m_MapTransform.GetRotation();
    InitializeMapSurfaceSampler(bMapInit, GetSelectedMapSurfacePath());
```

1-9. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameMapNav.cpp

`CScene_InGame::InitializeMapSurfaceSampler` 바로 위에 추가:

```cpp
void CScene_InGame::AdoptPreparedMapSurfaceSampler(
    unique_ptr<Engine::CMapSurfaceSampler> sampler)
{
    if (sampler && sampler->IsReady())
        m_pMapSurfaceSampler = std::move(sampler);
}
```

`CScene_InGame::InitializeMapSurfaceSampler` 시작 부분을:

기존 코드:

```cpp
{
    m_pMapSurfaceSampler.reset();
    if (!bMapLoaded)
        return;
```

아래로 교체:

```cpp
{
    if (m_pMapSurfaceSampler && m_pMapSurfaceSampler->IsReady())
        return;

    m_pMapSurfaceSampler.reset();
    if (!bMapLoaded)
        return;
```

1-10. C:/Users/user/Desktop/Winters/Engine/Public/Manager/Navigation/MapSurfaceSampler.h

기존 코드:

```cpp
#include <vector>
```

아래로 교체:

```cpp
#include <atomic>
#include <vector>
```

기존 코드:

```cpp
    bool_t LoadFromWMesh(const wchar_t* pPath, const Mat4& matWorld);
```

아래로 교체:

```cpp
    bool_t LoadFromWMesh(
        const wchar_t* pPath,
        const Mat4& matWorld,
        const std::atomic_bool* pCancel = nullptr);
```

기존 코드:

```cpp
    bool_t BuildWorldVertices(
        const Winters::Asset::WMeshLoaded& mesh,
        const Mat4& matWorld,
        std::vector<Vec3>& outVertices);
```

아래로 교체:

```cpp
    bool_t BuildWorldVertices(
        const Winters::Asset::WMeshLoaded& mesh,
        const Mat4& matWorld,
        std::vector<Vec3>& outVertices,
        const std::atomic_bool* pCancel);
```

기존 코드:

```cpp
    void BuildSurfaceCells(
        const Winters::Asset::WMeshLoaded& mesh,
        const std::vector<Vec3>& vertices);
```

아래로 교체:

```cpp
    bool_t BuildSurfaceCells(
        const Winters::Asset::WMeshLoaded& mesh,
        const std::vector<Vec3>& vertices,
        const std::atomic_bool* pCancel);
```

1-11. C:/Users/user/Desktop/Winters/Engine/Private/Manager/Navigation/MapSurfaceSampler.cpp

`CMapSurfaceSampler::LoadFromWMesh`, `BuildWorldVertices`, `BuildSurfaceCells`에 동일한 `pCancel` 인자를 전달하고, WMesh load 직후·4096 vertex마다·4096 index마다 아래 취소 검사를 추가한다:

기존 코드:

```cpp
    BuildSurfaceCells(mesh, vertices);
    m_bReady = true;
```

아래로 교체:

```cpp
    if ((pCancel && pCancel->load(std::memory_order_acquire)) ||
        !BuildSurfaceCells(mesh, vertices, pCancel))
    {
        Reset();
        return false;
    }
    m_bReady = true;
```

반복문 안에 아래 취소 코드를 추가:

```cpp
        if ((i & 0x0FFFu) == 0u &&
            pCancel && pCancel->load(std::memory_order_acquire))
        {
            return false;
        }
```

1-12. C:/Users/user/Desktop/Winters/Engine/Public/Resource/Model.h

`class CModel`의 public 영역에 아래 기존 코드 바로 위에 추가:

기존 코드:

```cpp
    static unique_ptr<CModel> Create(IRHIDevice* pDevice, const string& strFilePath);
```

아래로 교체:

```cpp
    using LoadYieldCallback = bool_t(*)();

    static unique_ptr<CModel> Create(
        IRHIDevice* pDevice,
        const string& strFilePath,
        LoadYieldCallback pYield = nullptr);
```

기존 코드:

```cpp
	HRESULT LoadModel(IRHIDevice* pDevice, const string& strFilePath);
	void LoadCookedTextures(IRHIDevice* pDevice,
		const string& strMeshPath,
		const Winters::Asset::WMeshLoaded& wm);
```

아래로 교체:

```cpp
	HRESULT LoadModel(
		IRHIDevice* pDevice,
		const string& strFilePath,
		LoadYieldCallback pYield);
	HRESULT LoadCookedTextures(
		IRHIDevice* pDevice,
		const string& strMeshPath,
		const Winters::Asset::WMeshLoaded& wm,
		LoadYieldCallback pYield);
```

기존 코드:

```cpp
	vector<unique_ptr<CTexture>> m_vecTextures;
	vector<RHITextureHandle> m_vecRHITextures;
```

아래로 교체:

```cpp
	vector<unique_ptr<CTexture>> m_vecOwnedTextures;
	vector<CTexture*> m_vecTextures;
	vector<RHITextureHandle> m_vecOwnedRHITextures;
	vector<RHITextureHandle> m_vecRHITextures;
```

1-13. C:/Users/user/Desktop/Winters/Engine/Private/Resource/Model.cpp

`CModel::ResolveMaterialTexture`의 반환 코드를:

기존 코드:

```cpp
    if (matIdx < m_vecTextures.size() && m_vecTextures[matIdx])
        return m_vecTextures[matIdx].get();
```

아래로 교체:

```cpp
    if (matIdx < m_vecTextures.size() && m_vecTextures[matIdx])
        return m_vecTextures[matIdx];
```

`CModel::ReleaseRHIResources`에서 RHI 파괴 대상을:

기존 코드:

```cpp
        for (RHITextureHandle& hTexture : m_vecRHITextures)
```

아래로 교체:

```cpp
        for (RHITextureHandle& hTexture : m_vecOwnedRHITextures)
```

기존 코드:

```cpp
    m_vecRHITextures.clear();
```

아래로 교체:

```cpp
    m_vecRHITextures.clear();
    m_vecOwnedRHITextures.clear();
```

`CModel::Create`, `LoadModel`, `LoadCookedTextures`에 `LoadYieldCallback`을 전달하고, WMesh/WMat 단계 및 각 고유 텍스처의 legacy/RHI 생성 전후에 아래 검사를 추가:

```cpp
    if (pYield && !pYield())
        return E_ABORT;
```

`CModel::LoadCookedTextures`의 material loop를 동일한 정규화 경로·Wrap sampler·ShaderLocalSRGB 키 기준 고유 owner 캐시로 교체:

```cpp
    std::unordered_map<std::wstring, CTexture*> legacyTextures{};
    std::unordered_map<std::wstring, RHITextureHandle> rhiTextures{};

    for (const auto& entry : materials.entries)
    {
        if (entry.material_index >= m_vecTextures.size() || entry.diffuse_path[0] == L'\0')
            continue;

        std::wstring key(entry.diffuse_path);
        std::replace(key.begin(), key.end(), L'\\', L'/');
        std::transform(key.begin(), key.end(), key.begin(), ::towlower);
        key += L"|wrap|shader-local-srgb";

        auto legacyIt = legacyTextures.find(key);
        if (legacyIt == legacyTextures.end())
        {
            auto texture = CTexture::Create(
                pDevice,
                entry.diffuse_path,
                eTexSamplerMode::Wrap,
                eTexColorSpace::ShaderLocalSRGB);
            CTexture* pTexture = texture.get();
            if (texture)
                m_vecOwnedTextures.push_back(std::move(texture));
            legacyIt = legacyTextures.emplace(key, pTexture).first;
        }
        m_vecTextures[entry.material_index] = legacyIt->second;

        auto rhiIt = rhiTextures.find(key);
        if (rhiIt == rhiTextures.end())
        {
            RHITextureHandle handle = RHI_CreateTextureFromFile(
                pDevice,
                entry.diffuse_path,
                "CModel.MaterialRHITexture");
            if (handle.IsValid())
                m_vecOwnedRHITextures.push_back(handle);
            rhiIt = rhiTextures.emplace(key, handle).first;
        }
        m_vecRHITextures[entry.material_index] = rhiIt->second;

        if (pYield && !pYield())
            return E_ABORT;
    }

    WINTERS_PROFILE_COUNT("Model::MaterialBindings", materials.entries.size());
    WINTERS_PROFILE_COUNT("Model::UniqueMaterialTextures", legacyTextures.size());
    return S_OK;
```

1-14. C:/Users/user/Desktop/Winters/Engine/Public/Resource/ResourceCache.h

기존 코드:

```cpp
    shared_ptr<CModel> LoadModel(IRHIDevice* pDevice, const string& strPath);
```

아래로 교체:

```cpp
    shared_ptr<CModel> LoadModel(
        IRHIDevice* pDevice,
        const string& strPath,
        CModel::LoadYieldCallback pYield = nullptr);
```

1-15. C:/Users/user/Desktop/Winters/Engine/Private/Resource/ResourceCache.cpp

`CResourceCache::LoadModel`의 선언과 `CModel::Create` 호출에 `CModel::LoadYieldCallback pYield`을 추가하고 아래 호출로 교체:

```cpp
    auto pModel = CModel::Create(pDevice, strCookedPath, pYield);
```

1-16. C:/Users/user/Desktop/Winters/Engine/Public/Platform/CWin32Window.h

기존 코드:

```cpp
    bool    PumpMessages();
```

아래에 추가:

```cpp
    void    SetSystemCursorVisible(bool bVisible);
    bool    IsQuitRequested() const { return m_bQuitRequested; }
```

기존 코드:

```cpp
    int32   m_Height = 0;
```

아래에 추가:

```cpp
    bool    m_bQuitRequested = false;
    bool    m_bSystemCursorVisible = true;
```

1-17. C:/Users/user/Desktop/Winters/Engine/Private/Platform/CWin32Window.cpp

삭제할 코드:

```cpp
static bool s_bCursorHIDden = false;
```

`CWin32Window::Create`에서 `m_bQuitRequested = false;`를 설정하고 기존 `ShowCursor` 블록을 아래로 교체:

```cpp
    SetSystemCursorVisible(false);
```

`CWin32Window::Destroy`의 기존 `ShowCursor` 블록을 아래로 교체:

```cpp
    SetSystemCursorVisible(true);
```

`CWin32Window::PumpMessages`의 시작과 `WM_QUIT` 처리를 아래로 교체:

```cpp
bool CWin32Window::PumpMessages()
{
    if (m_bQuitRequested)
        return false;

    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            m_bQuitRequested = true;
            return false;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (m_bQuitRequested)
            return false;
    }
    return true;
}
```

`CWin32Window::PumpMessages` 바로 위에 추가:

```cpp
void CWin32Window::SetSystemCursorVisible(bool bVisible)
{
    if (m_bSystemCursorVisible == bVisible)
        return;

    if (bVisible)
    {
        while (::ShowCursor(TRUE) < 0) {}
    }
    else
    {
        while (::ShowCursor(FALSE) >= 0) {}
    }
    m_bSystemCursorVisible = bVisible;
}
```

`CWin32Window::WndProc`의 `WM_DESTROY` 분기를:

기존 코드:

```cpp
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
```

아래로 교체:

```cpp
    case WM_DESTROY:
        if (pThis)
            pThis->m_bQuitRequested = true;
        PostQuitMessage(0);
        return 0;
```

1-18. C:/Users/user/Desktop/Winters/Engine/Include/GameInstance.h

기존 코드:

```cpp
    bool_t Preload_ModelResource(const char* pPath);
    bool_t Preload_TextureResource(const wchar_t* pPath);
```

아래로 교체:

```cpp
    bool_t Preload_ModelResource(const char* pPath);
    bool_t Preload_TextureResource(const wchar_t* pPath);
    void SetLoadingCursorMode(bool_t bEnabled);
```

1-19. C:/Users/user/Desktop/Winters/Engine/Private/GameInstance.cpp

`CGameInstance::Preload_ModelResource`의 cache 호출을:

기존 코드:

```cpp
    return CEngineApp::Get().GetResourceCache().LoadModel(Get_RHIDevice(), pPath) != nullptr;
```

아래로 교체:

```cpp
    const auto PumpLoadingMessages = []() -> bool_t
    {
        return CEngineApp::Get().GetWindow().PumpMessages();
    };
    return CEngineApp::Get().GetResourceCache().LoadModel(
        Get_RHIDevice(),
        pPath,
        PumpLoadingMessages) != nullptr;
```

`CGameInstance::Preload_TextureResource` 바로 아래에 추가:

```cpp
void CGameInstance::SetLoadingCursorMode(bool_t bEnabled)
{
    CEngineApp::Get().GetWindow().SetSystemCursorVisible(bEnabled != 0);
    if (m_pUI_Manager)
        m_pUI_Manager->Set_ShowMouseCursor(!bEnabled);
}
```

1-20. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_MatchLoading.cpp

`CScene_MatchLoading::OnEnter` 시작에 추가:

```cpp
    CGameInstance::Get()->SetLoadingCursorMode(true);
```

`CScene_MatchLoading::OnExit` 시작에 추가:

```cpp
    CGameInstance::Get()->SetLoadingCursorMode(false);
```

1-21. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_Loading.cpp

`CScene_Loading::OnEnter` 시작에 추가:

```cpp
    CGameInstance::Get()->SetLoadingCursorMode(true);
```

`CScene_Loading::OnExit` 시작에 추가:

```cpp
    CGameInstance::Get()->SetLoadingCursorMode(false);
```

1-22. C:/Users/user/Desktop/Winters/Plan/S003_MAP_BUSH_AMBIENT_RESTORE_SESSION_20260711.md

기존 코드:

```text
Session - Map11의 부쉬 64개와 곤충 앰비언트를 canonical Stage 좌표로 복구하고 normal F5 렌더 경로에 연결한다.
```

아래에 추가:

```text
RETIRED - 2026-07-11 S004에서 crossed-card foliage 결과를 시각 실패로 판정했다. 이 문서는 실패한 시도의 기록이며 normal F5 활성 계획이 아니다.
```

1-23. C:/Users/user/Desktop/Winters/Plan/S003_MAP_BUSH_AMBIENT_RESTORE_RESULT_20260711.md

기존 코드:

```text
Session - Map11 부쉬·환경 오브젝트 복구 반영 결과와 비실행/화면 검증 경계를 고정한다.
```

아래에 추가:

```text
RETIRED - crossed-card WMesh는 PNG 카드가 여러 장 겹친 평면성 때문에 부쉬의 체적·실루엣·재질감을 만들지 못했다. ABI/빌드 성공과 시각 성공은 별개이며, S004에서 Stage B0로 normal F5 경로를 폐기한다. 곤충 앰비언트는 폐기 대상이 아니다.
```

1-24. C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/MAP/Map11_Rebuild/source/bush/map11_bush_cluster.obj

삭제할 범위:
파일 전체를 삭제한다.

1-25. C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/MAP/Map11_Rebuild/source/bush/map11_bush_cluster.mtl

삭제할 범위:
파일 전체를 삭제한다.

1-26. C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/map11_bush_cluster.wmesh

삭제할 범위:
파일 전체를 삭제한다.

1-27. C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/map11_bush_cluster.wmat

삭제할 범위:
파일 전체를 삭제한다.

2. 검증

미검증:
- S004 코드 반영 전이다.
- normal F5 인게임에서 HUD 프레임·원형 portrait·passive icon이 실제로 보이는지 미검증이다.
- 로딩 중 Windows 제목 표시줄이 `응답 없음`으로 바뀌지 않고 시스템 커서가 움직이는지 미검증이다.
- crossed-card 부쉬가 normal F5와 Editor Stage load에서 사라지는지 미검증이다.

검증 명령:
- `git diff --check`
- `python Tools/cook_map11_brush_volumes.py`
- Stage1 header/count/hash 검사: `WSTG v4`, `B0`, `5408 bytes`, SHA-256 `6494A5637D2D24BD962C11ACBD1E495E7DDC5319E2216126C0AF86A369731A28`
- 리소스 존재 검사: `HUD_Irelia_2.png`, `hud_irelia_layout.json`, `lol_ingame_hit.png`, `lol_ingame_stun.png`, passive pip texture
- profiler counter 확인: `Model::MaterialBindings`, `Model::UniqueMaterialTextures`
- `UpdateLib.bat`
- `msbuild Winters.sln /t:Client /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:
- 1920x1080 Irelia에서 하단 HUD frame, 원형 portrait/face/frame, passive, QWER, HP/MP, stats panel을 확인한다.
- Viego/Yasuo에서 actor content 교체 후 portrait·passive·skill icon만 교체되고 HUD frame/layout은 유지되는지 확인한다.
- 로딩 카드 화면에서 시스템 커서를 연속 이동하고 창을 드래그하며 제목에 `응답 없음`이 뜨지 않는지 확인한다.
- 로딩 도중 창 닫기와 ESC가 `WM_QUIT` latch를 통해 즉시 중단되고 파괴된 HWND로 다음 프레임을 진행하지 않는지 확인한다.
- 맵·챔피언 모델은 render-owner thread에서 생성되고 `CRHIResourceTable must be accessed from render thread` assert가 재발하지 않는지 확인한다.
- normal F5에서 crossed-card 부쉬가 0개이고 bird/duck/firefly 앰비언트는 유지되는지 확인한다.

성능 기준:
- `Model::UniqueMaterialTextures`가 map material binding 수보다 작아야 하며, 동일 경로 텍스처는 legacy/RHI owner 각각 한 번만 생성·파괴되어야 한다.
- CPU-only MapSurfaceSampler가 JobSystem worker에서 map GPU preload와 겹쳐 실행되어야 한다.
- 이번 단계의 목표는 창/입력 응답 복구다. map GPU finalize는 메인 스레드에 남으므로 로딩 애니메이션의 완전 무정지는 후속 CPU prepare/GPU finalize 분리 대상으로 남긴다.

후속 동기화:
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.
