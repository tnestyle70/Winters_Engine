#include "Resource/ResourceCache.h"
#include "Resource/Texture.h"
#include <algorithm>
#include <cctype>
#include <mutex>

using namespace Engine;

void CResourceCache::Initialize(IRHIDevice* pDevice)
{
    m_pDevice = pDevice;
}

CTexture* CResourceCache::LoadTexture(const wstring& strPath,
    eTexColorSpace eColorSpace)
{
    wstring strKey = NormalizePath(strPath);
    if (eColorSpace == eTexColorSpace::ShaderLocalSRGB ||
        eColorSpace == eTexColorSpace::IgnoreSRGB)
    {
        strKey += L"|ignore-srgb";
    }

    std::lock_guard<std::mutex> lock(m_Mutex);

    auto it = m_mapTextures.find(strKey);
    if (it != m_mapTextures.end())
        return it->second.get();  // 캐시 히트

    auto pTexture = CTexture::Create(
        m_pDevice,
        strPath,
        eTexSamplerMode::Wrap,
        eColorSpace);  // 원본 경로로 로드
    if (!pTexture)
    {
        OutputDebugStringA("[CResourceCache] Texture load FAILED\n");
        return nullptr;
    }

    CTexture* pRaw = pTexture.get();
    m_mapTextures[strKey] = move(pTexture);

    return pRaw;
}

shared_ptr<CModel> CResourceCache::LoadModel(IRHIDevice* pDevice, const string& strPath)
{
    const string strCookedPath = ToCookedModelPath(strPath);
    const string strKey = NormalizeModelPath(strCookedPath);

    std::lock_guard<std::mutex> lock(m_Mutex);

    auto it = m_mapModels.find(strKey);
    //Cache Hit - 이미 캐싱된 모델이 존재하는 경우
    if (it != m_mapModels.end())
    {
        return it->second;
    }
    //Cache Miss - 존재하지 않아서 unique_ptr로 create해서 반환
    auto pModel = CModel::Create(pDevice, strCookedPath);
    if (!pModel)
    {
        OutputDebugStringA(("[CResourceCache] Model load FAILED: " + strCookedPath + "\n").c_str());
        return nullptr;
    }
    shared_ptr<CModel> shared(pModel.release());
    m_mapModels[strKey] = shared;
    return shared;
}

void CResourceCache::Clear()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_mapTextures.clear();
    m_mapModels.clear();
}

u32_t CResourceCache::GetCachedTextureCount() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return static_cast<u32_t>(m_mapTextures.size());
}

u32_t CResourceCache::GetCachedModelCount() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return static_cast<u32_t>(m_mapModels.size());
}

wstring CResourceCache::NormalizePath(const wstring& strPath)
{
    wstring strNorm = strPath;
    // 백슬래시 → 슬래시 통일
    replace(strNorm.begin(), strNorm.end(), L'\\', L'/');
    // 소문자 변환
    transform(strNorm.begin(), strNorm.end(), strNorm.begin(), ::towlower);
    return strNorm;
}

string CResourceCache::NormalizeModelPath(const string& strPath)
{
    string strNorm = strPath;
    replace(strNorm.begin(), strNorm.end(), '\\', '/');
    transform(strNorm.begin(), strNorm.end(), strNorm.begin(),
        [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return strNorm;
}

string CResourceCache::ToCookedModelPath(const string& strPath)
{
    const size_t dot = strPath.find_last_of('.');
    if (dot == string::npos)
        return strPath + ".wmesh";
    return strPath.substr(0, dot) + ".wmesh";
}
