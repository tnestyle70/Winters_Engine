Session - AssetDatabase를 CEngineApp과 CResourceCache에 연결해 virtual path 로딩을 연다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Engine/Public/Resource/ResourceCache.h

기존 코드:

```cpp
NS_BEGIN(Engine)

class CResourceCache
```

아래로 교체:

```cpp
NS_BEGIN(Engine)

class CAssetDatabase;

class CResourceCache
```

기존 코드:

```cpp
	void Initialize(IRHIDevice* pDevice);

	//텍스쳐 로드 같은 경로일 경우 동일 포인터 반환!
	CTexture* LoadTexture(const wstring& strPath,
		eTexColorSpace eColorSpace = eTexColorSpace::Auto);
    //같은 경로일 경우 shared_ptr 반환!
    shared_ptr<CModel> LoadModel(IRHIDevice* pDevice, const string& strPath);
```

아래로 교체:

```cpp
	void Initialize(IRHIDevice* pDevice);
    void SetAssetDatabase(const CAssetDatabase* pDatabase);

	//텍스쳐 로드 같은 경로일 경우 동일 포인터 반환!
	CTexture* LoadTexture(const wstring& strPath,
		eTexColorSpace eColorSpace = eTexColorSpace::Auto);
    //같은 경로일 경우 shared_ptr 반환!
    shared_ptr<CModel> LoadModel(IRHIDevice* pDevice, const string& strPath);
    shared_ptr<CModel> LoadModelAsset(IRHIDevice* pDevice, const string& strVirtualPath);
    void InvalidateTexture(const wstring& strPath,
        eTexColorSpace eColorSpace = eTexColorSpace::Auto);
    void InvalidateModel(const string& strPath);
```

기존 코드:

```cpp
    IRHIDevice* m_pDevice = nullptr;
    // TODO: JobSystem 도입 시 std::mutex 추가
    unordered_map<wstring, unique_ptr<CTexture>> m_mapTextures;
```

아래로 교체:

```cpp
    IRHIDevice* m_pDevice = nullptr;
    const CAssetDatabase* m_pAssetDatabase = nullptr;
    // TODO: JobSystem 도입 시 std::mutex 추가
    unordered_map<wstring, unique_ptr<CTexture>> m_mapTextures;
```

1-2. C:/Users/user/Desktop/Winters/Engine/Private/Resource/ResourceCache.cpp

기존 코드:

```cpp
#include "Resource/ResourceCache.h"
#include "Resource/Texture.h"
```

아래로 교체:

```cpp
#include "Resource/ResourceCache.h"
#include "Asset/AssetDatabase.h"
#include "Resource/Texture.h"
```

기존 코드:

```cpp
void CResourceCache::Initialize(IRHIDevice* pDevice)
{
    m_pDevice = pDevice;
}

CTexture* CResourceCache::LoadTexture(const wstring& strPath,
```

아래로 교체:

```cpp
void CResourceCache::Initialize(IRHIDevice* pDevice)
{
    m_pDevice = pDevice;
}

void CResourceCache::SetAssetDatabase(const CAssetDatabase* pDatabase)
{
    m_pAssetDatabase = pDatabase;
}

CTexture* CResourceCache::LoadTexture(const wstring& strPath,
```

기존 코드:

```cpp
    return pRaw;
}

shared_ptr<CModel> CResourceCache::LoadModel(IRHIDevice* pDevice, const string& strPath)
```

아래로 교체:

```cpp
    return pRaw;
}

shared_ptr<CModel> CResourceCache::LoadModelAsset(IRHIDevice* pDevice, const string& strVirtualPath)
{
    if (!m_pAssetDatabase)
        return LoadModel(pDevice, strVirtualPath);

    const AssetRecord* pRecord = m_pAssetDatabase->FindByVirtualPath(strVirtualPath);
    if (!pRecord || pRecord->strCookedPath.empty())
        return LoadModel(pDevice, strVirtualPath);

    const string strCookedPath(pRecord->strCookedPath.begin(), pRecord->strCookedPath.end());
    return LoadModel(pDevice, strCookedPath);
}

shared_ptr<CModel> CResourceCache::LoadModel(IRHIDevice* pDevice, const string& strPath)
```

기존 코드:

```cpp
void CResourceCache::Clear()
{
    OutputDebugStringA(("[CResourceCache] Clearing "
        + to_string(m_mapTextures.size()) + " textures + "
        + to_string(m_mapModels.size()) + " models\n").c_str());
    m_mapTextures.clear();
    m_mapModels.clear();
}
```

아래에 추가:

```cpp
void CResourceCache::InvalidateTexture(const wstring& strPath, eTexColorSpace eColorSpace)
{
    wstring strKey = NormalizePath(strPath);
    if (eColorSpace == eTexColorSpace::ShaderLocalSRGB ||
        eColorSpace == eTexColorSpace::IgnoreSRGB)
    {
        strKey += L"|ignore-srgb";
    }
    m_mapTextures.erase(strKey);
}

void CResourceCache::InvalidateModel(const string& strPath)
{
    const string strCookedPath = ToCookedModelPath(strPath);
    const string strKey = NormalizeModelPath(strCookedPath);
    m_mapModels.erase(strKey);
}
```

1-3. C:/Users/user/Desktop/Winters/Engine/Public/Framework/CEngineApp.h

기존 코드:

```cpp
#include "Resource/ResourceCache.h"
#include "Editor/ImGuiLayer.h"
```

아래로 교체:

```cpp
#include "Asset/AssetDatabase.h"
#include "Resource/ResourceCache.h"
#include "Editor/ImGuiLayer.h"
```

기존 코드:

```cpp
    CWin32Window& GetWindow() { return m_Window; }
    CResourceCache& GetResourceCache() { return m_ResourceCache; }
```

아래로 교체:

```cpp
    CWin32Window& GetWindow() { return m_Window; }
    CResourceCache& GetResourceCache() { return m_ResourceCache; }
    const CAssetDatabase& GetAssetDatabase() const { return m_AssetDatabase; }
```

기존 코드:

```cpp
    CResourceCache m_ResourceCache{};

    unique_ptr<DX11Shader> m_pMeshShader = { nullptr };
```

아래로 교체:

```cpp
    CAssetDatabase m_AssetDatabase{};
    CResourceCache m_ResourceCache{};

    unique_ptr<DX11Shader> m_pMeshShader = { nullptr };
```

1-4. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

기존 코드:

```cpp
    m_ResourceCache.Initialize(m_pDevice.get());



    if (!InitializeSharedShaders())
```

아래로 교체:

```cpp
    m_AssetDatabase.ScanManifestDirectory(L"Data");
    m_AssetDatabase.ScanManifestDirectory(L"Client/Bin/Resource");
    m_ResourceCache.Initialize(m_pDevice.get());
    m_ResourceCache.SetAssetDatabase(&m_AssetDatabase);



    if (!InitializeSharedShaders())
```

2. 검증

미검증:
- virtual path를 통한 `LoadModelAsset` 실제 로딩 미검증
- `.wasset` scan root가 실행 파일 working directory와 맞는지 미검증

검증 명령:
- git diff --check
- msbuild Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64
- msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64

확인 필요:
- `LoadModelAsset`에서 wide path를 narrow path로 단순 변환하는 것이 한글/공백 경로에 충분한지 확인. 필요하면 `WideCharToMultiByte` 기반 helper로 교체.
- `Data`와 `Client/Bin/Resource` scan은 디렉터리 존재 여부에 따라 조용히 실패한다. packaged 실행 위치 기준 root 정책을 확정해야 한다.

후속 동기화:
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.
