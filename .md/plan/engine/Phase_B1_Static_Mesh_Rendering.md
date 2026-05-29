# Phase B-1: 이렐리아 정적 메시 DX11 렌더링 — 상세 구현 계획서

> **작성일**: 2026-04-14
> **목표**: 이렐리아 FBX → Assimp 로드 → DX11 화면 출력 (T-pose + 텍스처)
> **컨벤션**: Engine_Defines.h 체계 (NS_BEGIN, f32_t, FAILED_CHECK, C접두사, private 생성자 + Create())

---

## 사전 작업: vcpkg 설치

```cmd
vcpkg install assimp:x64-windows
vcpkg install directxtk:x64-windows
```

Engine.vcxproj 추가:
- IncludePath: vcpkg include 경로
- LibraryPath: vcpkg lib 경로
- Link: `assimp-vc143-mt.lib` (Release) / `assimp-vc143-mtd.lib` (Debug)
- PostBuild: `assimp-vc143-mt.dll` 복사

---

## 파일 1: `Engine/Public/Engine_Struct.h` (수정)

```cpp
#ifndef Engine_Struct_h__
#define Engine_Struct_h__

#include "Engine_Typedef.h"

namespace Engine
{
	// ── 정적 메시 정점 (44 bytes) ──
	struct VTXMESH
	{
		float3_t	vPosition;		// POSITION
		float3_t	vNormal;		// NORMAL
		float2_t	vTexCoord;		// TEXCOORD0
		float3_t	vTangent;		// TANGENT
	};

	// ── 스키닝 메시 정점 (76 bytes) ──
	struct VTXANIM
	{
		float3_t	vPosition;		// POSITION
		float3_t	vNormal;		// NORMAL
		float2_t	vTexCoord;		// TEXCOORD0
		float3_t	vTangent;		// TANGENT
		u32_t		iBoneIndices[4];	// BLENDINDICES
		f32_t		fBoneWeights[4];	// BLENDWEIGHT
	};

	// ── 본 행렬 상수 버퍼 (16KB, DX11 64KB 제한 이내) ──
	struct CBBoneMatrices
	{
		DirectX::XMFLOAT4X4 bones[256];
	};
}

#endif // Engine_Struct_h__
```

---

## 파일 2: `Engine/Public/RHI/DX11/DX11Pipeline.h` (수정 — 2개 메서드 추가)

L31 뒤에 추가:

```cpp
    // 정적 메시 레이아웃 (Mesh3D.hlsl)
    // POSITION(float3) + NORMAL(float3) + TEXCOORD(float2) + TANGENT(float3)
    bool CreateMesh(ID3D11Device* device, ID3DBlob* vsBlob);

    // 스키닝 메시 레이아웃 (Skinned3D.hlsl)
    // POSITION + NORMAL + TEXCOORD + TANGENT + BLENDINDICES(uint4) + BLENDWEIGHT(float4)
    bool CreateSkinnedMesh(ID3D11Device* device, ID3DBlob* vsBlob);
```

---

## 파일 3: `Engine/Private/RHI/DX11/DX11Pipeline.cpp` (수정 — 2개 메서드 구현 추가)

기존 Create3D() 아래에 추가:

```cpp
bool DX11Pipeline::CreateMesh(ID3D11Device* device, ID3DBlob* vsBlob)
{
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    return CreateInternal(device, vsBlob, layout, 4, D3D11_CULL_BACK);
}

bool DX11Pipeline::CreateSkinnedMesh(ID3D11Device* device, ID3DBlob* vsBlob)
{
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    return CreateInternal(device, vsBlob, layout, 6, D3D11_CULL_BACK);
}
```

---

## 파일 4: `Engine/Public/Resource/CMesh.h` (신규)

```cpp
#pragma once
#include "Engine_Defines.h"
#include "RHI/DX11/DX11Buffer.h"

NS_BEGIN(Engine)

class CMesh
{
public:
	~CMesh() = default;

	static unique_ptr<CMesh> Create(
		ID3D11Device* pDevice,
		const void* pVertices, u32_t iVertexStride, u32_t iVertexCount,
		const void* pIndices, u32_t iIndexCount, bool_t bUse32Bit = true);

	void	Render(ID3D11DeviceContext* pContext);

	u32_t	GetIndexCount() const { return m_iIndexCount; }
	u32_t	GetMaterialIndex() const { return m_iMaterialIndex; }
	void	SetMaterialIndex(u32_t iIndex) { m_iMaterialIndex = iIndex; }

private:
	CMesh() = default;

	DX11Buffer	m_VB;
	DX11Buffer	m_IB;
	u32_t		m_iVertexStride = 0;
	u32_t		m_iIndexCount = 0;
	u32_t		m_iMaterialIndex = 0;
};

NS_END
```

---

## 파일 5: `Engine/Private/Resource/CMesh.cpp` (신규)

```cpp
#include "Resource/CMesh.h"

using namespace Engine;

unique_ptr<CMesh> CMesh::Create(
	ID3D11Device* pDevice,
	const void* pVertices, u32_t iVertexStride, u32_t iVertexCount,
	const void* pIndices, u32_t iIndexCount, bool_t bUse32Bit)
{
	auto pInstance = unique_ptr<CMesh>(new CMesh());

	pInstance->m_iVertexStride = iVertexStride;
	pInstance->m_iIndexCount = iIndexCount;

	if (!pInstance->m_VB.CreateVertex(pDevice, pVertices, iVertexStride, iVertexCount))
		return nullptr;

	if (!pInstance->m_IB.CreateIndex(pDevice, pIndices, iIndexCount, bUse32Bit))
		return nullptr;

	return pInstance;
}

void CMesh::Render(ID3D11DeviceContext* pContext)
{
	m_VB.BindVertex(pContext, m_iVertexStride);
	m_IB.BindIndex(pContext);
	m_IB.DrawIndexed(pContext);
}
```

---

## 파일 6: `Engine/Public/Resource/CTexture.h` (신규)

```cpp
#pragma once
#include "Engine_Defines.h"

NS_BEGIN(Engine)

class CTexture
{
public:
	~CTexture();

	static unique_ptr<CTexture> Create(ID3D11Device* pDevice, const wstring& strFilePath);
	static unique_ptr<CTexture> CreateDefault(ID3D11Device* pDevice);

	void	Bind(ID3D11DeviceContext* pContext, u32_t iSlot = 0);

private:
	CTexture() = default;

	ID3D11ShaderResourceView*	m_pSRV = nullptr;
	ID3D11SamplerState*			m_pSampler = nullptr;
};

NS_END
```

---

## 파일 7: `Engine/Private/Resource/CTexture.cpp` (신규)

```cpp
#include "Resource/CTexture.h"

#pragma push_macro("new")
#undef new
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>
#pragma pop_macro("new")

using namespace Engine;
using namespace DirectX;

CTexture::~CTexture()
{
	if (m_pSampler) { m_pSampler->Release(); m_pSampler = nullptr; }
	if (m_pSRV) { m_pSRV->Release(); m_pSRV = nullptr; }
}

unique_ptr<CTexture> CTexture::Create(ID3D11Device* pDevice, const wstring& strFilePath)
{
	auto pInstance = unique_ptr<CTexture>(new CTexture());

	// 확장자 판별
	wstring ext = strFilePath.substr(strFilePath.find_last_of(L'.'));
	HRESULT hr = E_FAIL;

	if (ext == L".dds" || ext == L".DDS")
		hr = CreateDDSTextureFromFile(pDevice, strFilePath.c_str(), nullptr, &pInstance->m_pSRV);
	else
		hr = CreateWICTextureFromFile(pDevice, strFilePath.c_str(), nullptr, &pInstance->m_pSRV);

	if (FAILED(hr))
		return nullptr;

	// 샘플러 생성
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.MaxAnisotropy = 4;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = pDevice->CreateSamplerState(&sampDesc, &pInstance->m_pSampler);
	if (FAILED(hr))
		return nullptr;

	return pInstance;
}

unique_ptr<CTexture> CTexture::CreateDefault(ID3D11Device* pDevice)
{
	auto pInstance = unique_ptr<CTexture>(new CTexture());

	// 1x1 흰색 텍스처 (폴백용)
	u32_t white = 0xFFFFFFFF;
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = 1;
	texDesc.Height = 1;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_IMMUTABLE;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = &white;
	initData.SysMemPitch = 4;

	ID3D11Texture2D* pTex = nullptr;
	pDevice->CreateTexture2D(&texDesc, &initData, &pTex);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	pDevice->CreateShaderResourceView(pTex, &srvDesc, &pInstance->m_pSRV);

	if (pTex) pTex->Release();

	// 샘플러
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	pDevice->CreateSamplerState(&sampDesc, &pInstance->m_pSampler);

	return pInstance;
}

void CTexture::Bind(ID3D11DeviceContext* pContext, u32_t iSlot)
{
	if (m_pSRV)
		pContext->PSSetShaderResources(iSlot, 1, &m_pSRV);
	if (m_pSampler)
		pContext->PSSetSamplers(iSlot, 1, &m_pSampler);
}
```

---

## 파일 8: `Engine/Public/Resource/CModel.h` (신규)

```cpp
#pragma once
#include "Engine_Defines.h"

struct aiScene;
struct aiMesh;
struct aiNode;

NS_BEGIN(Engine)

class CMesh;
class CTexture;

class CModel
{
public:
	~CModel() = default;

	static unique_ptr<CModel> Create(ID3D11Device* pDevice, const string& strFilePath);

	void	Render(ID3D11DeviceContext* pContext);

	u32_t	GetMeshCount() const { return (u32_t)m_vecMeshes.size(); }
	u32_t	GetAnimationCount() const { return m_iAnimCount; }

	// 텍스처 바인딩 (서브메시별)
	void	BindMaterial(ID3D11DeviceContext* pContext, u32_t iMeshIndex);

private:
	CModel() = default;

	HRESULT	LoadModel(ID3D11Device* pDevice, const string& strFilePath);
	void	ProcessNode(ID3D11Device* pDevice, aiNode* pNode, const aiScene* pScene);
	unique_ptr<CMesh> ProcessMesh(ID3D11Device* pDevice, aiMesh* pMesh, const aiScene* pScene);
	void	LoadTextures(ID3D11Device* pDevice, const aiScene* pScene, const string& strDirectory);

	vector<unique_ptr<CMesh>>		m_vecMeshes;
	vector<unique_ptr<CTexture>>	m_vecTextures;
	unique_ptr<CTexture>			m_pDefaultTexture;
	u32_t							m_iAnimCount = 0;
};

NS_END
```

---

## 파일 9: `Engine/Private/Resource/CModel.cpp` (신규)

```cpp
#include "Resource/CModel.h"
#include "Resource/CMesh.h"
#include "Resource/CTexture.h"
#include "Engine_Struct.h"

#pragma push_macro("new")
#undef new
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#pragma pop_macro("new")

using namespace Engine;

unique_ptr<CModel> CModel::Create(ID3D11Device* pDevice, const string& strFilePath)
{
	auto pInstance = unique_ptr<CModel>(new CModel());

	if (FAILED(pInstance->LoadModel(pDevice, strFilePath)))
		return nullptr;

	return pInstance;
}

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

	// 디렉토리 추출 (텍스처 경로용)
	string strDirectory = strFilePath.substr(0, strFilePath.find_last_of("/\\") + 1);

	// 텍스처 로드
	m_pDefaultTexture = CTexture::CreateDefault(pDevice);
	LoadTextures(pDevice, pScene, strDirectory);

	// 메시 로드
	ProcessNode(pDevice, pScene->mRootNode, pScene);

	// 애니메이션 수 저장
	m_iAnimCount = pScene->mNumAnimations;

	return S_OK;
}

void CModel::ProcessNode(ID3D11Device* pDevice, aiNode* pNode, const aiScene* pScene)
{
	for (u32_t i = 0; i < pNode->mNumMeshes; ++i)
	{
		aiMesh* pMesh = pScene->mMeshes[pNode->mMeshes[i]];
		auto mesh = ProcessMesh(pDevice, pMesh, pScene);
		if (mesh)
			m_vecMeshes.push_back(move(mesh));
	}

	for (u32_t i = 0; i < pNode->mNumChildren; ++i)
		ProcessNode(pDevice, pNode->mChildren[i], pScene);
}

unique_ptr<CMesh> CModel::ProcessMesh(ID3D11Device* pDevice, aiMesh* pMesh, const aiScene* pScene)
{
	vector<VTXMESH> vertices(pMesh->mNumVertices);

	for (u32_t i = 0; i < pMesh->mNumVertices; ++i)
	{
		// Position
		vertices[i].vPosition.x = pMesh->mVertices[i].x;
		vertices[i].vPosition.y = pMesh->mVertices[i].y;
		vertices[i].vPosition.z = pMesh->mVertices[i].z;

		// Normal
		if (pMesh->mNormals)
		{
			vertices[i].vNormal.x = pMesh->mNormals[i].x;
			vertices[i].vNormal.y = pMesh->mNormals[i].y;
			vertices[i].vNormal.z = pMesh->mNormals[i].z;
		}

		// TexCoord
		if (pMesh->mTextureCoords[0])
		{
			vertices[i].vTexCoord.x = pMesh->mTextureCoords[0][i].x;
			vertices[i].vTexCoord.y = pMesh->mTextureCoords[0][i].y;
		}

		// Tangent
		if (pMesh->mTangents)
		{
			vertices[i].vTangent.x = pMesh->mTangents[i].x;
			vertices[i].vTangent.y = pMesh->mTangents[i].y;
			vertices[i].vTangent.z = pMesh->mTangents[i].z;
		}
	}

	// 인덱스
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

	if (mesh)
		mesh->SetMaterialIndex(pMesh->mMaterialIndex);

	return mesh;
}

void CModel::LoadTextures(ID3D11Device* pDevice, const aiScene* pScene, const string& strDirectory)
{
	m_vecTextures.resize(pScene->mNumMaterials);

	for (u32_t i = 0; i < pScene->mNumMaterials; ++i)
	{
		aiMaterial* pMaterial = pScene->mMaterials[i];
		aiString texPath;

		if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
		{
			string fullPath = strDirectory + texPath.C_Str();

			// 경로에서 파일명만 추출 (Assimp이 상대경로를 줄 수 있음)
			size_t lastSlash = fullPath.find_last_of("/\\");
			if (lastSlash != string::npos)
			{
				string fileName = fullPath.substr(lastSlash + 1);
				fullPath = strDirectory + fileName;
			}

			// string → wstring
			wstring wPath(fullPath.begin(), fullPath.end());
			m_vecTextures[i] = CTexture::Create(pDevice, wPath);
		}
	}
}

void CModel::BindMaterial(ID3D11DeviceContext* pContext, u32_t iMeshIndex)
{
	if (iMeshIndex >= m_vecMeshes.size())
		return;

	u32_t matIdx = m_vecMeshes[iMeshIndex]->GetMaterialIndex();

	if (matIdx < m_vecTextures.size() && m_vecTextures[matIdx])
		m_vecTextures[matIdx]->Bind(pContext, 0);
	else if (m_pDefaultTexture)
		m_pDefaultTexture->Bind(pContext, 0);
}

void CModel::Render(ID3D11DeviceContext* pContext)
{
	for (u32_t i = 0; i < (u32_t)m_vecMeshes.size(); ++i)
	{
		BindMaterial(pContext, i);
		m_vecMeshes[i]->Render(pContext);
	}
}
```

---

## 파일 10: `Shaders/Mesh3D.hlsl` (신규)

```hlsl
// ── Constant Buffers ──
cbuffer CBPerFrame : register(b0)
{
    matrix g_matViewProj;
};

cbuffer CBPerObject : register(b1)
{
    matrix g_matWorld;
};

// ── Texture ──
Texture2D    g_DiffuseMap : register(t0);
SamplerState g_Sampler    : register(s0);

// ── Input / Output ──
struct VS_INPUT
{
    float3 vPosition : POSITION;
    float3 vNormal   : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vTangent  : TANGENT;
};

struct PS_INPUT
{
    float4 vPosition : SV_POSITION;
    float3 vNormal   : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vWorldPos : TEXCOORD1;
};

// ── Vertex Shader ──
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = mul(float4(input.vPosition, 1.f), g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vNormal   = normalize(mul(input.vNormal, (float3x3)g_matWorld));
    output.vTexCoord = input.vTexCoord;
    output.vWorldPos = worldPos.xyz;

    return output;
}

// ── Pixel Shader ──
float4 PS(PS_INPUT input) : SV_TARGET
{
    // 디퓨즈 텍스처 샘플링
    float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);

    // 간단한 디렉셔널 라이트
    float3 lightDir = normalize(float3(0.5f, -1.f, 0.5f));
    float NdotL = saturate(dot(input.vNormal, -lightDir));

    float3 ambient = float3(0.2f, 0.2f, 0.22f);
    float3 diffuse = texColor.rgb * NdotL;

    return float4(ambient + diffuse, texColor.a);
}
```

---

## 파일 11: `Engine/Include/ModelRenderer.h` (신규 — Client 공개 API, pImpl)

```cpp
#pragma once
#include "WintersAPI.h"
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

	uint32	GetAnimationCount() const;

private:
	struct Impl;
	Impl* m_pImpl = nullptr;
};
```

---

## 파일 12: `Engine/Private/Renderer/ModelRenderer.cpp` (신규)

```cpp
#include "ModelRenderer.h"
#include "RHI/CDX11Device.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/DX11/DX11Pipeline.h"
#include "RHI/DX11/DX11ConstantBuffer.h"
#include "Resource/CModel.h"
#include "Framework/CEngineApp.h"

using namespace Engine;

struct ModelRenderer::Impl
{
	DX11Shader					shader;
	DX11Pipeline				pipeline;
	DX11ConstantBuffer<CBPerFrame>	cbPerFrame;
	DX11ConstantBuffer<CBPerObject>	cbPerObject;
	unique_ptr<CModel>			pModel;
	bool_t						bReady = false;
};

ModelRenderer::ModelRenderer() : m_pImpl(new Impl()) {}
ModelRenderer::~ModelRenderer() { Shutdown(); delete m_pImpl; }

bool ModelRenderer::Init(const string& strFbxPath, const wchar_t* pHlslPath)
{
	auto& device = CEngineApp::Get().GetDevice();
	ID3D11Device* pDevice = device.GetDevice();

	// 셰이더 로드
	wchar_t shaderPath[MAX_PATH] = {};
	if (pHlslPath)
		wcscpy_s(shaderPath, pHlslPath);

	if (!m_pImpl->shader.Load(pDevice, shaderPath))
		return false;

	// 파이프라인 (Mesh InputLayout)
	if (!m_pImpl->pipeline.CreateMesh(pDevice, m_pImpl->shader.GetVSBlob()))
		return false;

	// 상수 버퍼
	if (!m_pImpl->cbPerFrame.Create(pDevice))
		return false;
	if (!m_pImpl->cbPerObject.Create(pDevice))
		return false;

	// 모델 로드
	m_pImpl->pModel = CModel::Create(pDevice, strFbxPath);
	if (!m_pImpl->pModel)
		return false;

	m_pImpl->bReady = true;
	return true;
}

void ModelRenderer::UpdateTransform(const Mat4& matWorld)
{
	if (!m_pImpl->bReady) return;

	auto* pContext = CEngineApp::Get().GetDevice().GetContext();

	CBPerObject data;
	data.world = matWorld.m;
	m_pImpl->cbPerObject.Update(pContext, data);
}

void ModelRenderer::UpdateCamera(const Mat4& matViewProj)
{
	if (!m_pImpl->bReady) return;

	auto* pContext = CEngineApp::Get().GetDevice().GetContext();

	CBPerFrame data;
	data.viewProjection = matViewProj.m;
	m_pImpl->cbPerFrame.Update(pContext, data);
}

void ModelRenderer::Render()
{
	if (!m_pImpl->bReady) return;

	auto* pContext = CEngineApp::Get().GetDevice().GetContext();

	m_pImpl->shader.Bind(pContext);
	m_pImpl->pipeline.Bind(pContext);
	m_pImpl->cbPerFrame.BindVS(pContext, 0);
	m_pImpl->cbPerObject.BindVS(pContext, 1);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_pImpl->pModel->Render(pContext);

	m_pImpl->shader.Unbind(pContext);
}

void ModelRenderer::Shutdown()
{
	if (m_pImpl)
	{
		m_pImpl->pModel.reset();
		m_pImpl->shader.Release();
		m_pImpl->pipeline.Release();
		m_pImpl->cbPerFrame.Release();
		m_pImpl->cbPerObject.Release();
		m_pImpl->bReady = false;
	}
}

uint32 ModelRenderer::GetAnimationCount() const
{
	if (m_pImpl && m_pImpl->pModel)
		return m_pImpl->pModel->GetAnimationCount();
	return 0;
}
```

---

## 파일 13: `Client/Public/CGameApp.h` (수정)

기존 큐브 렌더러 유지 + ModelRenderer 추가:

```cpp
#pragma once
#include "WintersEngine.h"
#include "CubeRenderer.h"
#include "CCamera.h"
#include "CTransform.h"
#include "ModelRenderer.h"

class CGameApp final : public IWintersApp
{
public:
    CGameApp()  = default;
    ~CGameApp() = default;

    bool    OnInit()                     override;
    void    OnUpdate(float32 deltaTime)  override;
    void    OnRender()                   override;
    void    OnShutdown()                 override;

private:
    // ── 기존 Cube ──
    CubeRenderer    m_Cube;
    CTransform      m_CubeTransform;
    float32         m_fElapsed = 0.f;

    // ── Camera ──
    CCamera         m_Camera;

    // ── Irelia Model ──
    ModelRenderer   m_Irelia;
    CTransform      m_IreliaTransform;
};
```

---

## 파일 14: `Client/Private/CGameApp.cpp` (수정)

```cpp
#include <Windows.h>
#include "CGameApp.h"
#include "CInput.h"

bool CGameApp::OnInit()
{
    // ── 큐브 초기화 ──
    wchar_t shaderPath[MAX_PATH] = {};
    if (WintersResolveContentPath(L"Shaders/Default3D.hlsl", shaderPath, MAX_PATH))
        m_Cube.Init(shaderPath);

    // ── 카메라 ──
    m_Camera.SetPerspective(0.7854f, 1280.f / 720.f, 0.1f, 1000.f);
    m_Camera.SetPosition({ 0.f, 100.f, -200.f });
    m_Camera.SetPitch(-0.2f);

    // ── 이렐리아 모델 ──
    m_Irelia.Init("C:/Users/user/Desktop/LOL_Resource/Irelia/irelia.fbx",
                  L"Shaders/Mesh3D.hlsl");

    m_IreliaTransform.SetPosition(0.f, 0.f, 0.f);
    m_IreliaTransform.SetScale(1.f);

    m_CubeTransform.SetPosition(5.f, 0.f, 0.f);

    return true;
}

void CGameApp::OnUpdate(float32 deltaTime)
{
    m_fElapsed += deltaTime;
    m_Camera.Update(deltaTime, CInput::Get());

    m_CubeTransform.SetRotationY(m_fElapsed * 0.8f);

    // 이렐리아 천천히 회전
    m_IreliaTransform.SetRotationY(m_fElapsed * 0.3f);
}

void CGameApp::OnRender()
{
    Mat4 vp = m_Camera.GetViewProjection();

    // ── 큐브 렌더 ──
    m_Cube.UpdateCamera(vp);
    m_Cube.UpdateTransform(m_CubeTransform.GetWorldMatrix());
    m_Cube.Render();

    // ── 이렐리아 렌더 ──
    m_Irelia.UpdateCamera(vp);
    m_Irelia.UpdateTransform(m_IreliaTransform.GetWorldMatrix());
    m_Irelia.Render();
}

void CGameApp::OnShutdown()
{
    m_Cube.Shutdown();
    m_Irelia.Shutdown();
}
```

---

## vcxproj 수정 사항

### Engine.vcxproj
- 소스 추가: `CMesh.cpp`, `CTexture.cpp`, `CModel.cpp`, `ModelRenderer.cpp`
- 헤더 추가: `CMesh.h`, `CTexture.h`, `CModel.h`, `ModelRenderer.h`
- 링크 추가: `assimp-vc143-mtd.lib` (Debug), `assimp-vc143-mt.lib` (Release)
- Include 경로: vcpkg include
- PostBuild: `assimp-vc143-mt(d).dll` 복사

### Engine.vcxproj.filters
- `06. Resource\00. Mesh` 필터에 CMesh.h/.cpp
- `06. Resource\01. Texture` 필터에 CTexture.h/.cpp
- `06. Resource\02. Model` 필터에 CModel.h/.cpp
- `Include` 필터에 ModelRenderer.h
- `03. Renderer\03. Model` 필터에 ModelRenderer.cpp

---

## 주의사항

1. **`#define new DBG_NEW` 충돌**: Assimp/DirectXTK 헤더 include 시 반드시 `#pragma push_macro("new") / #undef new / #pragma pop_macro("new")` 사용
2. **LoL 모델 스케일**: 이렐리아는 약 100~200 유닛. 카메라를 멀리 배치해야 보임 (`SetPosition(0, 100, -200)`)
3. **텍스처 경로**: Assimp이 FBX 내부의 상대 경로를 반환할 수 있음. FBX와 같은 폴더에 텍스처 배치 필요
4. **cbuffer 슬롯**: b0 = ViewProjection, b1 = World (기존 Default3D.hlsl과 동일)

---

## 검증 체크리스트

```
[ ] vcpkg assimp + directxtk 설치 성공
[ ] Engine 빌드 성공
[ ] Client 빌드 성공
[ ] 이렐리아 T-pose 화면 출력
[ ] 텍스처 적용 확인 (회색 아닌 컬러)
[ ] 카메라 WASD+마우스로 모델 관찰 가능
[ ] 큐브와 이렐리아 동시 렌더링
[ ] OutputDebugString으로 메시 수, 애니메이션 수 로그 확인
```

---

## 파일 목록 총 14개

### 수정 (4개)
| 파일 | 변경 |
|------|------|
| `Engine/Public/Engine_Struct.h` | VTXMESH, VTXANIM, CBBoneMatrices 추가 |
| `Engine/Public/RHI/DX11/DX11Pipeline.h` | CreateMesh, CreateSkinnedMesh 선언 |
| `Engine/Private/RHI/DX11/DX11Pipeline.cpp` | CreateMesh, CreateSkinnedMesh 구현 |
| `Client/Public/CGameApp.h` + `Client/Private/CGameApp.cpp` | ModelRenderer 통합 |

### 신규 (10개)
| 파일 | 용도 |
|------|------|
| `Engine/Public/Resource/CMesh.h` | 메시 VB/IB 래핑 |
| `Engine/Private/Resource/CMesh.cpp` | 메시 구현 |
| `Engine/Public/Resource/CTexture.h` | 텍스처 SRV + Sampler |
| `Engine/Private/Resource/CTexture.cpp` | WIC/DDS 로드 |
| `Engine/Public/Resource/CModel.h` | Assimp FBX 로더 |
| `Engine/Private/Resource/CModel.cpp` | 메시/텍스처 파싱 |
| `Engine/Include/ModelRenderer.h` | Client 공개 API (pImpl) |
| `Engine/Private/Renderer/ModelRenderer.cpp` | 렌더링 통합 |
| `Shaders/Mesh3D.hlsl` | 정적 메시 셰이더 |
