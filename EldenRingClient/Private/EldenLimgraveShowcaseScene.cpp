#include "EldenLimgraveShowcaseScene.h"

#include "GameInstance.h"
#include "Renderer/ModelRenderer.h"

#ifdef new
#pragma push_macro("new")
#undef new
#define WINTERS_RESTORE_DEBUG_NEW
#endif
#include "imgui.h"
#ifdef WINTERS_RESTORE_DEBUG_NEW
#pragma pop_macro("new")
#undef WINTERS_RESTORE_DEBUG_NEW
#endif

#include <Windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
    constexpr f32_t kDegToRad = 3.14159265358979323846f / 180.f;

    Mat4 BuildRotationDeg(const Vec3& vRotationDeg);

    void EnsureCursorReleasedAndVisible()
    {
        ::ClipCursor(nullptr);
        for (int i = 0; i < 8; ++i)
        {
            if (::ShowCursor(TRUE) >= 0)
                break;
        }
    }

    void ResetSpawnFailureLog()
    {
        std::remove("limgrave_spawn_failed_assets.txt");
        std::remove("limgrave_showcase_failures.txt");
    }

    void AppendMapSpawnFailure(const char* pReason,
                               const std::string& strTile,
                               const std::vector<std::string>& fields)
    {
        FILE* pLog = nullptr;
        if (fopen_s(&pLog, "limgrave_spawn_failed_assets.txt", "a") != 0 || !pLog)
            return;

        const char* pKind = fields.size() > 0 ? fields[0].c_str() : "";
        const char* pInstance = fields.size() > 1 ? fields[1].c_str() : "";
        const char* pAsset = fields.size() > 2 ? fields[2].c_str() : "";
        const char* pWmesh = fields.size() > 3 ? fields[3].c_str() : "";
        const char* pPosition = fields.size() > 4 ? fields[4].c_str() : "";
        const char* pRotation = fields.size() > 5 ? fields[5].c_str() : "";
        const char* pScale = fields.size() > 6 ? fields[6].c_str() : "";

        fprintf(pLog,
                "%s|%s|%s|%s|%s|%s|%s|%s|%s\n",
                pReason,
                strTile.c_str(),
                pKind,
                pInstance,
                pAsset,
                pWmesh,
                pPosition,
                pRotation,
                pScale);
        fclose(pLog);
    }

    EldenShowcaseStage GetRequestedInitialStage()
    {
        const wchar_t* const pCommandLine = ::GetCommandLineW();
        if (pCommandLine &&
            (wcsstr(pCommandLine, L"--stage=cave") ||
             wcsstr(pCommandLine, L"/stage:cave") ||
             wcsstr(pCommandLine, L"--stage=startingcave") ||
             wcsstr(pCommandLine, L"/stage:startingcave")))
        {
            return EldenShowcaseStage::StartingCave;
        }
        return EldenShowcaseStage::LimgraveVista;
    }

    // Walk up from the current working directory until a repo-relative
    // resource path exists (mirrors WintersResolveContentPath behavior).
    std::string ResolveRepoRelativePath(const std::string& strRelative)
    {
        namespace fs = std::filesystem;
        fs::path base = fs::current_path();
        for (int depth = 0; depth < 8; ++depth)
        {
            const fs::path candidate = base / strRelative;
            if (fs::exists(candidate))
                return candidate.string();
            if (!base.has_parent_path() || base.parent_path() == base)
                break;
            base = base.parent_path();
        }
        return {};
    }

    Mat4 BuildWorldMatrix(const Vec3& vPosition, const Vec3& vRotationDeg, const Vec3& vScale)
    {
        const Mat4 matScale = Mat4::Scale(vScale);
        const Mat4 matRot = BuildRotationDeg(vRotationDeg);
        const Mat4 matTrans = Mat4::Translation(vPosition);
        return matScale * matRot * matTrans;
    }

    bool ParseVec3(const std::string& strField, Vec3& vOut)
    {
        std::istringstream stream(strField);
        return static_cast<bool>(stream >> vOut.x >> vOut.y >> vOut.z);
    }

    std::vector<std::string> SplitPipeLine(const std::string& strLine)
    {
        std::vector<std::string> fields;
        std::istringstream stream(strLine);
        std::string strField;
        while (std::getline(stream, strField, '|'))
            fields.push_back(strField);
        return fields;
    }

    bool ExtractStringField(const std::string& strBlock, const char* pKey, std::string& strOut)
    {
        const std::string strNeedle = std::string("\"") + pKey + "\"";
        const size_t keyPos = strBlock.find(strNeedle);
        if (keyPos == std::string::npos)
            return false;
        const size_t colonPos = strBlock.find(':', keyPos + strNeedle.size());
        if (colonPos == std::string::npos)
            return false;
        const size_t begin = strBlock.find('"', colonPos + 1);
        if (begin == std::string::npos)
            return false;
        const size_t end = strBlock.find('"', begin + 1);
        if (end == std::string::npos)
            return false;
        strOut = strBlock.substr(begin + 1, end - begin - 1);
        return true;
    }

    bool ExtractBoolField(const std::string& strBlock, const char* pKey, bool& bOut)
    {
        const std::string strNeedle = std::string("\"") + pKey + "\"";
        const size_t keyPos = strBlock.find(strNeedle);
        if (keyPos == std::string::npos)
            return false;
        const size_t colonPos = strBlock.find(':', keyPos + strNeedle.size());
        if (colonPos == std::string::npos)
            return false;
        const size_t tokenPos = strBlock.find_first_not_of(" \t\r\n", colonPos + 1);
        if (tokenPos == std::string::npos)
            return false;
        if (strBlock.compare(tokenPos, 4, "true") == 0)
        {
            bOut = true;
            return true;
        }
        if (strBlock.compare(tokenPos, 5, "false") == 0)
        {
            bOut = false;
            return true;
        }
        return false;
    }

    bool ExtractUIntField(const std::string& strBlock, const char* pKey, u32_t& iOut)
    {
        const std::string strNeedle = std::string("\"") + pKey + "\"";
        const size_t keyPos = strBlock.find(strNeedle);
        if (keyPos == std::string::npos)
            return false;
        const size_t colonPos = strBlock.find(':', keyPos + strNeedle.size());
        if (colonPos == std::string::npos)
            return false;
        const size_t tokenPos = strBlock.find_first_not_of(" \t\r\n", colonPos + 1);
        if (tokenPos == std::string::npos)
            return false;

        char* pEnd = nullptr;
        const unsigned long value = std::strtoul(strBlock.c_str() + tokenPos, &pEnd, 10);
        if (pEnd == strBlock.c_str() + tokenPos)
            return false;

        iOut = static_cast<u32_t>(value);
        return true;
    }

    bool ExtractObjectField(const std::string& strJson, const char* pKey, std::string& strOut)
    {
        const std::string strNeedle = std::string("\"") + pKey + "\"";
        const size_t keyPos = strJson.find(strNeedle);
        if (keyPos == std::string::npos)
            return false;
        const size_t objectOpen = strJson.find('{', keyPos + strNeedle.size());
        if (objectOpen == std::string::npos)
            return false;

        int depth = 0;
        for (size_t i = objectOpen; i < strJson.size(); ++i)
        {
            if (strJson[i] == '{')
                ++depth;
            else if (strJson[i] == '}')
            {
                --depth;
                if (depth == 0)
                {
                    strOut = strJson.substr(objectOpen, i - objectOpen + 1);
                    return true;
                }
            }
        }
        return false;
    }

    bool ExtractVec3Field(const std::string& strBlock, const char* pKey, Vec3& vOut)
    {
        const std::string strNeedle = std::string("\"") + pKey + "\"";
        const size_t keyPos = strBlock.find(strNeedle);
        if (keyPos == std::string::npos)
            return false;
        const size_t open = strBlock.find('[', keyPos + strNeedle.size());
        const size_t close = strBlock.find(']', open + 1);
        if (open == std::string::npos || close == std::string::npos)
            return false;

        std::string strValues = strBlock.substr(open + 1, close - open - 1);
        for (char& ch : strValues)
        {
            if (ch == ',')
                ch = ' ';
        }
        std::istringstream stream(strValues);
        return static_cast<bool>(stream >> vOut.x >> vOut.y >> vOut.z);
    }

    std::vector<std::string> ExtractArrayObjectBlocks(const std::string& strJson, const char* pArrayKey)
    {
        std::vector<std::string> blocks;
        const std::string strNeedle = std::string("\"") + pArrayKey + "\"";
        const size_t keyPos = strJson.find(strNeedle);
        if (keyPos == std::string::npos)
            return blocks;
        const size_t arrayOpen = strJson.find('[', keyPos + strNeedle.size());
        if (arrayOpen == std::string::npos)
            return blocks;

        int depth = 0;
        size_t objectBegin = std::string::npos;
        for (size_t i = arrayOpen + 1; i < strJson.size(); ++i)
        {
            const char ch = strJson[i];
            if (ch == '{')
            {
                if (depth == 0)
                    objectBegin = i;
                ++depth;
            }
            else if (ch == '}')
            {
                --depth;
                if (depth == 0 && objectBegin != std::string::npos)
                {
                    blocks.push_back(strJson.substr(objectBegin, i - objectBegin + 1));
                    objectBegin = std::string::npos;
                }
            }
            else if (ch == ']' && depth == 0)
            {
                break;
            }
        }
        return blocks;
    }

    Mat4 BuildRotationDeg(const Vec3& vRotationDeg)
    {
        return
            Mat4::RotationZ(vRotationDeg.z * kDegToRad) *
            Mat4::RotationX(vRotationDeg.x * kDegToRad) *
            Mat4::RotationY(vRotationDeg.y * kDegToRad);
    }

    std::string EscapeJsonString(const std::string& strValue)
    {
        std::string strEscaped;
        strEscaped.reserve(strValue.size());
        for (char ch : strValue)
        {
            if (ch == '\\' || ch == '"')
            {
                strEscaped.push_back('\\');
                strEscaped.push_back(ch);
            }
            else if (ch == '\n')
            {
                strEscaped += "\\n";
            }
            else if (ch == '\r')
            {
                strEscaped += "\\r";
            }
            else if (ch == '\t')
            {
                strEscaped += "\\t";
            }
            else
            {
                strEscaped.push_back(ch);
            }
        }
        return strEscaped;
    }

    std::vector<EldenCharacterPlacement> LoadCharacterPlacements(const std::string& strRelative)
    {
        std::vector<EldenCharacterPlacement> placements;
        const std::string strPath = ResolveRepoRelativePath(strRelative);
        if (strPath.empty())
            return placements;

        std::ifstream file(strPath);
        if (!file)
            return placements;
        std::stringstream buffer;
        buffer << file.rdbuf();

        for (const std::string& strBlock : ExtractArrayObjectBlocks(buffer.str(), "characters"))
        {
            EldenCharacterPlacement def{};
            if (!ExtractStringField(strBlock, "label", def.strLabel) ||
                !ExtractStringField(strBlock, "wmesh", def.strWmesh) ||
                !ExtractVec3Field(strBlock, "position", def.vPosition) ||
                !ExtractVec3Field(strBlock, "rotationDeg", def.vRotationDeg) ||
                !ExtractVec3Field(strBlock, "scale", def.vScale))
            {
                continue;
            }
            ExtractStringField(strBlock, "model", def.strModel);
            ExtractVec3Field(strBlock, "axisFixDeg", def.vAxisFixDeg);
            ExtractBoolField(strBlock, "animated", def.bAnimated);
            placements.push_back(def);
        }
        return placements;
    }
}

std::unique_ptr<CEldenLimgraveShowcaseScene> CEldenLimgraveShowcaseScene::Create()
{
    return std::unique_ptr<CEldenLimgraveShowcaseScene>(new CEldenLimgraveShowcaseScene());
}

CEldenLimgraveShowcaseScene::~CEldenLimgraveShowcaseScene() = default;

bool CEldenLimgraveShowcaseScene::OnEnter()
{
    ::OutputDebugStringA("[LimgraveShowcase] OnEnter\n");
    EnsureCursorReleasedAndVisible();

    m_eStage = GetRequestedInitialStage();
    m_bStageLoaded = false;
    m_bStageLoadAttempted = false;

    char msg[160]{};
    sprintf_s(msg, "[LimgraveShowcase] pending stage=%s\n", GetStageName());
    ::OutputDebugStringA(msg);

    if (FILE* pLog = nullptr; fopen_s(&pLog, "limgrave_showcase_log.txt", "w") == 0 && pLog)
    {
        fprintf(pLog, "OnEnter pending: stage=%s\n", GetStageName());
        fclose(pLog);
    }

    return true;
}

const char* CEldenLimgraveShowcaseScene::GetStageName() const
{
    switch (m_eStage)
    {
    case EldenShowcaseStage::StartingCave:
        return "Starting Cave";
    case EldenShowcaseStage::LimgraveVista:
        return "Limgrave Vista";
    default:
        return "Unknown";
    }
}

void CEldenLimgraveShowcaseScene::ClearShowcaseWorld()
{
    m_CharacterPlacements.clear();
    m_MapPlacements.clear();
    m_Instances.clear();
    m_iPlacedCount = 0;
    m_iFailedCount = 0;
    m_iAnimatedCount = 0;
    m_iSelectedCharacter = 0;
    m_iSelectedMapPlacement = 0;
    m_bErdtreeLoaded = false;
}

void CEldenLimgraveShowcaseScene::ResetMapCounters()
{
    m_iSlicePlacedTotal = 0;
    m_iSliceUnresolvedTotal = 0;
    m_iTilesScanned = 0;
    m_iTilesLoaded = 0;
    m_iTilesSkipped = 0;
    m_iMapSourceCount = 0;
    m_iMapClosedCount = 0;
    m_iMapMissingAssetCount = 0;
    m_iMapSpawnFailedCount = 0;
    m_iMapDraftLoadedCount = 0;
    m_iMapDraftAppliedCount = 0;
    m_iMapDraftSkippedCount = 0;
}

bool CEldenLimgraveShowcaseScene::LoadStage(EldenShowcaseStage eStage)
{
    m_bStageLoadAttempted = true;
    ResetSpawnFailureLog();
    ClearShowcaseWorld();
    ResetMapCounters();
    m_FocusTiles.clear();
    m_bVerticalSliceLoaded = false;
    m_eStage = eStage;
    m_bLoadOnlyFocusTiles = true;

    if (m_eStage == EldenShowcaseStage::StartingCave)
    {
        LoadVerticalSliceManifest(
            "Client/Bin/Resource/EldenRing/Maps/StartingCave/starting_cave_vertical_slice_manifest.json",
            "tiles",
            "coverage");
        SpawnPlacements("Client/Bin/Resource/EldenRing/Maps/StartingCave");
        LoadMapPlacementDraft();
        WriteMapClosureAudit("Client/Bin/Resource/EldenRing/Manifests/starting_cave_map_closure_audit.json");
        FrameStartingCave();
    }
    else
    {
        LoadVerticalSliceManifest(
            "Client/Bin/Resource/EldenRing/Maps/Limgrave/limgrave_vertical_slice_manifest.json",
            "focusTiles",
            "currentLimgraveCoverage");
        SpawnPlacements("Client/Bin/Resource/EldenRing/Maps/Limgrave");
        LoadMapPlacementDraft();
        WriteMapClosureAudit("Client/Bin/Resource/EldenRing/Manifests/limgrave_map_closure_audit.json");
        SpawnErdtree();
        SpawnCharacterPlacements();
        FrameLimgraveVista();
    }

    char msg[256]{};
    sprintf_s(msg,
              "[LimgraveShowcase] LoadStage %s placed=%u failed=%u animated=%u source=%u closed=%u open=%u draftApplied=%u\n",
              GetStageName(),
              m_iPlacedCount,
              m_iFailedCount,
              m_iAnimatedCount,
              m_iMapSourceCount,
              m_iMapClosedCount,
              m_iMapMissingAssetCount + m_iMapSpawnFailedCount,
              m_iMapDraftAppliedCount);
    ::OutputDebugStringA(msg);
    if (FILE* pLog = nullptr; fopen_s(&pLog, "limgrave_showcase_log.txt", "a") == 0 && pLog)
    {
        fprintf(pLog,
                "LoadStage stage=%s placed=%u failed=%u animated=%u source=%u closed=%u open=%u missingAsset=%u spawnFailed=%u draftLoaded=%u draftApplied=%u draftSkipped=%u\n",
                GetStageName(),
                m_iPlacedCount,
                m_iFailedCount,
                m_iAnimatedCount,
                m_iMapSourceCount,
                m_iMapClosedCount,
                m_iMapMissingAssetCount + m_iMapSpawnFailedCount,
                m_iMapMissingAssetCount,
                m_iMapSpawnFailedCount,
                m_iMapDraftLoadedCount,
                m_iMapDraftAppliedCount,
                m_iMapDraftSkippedCount);
        fclose(pLog);
    }
    m_bStageLoaded = m_iPlacedCount > 0;
    return m_bStageLoaded;
}

bool CEldenLimgraveShowcaseScene::LoadVerticalSliceManifest(const std::string& strManifestRelative,
                                                            const char* pTilesArrayKey,
                                                            const char* pCoverageKey)
{
    const std::string strPath = ResolveRepoRelativePath(strManifestRelative);
    if (strPath.empty())
    {
        ::OutputDebugStringA("[LimgraveShowcase] vertical slice manifest not found\n");
        return false;
    }

    std::ifstream file(strPath);
    if (!file)
        return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string strJson = buffer.str();

    m_FocusTiles.clear();
    for (const std::string& strBlock : ExtractArrayObjectBlocks(strJson, pTilesArrayKey))
    {
        EldenLimgraveTileSummary tile{};
        if (!ExtractStringField(strBlock, "tile", tile.strTile))
            continue;
        ExtractStringField(strBlock, "role", tile.strRole);
        ExtractUIntField(strBlock, "placed", tile.iPlaced);
        ExtractUIntField(strBlock, "unresolved", tile.iUnresolved);
        m_FocusTiles.push_back(tile);
    }

    std::string strCoverage;
    if (ExtractObjectField(strJson, pCoverageKey, strCoverage))
    {
        ExtractUIntField(strCoverage, "placed", m_iSlicePlacedTotal);
        ExtractUIntField(strCoverage, "unresolved", m_iSliceUnresolvedTotal);
    }

    m_bVerticalSliceLoaded = !m_FocusTiles.empty();
    char msg[192]{};
    sprintf_s(msg,
              "[LimgraveShowcase] vertical slice loaded=%u focusTiles=%zu placed=%u unresolved=%u\n",
              m_bVerticalSliceLoaded ? 1u : 0u,
              m_FocusTiles.size(),
              m_iSlicePlacedTotal,
              m_iSliceUnresolvedTotal);
    ::OutputDebugStringA(msg);
    return m_bVerticalSliceLoaded;
}

bool CEldenLimgraveShowcaseScene::IsFocusTile(const std::string& strTile) const
{
    for (const EldenLimgraveTileSummary& tile : m_FocusTiles)
    {
        if (tile.strTile == strTile)
            return true;
    }
    return false;
}

EldenMapClosure CEldenLimgraveShowcaseScene::TrySpawnMapPlacement(const std::string& strTile,
                                                                  const std::vector<std::string>& fields,
                                                                  bool bCycleAnims)
{
    const std::string& strWmesh = fields[3];
    if (strWmesh.empty() || ResolveRepoRelativePath(strWmesh).empty())
    {
        AppendMapSpawnFailure("MissingAsset", strTile, fields);
        return EldenMapClosure::MissingAsset;
    }

    Vec3 vPosition{}, vRotationDeg{}, vScale{ 1.f, 1.f, 1.f };
    if (!ParseVec3(fields[4], vPosition) ||
        !ParseVec3(fields[5], vRotationDeg) ||
        !ParseVec3(fields[6], vScale))
    {
        AppendMapSpawnFailure("BadTransform", strTile, fields);
        return EldenMapClosure::SpawnFailed;
    }

    ModelRenderer* const pRenderer = SpawnInstance(strWmesh, BuildWorldMatrix(vPosition, vRotationDeg, vScale), bCycleAnims);
    if (!pRenderer)
    {
        AppendMapSpawnFailure(ModelRenderer::PrewarmModel(strWmesh)
            ? "RendererInitFailed"
            : "ModelLoadFailed", strTile, fields);
        return EldenMapClosure::SpawnFailed;
    }

    EldenRuntimeMapPlacement placement{};
    placement.strTile = strTile;
    placement.strKind = fields[0];
    placement.strName = fields[1];
    placement.strModel = fields[2];
    placement.strWmesh = strWmesh;
    placement.vPosition = vPosition;
    placement.vRotationDeg = vRotationDeg;
    placement.vScale = vScale;
    placement.pRenderer = pRenderer;
    placement.bAnimated = bCycleAnims;
    m_MapPlacements.push_back(placement);

    return EldenMapClosure::Closed;
}

void CEldenLimgraveShowcaseScene::SpawnPlacements(const std::string& strMapsRootRelative)
{
    namespace fs = std::filesystem;

    m_iMapSourceCount = 0;
    m_iMapClosedCount = 0;
    m_iMapMissingAssetCount = 0;
    m_iMapSpawnFailedCount = 0;

    const std::string strMapsRoot = ResolveRepoRelativePath(strMapsRootRelative);
    if (strMapsRoot.empty())
    {
        ::OutputDebugStringA("[LimgraveShowcase] map root not found\n");
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

            std::vector<std::string> fields = SplitPipeLine(strLine);
            if (fields.size() < 7)
                continue;

            const std::string& strKind = fields[0];
            const bool bStaticPlacement = strKind == "MapPiece" || strKind == "Asset";
            const bool bCharacterPlacement = strKind == "Enemy" || strKind == "Npc";
            if (!bStaticPlacement && !bCharacterPlacement)
                continue;

            ++m_iMapSourceCount;
            const EldenMapClosure eClosure = TrySpawnMapPlacement(strTile, fields, bCharacterPlacement);
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
        sprintf_s(msg, "[LimgraveShowcase] tile %s placed=%u\n",
                  strTile.c_str(), tilePlaced);
        ::OutputDebugStringA(msg);
    }
}

void CEldenLimgraveShowcaseScene::SpawnErdtree()
{
    // Giant tree AEG099_720 (132m) as the golden-tree showcase piece,
    // planted on the ridge north-east of the lineup plaza.
    m_vErdtreePos = Vec3{ m_vLineupCenter.x + 95.f, m_vLineupCenter.y - 4.f, m_vLineupCenter.z + 130.f };
    const Mat4 matWorld =
        Mat4::RotationY(35.f * kDegToRad) *
        Mat4::Translation(m_vErdtreePos);
    m_bErdtreeLoaded = SpawnInstance(
        "Client/Bin/Resource/EldenRing/Assets/LimgraveStatic/"
        "AEG099_720_aeg099_720-geombnd-dcx_sib_AEG099_720/Model/"
        "AEG099_720_aeg099_720-geombnd-dcx_sib_AEG099_720.wmesh",
        matWorld,
        false) != nullptr;
}

Mat4 CEldenLimgraveShowcaseScene::BuildCharacterWorldMatrix(const EldenCharacterPlacement& placement) const
{
    return
        Mat4::Scale(placement.vScale) *
        BuildRotationDeg(placement.vAxisFixDeg) *
        BuildRotationDeg(placement.vRotationDeg) *
        Mat4::Translation(placement.vPosition);
}

void CEldenLimgraveShowcaseScene::ApplyCharacterTransform(EldenCharacterPlacement& placement)
{
    if (!placement.pRenderer)
        return;
    placement.pRenderer->UpdateTransform(BuildCharacterWorldMatrix(placement));
}

void CEldenLimgraveShowcaseScene::ApplyMapPlacementTransform(EldenRuntimeMapPlacement& placement)
{
    if (!placement.pRenderer)
        return;
    placement.pRenderer->UpdateTransform(
        BuildWorldMatrix(placement.vPosition, placement.vRotationDeg, placement.vScale));
}

void CEldenLimgraveShowcaseScene::SpawnCharacterPlacements()
{
    m_CharacterPlacements = LoadCharacterPlacements(
        "Client/Bin/Resource/EldenRing/Maps/Limgrave/showcase_character_placement.json");
    for (EldenCharacterPlacement& placement : m_CharacterPlacements)
    {
        ModelRenderer* pRenderer = SpawnInstance(
            placement.strWmesh,
            BuildCharacterWorldMatrix(placement),
            placement.bAnimated);
        placement.pRenderer = pRenderer;
        if (FILE* pLog = nullptr; pRenderer && fopen_s(&pLog, "limgrave_lineup_log.txt", "a") == 0 && pLog)
        {
            fprintf(pLog,
                    "%-16s pos=(%.3f,%.3f,%.3f) scale=(%.4f,%.4f,%.4f) axisFix=(%.1f,%.1f,%.1f) anims=%u\n",
                    placement.strLabel.c_str(),
                    placement.vPosition.x,
                    placement.vPosition.y,
                    placement.vPosition.z,
                    placement.vScale.x,
                    placement.vScale.y,
                    placement.vScale.z,
                    placement.vAxisFixDeg.x,
                    placement.vAxisFixDeg.y,
                    placement.vAxisFixDeg.z,
                    pRenderer->GetAnimationCount());
            fclose(pLog);
        }
    }
}

bool CEldenLimgraveShowcaseScene::SaveCharacterPlacements() const
{
    const std::string strPath = ResolveRepoRelativePath(
        "Client/Bin/Resource/EldenRing/Maps/Limgrave/showcase_character_placement.json");
    if (strPath.empty())
        return false;

    FILE* pFile = nullptr;
    if (fopen_s(&pFile, strPath.c_str(), "w") != 0 || !pFile)
        return false;

    fprintf(pFile,
            "{\n"
            "  \"schema\": \"winters.elden.character_placement.v1\",\n"
            "  \"generatedBy\": \"limgrave-showcase-editor\",\n"
            "  \"source\": {\n"
            "    \"mapTile\": \"m60_44_35_00\",\n"
            "    \"anchorPart\": \"Enemy|c2010_9000\",\n"
            "    \"anchorReason\": \"extracted MSB enemy placement used as a visible Limgrave lineup stage\",\n"
            "    \"unitScale\": 0.01\n"
            "  },\n"
            "  \"characters\": [\n");

    for (size_t i = 0; i < m_CharacterPlacements.size(); ++i)
    {
        const EldenCharacterPlacement& placement = m_CharacterPlacements[i];
        const std::string strLabel = EscapeJsonString(placement.strLabel);
        const std::string strModel = EscapeJsonString(placement.strModel);
        const std::string strWmesh = EscapeJsonString(placement.strWmesh);

        fprintf(pFile,
                "    {\n"
                "      \"label\": \"%s\",\n"
                "      \"model\": \"%s\",\n"
                "      \"wmesh\": \"%s\",\n"
                "      \"position\": [%.3f, %.3f, %.3f],\n"
                "      \"rotationDeg\": [%.3f, %.3f, %.3f],\n"
                "      \"axisFixDeg\": [%.3f, %.3f, %.3f],\n"
                "      \"scale\": [%.4f, %.4f, %.4f],\n"
                "      \"animated\": %s\n"
                "    }%s\n",
                strLabel.c_str(),
                strModel.c_str(),
                strWmesh.c_str(),
                placement.vPosition.x,
                placement.vPosition.y,
                placement.vPosition.z,
                placement.vRotationDeg.x,
                placement.vRotationDeg.y,
                placement.vRotationDeg.z,
                placement.vAxisFixDeg.x,
                placement.vAxisFixDeg.y,
                placement.vAxisFixDeg.z,
                placement.vScale.x,
                placement.vScale.y,
                placement.vScale.z,
                placement.bAnimated ? "true" : "false",
                (i + 1 == m_CharacterPlacements.size()) ? "" : ",");
    }

    fprintf(pFile, "  ]\n}\n");
    fclose(pFile);
    return true;
}

bool CEldenLimgraveShowcaseScene::SaveMapPlacementDraft() const
{
    namespace fs = std::filesystem;

    const std::string strRelative = GetMapPlacementDraftRelative();

    std::string strPath = ResolveRepoRelativePath(strRelative);
    if (strPath.empty())
    {
        const fs::path relativePath(strRelative);
        const std::string strParent = ResolveRepoRelativePath(relativePath.parent_path().string());
        if (!strParent.empty())
            strPath = (fs::path(strParent) / relativePath.filename()).string();
    }
    if (strPath.empty())
        return false;

    FILE* pFile = nullptr;
    if (fopen_s(&pFile, strPath.c_str(), "w") != 0 || !pFile)
        return false;

    fprintf(pFile, "# winters.elden.runtime_placement_draft.v1 %s\n", GetStageName());
    fprintf(pFile, "# tile|kind|name|model|wmesh|position|rotationDeg|scale|animated\n");
    for (const EldenRuntimeMapPlacement& placement : m_MapPlacements)
    {
        fprintf(pFile,
                "%s|%s|%s|%s|%s|%.6f %.6f %.6f|%.6f %.6f %.6f|%.6f %.6f %.6f|%s\n",
                placement.strTile.c_str(),
                placement.strKind.c_str(),
                placement.strName.c_str(),
                placement.strModel.c_str(),
                placement.strWmesh.c_str(),
                placement.vPosition.x,
                placement.vPosition.y,
                placement.vPosition.z,
                placement.vRotationDeg.x,
                placement.vRotationDeg.y,
                placement.vRotationDeg.z,
                placement.vScale.x,
                placement.vScale.y,
                placement.vScale.z,
                placement.bAnimated ? "true" : "false");
    }

    fclose(pFile);
    return true;
}

std::string CEldenLimgraveShowcaseScene::GetMapPlacementDraftRelative() const
{
    return (m_eStage == EldenShowcaseStage::StartingCave)
        ? "Client/Bin/Resource/EldenRing/Manifests/starting_cave_runtime_placement_draft.txt"
        : "Client/Bin/Resource/EldenRing/Manifests/limgrave_runtime_placement_draft.txt";
}

void CEldenLimgraveShowcaseScene::LoadMapPlacementDraft()
{
    m_iMapDraftLoadedCount = 0;
    m_iMapDraftAppliedCount = 0;
    m_iMapDraftSkippedCount = 0;

    const std::string strPath = ResolveRepoRelativePath(GetMapPlacementDraftRelative());
    if (strPath.empty())
        return;

    std::ifstream file(strPath);
    if (!file)
        return;

    std::string strLine;
    while (std::getline(file, strLine))
    {
        if (strLine.empty() || strLine[0] == '#')
            continue;

        const std::vector<std::string> fields = SplitPipeLine(strLine);
        ++m_iMapDraftLoadedCount;
        if (fields.size() < 8)
        {
            ++m_iMapDraftSkippedCount;
            continue;
        }

        Vec3 vPosition{}, vRotationDeg{}, vScale{ 1.f, 1.f, 1.f };
        if (!ParseVec3(fields[5], vPosition) ||
            !ParseVec3(fields[6], vRotationDeg) ||
            !ParseVec3(fields[7], vScale))
        {
            ++m_iMapDraftSkippedCount;
            continue;
        }

        bool bApplied = false;
        for (EldenRuntimeMapPlacement& placement : m_MapPlacements)
        {
            if (placement.strTile != fields[0] ||
                placement.strKind != fields[1] ||
                placement.strName != fields[2])
            {
                continue;
            }

            placement.vPosition = vPosition;
            placement.vRotationDeg = vRotationDeg;
            placement.vScale = vScale;
            ApplyMapPlacementTransform(placement);
            ++m_iMapDraftAppliedCount;
            bApplied = true;
            break;
        }

        if (!bApplied)
            ++m_iMapDraftSkippedCount;
    }
}

void CEldenLimgraveShowcaseScene::FrameLineup()
{
    m_bFreeCam = true;
    m_fOrbitAngle = -1.5707963f;
    m_fOrbitRadius = 28.f;
    m_fOrbitHeight = 8.f;
    m_vCamPos = Vec3{ m_vLineupCenter.x, m_vLineupCenter.y + 7.2f, m_vLineupCenter.z - 28.f };
    m_fYaw = 0.f;
    m_fPitch = -0.04f;
}

void CEldenLimgraveShowcaseScene::FrameErdtree()
{
    m_bFreeCam = true;
    m_vCamPos = Vec3{ m_vErdtreePos.x - 130.f, m_vErdtreePos.y + 58.f, m_vErdtreePos.z - 210.f };
    m_fYaw = 0.55f;
    m_fPitch = -0.18f;
}

void CEldenLimgraveShowcaseScene::SetFreeCameraLookAt(const Vec3& vEye, const Vec3& vTarget)
{
    m_bFreeCam = true;
    m_vCamPos = vEye;

    const f32_t fDx = vTarget.x - vEye.x;
    const f32_t fDy = vTarget.y - vEye.y;
    const f32_t fDz = vTarget.z - vEye.z;
    const f32_t fHorizontal = std::sqrt(fDx * fDx + fDz * fDz);
    m_fYaw = std::atan2(fDx, fDz);
    m_fPitch = std::atan2(fDy, fHorizontal);
}

void CEldenLimgraveShowcaseScene::FrameStartingCave()
{
    m_vLineupCenter = Vec3{ -36.f, 5.5f, 15.f };
    SetFreeCameraLookAt(Vec3{ -54.f, 12.f, -10.f }, Vec3{ -34.f, 6.f, 25.f });
    m_fOrbitRadius = 18.f;
    m_fOrbitHeight = 5.f;
}

void CEldenLimgraveShowcaseScene::FrameLimgraveVista()
{
    m_vLineupCenter = Vec3{ -16.138f, 104.800f, -120.100f };
    SetFreeCameraLookAt(Vec3{ -210.f, 245.f, -210.f }, Vec3{ 35.f, 105.f, -10.f });
    m_fOrbitRadius = 72.f;
    m_fOrbitHeight = 24.f;
}

ModelRenderer* CEldenLimgraveShowcaseScene::SpawnInstance(const std::string& strWmeshPath,
                                                          const Mat4& matWorld,
                                                          bool bCycleAnims)
{
    auto pRenderer = std::make_unique<ModelRenderer>();
    if (!pRenderer->Initialize(strWmeshPath))
    {
        static u32_t s_iLoggedFailures = 0;
        if (s_iLoggedFailures < 24u)
        {
            if (FILE* pLog = nullptr; fopen_s(&pLog, "limgrave_showcase_failures.txt", "a") == 0 && pLog)
            {
                fprintf(pLog, "ModelRenderer::Initialize failed: %s\n", strWmeshPath.c_str());
                fclose(pLog);
            }
            ++s_iLoggedFailures;
        }
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

void CEldenLimgraveShowcaseScene::WriteMapClosureAudit(const std::string& strAuditRelative) const
{
    namespace fs = std::filesystem;

    std::string strPath = ResolveRepoRelativePath(strAuditRelative);
    if (strPath.empty())
    {
        const fs::path relativePath(strAuditRelative);
        const std::string strParent = ResolveRepoRelativePath(relativePath.parent_path().string());
        if (!strParent.empty())
            strPath = (fs::path(strParent) / relativePath.filename()).string();
    }
    if (strPath.empty())
        return;

    FILE* pFile = nullptr;
    if (fopen_s(&pFile, strPath.c_str(), "w") != 0 || !pFile)
        return;

    fprintf(pFile,
            "{\n"
            "  \"schema\": \"winters.elden.map_closure.v1\",\n"
            "  \"essence\": \"source placement -> runtime asset -> spawn\",\n"
            "  \"stage\": \"%s\",\n"
            "  \"source\": %u,\n"
            "  \"closed\": %u,\n"
            "  \"open\": %u,\n"
            "  \"missingAsset\": %u,\n"
            "  \"spawnFailed\": %u\n"
            "}\n",
            GetStageName(),
            m_iMapSourceCount,
            m_iMapClosedCount,
            m_iMapMissingAssetCount + m_iMapSpawnFailedCount,
            m_iMapMissingAssetCount,
            m_iMapSpawnFailedCount);

    fclose(pFile);
}

void CEldenLimgraveShowcaseScene::OnExit()
{
    EnsureCursorReleasedAndVisible();
    m_CharacterPlacements.clear();
    m_Instances.clear();
}

void CEldenLimgraveShowcaseScene::UpdateFreeCamera(f32_t deltaTime)
{
    HWND hWnd = ::GetForegroundWindow();
    DWORD windowPid = 0;
    if (hWnd)
        ::GetWindowThreadProcessId(hWnd, &windowPid);
    const bool bFocused = hWnd && windowPid == ::GetCurrentProcessId();

    // F2 toggles free camera <-> auto orbit. TAB is left to ImGui focus.
    const bool bToggleDown = bFocused && (::GetAsyncKeyState(VK_F2) & 0x8000) != 0;
    if (!bToggleDown)
        m_bFreeCamToggleArmed = true;
    if (bToggleDown && m_bFreeCamToggleArmed && !m_bFreeCamToggleWasDown)
    {
        m_bFreeCam = !m_bFreeCam;
        m_bFreeCamToggleArmed = false;
    }
    m_bFreeCamToggleWasDown = bToggleDown;

    if (!m_bFreeCam)
        return;

    if (!bFocused)
    {
        m_bMouseLookInit = false;
        return;
    }

    // F1 toggles mouse-look capture (release the cursor to use the window).
    const bool bF1Down = (::GetAsyncKeyState(VK_F1) & 0x8000) != 0;
    if (bF1Down && !m_bF1WasDown)
    {
        m_bMouseLook = !m_bMouseLook;
        m_bMouseLookInit = false;
    }
    m_bF1WasDown = bF1Down;

    // Always-on FPS mouse look while our window is focused: read the delta
    // from the client center, then re-center the cursor.
    if (m_bMouseLook)
    {
        // FPS 시점: 커서를 중앙에 고정(SetCursorPos)하지 않고, 직전 프레임 대비
        // 델타로 화면을 돌린다. 커서는 창 안에서 자유롭게 움직인다.
        POINT cursor{};
        ::GetCursorPos(&cursor);
        if (m_bMouseLookInit)
        {
            m_fYaw += static_cast<f32_t>(cursor.x - m_iLastCursorX) * 0.0030f;
            m_fPitch -= static_cast<f32_t>(cursor.y - m_iLastCursorY) * 0.0030f;
            m_fPitch = max(-1.45f, min(1.45f, m_fPitch));
        }
        m_iLastCursorX = cursor.x;
        m_iLastCursorY = cursor.y;
        m_bMouseLookInit = true;
    }
    else
    {
        // 마우스룩 비활성(F1 off): 커서 위치만 추적, 회전 없음.
        POINT cursor{};
        ::GetCursorPos(&cursor);
        m_iLastCursorX = cursor.x;
        m_iLastCursorY = cursor.y;
        m_bMouseLookInit = false;
    }

    // Arrow keys: keyboard look (automation/no-mouse driving).
    const f32_t fLook = 1.6f * deltaTime;
    if (::GetAsyncKeyState(VK_LEFT) & 0x8000)  m_fYaw -= fLook;
    if (::GetAsyncKeyState(VK_RIGHT) & 0x8000) m_fYaw += fLook;
    if (::GetAsyncKeyState(VK_UP) & 0x8000)    m_fPitch += fLook;
    if (::GetAsyncKeyState(VK_DOWN) & 0x8000)  m_fPitch -= fLook;
    m_fPitch = max(-1.45f, min(1.45f, m_fPitch));

    // G: wide shot framing the whole lineup (centered, 75m back, 25m up).
    if (::GetAsyncKeyState('G') & 0x8000)
    {
        m_vCamPos = Vec3{ m_vLineupCenter.x + 18.5f, m_vLineupCenter.y + 25.f, m_vLineupCenter.z - 75.f };
        m_fYaw = 0.f;
        m_fPitch = -0.30f;
    }
    // H: jump to a wide shot looking back at the Limgrave map from the stage.
    if (::GetAsyncKeyState('H') & 0x8000)
    {
        m_vCamPos = Vec3{ -120.f, 170.f, 0.f };
        m_fYaw = 1.40f;   // look east toward the map
        m_fPitch = -0.30f;
    }

    const Vec3 vForward{
        std::sin(m_fYaw) * std::cos(m_fPitch),
        std::sin(m_fPitch),
        std::cos(m_fYaw) * std::cos(m_fPitch),
    };
    const Vec3 vRight{ std::cos(m_fYaw), 0.f, -std::sin(m_fYaw) };

    f32_t fSpeed = 140.f;          // 기존 14 → 140 (×10)
    if (::GetAsyncKeyState(VK_SHIFT) & 0x8000)
        fSpeed *= 4.f;             // Shift 가속(560)

    const f32_t fStep = fSpeed * deltaTime;
    if (::GetAsyncKeyState('W') & 0x8000)
    {
        m_vCamPos.x += vForward.x * fStep; m_vCamPos.y += vForward.y * fStep; m_vCamPos.z += vForward.z * fStep;
    }
    if (::GetAsyncKeyState('S') & 0x8000)
    {
        m_vCamPos.x -= vForward.x * fStep; m_vCamPos.y -= vForward.y * fStep; m_vCamPos.z -= vForward.z * fStep;
    }
    if (::GetAsyncKeyState('A') & 0x8000)
    {
        m_vCamPos.x -= vRight.x * fStep; m_vCamPos.z -= vRight.z * fStep;
    }
    if (::GetAsyncKeyState('D') & 0x8000)
    {
        m_vCamPos.x += vRight.x * fStep; m_vCamPos.z += vRight.z * fStep;
    }
    if ((::GetAsyncKeyState('E') & 0x8000) || (::GetAsyncKeyState(VK_SPACE) & 0x8000))
        m_vCamPos.y += fStep;
    if (::GetAsyncKeyState('Q') & 0x8000)
        m_vCamPos.y -= fStep;
}

void CEldenLimgraveShowcaseScene::UpdateStageProgression()
{
    const bool bAdvanceDown =
        ((::GetAsyncKeyState('T') & 0x8000) != 0) ||
        ((::GetAsyncKeyState(VK_RETURN) & 0x8000) != 0);

    if (bAdvanceDown && !m_bStageAdvanceWasDown && m_eStage == EldenShowcaseStage::StartingCave)
        m_bStageLoaded = LoadStage(EldenShowcaseStage::LimgraveVista);

    m_bStageAdvanceWasDown = bAdvanceDown;

    const bool bCaveDown = (::GetAsyncKeyState('C') & 0x8000) != 0;
    if (bCaveDown && !m_bStageCaveWasDown && m_eStage != EldenShowcaseStage::StartingCave)
        m_bStageLoaded = LoadStage(EldenShowcaseStage::StartingCave);
    m_bStageCaveWasDown = bCaveDown;
}

void CEldenLimgraveShowcaseScene::OnUpdate(f32_t deltaTime)
{
    m_fOrbitAngle += deltaTime * 0.12f;
    if (!m_bStageLoaded)
    {
        if (!m_bStageLoadAttempted)
            LoadStage(m_eStage);
        if (!m_bStageLoaded)
            return;
    }

    UpdateStageProgression();
    UpdateFreeCamera(deltaTime);

    static f32_t s_fElapsed = 0.f;
    static f32_t s_fNextHeartbeat = 5.f;
    s_fElapsed += deltaTime;
    if (s_fElapsed >= s_fNextHeartbeat)
    {
        s_fNextHeartbeat += 5.f;
        if (FILE* pLog = nullptr; fopen_s(&pLog, "limgrave_showcase_log.txt", "a") == 0 && pLog)
        {
            fprintf(pLog, "heartbeat t=%.1f cam=(%.1f,%.1f,%.1f) yaw=%.2f pitch=%.2f freeCam=%d\n",
                    s_fElapsed, m_vCamPos.x, m_vCamPos.y, m_vCamPos.z, m_fYaw, m_fPitch, m_bFreeCam ? 1 : 0);
            fclose(pLog);
        }
    }

    for (ShowcaseInstance& instance : m_Instances)
    {
        if (!instance.pRenderer || !instance.bCycleAnims)
            continue;

        instance.fAnimTimer += deltaTime;
        if (instance.fAnimTimer >= kAnimCycleSeconds && instance.iAnimCount > 1)
        {
            instance.fAnimTimer = 0.f;
            instance.iAnimIndex = (instance.iAnimIndex + 1) % instance.iAnimCount;
            instance.pRenderer->PlayAnimation(instance.iAnimIndex);
        }
        instance.pRenderer->Update(deltaTime);
    }
}

void CEldenLimgraveShowcaseScene::OnRender()
{
    Vec3 vEye, vTarget;
    if (m_bFreeCam)
    {
        vEye = m_vCamPos;
        vTarget = Vec3{
            m_vCamPos.x + std::sin(m_fYaw) * std::cos(m_fPitch),
            m_vCamPos.y + std::sin(m_fPitch),
            m_vCamPos.z + std::cos(m_fYaw) * std::cos(m_fPitch),
        };
    }
    else
    {
        vEye = Vec3{
            m_vLineupCenter.x + std::cos(m_fOrbitAngle) * m_fOrbitRadius,
            m_vLineupCenter.y + m_fOrbitHeight,
            m_vLineupCenter.z + std::sin(m_fOrbitAngle) * m_fOrbitRadius,
        };
        vTarget = Vec3{ m_vLineupCenter.x, m_vLineupCenter.y + 1.8f, m_vLineupCenter.z };
    }

    const Mat4 matView = Mat4::LookAt(vEye, vTarget, Vec3{ 0.f, 1.f, 0.f });
    const Mat4 matProj = Mat4::Perspective(62.f * kDegToRad, m_fAspect, 0.3f, 4000.f);
    const Mat4 matViewProj = matView * matProj;

    for (ShowcaseInstance& instance : m_Instances)
    {
        if (!instance.pRenderer)
            continue;
        instance.pRenderer->UpdateCamera(matViewProj, vEye);
        instance.pRenderer->RenderFrustumCulled(matViewProj);
    }
}

void CEldenLimgraveShowcaseScene::OnImGui()
{
    ImGui::SetNextWindowPos(ImVec2(18.f, 190.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(390.f, 520.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Elden Placement Editor"))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Stage %s", GetStageName());
    if (!m_bStageLoaded)
        ImGui::TextUnformatted("Waiting for stage load...");
    if (m_eStage == EldenShowcaseStage::StartingCave)
        ImGui::TextUnformatted("Tutorial: WASD / mouse, then T or Enter to emerge.");
    else
        ImGui::TextUnformatted("Limgrave vista: baseline scene. Press C only for cave extraction lab.");
    if (ImGui::Button("Load Cave"))
        m_bStageLoaded = LoadStage(EldenShowcaseStage::StartingCave);
    ImGui::SameLine();
    if (ImGui::Button("Emerge to Limgrave"))
        m_bStageLoaded = LoadStage(EldenShowcaseStage::LimgraveVista);
    ImGui::Separator();

    ImGui::Text("Placed %u / Failed %u / Animated %u", m_iPlacedCount, m_iFailedCount, m_iAnimatedCount);
    ImGui::Text("Erdtree %s", m_bErdtreeLoaded ? "Loaded" : "Missing");
    ImGui::Text("Slice %s / Tiles loaded %u skipped %u scanned %u",
                m_bVerticalSliceLoaded ? "Loaded" : "Missing",
                m_iTilesLoaded,
                m_iTilesSkipped,
                m_iTilesScanned);
    ImGui::Text("Limgrave audit placed %u unresolved %u",
                m_iSlicePlacedTotal,
                m_iSliceUnresolvedTotal);
    ImGui::Text("Map load mode: %s",
                (m_bLoadOnlyFocusTiles && m_bVerticalSliceLoaded) ? "Focus tiles" : "All tiles");
    ImGui::Text("Map closure source=%u closed=%u open=%u",
                m_iMapSourceCount,
                m_iMapClosedCount,
                m_iMapMissingAssetCount + m_iMapSpawnFailedCount);
    ImGui::Text("Open missingAsset=%u spawnFailed=%u",
                m_iMapMissingAssetCount,
                m_iMapSpawnFailedCount);
    ImGui::Text("Draft loaded=%u applied=%u skipped=%u",
                m_iMapDraftLoadedCount,
                m_iMapDraftAppliedCount,
                m_iMapDraftSkippedCount);
    ImGui::TextWrapped("Draft %s", GetMapPlacementDraftRelative().c_str());
    if (ImGui::CollapsingHeader("Vertical Slice Focus", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (m_FocusTiles.empty())
        {
            ImGui::TextDisabled("No focus tiles loaded.");
        }
        else
        {
            for (const EldenLimgraveTileSummary& tile : m_FocusTiles)
            {
                ImGui::BulletText("%s  placed=%u unresolved=%u  %s",
                                  tile.strTile.c_str(),
                                  tile.iPlaced,
                                  tile.iUnresolved,
                                  tile.strRole.empty() ? "" : tile.strRole.c_str());
            }
        }
    }
    ImGui::Separator();

    if (ImGui::Button("Frame Cave"))
        FrameStartingCave();
    ImGui::SameLine();
    if (ImGui::Button("Frame Vista"))
        FrameLimgraveVista();

    if (ImGui::Button("Frame Lineup"))
        FrameLineup();
    ImGui::SameLine();
    if (ImGui::Button("Frame Erdtree"))
        FrameErdtree();
    ImGui::SameLine();
    ImGui::Checkbox("FreeCam", &m_bFreeCam);

    ImGui::DragFloat3("Lineup Center", &m_vLineupCenter.x, 0.1f, -1000.f, 1000.f, "%.3f");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Map Placement Tuner", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Runtime map placements %zu", m_MapPlacements.size());
        if (m_MapPlacements.empty())
        {
            ImGui::TextDisabled("No runtime map placements loaded.");
        }
        else
        {
            if (m_iSelectedMapPlacement < 0)
                m_iSelectedMapPlacement = 0;
            if (m_iSelectedMapPlacement >= static_cast<i32_t>(m_MapPlacements.size()))
                m_iSelectedMapPlacement = static_cast<i32_t>(m_MapPlacements.size() - 1);

            if (ImGui::BeginListBox("Map Instances", ImVec2(-1.f, 125.f)))
            {
                for (size_t i = 0; i < m_MapPlacements.size(); ++i)
                {
                    const EldenRuntimeMapPlacement& mapPlacement = m_MapPlacements[i];
                    char label[192]{};
                    sprintf_s(label,
                              "%s %s %s",
                              mapPlacement.strTile.c_str(),
                              mapPlacement.strKind.c_str(),
                              mapPlacement.strName.c_str());
                    const bool bSelected = m_iSelectedMapPlacement == static_cast<i32_t>(i);
                    if (ImGui::Selectable(label, bSelected))
                        m_iSelectedMapPlacement = static_cast<i32_t>(i);
                    if (bSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndListBox();
            }

            EldenRuntimeMapPlacement& mapPlacement =
                m_MapPlacements[static_cast<size_t>(m_iSelectedMapPlacement)];
            ImGui::TextWrapped("%s / %s", mapPlacement.strModel.c_str(), mapPlacement.strWmesh.c_str());

            bool bMapTransformChanged = false;
            bMapTransformChanged |= ImGui::DragFloat3("Map Position", &mapPlacement.vPosition.x, 0.1f, -3000.f, 3000.f, "%.3f");
            bMapTransformChanged |= ImGui::DragFloat3("Map Rotation Deg", &mapPlacement.vRotationDeg.x, 0.25f, -360.f, 360.f, "%.3f");
            bMapTransformChanged |= ImGui::DragFloat3("Map Scale", &mapPlacement.vScale.x, 0.001f, 0.001f, 10.f, "%.4f");
            if (bMapTransformChanged)
                ApplyMapPlacementTransform(mapPlacement);

            if (ImGui::Button("Frame Selected"))
            {
                const Vec3 vEye{
                    mapPlacement.vPosition.x - 8.f,
                    mapPlacement.vPosition.y + 4.f,
                    mapPlacement.vPosition.z - 10.f
                };
                SetFreeCameraLookAt(vEye, mapPlacement.vPosition);
            }
            ImGui::SameLine();
            static bool s_bLastMapSaveOk = false;
            static bool s_bHasMapSaveResult = false;
            if (ImGui::Button("Save Map Draft"))
            {
                s_bLastMapSaveOk = SaveMapPlacementDraft();
                s_bHasMapSaveResult = true;
            }
            if (s_bHasMapSaveResult)
            {
                ImGui::TextColored(
                    s_bLastMapSaveOk ? ImVec4(0.45f, 0.95f, 0.55f, 1.f) : ImVec4(1.f, 0.35f, 0.35f, 1.f),
                    "%s",
                    s_bLastMapSaveOk ? "Draft saved" : "Draft save failed");
            }
        }
    }
    ImGui::Separator();

    if (m_CharacterPlacements.empty())
    {
        ImGui::TextDisabled("No character placements loaded.");
        ImGui::End();
        return;
    }

    if (m_iSelectedCharacter < 0)
        m_iSelectedCharacter = 0;
    if (m_iSelectedCharacter >= static_cast<i32_t>(m_CharacterPlacements.size()))
        m_iSelectedCharacter = static_cast<i32_t>(m_CharacterPlacements.size() - 1);

    if (ImGui::BeginListBox("Characters", ImVec2(-1.f, 125.f)))
    {
        for (size_t i = 0; i < m_CharacterPlacements.size(); ++i)
        {
            const bool bSelected = m_iSelectedCharacter == static_cast<i32_t>(i);
            const std::string& strLabel = m_CharacterPlacements[i].strLabel;
            const char* pLabel = strLabel.empty() ? "(unnamed)" : strLabel.c_str();
            if (ImGui::Selectable(pLabel, bSelected))
                m_iSelectedCharacter = static_cast<i32_t>(i);
            if (bSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
    }

    EldenCharacterPlacement& placement = m_CharacterPlacements[static_cast<size_t>(m_iSelectedCharacter)];
    bool bTransformChanged = false;

    ImGui::TextUnformatted(placement.strModel.empty() ? placement.strWmesh.c_str() : placement.strModel.c_str());
    bTransformChanged |= ImGui::DragFloat3("Position", &placement.vPosition.x, 0.1f, -2000.f, 2000.f, "%.3f");
    bTransformChanged |= ImGui::DragFloat3("Rotation Deg", &placement.vRotationDeg.x, 0.25f, -360.f, 360.f, "%.3f");
    bTransformChanged |= ImGui::DragFloat3("Axis Fix Deg", &placement.vAxisFixDeg.x, 0.25f, -360.f, 360.f, "%.3f");
    bTransformChanged |= ImGui::DragFloat3("Scale", &placement.vScale.x, 0.0001f, 0.0001f, 2.f, "%.4f");

    if (ImGui::Button("Axis X +90"))
    {
        placement.vAxisFixDeg = Vec3{ 90.f, 0.f, 0.f };
        bTransformChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Axis X -90"))
    {
        placement.vAxisFixDeg = Vec3{ -90.f, 0.f, 0.f };
        bTransformChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Flip Y 180"))
    {
        placement.vAxisFixDeg.y += 180.f;
        bTransformChanged = true;
    }

    if (bTransformChanged)
        ApplyCharacterTransform(placement);

    if (ImGui::Checkbox("Animated", &placement.bAnimated))
    {
        for (ShowcaseInstance& instance : m_Instances)
        {
            if (instance.pRenderer.get() != placement.pRenderer)
                continue;
            instance.bCycleAnims = placement.bAnimated && instance.iAnimCount > 0;
            instance.iAnimIndex = 0;
            instance.fAnimTimer = 0.f;
            if (instance.bCycleAnims)
                instance.pRenderer->PlayAnimation(0);
            break;
        }
    }

    u32_t iAnimCount = 0;
    if (placement.pRenderer)
        iAnimCount = placement.pRenderer->GetAnimationCount();
    ImGui::Text("Animations %u", iAnimCount);

    ImGui::Separator();
    static bool s_bLastSaveOk = false;
    static bool s_bHasSaveResult = false;
    if (ImGui::Button("Save Character JSON", ImVec2(-1.f, 0.f)))
    {
        s_bLastSaveOk = SaveCharacterPlacements();
        s_bHasSaveResult = true;
    }
    if (s_bHasSaveResult)
        ImGui::TextColored(s_bLastSaveOk ? ImVec4(0.45f, 0.95f, 0.55f, 1.f) : ImVec4(1.f, 0.35f, 0.35f, 1.f),
                           "%s", s_bLastSaveOk ? "Saved" : "Save failed");

    ImGui::End();
}
