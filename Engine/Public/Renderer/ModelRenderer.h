#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Components/MeshGroupVisibilityComponent.h"
#include <string>
#include <memory>

// [Phase T] CAnimator 전방선언 — GetAnimator() 반환 타입용
namespace Engine { class CAnimator; }
class DX11Shader;
class DX11Pipeline;

enum class eModelMaterialOverrideMode : u32_t
{
	None = 0,
	LitTint = 1,
	UnlitColor = 2,
};

//이거도 GameInstance에 안 넣고 그냥 공개하는 방식으로 진행?
class WINTERS_ENGINE ModelRenderer
{
public:
	ModelRenderer();
	~ModelRenderer();

	bool	Initialize(const std::string& strFbxPath,
		const wchar_t* pHlslPath = L"Shaders/Mesh3D.hlsl");
	void	UpdateTransform(const Mat4& matWorld);
	void	SetYawTraceContext(u64_t snapshotTick,
		u32_t entity,
		u32_t champion,
		u32_t commandSeq,
		f32_t expectedYaw,
		const Vec3& expectedForward);
	void	ClearYawTraceContext();
	void	UpdateCamera(const Mat4& matViewProj);
	void	UpdateCamera(const Mat4& matViewProj, const Vec3& vCameraWorld);
	void	Render();
	void	RenderWithVisibility(const VisibilityMask& mask);
	void	RenderNormalPass(DX11Shader* pMeshShader,
		DX11Pipeline* pMeshPipeline,
		DX11Shader* pSkinnedShader,
		DX11Pipeline* pSkinnedPipeline);
	void	RenderNormalPassWithVisibility(DX11Shader* pMeshShader,
		DX11Pipeline* pMeshPipeline,
		DX11Shader* pSkinnedShader,
		DX11Pipeline* pSkinnedPipeline,
		const VisibilityMask& mask);
	void	Shutdown();

	//텍스쳐 로드
	bool LoadMeshTexture(uint32_t iMeshIndex, const wstring& strPath);
	void LoadTextureForAllMeshes(const std::wstring& strPath);
	uint32 GetMeshCount() const;

	// 텍스처 수동 로드 (FBX에 텍스처 경로 없을 때)
	bool	LoadTexture(const std::wstring& strPath);

	//애니메이션
	void Update(f32_t fDeltaTime);
	void PlayAnimation(uint32 iIndex);
	void PlayAnimationByName(const std::string& strKeyword);
	void PlayAnimationByName(const std::string& strKeyword, bool bLoop);
	bool PlayAnimationByNameAdvanced(const std::string& strKeyword,
		bool bLoop,
		bool_t bReverse,
		f32_t fPlaySpeed = 1.f);
	bool HasAnimationByName(const std::string& strKeyword) const;
	f32_t GetAnimationDurationSecondsByName(const std::string& strKeyword) const;
	bool HasSkeleton() const;
	bool UsesPBR() const;
	void SetAmbientOcclusionSRV(void* pNativeSRV);
	void SetMaterialOverrideColor(const Vec4& color, bool_t bEnabled);
	void ClearMaterialOverrideColor();
	void SetHoverOutline(const Vec4& color, f32_t fIntensity = 1.f);
	void ClearHoverOutline();

	uint32	GetAnimationCount() const;

	// [Phase T] 프레임 이벤트 감지용 — 활성 Animator 접근 (null 가능)
	const Engine::CAnimator* GetAnimator() const;
	Engine::CAnimator* GetAnimator(); //Tuner용

	//Local AABB - Picking Editor
	bool HasValidAABB() const;
	Vec3 GetLocalAABBMin() const;
	Vec3 GetLocalAABBMax() const;

private:
	struct Impl;
	Impl* m_pImpl = nullptr;
};
