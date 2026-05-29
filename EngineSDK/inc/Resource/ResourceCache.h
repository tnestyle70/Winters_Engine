#pragma once
#include "Engine_Defines.h"
#include "RHI/IRHIDevice.h"
#include "Resource/Texture.h"
#include "Resource/Model.h"
#include <mutex>

NS_BEGIN(Engine)

class CResourceCache
{
public:
	CResourceCache() = default;
	~CResourceCache() = default;

	void Initialize(IRHIDevice* pDevice);

	//텍스쳐 로드 같은 경로일 경우 동일 포인터 반환!
	CTexture* LoadTexture(const wstring& strPath,
		eTexColorSpace eColorSpace = eTexColorSpace::Auto);
    //같은 경로일 경우 shared_ptr 반환!
    shared_ptr<CModel> LoadModel(IRHIDevice* pDevice, const string& strPath);

    // 모든 캐시 해제 (OnShutdown 후, Device.Destroy 전에 호출)
    void Clear();

    u32_t GetCachedTextureCount() const;
    u32_t GetCachedModelCount() const;

private:
    // 경로 정규화: 슬래시 통일 + 소문자
    static wstring NormalizePath(const wstring& strPath);
    static string NormalizeModelPath(const string& strPath);
    static string ToCookedModelPath(const string& strPath);

    IRHIDevice* m_pDevice = nullptr;
    mutable std::mutex m_Mutex;
    unordered_map<wstring, unique_ptr<CTexture>> m_mapTextures;
    //이것도 CName 해싱으로 string 전부 교체하기!!! - CDPR 방식
    unordered_map<string, shared_ptr<CModel>> m_mapModels;
};

NS_END
