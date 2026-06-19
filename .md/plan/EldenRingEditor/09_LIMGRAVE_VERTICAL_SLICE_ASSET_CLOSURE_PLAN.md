Session - Limgrave 복원을 source placement가 runtime spawn까지 닫히는 최소 closure 측정으로 고정한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/EldenRingClient/Public/EldenLimgraveShowcaseScene.h

`struct EldenLimgraveTileSummary` 바로 아래에 추가:

기존 코드:

```cpp
struct EldenLimgraveTileSummary
{
    std::string strTile;
    std::string strRole;
    u32_t iPlaced = 0;
    u32_t iUnresolved = 0;
};
```

아래에 추가:

```cpp
enum class EldenMapClosure
{
    Closed,
    MissingAsset,
    SpawnFailed
};
```

`class CEldenLimgraveShowcaseScene final : public IScene`의 private 영역에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    void SpawnErdtree();
    bool LoadVerticalSliceManifest();
    bool IsFocusTile(const std::string& strTile) const;
```

아래에 추가:

```cpp
    EldenMapClosure TrySpawnMapPlacement(const std::vector<std::string>& fields);
    void WriteMapClosureAudit() const;
```

`m_iTilesSkipped` 바로 아래에 추가:

기존 코드:

```cpp
    u32_t m_iTilesScanned = 0;
    u32_t m_iTilesLoaded = 0;
    u32_t m_iTilesSkipped = 0;
```

아래에 추가:

```cpp
    u32_t m_iMapSourceCount = 0;
    u32_t m_iMapClosedCount = 0;
    u32_t m_iMapMissingAssetCount = 0;
    u32_t m_iMapSpawnFailedCount = 0;
```

1-2. C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenLimgraveShowcaseScene.cpp

`CEldenLimgraveShowcaseScene::OnEnter`에서 아래 기존 코드를 교체:

기존 코드:

```cpp
    LoadVerticalSliceManifest();
    SpawnPlacements();
    SpawnErdtree();
    SpawnCharacterPlacements();
    FrameLineup();
```

아래로 교체:

```cpp
    LoadVerticalSliceManifest();
    SpawnPlacements();
    WriteMapClosureAudit();
    SpawnErdtree();
    SpawnCharacterPlacements();
    FrameLineup();
```

`CEldenLimgraveShowcaseScene::IsFocusTile` 바로 아래에 추가:

기존 코드:

```cpp
bool CEldenLimgraveShowcaseScene::IsFocusTile(const std::string& strTile) const
{
    for (const EldenLimgraveTileSummary& tile : m_FocusTiles)
    {
        if (tile.strTile == strTile)
            return true;
    }
    return false;
}
```

아래에 추가:

```cpp
EldenMapClosure CEldenLimgraveShowcaseScene::TrySpawnMapPlacement(const std::vector<std::string>& fields)
{
    const std::string& strWmesh = fields[3];
    if (strWmesh.empty() || ResolveRepoRelativePath(strWmesh).empty())
        return EldenMapClosure::MissingAsset;

    Vec3 vPosition{}, vRotationDeg{}, vScale{ 1.f, 1.f, 1.f };
    if (!ParseVec3(fields[4], vPosition) ||
        !ParseVec3(fields[5], vRotationDeg) ||
        !ParseVec3(fields[6], vScale))
    {
        return EldenMapClosure::SpawnFailed;
    }

    if (!SpawnInstance(strWmesh, BuildWorldMatrix(vPosition, vRotationDeg, vScale), false))
        return EldenMapClosure::SpawnFailed;

    return EldenMapClosure::Closed;
}
```

`CEldenLimgraveShowcaseScene::SpawnPlacements` 전체를 아래로 교체:

기존 코드:

```cpp
void CEldenLimgraveShowcaseScene::SpawnPlacements()
{
    namespace fs = std::filesystem;

    const std::string strMapsRoot = ResolveRepoRelativePath("Client/Bin/Resource/EldenRing/Maps/Limgrave");
    if (strMapsRoot.empty())
    {
        ::OutputDebugStringA("[LimgraveShowcase] Maps/Limgrave root not found\n");
        return;
    }

    for (const fs::directory_entry& tileDir : fs::directory_iterator(strMapsRoot))
    {
        if (!tileDir.is_directory())
            continue;
        ++m_iTilesScanned;

        const std::string strTile = tileDir.path().filename().string();
        if (m_bLoadOnlyFocusTiles && m_bVerticalSliceLoaded && !IsFocusTile(strTile))
        {
            ++m_iTilesSkipped;
            continue;
        }

        const fs::path placement = tileDir.path() / "map_placement.txt";
        if (!fs::exists(placement))
            continue;
        ++m_iTilesLoaded;

        std::ifstream file(placement);
        std::string strLine;
        u32_t tilePlaced = 0;
        while (std::getline(file, strLine))
        {
            if (strLine.empty() || strLine[0] == '#')
                continue;

            std::vector<std::string> fields;
            std::istringstream stream(strLine);
            std::string strField;
            while (std::getline(stream, strField, '|'))
                fields.push_back(strField);
            if (fields.size() < 7)
                continue;

            const std::string& strKind = fields[0];
            const std::string& strWmesh = fields[3];
            // Players are spawn refs. Showcase characters are loaded from a separate
            // character placement JSON so their scale/unit contract is explicit.
            if (strKind == "Player" || strKind == "Enemy" || strWmesh.empty())
                continue;

            Vec3 vPosition{}, vRotationDeg{}, vScale{ 1.f, 1.f, 1.f };
            if (!ParseVec3(fields[4], vPosition) ||
                !ParseVec3(fields[5], vRotationDeg) ||
                !ParseVec3(fields[6], vScale))
                continue;

            // Keep an open stage around the character lineup: skip props
            // (but not terrain MapPieces) near the lineup plaza.
            if (strKind == "Asset")
            {
                const f32_t fDx = vPosition.x - m_vLineupCenter.x;
                const f32_t fDz = vPosition.z - m_vLineupCenter.z;
                if (fDx * fDx + fDz * fDz < 18.f * 18.f)
                    continue;
            }

            if (SpawnInstance(strWmesh, BuildWorldMatrix(vPosition, vRotationDeg, vScale), false))
                ++tilePlaced;
        }

        char msg[256]{};
        sprintf_s(msg, "[LimgraveShowcase] tile %s placed=%u\n",
                  tileDir.path().filename().string().c_str(), tilePlaced);
        ::OutputDebugStringA(msg);
    }
}
```

아래로 교체:

```cpp
void CEldenLimgraveShowcaseScene::SpawnPlacements()
{
    namespace fs = std::filesystem;

    m_iMapSourceCount = 0;
    m_iMapClosedCount = 0;
    m_iMapMissingAssetCount = 0;
    m_iMapSpawnFailedCount = 0;

    const std::string strMapsRoot = ResolveRepoRelativePath("Client/Bin/Resource/EldenRing/Maps/Limgrave");
    if (strMapsRoot.empty())
    {
        ::OutputDebugStringA("[LimgraveShowcase] Maps/Limgrave root not found\n");
        return;
    }

    for (const fs::directory_entry& tileDir : fs::directory_iterator(strMapsRoot))
    {
        if (!tileDir.is_directory())
            continue;
        ++m_iTilesScanned;

        const std::string strTile = tileDir.path().filename().string();
        if (m_bLoadOnlyFocusTiles && m_bVerticalSliceLoaded && !IsFocusTile(strTile))
        {
            ++m_iTilesSkipped;
            continue;
        }

        const fs::path placement = tileDir.path() / "map_placement.txt";
        if (!fs::exists(placement))
            continue;
        ++m_iTilesLoaded;

        std::ifstream file(placement);
        std::string strLine;
        u32_t tilePlaced = 0;
        while (std::getline(file, strLine))
        {
            if (strLine.empty() || strLine[0] == '#')
                continue;

            std::vector<std::string> fields;
            std::istringstream stream(strLine);
            std::string strField;
            while (std::getline(stream, strField, '|'))
                fields.push_back(strField);
            if (fields.size() < 7)
                continue;

            const std::string& strKind = fields[0];
            if (strKind != "MapPiece" && strKind != "Asset")
                continue;

            ++m_iMapSourceCount;
            const EldenMapClosure eClosure = TrySpawnMapPlacement(fields);
            if (eClosure == EldenMapClosure::Closed)
            {
                ++m_iMapClosedCount;
                ++tilePlaced;
            }
            else if (eClosure == EldenMapClosure::MissingAsset)
            {
                ++m_iMapMissingAssetCount;
            }
            else
            {
                ++m_iMapSpawnFailedCount;
            }
        }

        char msg[256]{};
        sprintf_s(msg,
                  "[LimgraveShowcase] tile %s placed=%u\n",
                  strTile.c_str(),
                  tilePlaced);
        ::OutputDebugStringA(msg);
    }
}
```

`CEldenLimgraveShowcaseScene::OnImGui`에서 map load mode 표시 바로 아래에 추가:

기존 코드:

```cpp
    ImGui::Text("Map load mode: %s",
                (m_bLoadOnlyFocusTiles && m_bVerticalSliceLoaded) ? "Focus tiles" : "All tiles");
```

아래에 추가:

```cpp
    ImGui::Text("Map closure source=%u closed=%u open=%u",
                m_iMapSourceCount,
                m_iMapClosedCount,
                m_iMapMissingAssetCount + m_iMapSpawnFailedCount);
    ImGui::Text("Open missingAsset=%u spawnFailed=%u",
                m_iMapMissingAssetCount,
                m_iMapSpawnFailedCount);
```

`CEldenLimgraveShowcaseScene::OnExit` 바로 위에 추가:

기존 코드:

```cpp
ModelRenderer* CEldenLimgraveShowcaseScene::SpawnInstance(const std::string& strWmeshPath,
                                                          const Mat4& matWorld,
                                                          bool bCycleAnims)
{
    auto pRenderer = std::make_unique<ModelRenderer>();
    if (!pRenderer->Initialize(strWmeshPath))
    {
        ++m_iFailedCount;
        return nullptr;
    }

    pRenderer->UpdateTransform(matWorld);

    ShowcaseInstance instance;
    instance.iAnimCount = pRenderer->GetAnimationCount();
    instance.bCycleAnims = bCycleAnims && instance.iAnimCount > 0;
    if (instance.bCycleAnims)
    {
        pRenderer->PlayAnimation(0);
        ++m_iAnimatedCount;
    }

    instance.pRenderer = std::move(pRenderer);
    m_Instances.push_back(std::move(instance));
    ++m_iPlacedCount;
    return m_Instances.back().pRenderer.get();
}
```

아래에 추가:

```cpp
void CEldenLimgraveShowcaseScene::WriteMapClosureAudit() const
{
    const std::string strPath = ResolveRepoRelativePath(
        "Client/Bin/Resource/EldenRing/Manifests/limgrave_map_closure_audit.json");
    if (strPath.empty())
        return;

    FILE* pFile = nullptr;
    if (fopen_s(&pFile, strPath.c_str(), "w") != 0 || !pFile)
        return;

    fprintf(pFile,
            "{\n"
            "  \"schema\": \"winters.elden.limgrave_map_closure.v1\",\n"
            "  \"essence\": \"source placement -> runtime asset -> spawn\",\n"
            "  \"source\": %u,\n"
            "  \"closed\": %u,\n"
            "  \"open\": %u,\n"
            "  \"missingAsset\": %u,\n"
            "  \"spawnFailed\": %u\n"
            "}\n",
            m_iMapSourceCount,
            m_iMapClosedCount,
            m_iMapMissingAssetCount + m_iMapSpawnFailedCount,
            m_iMapMissingAssetCount,
            m_iMapSpawnFailedCount);

    fclose(pFile);
}
```

1-3. C:/Users/tnest/Desktop/Winters/Client/Bin/Resource/EldenRing/Maps/Limgrave/limgrave_vertical_slice_manifest.json

`goal` 바로 아래에 추가:

기존 코드:

```json
  "goal": "Reconstruct a Limgrave vertical slice from original map placement data, then validate terrain, static assets, Erdtree stand-in, and animated actors in Winters.",
```

아래에 추가:

```json
  "essence": "source placement -> runtime asset -> spawn",
```

2. 검증

검증 명령:
- `git diff --check`
- `cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build out\build\msvc-ninja --config Debug --target WintersElden --parallel'`

수동 확인:
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Bin/Debug/WintersElden.exe --rhi=dx11 --scene=limgrave`
- ImGui에 `Map closure source/closed/open`이 표시되는지 확인.
- `Client/Bin/Resource/EldenRing/Manifests/limgrave_map_closure_audit.json`이 생성되는지 확인.

확인 필요:
- `Player`/`Enemy`는 P0 map closure에서 제외한다. actor closure는 별도 파이프라인에서 다룬다.
- `Erdtree Missing`은 runtime 탐색으로 풀지 않는다. 황금나무도 source placement 또는 cook contract가 닫혀야 해결된 것으로 본다.
