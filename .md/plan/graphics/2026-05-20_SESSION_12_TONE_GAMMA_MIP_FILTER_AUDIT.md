Session - DX11 tone gamma mip filtering audit

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Engine/Public/Resource/Texture.h

기존 코드:

```cpp
enum class eTexSamplerMode : uint8_t { Wrap, Clamp };
enum class eTexColorSpace : uint8_t { Auto, IgnoreSRGB };
```

아래로 교체:

```cpp
enum class eTexSamplerMode : uint8_t { Wrap, Clamp };
enum class eTexColorSpace : uint8_t
{
    Auto,
    ShaderLocalSRGB,
    IgnoreSRGB
};
```

1-2. C:/Users/user/Desktop/Winters/Engine/Private/Resource/Texture.cpp

기존 코드:

```cpp
    HRESULT hr = E_FAIL;
    const bool_t bIgnoreSRGB = (eColorSpace == eTexColorSpace::IgnoreSRGB);
```

아래로 교체:

```cpp
    HRESULT hr = E_FAIL;
    const bool_t bIgnoreSRGB =
        eColorSpace == eTexColorSpace::ShaderLocalSRGB ||
        eColorSpace == eTexColorSpace::IgnoreSRGB;
```

1-3. C:/Users/user/Desktop/Winters/Engine/Public/Resource/ResourceCache.h

기존 코드:

```cpp
	CTexture* LoadTexture(const wstring& strPath);
```

아래로 교체:

```cpp
	CTexture* LoadTexture(const wstring& strPath,
        eTexColorSpace eColorSpace = eTexColorSpace::Auto);
```

1-4. C:/Users/user/Desktop/Winters/Engine/Private/Resource/ResourceCache.cpp

기존 코드:

```cpp
CTexture* CResourceCache::LoadTexture(const wstring& strPath)
{
    wstring strKey = NormalizePath(strPath);

    auto it = m_mapTextures.find(strKey);
```

아래로 교체:

```cpp
CTexture* CResourceCache::LoadTexture(const wstring& strPath,
    eTexColorSpace eColorSpace)
{
    wstring strKey = NormalizePath(strPath);
    if (eColorSpace == eTexColorSpace::ShaderLocalSRGB ||
        eColorSpace == eTexColorSpace::IgnoreSRGB)
    {
        strKey += L"|ignore-srgb";
    }

    auto it = m_mapTextures.find(strKey);
```

기존 코드:

```cpp
    auto pTexture = CTexture::Create(m_pDevice, strPath);  // 원본 경로로 로드
```

아래로 교체:

```cpp
    auto pTexture = CTexture::Create(
        m_pDevice,
        strPath,
        eTexSamplerMode::Wrap,
        eColorSpace);
```

1-5. C:/Users/user/Desktop/Winters/Engine/Private/Resource/Model.cpp

기존 코드:

```cpp
		m_vecTextures[entry.material_index] = CTexture::Create(pDevice, entry.diffuse_path);
```

아래로 교체:

```cpp
		m_vecTextures[entry.material_index] = CTexture::Create(
			pDevice,
			entry.diffuse_path,
			eTexSamplerMode::Wrap,
			eTexColorSpace::ShaderLocalSRGB);
```

1-6. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/ModelRenderer.cpp

기존 코드:

```cpp
    m_pImpl->pManualTexture = CTexture::Create(pDevice, strPath);
```

아래로 교체:

```cpp
    m_pImpl->pManualTexture = CTexture::Create(
        pDevice,
        strPath,
        eTexSamplerMode::Wrap,
        eTexColorSpace::ShaderLocalSRGB);
```

기존 코드:

```cpp
    CTexture* pTex = CEngineApp::Get().GetResourceCache().LoadTexture(strPath);
```

아래로 교체:

```cpp
    CTexture* pTex = CEngineApp::Get().GetResourceCache().LoadTexture(
        strPath,
        eTexColorSpace::ShaderLocalSRGB);
```

2. 검증

1차 코드 반영 및 빌드 검증 완료.

- `git diff --check -- Engine/Public/Resource/Texture.h Engine/Private/Resource/Texture.cpp Engine/Public/Resource/ResourceCache.h Engine/Private/Resource/ResourceCache.cpp Engine/Private/Resource/Model.cpp Engine/Private/Renderer/ModelRenderer.cpp`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m` 통과, warning 8 / error 0
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m` 통과, warning 17 / error 0
- Public header 변경으로 `UpdateLib.bat`는 Engine/Client 빌드 과정에서 실행됐다.

수동 확인 필요:

- F5에서 챔피언/맵 diffuse가 과하게 파래지거나 두 번 감마 보정된 것처럼 뜨면 실패다.
- UI, HP bar, Attack Range, FX는 이 세션에서 색감이 바뀌면 실패다.
