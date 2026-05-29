# Phase B-2 + B-3: 스켈레톤 + 애니메이션 구현 가이드

> **상태**: B-1 완료 (정적 메시 + 텍스처 렌더링 + row_major 클리핑 수정)
> **목표**: 이렐리아 idle 애니메이션 재생
> **최종 갱신**: 2026-04-14

---

## 현재 엔진 상태 (B-1 완료 기준)

```
이미 존재하는 파일:
  Engine/Public/Resource/Model.h          — CModel (정적 메시 전용)
  Engine/Private/Resource/Model.cpp       — Assimp 로딩 (VTXMESH만)
  Engine/Public/Resource/Mesh.h           — CMesh (VB+IB 래퍼)
  Engine/Private/Resource/Mesh.cpp        — Render, Create
  Engine/Public/Resource/CTexture.h       — CTexture
  Engine/Public/Engine_Struct.h           — VTXMESH, VTXANIM, CBBoneMatrices (이미 정의됨)
  Engine/Public/Renderer/ModelRenderer.h  — pImpl, Init/Render/UpdateCamera/UpdateTransform
  Engine/Private/Renderer/ModelRenderer.cpp
  Engine/Public/RHI/DX11/DX11Pipeline.h   — CreateSkinnedMesh() 이미 존재
  Engine/Public/RHI/DX11/DX11ConstantBuffer.h — CBPerFrame, CBPerObject
  Shaders/Mesh3D.hlsl                     — row_major matrix (수정 완료)
  Client/Private/CGameApp.cpp             — CDynamicCamera + 이렐리아 렌더링
```

---

## 구현 순서 (12 Step)

```
Step  1. CBone.h 생성                    → Engine/Public/Resource/
Step  2. CSkeleton.h + .cpp 생성          → Engine/Public/Resource/ + Private/Resource/
Step  3. CAnimation.h + .cpp 생성         → Engine/Public/Resource/ + Private/Resource/
Step  4. CAnimator.h + .cpp 생성          → Engine/Public/Resource/ + Private/Resource/
Step  5. CModel.h 수정 — CSkeleton/CAnimation 멤버, HasSkeleton(), GetSkeleton()/GetAnimator() 추가
Step  6. CModel.cpp 수정 — ConvertMatrix(), LoadSkeleton(), ExtractBoneWeights(), LoadAnimations(), ProcessMesh 분기
Step  7. Skinned3D.hlsl 생성              → Shaders/ (row_major 필수!)
Step  8. ModelRenderer.h 수정 — Update(dt), PlayAnimation(), HasSkeleton() 추가
Step  9. ModelRenderer.cpp 수정 — Impl에 skinnedShader/Pipeline/cbBones 추가, Render 분기
Step 10. CGameApp.cpp 수정 — OnUpdate에서 Update(dt), 키 1~6 애니메이션 전환
Step 11. Engine.vcxproj / filters 업데이트
Step 12. 빌드 + 테스트
```

---

## 파일별 전체 코드

### Step 1. `Engine/Public/Resource/CBone.h` (신규)

```cpp
#pragma once
#include "Engine_Defines.h"

NS_BEGIN(Engine)

struct BoneInfo
{
	string			strName;
	i32_t			iParentIndex = -1;
	DirectX::XMFLOAT4X4	matOffset;		// 본 오프셋 행렬 (Inverse Bind Pose)
};

NS_END
```

---

### Step 2. `Engine/Public/Resource/CSkeleton.h` (신규)

```cpp
#pragma once
#include "Engine_Defines.h"
#include "Resource/CBone.h"

NS_BEGIN(Engine)

class CSkeleton
{
private:
	CSkeleton() = default;
public:
	~CSkeleton() = default;
	static unique_ptr<CSkeleton> Create();

	i32_t	AddBone(const string& strName, i32_t iParentIndex, const DirectX::XMFLOAT4X4& matOffset);
	i32_t	FindBoneIndex(const string& strName) const;
	void	ComputeFinalTransforms(
				const vector<DirectX::XMFLOAT4X4>& vecLocalTransforms,
				vector<DirectX::XMFLOAT4X4>& vecOutFinal) const;

	u32_t	GetBoneCount() const { return (u32_t)m_vecBones.size(); }
	const BoneInfo& GetBone(u32_t iIndex) const { return m_vecBones[iIndex]; }

private:
	vector<BoneInfo>				m_vecBones;
	unordered_map<string, i32_t>	m_mapBoneIndex;
};

NS_END
```

### `Engine/Private/Resource/CSkeleton.cpp` (신규)

```cpp
#include "Resource/CSkeleton.h"

using namespace Engine;
using namespace DirectX;

unique_ptr<CSkeleton> CSkeleton::Create()
{
	return unique_ptr<CSkeleton>(new CSkeleton());
}

i32_t CSkeleton::AddBone(const string& strName, i32_t iParentIndex, const XMFLOAT4X4& matOffset)
{
	i32_t iIndex = (i32_t)m_vecBones.size();
	BoneInfo bone;
	bone.strName = strName;
	bone.iParentIndex = iParentIndex;
	bone.matOffset = matOffset;
	m_vecBones.push_back(bone);
	m_mapBoneIndex[strName] = iIndex;
	return iIndex;
}

i32_t CSkeleton::FindBoneIndex(const string& strName) const
{
	auto it = m_mapBoneIndex.find(strName);
	if (it == m_mapBoneIndex.end()) return -1;
	return it->second;
}

void CSkeleton::ComputeFinalTransforms(
	const vector<XMFLOAT4X4>& vecLocalTransforms,
	vector<XMFLOAT4X4>& vecOutFinal) const
{
	u32_t iBoneCount = (u32_t)m_vecBones.size();
	vecOutFinal.resize(iBoneCount);
	vector<XMMATRIX> vecGlobal(iBoneCount);

	for (u32_t i = 0; i < iBoneCount; ++i)
	{
		XMMATRIX matLocal = XMLoadFloat4x4(&vecLocalTransforms[i]);
		if (m_vecBones[i].iParentIndex >= 0)
			vecGlobal[i] = matLocal * vecGlobal[m_vecBones[i].iParentIndex];
		else
			vecGlobal[i] = matLocal;

		XMMATRIX matOffset = XMLoadFloat4x4(&m_vecBones[i].matOffset);
		XMStoreFloat4x4(&vecOutFinal[i], matOffset * vecGlobal[i]);
	}
}
```

---

### Step 3. `Engine/Public/Resource/CAnimation.h` (신규)

```cpp
#pragma once
#include "Engine_Defines.h"

NS_BEGIN(Engine)

struct VectorKey { f64_t dTime; float3_t vValue; };
struct QuatKey   { f64_t dTime; float4_t vValue; };

struct BoneChannel
{
	string				strBoneName;
	i32_t				iBoneIndex = -1;
	vector<VectorKey>	vecPositionKeys;
	vector<QuatKey>		vecRotationKeys;
	vector<VectorKey>	vecScaleKeys;
};

class CSkeleton;

class CAnimation
{
private:
	CAnimation() = default;
public:
	~CAnimation() = default;
	static unique_ptr<CAnimation> Create(const string& strName, f64_t dDuration, f64_t dTicksPerSecond);

	void	AddChannel(const BoneChannel& channel);
	void	ResolveBoneIndices(const CSkeleton* pSkeleton);
	void	Evaluate(f64_t dTimeInTicks, vector<DirectX::XMFLOAT4X4>& vecOut, u32_t iBoneCount) const;

	const string&	GetName() const { return m_strName; }
	f64_t			GetDuration() const { return m_dDuration; }
	f64_t			GetTicksPerSecond() const { return m_dTicksPerSecond; }

private:
	DirectX::XMVECTOR InterpolatePosition(const BoneChannel& ch, f64_t t) const;
	DirectX::XMVECTOR InterpolateRotation(const BoneChannel& ch, f64_t t) const;
	DirectX::XMVECTOR InterpolateScale(const BoneChannel& ch, f64_t t) const;

	string				m_strName;
	f64_t				m_dDuration = 0.0;
	f64_t				m_dTicksPerSecond = 25.0;
	vector<BoneChannel>	m_vecChannels;
};

NS_END
```

### `Engine/Private/Resource/CAnimation.cpp` (신규)

```cpp
#include "Resource/CAnimation.h"
#include "Resource/CSkeleton.h"

using namespace Engine;
using namespace DirectX;

unique_ptr<CAnimation> CAnimation::Create(const string& strName, f64_t dDuration, f64_t dTicksPerSecond)
{
	auto p = unique_ptr<CAnimation>(new CAnimation());
	p->m_strName = strName;
	p->m_dDuration = dDuration;
	p->m_dTicksPerSecond = (dTicksPerSecond > 0.0) ? dTicksPerSecond : 25.0;
	return p;
}

void CAnimation::AddChannel(const BoneChannel& channel) { m_vecChannels.push_back(channel); }

void CAnimation::ResolveBoneIndices(const CSkeleton* pSkeleton)
{
	for (auto& ch : m_vecChannels)
		ch.iBoneIndex = pSkeleton->FindBoneIndex(ch.strBoneName);
}

void CAnimation::Evaluate(f64_t dTime, vector<XMFLOAT4X4>& vecOut, u32_t iBoneCount) const
{
	vecOut.resize(iBoneCount);
	for (u32_t i = 0; i < iBoneCount; ++i)
		XMStoreFloat4x4(&vecOut[i], XMMatrixIdentity());

	for (const auto& ch : m_vecChannels)
	{
		if (ch.iBoneIndex < 0 || ch.iBoneIndex >= (i32_t)iBoneCount) continue;
		XMVECTOR p = InterpolatePosition(ch, dTime);
		XMVECTOR r = InterpolateRotation(ch, dTime);
		XMVECTOR s = InterpolateScale(ch, dTime);
		XMStoreFloat4x4(&vecOut[ch.iBoneIndex],
			XMMatrixScalingFromVector(s) * XMMatrixRotationQuaternion(r) * XMMatrixTranslationFromVector(p));
	}
}

XMVECTOR CAnimation::InterpolatePosition(const BoneChannel& ch, f64_t t) const
{
	auto& k = ch.vecPositionKeys;
	if (k.empty()) return XMVectorZero();
	if (k.size() == 1) return XMLoadFloat3(&k[0].vValue);

	u32_t i = 0;
	for (u32_t j = 0; j < (u32_t)k.size() - 1; ++j)
	{
		if (t < k[j + 1].dTime) { i = j; break; }
	}

	f32_t f = (f32_t)((t - k[i].dTime) / (k[i + 1].dTime - k[i].dTime));
	f = max(0.f, min(1.f, f));
	return XMVectorLerp(XMLoadFloat3(&k[i].vValue), XMLoadFloat3(&k[i + 1].vValue), f);
}

XMVECTOR CAnimation::InterpolateRotation(const BoneChannel& ch, f64_t t) const
{
	auto& k = ch.vecRotationKeys;
	if (k.empty()) return XMQuaternionIdentity();
	if (k.size() == 1) return XMLoadFloat4(&k[0].vValue);

	u32_t i = 0;
	for (u32_t j = 0; j < (u32_t)k.size() - 1; ++j)
	{
		if (t < k[j + 1].dTime) { i = j; break; }
	}

	f32_t f = (f32_t)((t - k[i].dTime) / (k[i + 1].dTime - k[i].dTime));
	f = max(0.f, min(1.f, f));
	return XMQuaternionSlerp(XMLoadFloat4(&k[i].vValue), XMLoadFloat4(&k[i + 1].vValue), f);
}

XMVECTOR CAnimation::InterpolateScale(const BoneChannel& ch, f64_t t) const
{
	auto& k = ch.vecScaleKeys;
	if (k.empty()) return XMVectorSet(1, 1, 1, 0);
	if (k.size() == 1) return XMLoadFloat3(&k[0].vValue);

	u32_t i = 0;
	for (u32_t j = 0; j < (u32_t)k.size() - 1; ++j)
	{
		if (t < k[j + 1].dTime) { i = j; break; }
	}

	f32_t f = (f32_t)((t - k[i].dTime) / (k[i + 1].dTime - k[i].dTime));
	f = max(0.f, min(1.f, f));
	return XMVectorLerp(XMLoadFloat3(&k[i].vValue), XMLoadFloat3(&k[i + 1].vValue), f);
}
```

---

### Step 4. `Engine/Public/Resource/CAnimator.h` (신규)

```cpp
#pragma once
#include "Engine_Defines.h"

NS_BEGIN(Engine)

class CSkeleton;
class CAnimation;

class CAnimator
{
private:
	CAnimator() = default;
public:
	~CAnimator() = default;
	static unique_ptr<CAnimator> Create(CSkeleton* pSkeleton);

	void	Update(f32_t fDeltaTime);
	void	PlayAnimation(CAnimation* pAnim, bool_t bLoop = true);
	void	Stop();

	const vector<DirectX::XMFLOAT4X4>&	GetFinalBoneMatrices() const { return m_vecFinalMatrices; }
	u32_t	GetBoneCount() const;
	bool_t	IsPlaying() const { return m_bPlaying; }

private:
	CSkeleton*	m_pSkeleton = nullptr;
	CAnimation*	m_pCurrentAnim = nullptr;
	f64_t		m_dCurrentTime = 0.0;
	bool_t		m_bLoop = true;
	bool_t		m_bPlaying = false;
	vector<DirectX::XMFLOAT4X4>	m_vecLocalTransforms;
	vector<DirectX::XMFLOAT4X4>	m_vecFinalMatrices;
};

NS_END
```

### `Engine/Private/Resource/CAnimator.cpp` (신규)

```cpp
#include "Resource/CAnimator.h"
#include "Resource/CSkeleton.h"
#include "Resource/CAnimation.h"

using namespace Engine;
using namespace DirectX;

unique_ptr<CAnimator> CAnimator::Create(CSkeleton* pSkeleton)
{
	auto p = unique_ptr<CAnimator>(new CAnimator());
	p->m_pSkeleton = pSkeleton;
	u32_t n = pSkeleton->GetBoneCount();
	p->m_vecLocalTransforms.resize(n);
	p->m_vecFinalMatrices.resize(n);
	for (u32_t i = 0; i < n; ++i)
		XMStoreFloat4x4(&p->m_vecFinalMatrices[i], XMMatrixIdentity());
	return p;
}

void CAnimator::PlayAnimation(CAnimation* pAnim, bool_t bLoop)
{
	m_pCurrentAnim = pAnim;
	m_dCurrentTime = 0.0;
	m_bLoop = bLoop;
	m_bPlaying = true;
}

void CAnimator::Stop() { m_bPlaying = false; }

void CAnimator::Update(f32_t fDeltaTime)
{
	if (!m_bPlaying || !m_pCurrentAnim || !m_pSkeleton) return;

	m_dCurrentTime += (f64_t)fDeltaTime * m_pCurrentAnim->GetTicksPerSecond();
	f64_t dur = m_pCurrentAnim->GetDuration();
	if (m_dCurrentTime >= dur)
	{
		if (m_bLoop) m_dCurrentTime = fmod(m_dCurrentTime, dur);
		else { m_dCurrentTime = dur; m_bPlaying = false; }
	}

	u32_t n = m_pSkeleton->GetBoneCount();
	m_pCurrentAnim->Evaluate(m_dCurrentTime, m_vecLocalTransforms, n);
	m_pSkeleton->ComputeFinalTransforms(m_vecLocalTransforms, m_vecFinalMatrices);
}

u32_t CAnimator::GetBoneCount() const
{
	return m_pSkeleton ? m_pSkeleton->GetBoneCount() : 0;
}
```

---

### Step 5. `Engine/Public/Resource/Model.h` 수정

**수정 전/후 차이 — 추가되는 부분만 표시**

L4 이후 include 추가:
```cpp
#include "Resource/CBone.h"
#include "Resource/CSkeleton.h"
#include "Resource/CAnimation.h"
#include "Resource/CAnimator.h"
```

L6 이후 forward decl 추가:
```cpp
struct aiBone;
```

클래스 public 섹션에 추가 (L19 이후):
```cpp
	bool_t HasSkeleton() const { return m_pSkeleton != nullptr; }
	CSkeleton* GetSkeleton() const { return m_pSkeleton.get(); }
	CAnimator* GetAnimator() const { return m_pAnimator.get(); }
	CAnimation* GetAnimation(u32_t iIndex) const;
```

클래스 private 섹션에 추가 (L31 이후):
```cpp
	// ── 스켈레톤/애니메이션 ──
	void LoadSkeleton(const aiScene* pScene);
	void LoadAnimations(const aiScene* pScene);
	void ExtractBoneWeights(aiMesh* pMesh, vector<VTXANIM>& vecVertices);

	static DirectX::XMFLOAT4X4 ConvertMatrix(const aiMatrix4x4& m);

	unique_ptr<CSkeleton>			m_pSkeleton;
	unique_ptr<CAnimator>			m_pAnimator;
	vector<unique_ptr<CAnimation>>	m_vecAnimations;
	bool_t							m_bHasBones = false;
```

**수정 후 전체 Model.h:**

```cpp
#pragma once
#include "Engine_Defines.h"
#include "Resource/CTexture.h"
#include "Resource/Mesh.h"
#include "Resource/CBone.h"
#include "Resource/CSkeleton.h"
#include "Resource/CAnimation.h"
#include "Resource/CAnimator.h"

struct aiScene;
struct aiMesh;
struct aiNode;
struct aiBone;

NS_BEGIN(Engine)

class CModel
{
private:
	CModel() = default;
public:
	~CModel() = default;
	
	void Render(ID3D11DeviceContext* pContext);

	u32_t GetMeshCount() const { return (u32_t)m_vecMeshes.size(); }
	u32_t GetAnimationCount() const { return m_iAnimCount; }

	void BindMaterial(ID3D11DeviceContext* pContext, u32_t iMeshIndex);

	void SetOverrideTexture(CTexture* pTexture) { m_pOverrideTexture = pTexture; }

	// ── 스켈레톤/애니메이션 접근 ──
	bool_t HasSkeleton() const { return m_pSkeleton != nullptr; }
	CSkeleton* GetSkeleton() const { return m_pSkeleton.get(); }
	CAnimator* GetAnimator() const { return m_pAnimator.get(); }
	CAnimation* GetAnimation(u32_t iIndex) const;

	static unique_ptr<CModel> Create(ID3D11Device* pDevice, const string& strFilePath);

private:
	HRESULT LoadModel(ID3D11Device* pDevice, const string& strFilePath);
	void ProcessNode(ID3D11Device* pDevice, aiNode* pNode, const aiScene* pScene);
	unique_ptr<CMesh> ProcessMesh(ID3D11Device* pDevice, aiMesh* pMesh, const aiScene* pScene);
	void LoadTextures(ID3D11Device* pDevice, const aiScene* pScene, const string& strDirectory);

	// ── 스켈레톤/애니메이션 로딩 ──
	void LoadSkeleton(const aiScene* pScene);
	void LoadAnimations(const aiScene* pScene);
	void ExtractBoneWeights(aiMesh* pMesh, vector<VTXANIM>& vecVertices);

	static DirectX::XMFLOAT4X4 ConvertMatrix(const aiMatrix4x4& m);

	vector<unique_ptr<CMesh>>		m_vecMeshes;
	vector<unique_ptr<CTexture>>	m_vecTextures;
	unique_ptr<CTexture>			m_pDefaultTexture;
	CTexture*						m_pOverrideTexture = nullptr;
	u32_t							m_iAnimCount = 0;

	unique_ptr<CSkeleton>			m_pSkeleton;
	unique_ptr<CAnimator>			m_pAnimator;
	vector<unique_ptr<CAnimation>>	m_vecAnimations;
	bool_t							m_bHasBones = false;
};

NS_END
```

---

### Step 6. `Engine/Private/Resource/Model.cpp` 수정

**핵심 변경사항:**

1. `ConvertMatrix()` 정적 함수 추가 — aiMatrix4x4 → XMFLOAT4X4 **전치 필요** (Assimp column-major → DX row-major)
2. `LoadSkeleton()` — 모든 메시에서 본 수집, 부모-자식 계층 구축
3. `ExtractBoneWeights()` — 메시별 본 가중치 추출 + 정규화
4. `LoadAnimations()` — aiAnimation → CAnimation 변환
5. `ProcessMesh()` 분기 — 본이 있으면 VTXANIM으로, 없으면 기존 VTXMESH
6. `LoadModel()` — 스켈레톤/애니메이션 로딩 호출 추가

**수정 전 `LoadModel()` (L56-88):**
```cpp
HRESULT CModel::LoadModel(ID3D11Device* pDevice, const string& strFilePath)
{
	Assimp::Importer importer;
	const aiScene* pScene = importer.ReadFile(strFilePath,
		aiProcess_Triangulate |
		aiProcess_ConvertToLeftHanded |
		aiProcess_GenNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_LimitBoneWeights |
		aiProcess_JoinIdenticalVertices);

	if (!pScene || !pScene->mRootNode)
		return E_FAIL;

	string strDirectory = strFilePath.substr(0, strFilePath.find_last_of("/\\") + 1);

	m_pDefaultTexture = CTexture::CreateDefault(pDevice);
	LoadTextures(pDevice, pScene, strDirectory);

	ProcessNode(pDevice, pScene->mRootNode, pScene);

	m_iAnimCount = pScene->mNumAnimations;

	OutputDebugStringA(("[CModel] Loaded: meshes=" + to_string(m_vecMeshes.size())
		+ " materials=" + to_string(m_vecTextures.size())
		+ " animations=" + to_string(m_iAnimCount) + "\n").c_str());

	return S_OK;
}
```

**수정 후 `LoadModel()`:**
```cpp
HRESULT CModel::LoadModel(ID3D11Device* pDevice, const string& strFilePath)
{
	Assimp::Importer importer;
	const aiScene* pScene = importer.ReadFile(strFilePath,
		aiProcess_Triangulate |
		aiProcess_ConvertToLeftHanded |
		aiProcess_GenNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_LimitBoneWeights |
		aiProcess_JoinIdenticalVertices);

	if (!pScene || !pScene->mRootNode)
		return E_FAIL;

	string strDirectory = strFilePath.substr(0, strFilePath.find_last_of("/\\") + 1);

	// 텍스처 로드
	m_pDefaultTexture = CTexture::CreateDefault(pDevice);
	LoadTextures(pDevice, pScene, strDirectory);

	// ── 스켈레톤 로드 (메시 로딩 전에!) ──
	LoadSkeleton(pScene);

	// 메시 로드 (스켈레톤 유무에 따라 VTXMESH/VTXANIM 분기)
	ProcessNode(pDevice, pScene->mRootNode, pScene);

	// ── 애니메이션 로드 ──
	m_iAnimCount = pScene->mNumAnimations;
	if (m_iAnimCount > 0 && m_pSkeleton)
	{
		LoadAnimations(pScene);
		m_pAnimator = CAnimator::Create(m_pSkeleton.get());

		// 첫 번째 애니메이션 자동 재생
		if (!m_vecAnimations.empty())
			m_pAnimator->PlayAnimation(m_vecAnimations[0].get());
	}

	OutputDebugStringA(("[CModel] Loaded: meshes=" + to_string(m_vecMeshes.size())
		+ " materials=" + to_string(m_vecTextures.size())
		+ " animations=" + to_string(m_iAnimCount)
		+ " bones=" + to_string(m_pSkeleton ? m_pSkeleton->GetBoneCount() : 0)
		+ "\n").c_str());

	return S_OK;
}
```

**추가 함수들 (Model.cpp 하단에 추가):**

```cpp
DirectX::XMFLOAT4X4 CModel::ConvertMatrix(const aiMatrix4x4& m)
{
	// Assimp: row-major이지만 ConvertToLeftHanded 후에도 전치 필요할 수 있음
	// aiMatrix4x4는 row-major. XMFLOAT4X4도 row-major. 직접 대입.
	DirectX::XMFLOAT4X4 out;
	out._11 = m.a1; out._12 = m.a2; out._13 = m.a3; out._14 = m.a4;
	out._21 = m.b1; out._22 = m.b2; out._23 = m.b3; out._24 = m.b4;
	out._31 = m.c1; out._32 = m.c2; out._33 = m.c3; out._34 = m.c4;
	out._41 = m.d1; out._42 = m.d2; out._43 = m.d3; out._44 = m.d4;
	return out;
}

void CModel::LoadSkeleton(const aiScene* pScene)
{
	// 모든 메시에서 본 이름 수집
	unordered_map<string, DirectX::XMFLOAT4X4> mapBoneOffsets;

	for (u32_t mi = 0; mi < pScene->mNumMeshes; ++mi)
	{
		aiMesh* pMesh = pScene->mMeshes[mi];
		if (!pMesh->HasBones()) continue;
		m_bHasBones = true;

		for (u32_t bi = 0; bi < pMesh->mNumBones; ++bi)
		{
			aiBone* pBone = pMesh->mBones[bi];
			string strName = pBone->mName.C_Str();
			if (mapBoneOffsets.find(strName) == mapBoneOffsets.end())
				mapBoneOffsets[strName] = ConvertMatrix(pBone->mOffsetMatrix);
		}
	}

	if (!m_bHasBones) return;

	m_pSkeleton = CSkeleton::Create();

	// 노드 트리를 DFS 순회하여 본 계층 구축
	// (Assimp 노드 중 본에 해당하는 것만 등록)
	function<void(aiNode*, i32_t)> BuildHierarchy =
		[&](aiNode* pNode, i32_t iParentIndex)
	{
		string strName = pNode->mName.C_Str();
		i32_t iMyIndex = -1;

		auto it = mapBoneOffsets.find(strName);
		if (it != mapBoneOffsets.end())
		{
			iMyIndex = m_pSkeleton->AddBone(strName, iParentIndex, it->second);
		}
		else
		{
			// 본이 아닌 중간 노드도 계층 유지를 위해 등록 (Identity offset)
			DirectX::XMFLOAT4X4 identity;
			XMStoreFloat4x4(&identity, XMMatrixIdentity());
			iMyIndex = m_pSkeleton->AddBone(strName, iParentIndex, identity);
		}

		for (u32_t i = 0; i < pNode->mNumChildren; ++i)
			BuildHierarchy(pNode->mChildren[i], iMyIndex);
	};

	BuildHierarchy(pScene->mRootNode, -1);
}

void CModel::ExtractBoneWeights(aiMesh* pMesh, vector<VTXANIM>& vecVertices)
{
	if (!pMesh->HasBones() || !m_pSkeleton) return;

	for (u32_t bi = 0; bi < pMesh->mNumBones; ++bi)
	{
		aiBone* pBone = pMesh->mBones[bi];
		i32_t iBoneIndex = m_pSkeleton->FindBoneIndex(pBone->mName.C_Str());
		if (iBoneIndex < 0) continue;

		for (u32_t wi = 0; wi < pBone->mNumWeights; ++wi)
		{
			u32_t iVertexId = pBone->mWeights[wi].mVertexId;
			f32_t fWeight = pBone->mWeights[wi].mWeight;

			// 4개 슬롯 중 빈 곳에 삽입
			auto& v = vecVertices[iVertexId];
			for (u32_t s = 0; s < 4; ++s)
			{
				if (v.fBoneWeights[s] == 0.f)
				{
					v.iBoneIndices[s] = (u32_t)iBoneIndex;
					v.fBoneWeights[s] = fWeight;
					break;
				}
			}
		}
	}

	// 가중치 정규화 (합 == 1.0)
	for (auto& v : vecVertices)
	{
		f32_t fSum = v.fBoneWeights[0] + v.fBoneWeights[1] + v.fBoneWeights[2] + v.fBoneWeights[3];
		if (fSum > 0.f && fabsf(fSum - 1.f) > 0.001f)
		{
			for (u32_t s = 0; s < 4; ++s)
				v.fBoneWeights[s] /= fSum;
		}
	}
}

void CModel::LoadAnimations(const aiScene* pScene)
{
	for (u32_t ai = 0; ai < pScene->mNumAnimations; ++ai)
	{
		aiAnimation* pAiAnim = pScene->mAnimations[ai];
		auto pAnim = CAnimation::Create(
			pAiAnim->mName.C_Str(),
			pAiAnim->mDuration,
			pAiAnim->mTicksPerSecond);

		for (u32_t ci = 0; ci < pAiAnim->mNumChannels; ++ci)
		{
			aiNodeAnim* pChannel = pAiAnim->mChannels[ci];
			BoneChannel ch;
			ch.strBoneName = pChannel->mNodeName.C_Str();

			// Position Keys
			for (u32_t k = 0; k < pChannel->mNumPositionKeys; ++k)
			{
				auto& key = pChannel->mPositionKeys[k];
				ch.vecPositionKeys.push_back({ key.mTime, { key.mValue.x, key.mValue.y, key.mValue.z } });
			}

			// Rotation Keys (쿼터니언)
			for (u32_t k = 0; k < pChannel->mNumRotationKeys; ++k)
			{
				auto& key = pChannel->mRotationKeys[k];
				// Assimp 쿼터니언: (w, x, y, z) → XMFLOAT4: (x, y, z, w)
				ch.vecRotationKeys.push_back({ key.mTime,
					{ key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w } });
			}

			// Scale Keys
			for (u32_t k = 0; k < pChannel->mNumScalingKeys; ++k)
			{
				auto& key = pChannel->mScalingKeys[k];
				ch.vecScaleKeys.push_back({ key.mTime, { key.mValue.x, key.mValue.y, key.mValue.z } });
			}

			pAnim->AddChannel(ch);
		}

		pAnim->ResolveBoneIndices(m_pSkeleton.get());
		m_vecAnimations.push_back(move(pAnim));
	}
}

CAnimation* CModel::GetAnimation(u32_t iIndex) const
{
	if (iIndex >= m_vecAnimations.size()) return nullptr;
	return m_vecAnimations[iIndex].get();
}
```

**`ProcessMesh()` 수정 — 본이 있으면 VTXANIM으로 분기:**

수정 전 (L104-156): VTXMESH만 처리

수정 후:
```cpp
unique_ptr<CMesh> CModel::ProcessMesh(ID3D11Device* pDevice, aiMesh* pMesh, const aiScene* pScene)
{
	// ── 본이 있는 메시: VTXANIM (76 bytes) ──
	if (pMesh->HasBones() && m_bHasBones)
	{
		vector<VTXANIM> vertices(pMesh->mNumVertices);
		memset(vertices.data(), 0, sizeof(VTXANIM) * pMesh->mNumVertices);

		for (u32_t i = 0; i < pMesh->mNumVertices; ++i)
		{
			vertices[i].vPosition = { pMesh->mVertices[i].x, pMesh->mVertices[i].y, pMesh->mVertices[i].z };
			if (pMesh->mNormals)
				vertices[i].vNormal = { pMesh->mNormals[i].x, pMesh->mNormals[i].y, pMesh->mNormals[i].z };
			if (pMesh->mTextureCoords[0])
				vertices[i].vTexCoord = { pMesh->mTextureCoords[0][i].x, pMesh->mTextureCoords[0][i].y };
			if (pMesh->mTangents)
				vertices[i].vTangent = { pMesh->mTangents[i].x, pMesh->mTangents[i].y, pMesh->mTangents[i].z };
		}

		// 본 가중치 추출
		ExtractBoneWeights(pMesh, vertices);

		// 인덱스
		vector<u32_t> indices;
		for (u32_t i = 0; i < pMesh->mNumFaces; ++i)
		{
			aiFace& face = pMesh->mFaces[i];
			for (u32_t j = 0; j < face.mNumIndices; ++j)
				indices.push_back(face.mIndices[j]);
		}

		auto mesh = CMesh::Create(pDevice,
			vertices.data(), sizeof(VTXANIM), (u32_t)vertices.size(),
			indices.data(), (u32_t)indices.size(), true);

		if (mesh) mesh->SetMaterialIndex(pMesh->mMaterialIndex);
		return mesh;
	}

	// ── 본이 없는 메시: 기존 VTXMESH (44 bytes) ──
	vector<VTXMESH> vertices(pMesh->mNumVertices);

	for (u32_t i = 0; i < pMesh->mNumVertices; ++i)
	{
		vertices[i].vPosition = { pMesh->mVertices[i].x, pMesh->mVertices[i].y, pMesh->mVertices[i].z };
		if (pMesh->mNormals)
			vertices[i].vNormal = { pMesh->mNormals[i].x, pMesh->mNormals[i].y, pMesh->mNormals[i].z };
		if (pMesh->mTextureCoords[0])
			vertices[i].vTexCoord = { pMesh->mTextureCoords[0][i].x, pMesh->mTextureCoords[0][i].y };
		if (pMesh->mTangents)
			vertices[i].vTangent = { pMesh->mTangents[i].x, pMesh->mTangents[i].y, pMesh->mTangents[i].z };
	}

	vector<u32_t> indices;
	for (u32_t i = 0; i < pMesh->mNumFaces; ++i)
	{
		aiFace& face = pMesh->mFaces[i];
		for (u32_t j = 0; j < face.mNumIndices; ++j)
			indices.push_back(face.mIndices[j]);
	}

	auto mesh = CMesh::Create(pDevice,
		vertices.data(), sizeof(VTXMESH), (u32_t)vertices.size(),
		indices.data(), (u32_t)indices.size(), true);

	if (mesh) mesh->SetMaterialIndex(pMesh->mMaterialIndex);
	return mesh;
}
```

---

### Step 7. `Shaders/Skinned3D.hlsl` (신규)

> **주의: row_major 필수!** (CLAUDE.md Gotchas 참조)

```hlsl
// ── Skinned3D.hlsl — 스켈레톤 애니메이션 셰이더 ──

cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
};

cbuffer CBBones : register(b2)
{
    row_major matrix g_BoneMatrices[256];
};

Texture2D g_DiffuseMap : register(t0);
SamplerState g_Sampler : register(s0);

struct VS_INPUT
{
    float3 vPosition     : POSITION;
    float3 vNormal       : NORMAL;
    float2 vTexCoord     : TEXCOORD0;
    float3 vTangent      : TANGENT;
    uint4  iBoneIndices  : BLENDINDICES;
    float4 fBoneWeights  : BLENDWEIGHT;
};

struct PS_INPUT
{
    float4 vPosition : SV_POSITION;
    float3 vNormal   : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vWorldPos : TEXCOORD1;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;

    // 스키닝 행렬 블렌딩
    matrix skinMatrix =
        g_BoneMatrices[input.iBoneIndices.x] * input.fBoneWeights.x +
        g_BoneMatrices[input.iBoneIndices.y] * input.fBoneWeights.y +
        g_BoneMatrices[input.iBoneIndices.z] * input.fBoneWeights.z +
        g_BoneMatrices[input.iBoneIndices.w] * input.fBoneWeights.w;

    float4 skinned = mul(float4(input.vPosition, 1.f), skinMatrix);
    float4 worldPos = mul(skinned, g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vNormal = normalize(mul(input.vNormal, (float3x3)(skinMatrix)));
    output.vTexCoord = input.vTexCoord;
    output.vWorldPos = worldPos.xyz;

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    return g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
}
```

---

### Step 8. `Engine/Public/Renderer/ModelRenderer.h` 수정

**추가할 public 메서드 (L18 이후):**

```cpp
	// ── 애니메이션 ──
	void	Update(f32_t fDeltaTime);
	void	PlayAnimation(uint32 iIndex);
	bool	HasSkeleton() const;
```

**수정 후 전체:**

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include <string>
#include <memory>

class WINTERS_API ModelRenderer
{
public:
	ModelRenderer();
	~ModelRenderer();

	bool	Init(const std::string& strFbxPath,
		const wchar_t* pHlslPath = L"Shaders/Mesh3D.hlsl");
	void	UpdateTransform(const Mat4& matWorld);
	void	UpdateCamera(const Mat4& matViewProj);
	void	Render();
	void	Shutdown();

	bool	LoadTexture(const std::wstring& strPath);

	// ── 애니메이션 ──
	void	Update(f32_t fDeltaTime);
	void	PlayAnimation(uint32 iIndex);
	bool	HasSkeleton() const;

	uint32	GetAnimationCount() const;

private:
	struct Impl;
	Impl* m_pImpl = nullptr;
};
```

---

### Step 9. `Engine/Private/Renderer/ModelRenderer.cpp` 수정

**Impl 구조체에 추가:**

```cpp
struct ModelRenderer::Impl
{
	DX11Shader						shader;
	DX11Pipeline					pipeline;
	DX11ConstantBuffer<CBPerFrame>	cbPerFrame;
	DX11ConstantBuffer<CBPerObject>	cbPerObject;
	unique_ptr<CModel>				pModel;
	unique_ptr<CTexture>			pManualTexture;
	bool_t							bReady = false;

	// ── 스키닝 추가 ──
	DX11Shader						shaderSkinned;
	DX11Pipeline					pipelineSkinned;
	DX11ConstantBuffer<CBBoneMatrices> cbBones;
	bool_t							bSkinnedReady = false;
};
```

**include 추가 (L1 이후):**

```cpp
#include "Resource/CSkeleton.h"
#include "Resource/CAnimation.h"
#include "Resource/CAnimator.h"
```

**Init() 수정 — 스키닝 파이프라인 초기화 (L50 이후, bReady 설정 전):**

```cpp
	// ── 스키닝 파이프라인 (본이 있으면) ──
	if (m_pImpl->pModel->HasSkeleton())
	{
		wchar_t skinnedPath[MAX_PATH] = {};
		WintersResolveContentPath(L"Shaders/Skinned3D.hlsl", skinnedPath, MAX_PATH);

		if (m_pImpl->shaderSkinned.Load(pDevice, skinnedPath) &&
			m_pImpl->pipelineSkinned.CreateSkinnedMesh(pDevice, m_pImpl->shaderSkinned.GetVSBlob()) &&
			m_pImpl->cbBones.Create(pDevice))
		{
			m_pImpl->bSkinnedReady = true;
			OutputDebugStringA("[ModelRenderer] Skinned pipeline ready\n");
		}
	}
```

**Update() 함수 추가:**

```cpp
void ModelRenderer::Update(f32_t fDeltaTime)
{
	if (!m_pImpl->bReady || !m_pImpl->pModel) return;

	auto* pAnimator = m_pImpl->pModel->GetAnimator();
	if (pAnimator)
		pAnimator->Update(fDeltaTime);
}
```

**PlayAnimation() 함수 추가:**

```cpp
void ModelRenderer::PlayAnimation(uint32 iIndex)
{
	if (!m_pImpl->pModel) return;

	auto* pAnim = m_pImpl->pModel->GetAnimation(iIndex);
	auto* pAnimator = m_pImpl->pModel->GetAnimator();
	if (pAnim && pAnimator)
		pAnimator->PlayAnimation(pAnim);
}
```

**HasSkeleton() 함수 추가:**

```cpp
bool ModelRenderer::HasSkeleton() const
{
	return m_pImpl && m_pImpl->pModel && m_pImpl->pModel->HasSkeleton();
}
```

**Render() 수정 — 스키닝 분기:**

```cpp
void ModelRenderer::Render()
{
	if (!m_pImpl->bReady) return;

	auto* pContext = CEngineApp::Get().GetDevice().GetContext();

	// ── 스키닝 렌더링 ──
	if (m_pImpl->bSkinnedReady && m_pImpl->pModel->HasSkeleton())
	{
		auto* pAnimator = m_pImpl->pModel->GetAnimator();
		if (pAnimator)
		{
			// 본 행렬 업로드
			CBBoneMatrices boneData = {};
			const auto& matrices = pAnimator->GetFinalBoneMatrices();
			u32_t count = min((u32_t)matrices.size(), 256u);
			memcpy(boneData.bones, matrices.data(), count * sizeof(DirectX::XMFLOAT4X4));
			m_pImpl->cbBones.Update(pContext, boneData);
		}

		m_pImpl->shaderSkinned.Bind(pContext);
		m_pImpl->pipelineSkinned.Bind(pContext);
		m_pImpl->cbPerFrame.BindVS(pContext, 0);
		m_pImpl->cbPerObject.BindVS(pContext, 1);
		m_pImpl->cbBones.BindVS(pContext, 2);

		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pImpl->pModel->Render(pContext);
		m_pImpl->shaderSkinned.Unbind(pContext);
		return;
	}

	// ── 정적 메시 렌더링 (기존) ──
	m_pImpl->shader.Bind(pContext);
	m_pImpl->pipeline.Bind(pContext);
	m_pImpl->cbPerFrame.BindVS(pContext, 0);
	m_pImpl->cbPerObject.BindVS(pContext, 1);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pImpl->pModel->Render(pContext);
	m_pImpl->shader.Unbind(pContext);
}
```

**Shutdown() 수정 — 스키닝 리소스 해제:**

```cpp
void ModelRenderer::Shutdown()
{
	if (m_pImpl)
	{
		m_pImpl->pModel.reset();
		m_pImpl->shader.Release();
		m_pImpl->pipeline.Release();
		m_pImpl->cbPerFrame.Release();
		m_pImpl->cbPerObject.Release();

		// 스키닝
		m_pImpl->shaderSkinned.Release();
		m_pImpl->pipelineSkinned.Release();
		m_pImpl->cbBones.Release();
		m_pImpl->bSkinnedReady = false;

		m_pImpl->bReady = false;
	}
}
```

---

### Step 10. `Client/Private/CGameApp.cpp` 수정

**OnUpdate 수정 (L35-44):**

수정 전:
```cpp
void CGameApp::OnUpdate(f32_t deltaTime)
{
    m_fElapsed += deltaTime;
    m_pCamera->Update(deltaTime, CInput::Get());

    m_CubeTransform.SetRotationX(m_fElapsed * 0.8f);

    // 이렐리아 회전 OFF (원점/피벗 검증)
    //m_IreliaTransform.SetRotationX(m_fElapsed * 0.3f);
}
```

수정 후:
```cpp
void CGameApp::OnUpdate(f32_t deltaTime)
{
    m_fElapsed += deltaTime;
    m_pCamera->Update(deltaTime, CInput::Get());

    m_CubeTransform.SetRotationX(m_fElapsed * 0.8f);

    // ── 이렐리아 애니메이션 업데이트 ──
    m_Irelia.Update(deltaTime);

    // ── 키보드 1~6: 애니메이션 전환 ──
    auto& input = CInput::Get();
    u32_t animCount = m_Irelia.GetAnimationCount();
    for (u32_t i = 0; i < min(animCount, 6u); ++i)
    {
        if (input.IsKeyDown('1' + i))
            m_Irelia.PlayAnimation(i);
    }
}
```

---

### Step 11. vcxproj 등록

**Engine.vcxproj에 추가:**

```xml
<!-- 소스 -->
<ClCompile Include="..\Private\Resource\CSkeleton.cpp" />
<ClCompile Include="..\Private\Resource\CAnimation.cpp" />
<ClCompile Include="..\Private\Resource\CAnimator.cpp" />

<!-- 헤더 -->
<ClInclude Include="..\Public\Resource\CBone.h" />
<ClInclude Include="..\Public\Resource\CSkeleton.h" />
<ClInclude Include="..\Public\Resource\CAnimation.h" />
<ClInclude Include="..\Public\Resource\CAnimator.h" />
```

**Engine.vcxproj.filters에 추가:**

```xml
<Filter Include="06. Resource\03. Skeleton">
  <UniqueIdentifier>{신규GUID}</UniqueIdentifier>
</Filter>
<Filter Include="06. Resource\04. Animation">
  <UniqueIdentifier>{신규GUID}</UniqueIdentifier>
</Filter>
```

---

## 빌드 후 테스트

```
[ ] Engine 빌드 성공
[ ] Client 빌드 성공
[ ] VS Output: "[CModel] Loaded: meshes=N materials=M animations=K bones=B" 로그 확인
[ ] 이렐리아 T-pose → 첫 애니메이션 자동 재생 시작
[ ] 키보드 1: idle 전환
[ ] 키보드 2: run 전환 (있으면)
[ ] 키보드 3~6: 추가 애니메이션 전환
[ ] 부드러운 루프 (끊김 없음)
[ ] 카메라 이동 (WASD) 중 클리핑 없음 (row_major 검증)
```

---

## 주의사항

1. **Skinned3D.hlsl cbuffer에 `row_major` 필수!** — Mesh3D/Default3D와 동일 규칙 (CLAUDE.md Gotchas)
2. **Assimp `#pragma push_macro("new") / #undef new`** — Engine_Defines.h의 `DBG_NEW` 매크로와 충돌 방지
3. **본 가중치 정규화** — `ExtractBoneWeights()`에서 합이 1.0이 되도록 처리 (Assimp이 보장하지 않는 경우 있음)
4. **`aiProcess_LimitBoneWeights`** — 이미 LoadModel에서 적용 중 (본당 최대 4개 가중치)
5. **mTicksPerSecond == 0이면 25.0** — CAnimation::Create에서 fallback 처리
6. **CBBoneMatrices는 Engine_Struct.h에 이미 정의됨** — 256 × XMFLOAT4X4 (16KB, DX11 64KB 제한 이내)
7. **DX11Pipeline::CreateSkinnedMesh()도 이미 존재** — 76 bytes stride, CULL_BACK
8. **쿼터니언 순서**: Assimp `aiQuaternion(w,x,y,z)` → XMFLOAT4에 `(x,y,z,w)` 순서로 저장 (DirectXMath 관례)
