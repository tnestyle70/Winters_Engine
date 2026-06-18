#pragma once
#include "WintersAPI.h"
#include "Engine_Defines.h"
#include "RHI/IRHIDevice.h"

NS_BEGIN(Engine)

// 샘플러 주소 지정 모드. UI/Decal 은 CLAMP, 월드 타일링 텍스쳐는 WRAP.
enum class eTexSamplerMode : uint8_t { Wrap, Clamp };
enum class eTexColorSpace : uint8_t
{
	Auto,
	ShaderLocalSRGB,
	IgnoreSRGB
};

// [DLL 경계 결정] Client(Scene) 가 CTexture::Create() 를 직접 호출해 2D/UI 텍스쳐를 로드하므로
// WINTERS_ENGINE export 유지. Mesh/Skeleton/Animation 은 Model/ResourceCache 경유라 export 불필요.
// 향후 CGameInstance::LoadTexture2D() 포워딩으로 이관 시 export 제거 가능.
class WINTERS_ENGINE CTexture
{
private:
	CTexture() = default;
public:
	~CTexture();

	void Bind(IRHIDevice* pDevice, u32_t iSlot = 0);
	void* GetNativeSRV() const { return m_pSRV; }

	static unique_ptr<CTexture> Create(IRHIDevice* pDevice, const wstring& strFilePath,
		eTexSamplerMode eMode = eTexSamplerMode::Wrap,
		eTexColorSpace eColorSpace = eTexColorSpace::Auto);
	static unique_ptr<CTexture> CreateFromMemory(IRHIDevice* pDevice, const void* pData,
		size_t dataSize,
		eTexSamplerMode eMode = eTexSamplerMode::Wrap);
	static unique_ptr<CTexture> CreateDefault(IRHIDevice* pDevice);

private:
	void EnsureMipsOnBind(IRHIDevice* pDevice);

	void* m_pSRV = nullptr;
	void* m_pSampler = nullptr;
	// 로더 스레드에서는 immediate context를 쓸 수 없어 mip 생성을 첫 Bind(렌더 스레드)로 미룬다.
	bool_t m_bMipsPending = false;
};

NS_END
