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
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
    constexpr f32_t kDegToRad = 3.14159265358979323846f / 180.f;

    Mat4 BuildRotationDeg(const Vec3& vRotationDeg);

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

    SpawnPlacements();
    SpawnErdtree();
    SpawnCharacterPlacements();

    char msg[160]{};
    sprintf_s(msg, "[LimgraveShowcase] placed=%u failed=%u animated=%u\n",
              m_iPlacedCount, m_iFailedCount, m_iAnimatedCount);
    ::OutputDebugStringA(msg);

    if (FILE* pLog = nullptr; fopen_s(&pLog, "limgrave_showcase_log.txt", "w") == 0 && pLog)
    {
        fprintf(pLog, "OnEnter done: placed=%u failed=%u animated=%u\n",
                m_iPlacedCount, m_iFailedCount, m_iAnimatedCount);
        fclose(pLog);
    }

    return m_iPlacedCount > 0;
}

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
        const fs::path placement = tileDir.path() / "map_placement.txt";
        if (!fs::exists(placement))
            continue;

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

void CEldenLimgraveShowcaseScene::FrameLineup()
{
    m_bFreeCam = false;
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

void CEldenLimgraveShowcaseScene::OnExit()
{
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

void CEldenLimgraveShowcaseScene::OnUpdate(f32_t deltaTime)
{
    m_fOrbitAngle += deltaTime * 0.12f;
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

    ImGui::Text("Placed %u / Failed %u / Animated %u", m_iPlacedCount, m_iFailedCount, m_iAnimatedCount);
    ImGui::Text("Erdtree %s", m_bErdtreeLoaded ? "Loaded" : "Missing");
    ImGui::Separator();

    if (ImGui::Button("Frame Lineup"))
        FrameLineup();
    ImGui::SameLine();
    if (ImGui::Button("Frame Erdtree"))
        FrameErdtree();
    ImGui::SameLine();
    ImGui::Checkbox("FreeCam", &m_bFreeCam);

    ImGui::DragFloat3("Lineup Center", &m_vLineupCenter.x, 0.1f, -1000.f, 1000.f, "%.3f");
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
