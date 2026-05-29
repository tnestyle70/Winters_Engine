#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

#include "FxBillboardComponent.h"
#include "FX/FxAsset.h"
#include <memory>
#include <unordered_map>
#include <string>
#include "RHI/IRHIDevice.h"
#include "Renderer/RHIFxSpriteRenderer.h"

class CWorld;
class CCommandBuffer;
class CPlaneRenderer;
class CBlendStateCache;
class CDynamicCamera;
class DX11Shader;
class DX11Pipeline;

namespace Engine 
{ 
	class CTexture; 
}

class CFxSystem final
{
public:
	~CFxSystem() = default;

	static std::unique_ptr<CFxSystem> Create(
		IRHIDevice* pDevice,
		DX11Shader* pShader,
		DX11Pipeline* pPipeline,
		CBlendStateCache* pBlendCache);

	// 매 프레임 tick (lifetime 감소, attachTo 추종, 만료 시 Destroy)
	void Update(CWorld& world, f32_t fTimeDelta);

	// 모든 살아있는 빌보드 렌더 (알파 블렌드)
	void Render(CWorld& world, const CDynamicCamera* pCamera);

	// 정적 스폰 헬퍼 — FxBillboardComponent 템플릿 입력 → 신규 엔티티 반환
	static EntityID Spawn(CWorld& world, const FxBillboardComponent& tmpl);
	static CFxAssetRegistry& GetAssetRegistry();
	static FxAssetHandle RegisterAsset(FxAsset asset);
	static EntityID SpawnFromAsset(CWorld& world, FxAssetHandle handle,
		const Vec3& vWorldPos, EntityID attachTo = NULL_ENTITY);
	static EntityID SpawnFromAsset(CWorld& world, const FxAsset& asset,
		const Vec3& vWorldPos, EntityID attachTo = NULL_ENTITY);
	static void DeferSpawnFromAsset(CCommandBuffer& commandBuffer,
		FxAssetHandle handle,
		const Vec3& vWorldPos,
		EntityID attachTo = NULL_ENTITY);

	// 리소스 해제 (Scene 종료 시 호출 — 캐시 텍스처 먼저 해제 후 PlaneRenderer)
	void Shutdown();
private:
	CFxSystem() = default;

	Engine::CTexture* GetOrLoadTexture(const wchar_t* wszPath);
	RHITextureHandle GetOrLoadRHITexture(const wchar_t* wszPath);

	std::unique_ptr<CPlaneRenderer> m_pPlane = { nullptr };
	std::unique_ptr<CRHIFxSpriteRenderer> m_pRHISprite = { nullptr };
	std::unordered_map<std::wstring,
		std::unique_ptr<Engine::CTexture>> m_TextureCache = {};
	std::unordered_map<std::wstring, RHITextureHandle> m_RHITextureCache = {};
	IRHIDevice* m_pDevice = { nullptr };
	CBlendStateCache* m_pBlendCache = { nullptr };
};
