#include "WintersPCH.h"

#include "World/WorldPartitionSystem.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <unordered_map>

namespace Engine
{
    namespace
    {
        enum class eJsonType : u8_t
        {
            Null = 0,
            Object,
            Array,
            String,
            Number,
            Bool
        };

        struct JsonValue
        {
            eJsonType eType = eJsonType::Null;
            std::unordered_map<std::string, JsonValue> mapObject;
            std::vector<JsonValue> vecArray;
            std::string strValue;
            double dNumber = 0.0;
            bool_t bValue = false;

            const JsonValue* Find(const char* pKey) const
            {
                if (eType != eJsonType::Object || !pKey)
                    return nullptr;

                auto it = mapObject.find(pKey);
                return it == mapObject.end() ? nullptr : &it->second;
            }
        };

        class JsonParser
        {
        public:
            explicit JsonParser(const std::string& strText)
                : m_strText(strText)
            {
            }

            bool_t Parse(JsonValue& out)
            {
                SkipTrivia();
                if (!ParseValue(out))
                    return false;

                SkipTrivia();
                return m_iPos == m_strText.size();
            }

        private:
            bool_t ParseValue(JsonValue& out)
            {
                SkipTrivia();
                if (m_iPos >= m_strText.size())
                    return false;

                const char ch = m_strText[m_iPos];
                if (ch == '{')
                    return ParseObject(out);
                if (ch == '[')
                    return ParseArray(out);
                if (ch == '"')
                    return ParseStringValue(out);
                if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)))
                    return ParseNumber(out);
                if (Match("true"))
                {
                    out.eType = eJsonType::Bool;
                    out.bValue = true;
                    return true;
                }
                if (Match("false"))
                {
                    out.eType = eJsonType::Bool;
                    out.bValue = false;
                    return true;
                }
                if (Match("null"))
                {
                    out = JsonValue{};
                    return true;
                }
                return false;
            }

            bool_t ParseObject(JsonValue& out)
            {
                if (!Consume('{'))
                    return false;

                out = JsonValue{};
                out.eType = eJsonType::Object;
                SkipTrivia();
                if (Consume('}'))
                    return true;

                for (;;)
                {
                    std::string strKey;
                    if (!ParseString(strKey) || !Consume(':'))
                        return false;

                    JsonValue value;
                    if (!ParseValue(value))
                        return false;

                    out.mapObject.emplace(std::move(strKey), std::move(value));
                    SkipTrivia();
                    if (Consume('}'))
                        return true;
                    if (!Consume(','))
                        return false;
                }
            }

            bool_t ParseArray(JsonValue& out)
            {
                if (!Consume('['))
                    return false;

                out = JsonValue{};
                out.eType = eJsonType::Array;
                SkipTrivia();
                if (Consume(']'))
                    return true;

                for (;;)
                {
                    JsonValue value;
                    if (!ParseValue(value))
                        return false;

                    out.vecArray.push_back(std::move(value));
                    SkipTrivia();
                    if (Consume(']'))
                        return true;
                    if (!Consume(','))
                        return false;
                }
            }

            bool_t ParseStringValue(JsonValue& out)
            {
                out = JsonValue{};
                out.eType = eJsonType::String;
                return ParseString(out.strValue);
            }

            bool_t ParseString(std::string& out)
            {
                SkipTrivia();
                if (!Consume('"'))
                    return false;

                out.clear();
                while (m_iPos < m_strText.size())
                {
                    const char ch = m_strText[m_iPos++];
                    if (ch == '"')
                        return true;

                    if (ch != '\\')
                    {
                        out.push_back(ch);
                        continue;
                    }

                    if (m_iPos >= m_strText.size())
                        return false;

                    const char escaped = m_strText[m_iPos++];
                    switch (escaped)
                    {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    default:
                        out.push_back(escaped);
                        break;
                    }
                }

                return false;
            }

            bool_t ParseNumber(JsonValue& out)
            {
                const char* pBegin = m_strText.c_str() + m_iPos;
                char* pEnd = nullptr;
                const double dValue = std::strtod(pBegin, &pEnd);
                if (pEnd == pBegin)
                    return false;

                m_iPos = static_cast<size_t>(pEnd - m_strText.c_str());
                out = JsonValue{};
                out.eType = eJsonType::Number;
                out.dNumber = dValue;
                return true;
            }

            bool_t Consume(char ch)
            {
                SkipTrivia();
                if (m_iPos >= m_strText.size() || m_strText[m_iPos] != ch)
                    return false;

                ++m_iPos;
                return true;
            }

            bool_t Match(const char* pText)
            {
                const size_t iLen = std::strlen(pText);
                if (m_strText.compare(m_iPos, iLen, pText) != 0)
                    return false;

                m_iPos += iLen;
                return true;
            }

            void SkipTrivia()
            {
                for (;;)
                {
                    while (m_iPos < m_strText.size() &&
                        std::isspace(static_cast<unsigned char>(m_strText[m_iPos])))
                    {
                        ++m_iPos;
                    }

                    if (m_iPos + 1 >= m_strText.size() || m_strText[m_iPos] != '/')
                        return;

                    const char next = m_strText[m_iPos + 1];
                    if (next == '/')
                    {
                        m_iPos += 2;
                        while (m_iPos < m_strText.size() && m_strText[m_iPos] != '\n')
                            ++m_iPos;
                        continue;
                    }

                    if (next == '*')
                    {
                        m_iPos += 2;
                        while (m_iPos + 1 < m_strText.size() &&
                            !(m_strText[m_iPos] == '*' && m_strText[m_iPos + 1] == '/'))
                        {
                            ++m_iPos;
                        }
                        if (m_iPos + 1 < m_strText.size())
                            m_iPos += 2;
                        continue;
                    }

                    return;
                }
            }

            const std::string& m_strText;
            size_t m_iPos = 0u;
        };

        bool_t ReadTextFile(const std::filesystem::path& path, std::string& out)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return false;

            std::ostringstream stream;
            stream << file.rdbuf();
            out = stream.str();
            return true;
        }

        bool_t ParseJsonFile(const std::filesystem::path& path, JsonValue& out)
        {
            std::string strText;
            if (!ReadTextFile(path, strText))
                return false;

            JsonParser parser(strText);
            return parser.Parse(out);
        }

        std::string GetString(const JsonValue& obj, const char* pKey, const std::string& strDefault = {})
        {
            const JsonValue* pValue = obj.Find(pKey);
            if (!pValue || pValue->eType != eJsonType::String)
                return strDefault;

            return pValue->strValue;
        }

        bool_t GetBool(const JsonValue& obj, const char* pKey, bool_t bDefault)
        {
            const JsonValue* pValue = obj.Find(pKey);
            if (!pValue || pValue->eType != eJsonType::Bool)
                return bDefault;

            return pValue->bValue;
        }

        f32_t GetFloat(const JsonValue& obj, const char* pKey, f32_t fDefault)
        {
            const JsonValue* pValue = obj.Find(pKey);
            if (!pValue || pValue->eType != eJsonType::Number)
                return fDefault;

            return static_cast<f32_t>(pValue->dNumber);
        }

        i32_t GetI32(const JsonValue& obj, const char* pKey, i32_t iDefault)
        {
            const JsonValue* pValue = obj.Find(pKey);
            if (!pValue || pValue->eType != eJsonType::Number)
                return iDefault;

            return static_cast<i32_t>(pValue->dNumber);
        }

        bool_t TryGetVec3(const JsonValue& obj, const char* pKey, Vec3& out)
        {
            const JsonValue* pValue = obj.Find(pKey);
            if (!pValue || pValue->eType != eJsonType::Array || pValue->vecArray.size() < 3u)
                return false;

            const JsonValue& x = pValue->vecArray[0u];
            const JsonValue& y = pValue->vecArray[1u];
            const JsonValue& z = pValue->vecArray[2u];
            if (x.eType != eJsonType::Number ||
                y.eType != eJsonType::Number ||
                z.eType != eJsonType::Number)
            {
                return false;
            }

            out = Vec3{
                static_cast<f32_t>(x.dNumber),
                static_cast<f32_t>(y.dNumber),
                static_cast<f32_t>(z.dNumber)
            };
            return true;
        }

        u64_t LayerBitFromName(const std::string& strName)
        {
            if (strName.empty() || strName == "Base")
                return 1ull;
            if (strName == "Gameplay")
                return 2ull;

            u64_t uHash = 1469598103934665603ull;
            for (const char ch : strName)
            {
                uHash ^= static_cast<u8_t>(ch);
                uHash *= 1099511628211ull;
            }
            return 1ull << static_cast<u32_t>(uHash % 63ull);
        }

        CellInstanceDesc ParseInstance(const JsonValue& obj)
        {
            CellInstanceDesc inst{};
            inst.strName = GetString(obj, "name");
            inst.strModel = GetString(obj, "model");
            inst.strKind = GetString(obj, "kind");
            inst.strWmesh = GetString(obj, "wmesh");
            inst.strWmat = GetString(obj, "wmat");

            TryGetVec3(obj, "position", inst.vPosition);
            TryGetVec3(obj, "rotationDeg", inst.vRotationDeg);
            TryGetVec3(obj, "scale", inst.vScale);

            if (const JsonValue* pTransform = obj.Find("transform"))
            {
                if (pTransform->eType == eJsonType::Object)
                {
                    TryGetVec3(*pTransform, "position", inst.vPosition);
                    TryGetVec3(*pTransform, "rotationDeg", inst.vRotationDeg);
                    TryGetVec3(*pTransform, "scale", inst.vScale);
                }
            }

            inst.bPlaceable = GetBool(obj, "placeable", false);
            inst.bRequired = GetBool(obj, "required", true);

            const std::string strDataLayer = GetString(obj, "dataLayer");
            inst.layerBit = LayerBitFromName(strDataLayer);

            return inst;
        }

        void ParseInstancesInto(const JsonValue& obj, CellDescriptor& cell)
        {
            const JsonValue* pInstances = obj.Find("instances");
            if (!pInstances || pInstances->eType != eJsonType::Array)
                return;

            for (const JsonValue& value : pInstances->vecArray)
            {
                if (value.eType == eJsonType::Object)
                    cell.vecInstances.push_back(ParseInstance(value));
            }
        }

        bool_t ParseCellFile(const std::filesystem::path& path, CellDescriptor& cell)
        {
            JsonValue root;
            if (!ParseJsonFile(path, root) || root.eType != eJsonType::Object)
                return false;

            const std::string strId = GetString(root, "id");
            if (!strId.empty())
                cell.strId = strId;

            const std::string strNav = GetString(root, "nav");
            if (!strNav.empty())
                cell.strNavPath = strNav;

            ParseInstancesInto(root, cell);
            return true;
        }

        bool_t ParseCellHeader(const JsonValue& obj, CellDescriptor& out)
        {
            out.strId = GetString(obj, "id");
            if (out.strId.empty())
                return false;

            if (const JsonValue* pCoord = obj.Find("coord"))
            {
                if (pCoord->eType == eJsonType::Object)
                {
                    out.iCoordX = GetI32(*pCoord, "x", out.iCoordX);
                    out.iCoordZ = GetI32(*pCoord, "z", out.iCoordZ);
                }
            }

            TryGetVec3(obj, "boundsMin", out.vBoundsMin);
            TryGetVec3(obj, "boundsMax", out.vBoundsMax);

            out.strFile = GetString(obj, "file");
            out.strNavPath = GetString(obj, "nav");
            out.strHlodWmesh = GetString(obj, "hlod");

            ParseInstancesInto(obj, out);
            return true;
        }

        Mat4 BuildWorldMatrix(const CellInstanceDesc& inst)
        {
            constexpr f32_t kDegToRad = 3.14159265358979323846f / 180.f;
            return Mat4::Scale(inst.vScale) *
                Mat4::RotationX(inst.vRotationDeg.x * kDegToRad) *
                Mat4::RotationY(inst.vRotationDeg.y * kDegToRad) *
                Mat4::RotationZ(inst.vRotationDeg.z * kDegToRad) *
                Mat4::Translation(inst.vPosition);
        }

        Vec3 GetCellCenter(const CellDescriptor& cell, const WorldDescriptor& desc)
        {
            if (cell.vBoundsMax.x > cell.vBoundsMin.x ||
                cell.vBoundsMax.z > cell.vBoundsMin.z)
            {
                return Vec3{
                    (cell.vBoundsMin.x + cell.vBoundsMax.x) * 0.5f,
                    (cell.vBoundsMin.y + cell.vBoundsMax.y) * 0.5f,
                    (cell.vBoundsMin.z + cell.vBoundsMax.z) * 0.5f
                };
            }

            return Vec3{
                desc.vOrigin.x + (static_cast<f32_t>(cell.iCoordX) + 0.5f) * desc.fCellSizeMeters,
                desc.vOrigin.y,
                desc.vOrigin.z + (static_cast<f32_t>(cell.iCoordZ) + 0.5f) * desc.fCellSizeMeters
            };
        }

        u64_t CoordKey(i32_t iCoordX, i32_t iCoordZ)
        {
            return (static_cast<u64_t>(static_cast<u32_t>(iCoordX)) << 32u) |
                static_cast<u32_t>(iCoordZ);
        }

        bool_t IsFiniteVec3(const Vec3& v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }

        void SortInstances(std::vector<CellInstanceDesc>& instances)
        {
            std::stable_sort(instances.begin(), instances.end(),
                [](const CellInstanceDesc& lhs, const CellInstanceDesc& rhs)
                {
                    if (lhs.layerBit != rhs.layerBit)
                        return lhs.layerBit < rhs.layerBit;
                    if (lhs.strKind != rhs.strKind)
                        return lhs.strKind < rhs.strKind;
                    if (lhs.strName != rhs.strName)
                        return lhs.strName < rhs.strName;
                    if (lhs.strWmesh != rhs.strWmesh)
                        return lhs.strWmesh < rhs.strWmesh;
                    return lhs.strWmat < rhs.strWmat;
                });
        }

        void SortCells(std::vector<CellDescriptor>& cells)
        {
            for (CellDescriptor& cell : cells)
                SortInstances(cell.vecInstances);

            std::stable_sort(cells.begin(), cells.end(),
                [](const CellDescriptor& lhs, const CellDescriptor& rhs)
                {
                    if (lhs.iCoordZ != rhs.iCoordZ)
                        return lhs.iCoordZ < rhs.iCoordZ;
                    if (lhs.iCoordX != rhs.iCoordX)
                        return lhs.iCoordX < rhs.iCoordX;
                    return lhs.strId < rhs.strId;
                });
        }

        void DebugLogCellTransition(
            const CellDescriptor* pCell,
            eWorldCellState eFrom,
            eWorldCellState eTo,
            eWorldCellTransitionReason eReason)
        {
            if (!pCell)
                return;

            std::string strMessage = "[WorldPartition] cell ";
            strMessage += pCell->strId;
            strMessage += " ";
            strMessage += ToString(eFrom);
            strMessage += " -> ";
            strMessage += ToString(eTo);
            strMessage += " reason=";
            strMessage += ToString(eReason);
            strMessage += "\n";
            OutputDebugStringA(strMessage.c_str());
        }

        void DebugLogMissingAsset(
            const CellDescriptor& cell,
            const CellInstanceDesc& inst,
            const char* pWhat)
        {
            std::string strMessage = "[WorldPartition] missing ";
            strMessage += pWhat ? pWhat : "asset";
            strMessage += " cell=";
            strMessage += cell.strId;
            strMessage += " instance=";
            strMessage += inst.strName.empty() ? "<unnamed>" : inst.strName;
            strMessage += " required=";
            strMessage += inst.bRequired ? "true" : "false";
            strMessage += "\n";
            OutputDebugStringA(strMessage.c_str());
        }
    }

    struct CWorldPartitionSystem::Impl
    {
        struct DesiredCellState
        {
            eWorldCellState eState = eWorldCellState::Unloaded;
            eWorldCellTransitionReason eReason = eWorldCellTransitionReason::NoActiveSource;
        };

        CAssetStreamingSystem* pStreaming = nullptr;
        WorldDescriptor desc;
        std::vector<WorldCellRuntime> vecRuntime;
        std::unordered_map<std::string, size_t> mapCellById;
        std::unordered_map<u64_t, size_t> mapCellByCoord;
        std::map<u32_t, StreamingSourceComponent> mapSources;

        void BuildRuntime()
        {
            SortCells(desc.vecCells);
            vecRuntime.clear();
            mapCellById.clear();
            mapCellByCoord.clear();
            vecRuntime.reserve(desc.vecCells.size());

            for (size_t i = 0u; i < desc.vecCells.size(); ++i)
            {
                WorldCellRuntime runtime{};
                runtime.pDesc = &desc.vecCells[i];
                vecRuntime.push_back(std::move(runtime));
                mapCellById.emplace(desc.vecCells[i].strId, i);
                mapCellByCoord.emplace(CoordKey(desc.vecCells[i].iCoordX, desc.vecCells[i].iCoordZ), i);
            }
        }

        void SetCellState(
            WorldCellRuntime& cell,
            eWorldCellState eNext,
            eWorldCellTransitionReason eReason)
        {
            const eWorldCellState ePrev = cell.eState;
            if (ePrev != eNext)
            {
                ++cell.uTransitionCount;
                DebugLogCellTransition(cell.pDesc, ePrev, eNext, eReason);
            }

            cell.eState = eNext;
            cell.eLastReason = eReason;
        }

        void ReleaseHandles(WorldCellRuntime& cell)
        {
            if (pStreaming)
            {
                for (AssetHandle hAsset : cell.vecAssetHandles)
                    pStreaming->Release(hAsset);
            }

            cell.vecAssetHandles.clear();
            cell.vecInstanceMeshHandles.clear();
            cell.vecInstanceMaterialHandles.clear();
            cell.bHandlesRequested = false;
            cell.bRequiredReady = false;
            cell.uMissingRequiredAssets = 0u;
            cell.uMissingOptionalAssets = 0u;
        }

        void EnsureHandles(WorldCellRuntime& cell)
        {
            if (cell.bHandlesRequested || !cell.pDesc)
                return;

            const size_t iInstanceCount = cell.pDesc->vecInstances.size();
            cell.vecInstanceMeshHandles.assign(iInstanceCount, kInvalidAssetHandle);
            cell.vecInstanceMaterialHandles.assign(iInstanceCount, kInvalidAssetHandle);
            cell.uMissingRequiredAssets = 0u;
            cell.uMissingOptionalAssets = 0u;

            for (size_t i = 0u; i < iInstanceCount; ++i)
            {
                const CellInstanceDesc& inst = cell.pDesc->vecInstances[i];
                if (!inst.bPlaceable)
                    continue;

                if (inst.strWmesh.empty())
                {
                    if (inst.bRequired)
                        ++cell.uMissingRequiredAssets;
                    else
                        ++cell.uMissingOptionalAssets;

                    DebugLogMissingAsset(*cell.pDesc, inst, "wmesh");
                    continue;
                }

                if (pStreaming)
                {
                    AssetLoadRequest req{};
                    req.strPath = inst.strWmesh;
                    req.eKind = eAssetKind::Mesh;
                    req.bReadyImmediately = true;
                    const AssetHandle hMesh = pStreaming->Request(req);
                    cell.vecInstanceMeshHandles[i] = hMesh;
                    if (hMesh != kInvalidAssetHandle)
                    {
                        cell.vecAssetHandles.push_back(hMesh);
                    }
                    else if (inst.bRequired)
                    {
                        ++cell.uMissingRequiredAssets;
                        DebugLogMissingAsset(*cell.pDesc, inst, "mesh handle");
                    }
                    else
                    {
                        ++cell.uMissingOptionalAssets;
                        DebugLogMissingAsset(*cell.pDesc, inst, "mesh handle");
                    }
                }

                if (!inst.strWmat.empty() && pStreaming)
                {
                    AssetLoadRequest req{};
                    req.strPath = inst.strWmat;
                    req.eKind = eAssetKind::Material;
                    req.bReadyImmediately = true;
                    const AssetHandle hMaterial = pStreaming->Request(req);
                    cell.vecInstanceMaterialHandles[i] = hMaterial;
                    if (hMaterial != kInvalidAssetHandle)
                    {
                        cell.vecAssetHandles.push_back(hMaterial);
                    }
                    else if (inst.bRequired)
                    {
                        ++cell.uMissingRequiredAssets;
                        DebugLogMissingAsset(*cell.pDesc, inst, "material handle");
                    }
                    else
                    {
                        ++cell.uMissingOptionalAssets;
                        DebugLogMissingAsset(*cell.pDesc, inst, "material handle");
                    }
                }
            }

            cell.bHandlesRequested = true;
        }

        bool_t AreRequiredAssetsReady(WorldCellRuntime& cell)
        {
            if (!pStreaming || !cell.pDesc)
            {
                cell.bRequiredReady = true;
                return true;
            }

            if (cell.uMissingRequiredAssets > 0u)
            {
                cell.bRequiredReady = false;
                return false;
            }

            for (size_t i = 0u; i < cell.pDesc->vecInstances.size(); ++i)
            {
                const CellInstanceDesc& inst = cell.pDesc->vecInstances[i];
                if (!inst.bPlaceable || !inst.bRequired)
                    continue;

                const AssetHandle hMesh = i < cell.vecInstanceMeshHandles.size()
                    ? cell.vecInstanceMeshHandles[i]
                    : kInvalidAssetHandle;
                if (hMesh == kInvalidAssetHandle || !pStreaming->IsReady(hMesh))
                {
                    cell.bRequiredReady = false;
                    return false;
                }
            }

            cell.bRequiredReady = true;
            return true;
        }

        DesiredCellState ResolveDesiredState(const CellDescriptor& cell) const
        {
            DesiredCellState desired{};
            const Vec3 vCenter = GetCellCenter(cell, desc);
            bool_t bSawActiveSource = false;
            u32_t uBestRank = 0u;
            u32_t uBestPriority = 0u;
            f32_t fBestDistanceSq = 0.f;

            for (const auto& pair : mapSources)
            {
                const StreamingSourceComponent& src = pair.second;
                if (!src.bActive)
                    continue;

                bSawActiveSource = true;
                const f32_t fDx = vCenter.x - src.vPosition.x;
                const f32_t fDz = vCenter.z - src.vPosition.z;
                const f32_t fDistanceSq = fDx * fDx + fDz * fDz;

                const f32_t fVisibleSq = src.fVisibleRadius * src.fVisibleRadius;
                const f32_t fLoadSq = src.fLoadRadius * src.fLoadRadius;
                const f32_t fUnloadSq = src.fUnloadRadius * src.fUnloadRadius;

                DesiredCellState candidate{};
                u32_t uRank = 0u;
                if (fDistanceSq <= fVisibleSq)
                {
                    candidate.eState = eWorldCellState::Visible;
                    candidate.eReason = eWorldCellTransitionReason::WithinVisibleRadius;
                    uRank = 3u;
                }
                else if (fDistanceSq <= fLoadSq)
                {
                    candidate.eState = eWorldCellState::LoadedHidden;
                    candidate.eReason = eWorldCellTransitionReason::WithinLoadRadius;
                    uRank = 2u;
                }
                else if (fDistanceSq <= fUnloadSq)
                {
                    candidate.eState = eWorldCellState::Queued;
                    candidate.eReason = eWorldCellTransitionReason::WithinUnloadRadius;
                    uRank = 1u;
                }
                else
                {
                    candidate.eState = eWorldCellState::Unloaded;
                    candidate.eReason = eWorldCellTransitionReason::OutsideUnloadRadius;
                }

                const bool_t bBetter =
                    uRank > uBestRank ||
                    (uRank == uBestRank && uRank > 0u && src.uPriority > uBestPriority) ||
                    (uRank == uBestRank && uRank > 0u && src.uPriority == uBestPriority &&
                        (uBestRank == 0u || fDistanceSq < fBestDistanceSq));

                if (bBetter)
                {
                    desired = candidate;
                    uBestRank = uRank;
                    uBestPriority = src.uPriority;
                    fBestDistanceSq = fDistanceSq;
                }
            }

            if (!bSawActiveSource)
                desired.eReason = eWorldCellTransitionReason::NoActiveSource;
            else if (uBestRank == 0u)
                desired.eReason = eWorldCellTransitionReason::OutsideUnloadRadius;

            return desired;
        }
    };

    CWorldPartitionSystem::CWorldPartitionSystem() = default;

    CWorldPartitionSystem::~CWorldPartitionSystem()
    {
        Unload();
        delete m_pImpl;
        m_pImpl = nullptr;
    }

    std::unique_ptr<CWorldPartitionSystem> CWorldPartitionSystem::Create(CAssetStreamingSystem* pStreaming)
    {
        auto pInstance = std::unique_ptr<CWorldPartitionSystem>(new CWorldPartitionSystem());
        if (!pInstance || !pInstance->Initialize(pStreaming))
            return nullptr;

        return pInstance;
    }

    bool_t CWorldPartitionSystem::Initialize(CAssetStreamingSystem* pStreaming)
    {
        m_pImpl = new Impl();
        if (!m_pImpl)
            return false;

        m_pImpl->pStreaming = pStreaming;
        return true;
    }

    bool_t CWorldPartitionSystem::LoadWorld(const std::string& strWorldJsonPath)
    {
        if (!m_pImpl || strWorldJsonPath.empty())
            return false;

        JsonValue root;
        const std::filesystem::path worldPath(strWorldJsonPath);
        if (!ParseJsonFile(worldPath, root) || root.eType != eJsonType::Object)
            return false;

        WorldDescriptor desc{};
        desc.strSourcePath = strWorldJsonPath;
        desc.strName = GetString(root, "name");
        desc.fCellSizeMeters = GetFloat(root, "cellSizeMeters", desc.fCellSizeMeters);
        TryGetVec3(root, "origin", desc.vOrigin);

        if (const JsonValue* pGridDim = root.Find("gridDim"))
        {
            if (pGridDim->eType == eJsonType::Object)
            {
                desc.iGridDimX = GetI32(*pGridDim, "x", desc.iGridDimX);
                desc.iGridDimZ = GetI32(*pGridDim, "z", desc.iGridDimZ);
            }
        }

        if (const JsonValue* pTileBase = root.Find("tileBase"))
        {
            if (pTileBase->eType == eJsonType::Object)
            {
                desc.iTileBaseX = GetI32(*pTileBase, "x", desc.iTileBaseX);
                desc.iTileBaseZ = GetI32(*pTileBase, "z", desc.iTileBaseZ);
            }
        }

        const JsonValue* pCells = root.Find("cells");
        if (!pCells || pCells->eType != eJsonType::Array)
            return false;

        const std::filesystem::path baseDir = worldPath.has_parent_path()
            ? worldPath.parent_path()
            : std::filesystem::current_path();

        for (const JsonValue& value : pCells->vecArray)
        {
            if (value.eType != eJsonType::Object)
                continue;

            CellDescriptor cell{};
            if (!ParseCellHeader(value, cell))
                return false;

            if (!cell.strFile.empty())
            {
                const std::filesystem::path cellPath = baseDir / std::filesystem::path(cell.strFile);
                if (std::filesystem::exists(cellPath) && !ParseCellFile(cellPath, cell))
                    return false;
            }

            desc.iGridDimX = (std::max)(desc.iGridDimX, cell.iCoordX + 1);
            desc.iGridDimZ = (std::max)(desc.iGridDimZ, cell.iCoordZ + 1);
            desc.vecCells.push_back(std::move(cell));
        }

        if (desc.vecCells.empty() || desc.fCellSizeMeters <= 0.f)
            return false;

        Unload();
        m_pImpl->desc = std::move(desc);
        m_pImpl->BuildRuntime();
        return true;
    }

    void CWorldPartitionSystem::Unload()
    {
        if (!m_pImpl)
            return;

        for (WorldCellRuntime& cell : m_pImpl->vecRuntime)
            m_pImpl->ReleaseHandles(cell);

        m_pImpl->vecRuntime.clear();
        m_pImpl->mapCellById.clear();
        m_pImpl->mapCellByCoord.clear();
        m_pImpl->mapSources.clear();
        m_pImpl->desc = WorldDescriptor{};
    }

    void CWorldPartitionSystem::SetSource(u32_t uSourceId, const StreamingSourceComponent& src)
    {
        if (!m_pImpl)
            return;

        StreamingSourceComponent sanitized = src;
        if (!IsFiniteVec3(sanitized.vPosition))
            sanitized.bActive = false;

        sanitized.fVisibleRadius = (std::max)(0.f, sanitized.fVisibleRadius);
        sanitized.fLoadRadius = (std::max)(sanitized.fVisibleRadius, sanitized.fLoadRadius);
        sanitized.fUnloadRadius = (std::max)(sanitized.fLoadRadius, sanitized.fUnloadRadius);
        m_pImpl->mapSources[uSourceId] = sanitized;
    }

    void CWorldPartitionSystem::RemoveSource(u32_t uSourceId)
    {
        if (!m_pImpl)
            return;

        m_pImpl->mapSources.erase(uSourceId);
    }

    bool_t CWorldPartitionSystem::WorldToCellId(const Vec3& vWorld, std::string& outCellId) const
    {
        outCellId.clear();
        if (!m_pImpl || m_pImpl->desc.fCellSizeMeters <= 0.f)
            return false;

        if (!IsFiniteVec3(vWorld))
            return false;

        const f32_t fLocalX = vWorld.x - m_pImpl->desc.vOrigin.x;
        const f32_t fLocalZ = vWorld.z - m_pImpl->desc.vOrigin.z;
        const i32_t iCoordX = static_cast<i32_t>(std::floor(fLocalX / m_pImpl->desc.fCellSizeMeters));
        const i32_t iCoordZ = static_cast<i32_t>(std::floor(fLocalZ / m_pImpl->desc.fCellSizeMeters));

        const auto found = m_pImpl->mapCellByCoord.find(CoordKey(iCoordX, iCoordZ));
        if (found == m_pImpl->mapCellByCoord.end() || found->second >= m_pImpl->desc.vecCells.size())
            return false;

        outCellId = m_pImpl->desc.vecCells[found->second].strId;
        return !outCellId.empty();
    }

    void CWorldPartitionSystem::Update(f32_t)
    {
        if (!m_pImpl)
            return;

        for (WorldCellRuntime& cell : m_pImpl->vecRuntime)
        {
            if (!cell.pDesc)
                continue;

            const Impl::DesiredCellState desired = m_pImpl->ResolveDesiredState(*cell.pDesc);
            cell.eDesiredState = desired.eState;

            if (desired.eState == eWorldCellState::Unloaded)
            {
                if (cell.eState != eWorldCellState::Unloaded)
                {
                    m_pImpl->ReleaseHandles(cell);
                    m_pImpl->SetCellState(cell, eWorldCellState::Unloaded,
                        eWorldCellTransitionReason::Released);
                }
                else
                {
                    cell.eLastReason = desired.eReason;
                }
                continue;
            }

            if (cell.eState == eWorldCellState::Unloaded)
            {
                m_pImpl->EnsureHandles(cell);
                m_pImpl->SetCellState(cell, eWorldCellState::Queued,
                    eWorldCellTransitionReason::AssetsRequested);
                continue;
            }

            m_pImpl->EnsureHandles(cell);

            if (cell.eState == eWorldCellState::Queued)
            {
                if (!m_pImpl->AreRequiredAssetsReady(cell))
                {
                    m_pImpl->SetCellState(cell, eWorldCellState::Queued,
                        cell.uMissingRequiredAssets > 0u
                        ? eWorldCellTransitionReason::MissingRequiredAsset
                        : eWorldCellTransitionReason::WaitingForAssets);
                    continue;
                }

                m_pImpl->SetCellState(cell, eWorldCellState::LoadedHidden,
                    eWorldCellTransitionReason::RequiredAssetsReady);
                continue;
            }

            if (cell.eState == eWorldCellState::LoadedHidden)
            {
                if (desired.eState == eWorldCellState::Visible)
                {
                    m_pImpl->SetCellState(cell, eWorldCellState::Visible, desired.eReason);
                }
                else if (desired.eState == eWorldCellState::Queued)
                {
                    m_pImpl->SetCellState(cell, eWorldCellState::Queued, desired.eReason);
                }
                else
                {
                    cell.eLastReason = desired.eReason;
                }
                continue;
            }

            if (cell.eState == eWorldCellState::Visible)
            {
                if (desired.eState == eWorldCellState::Visible)
                {
                    cell.eLastReason = desired.eReason;
                }
                else if (desired.eState == eWorldCellState::LoadedHidden ||
                    desired.eState == eWorldCellState::Queued)
                {
                    m_pImpl->SetCellState(cell, eWorldCellState::LoadedHidden, desired.eReason);
                }
            }
        }
    }

    void CWorldPartitionSystem::CollectVisibleInstances(std::vector<VisibleInstance>& out) const
    {
        if (!m_pImpl)
            return;

        for (const WorldCellRuntime& cell : m_pImpl->vecRuntime)
        {
            if (!cell.pDesc || cell.eState != eWorldCellState::Visible)
                continue;

            for (const CellInstanceDesc& inst : cell.pDesc->vecInstances)
            {
                if (!inst.bPlaceable)
                    continue;

                if (inst.strWmesh.empty())
                    continue;

                const size_t iInstanceIndex = static_cast<size_t>(&inst - cell.pDesc->vecInstances.data());
                const AssetHandle hMesh = iInstanceIndex < cell.vecInstanceMeshHandles.size()
                    ? cell.vecInstanceMeshHandles[iInstanceIndex]
                    : kInvalidAssetHandle;
                if (m_pImpl->pStreaming && (hMesh == kInvalidAssetHandle || !m_pImpl->pStreaming->IsReady(hMesh)))
                    continue;

                VisibleInstance visible{};
                visible.pCell = cell.pDesc;
                visible.pInstance = &inst;
                visible.matWorld = BuildWorldMatrix(inst);
                out.push_back(visible);
            }
        }
    }

    CellStateCounts CWorldPartitionSystem::GetStateCounts() const
    {
        CellStateCounts counts{};
        if (!m_pImpl)
            return counts;

        for (const WorldCellRuntime& cell : m_pImpl->vecRuntime)
        {
            switch (cell.eState)
            {
            case eWorldCellState::Unloaded:
                ++counts.uUnloaded;
                break;
            case eWorldCellState::Queued:
                ++counts.uQueued;
                break;
            case eWorldCellState::LoadedHidden:
                ++counts.uLoadedHidden;
                break;
            case eWorldCellState::Visible:
                ++counts.uVisible;
                break;
            default:
                break;
            }
        }

        return counts;
    }

    WorldPartitionDebugStats CWorldPartitionSystem::GetDebugStats() const
    {
        WorldPartitionDebugStats stats{};
        if (!m_pImpl)
            return stats;

        stats.stateCounts = GetStateCounts();
        stats.uCellCount = static_cast<u32_t>(m_pImpl->vecRuntime.size());
        stats.uSourceCount = static_cast<u32_t>(m_pImpl->mapSources.size());

        for (const WorldCellRuntime& cell : m_pImpl->vecRuntime)
        {
            stats.uTotalTransitions += cell.uTransitionCount;
            stats.uMissingRequiredAssets += cell.uMissingRequiredAssets;
            stats.uMissingOptionalAssets += cell.uMissingOptionalAssets;

            if (!cell.pDesc || cell.eState != eWorldCellState::Visible)
                continue;

            for (size_t i = 0u; i < cell.pDesc->vecInstances.size(); ++i)
            {
                const CellInstanceDesc& inst = cell.pDesc->vecInstances[i];
                if (!inst.bPlaceable)
                {
                    ++stats.uSkippedNotPlaceableInstances;
                    continue;
                }

                if (inst.strWmesh.empty())
                {
                    ++stats.uSkippedMissingAssetInstances;
                    continue;
                }

                const AssetHandle hMesh = i < cell.vecInstanceMeshHandles.size()
                    ? cell.vecInstanceMeshHandles[i]
                    : kInvalidAssetHandle;
                if (m_pImpl->pStreaming &&
                    (hMesh == kInvalidAssetHandle || !m_pImpl->pStreaming->IsReady(hMesh)))
                {
                    ++stats.uSkippedNotReadyInstances;
                    continue;
                }

                ++stats.uVisibleInstances;
            }
        }

        return stats;
    }

    const WorldDescriptor& CWorldPartitionSystem::GetDescriptor() const
    {
        static const WorldDescriptor sEmpty{};
        return m_pImpl ? m_pImpl->desc : sEmpty;
    }

    const std::vector<WorldCellRuntime>& CWorldPartitionSystem::GetCells() const
    {
        static const std::vector<WorldCellRuntime> sEmpty{};
        return m_pImpl ? m_pImpl->vecRuntime : sEmpty;
    }
}
