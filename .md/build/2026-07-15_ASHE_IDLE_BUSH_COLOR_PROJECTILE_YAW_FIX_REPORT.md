# SUPERSEDED / FAILED VISUAL GATE

2026-07-15 사용자 인게임 검증 결과를 기준으로 이 보고서의 결론을 정정한다.

- Ashe idle `idle1` 수정만 시각 검증을 통과했다.
- foliage를 raw/unlit diffuse로 출력한 색 복구는 실패했다. `sru_brush.png` 자체가 저채도이며, 원본 재질의 `USE_GRASS_TINT_MAP` / `GrassTint_SRX` 단계를 빠뜨렸기 때문이다.
- BA/W WFX의 `rotation.y = 1.4234` 수정은 실패했다. replicated projectile은 `EventApplier::EnsureProjectilePresentation`이 매 snapshot마다 WFX 회전을 `direction yaw + ProjectileVisualDesc::fYawOffset`으로 덮어쓴다.
- 아래 본문의 unlit 및 `1.4234` 성공 판정은 폐기한다. 교정 구현과 검증은 `2026-07-15_ASHE_PROJECTILE_ZERO_YAW_GRASS_TINT_CORRECTIVE_REPORT.md`를 정본으로 삼는다.

# Session - Ashe Idle, Bush Color, Projectile Yaw Fix

Date: 2026-07-15

## 결론

요청한 세 항목을 모두 실제 런타임 경로에 반영했다.

1. 애쉬가 정지 상태에서도 run을 유지하던 원인은 idle 키의 대소문자 불일치였다. 실제 cooked animation 이름에 맞춰 `Idle1`을 `idle1`으로 고쳤다.
2. 부쉬가 회백색으로 보이던 원인은 텍스처나 mesh 누락이 아니라, 맵의 stylized diffuse/gamma/AO 조명이 foliage PNG 색을 다시 계산하고 있었기 때문이다. foliage material에만 원본 diffuse를 그대로 출력하는 unlit 경로를 적용했다.
3. 애쉬 기본 공격과 W의 화살 본체 회전값을 `-1.57079632679`에서 요청값 `1.4234`로 되돌렸다. R은 요청 범위가 아니므로 건드리지 않았다.

서버와 게임은 사용자의 요청대로 실행하지 않았다. 자동 검증은 엔진/클라이언트 빌드, HLSL 컴파일, asset hash/audit까지 완료했고 최종 화면 판정은 사용자 눈검증 게이트로 남겼다.

## Bush 방식에 대한 최종 판정

billboard PNG를 bush geometry처럼 배치하던 기존 방향은 실패한 방식으로 확정한다. 현재 진짜 bush는 이미 `sr_base_flip.wmesh` 안에 존재한다.

- visual map 전체: 1,080 submesh
- foliage material submesh: 803
- 일반 조명 map submesh: 277
- foliage material source name: `Maps/KitPieces/SRX/Base/Models/LevelProp/Materials/VertexDeform_inst`
- foliage diffuse: `Texture/MAP/output/textures/assets/maps/kitpieces/srx/textures/sru_brush.png`
- runtime material hash: `0x3E947741946CB83F`

즉 PNG는 입체 bush를 대신하는 billboard가 아니라, 이미 존재하는 foliage mesh에 입혀지는 diffuse다. 이 상태에서는 별도 FBX 탐색이나 Obsidian 재추출이 다음 단계가 아니다. geometry와 material 연결은 이미 복구되었고, 이번 회백색 문제는 DX11 main color shader의 조명 경로 문제였다.

## 1. 애쉬 정지 Idle 복구

### 원인

`CModel::FindAnimationIndex`는 case-sensitive substring 검색을 사용한다. 기존 등록값은 `ashe_Idle1`을 찾았지만 실제 파일은 아래 이름이다.

```text
Client/Bin/Resource/Texture/Character/Ashe/anims/skinned_mesh_ashe_idle1.wanim
```

검색 실패 시 idle로 전환하지 못해 직전에 재생 중이던 run animation이 남았다.

### 실질 코드

파일: `Client/Private/GameObject/Champion/Ashe/Ashe_Registration.cpp`

```cpp
ChampionDef cd{};
cd.id = eChampion::ASHE;
cd.animPrefix = "ashe_";
cd.idleAnimKey = "idle1";
cd.runAnimKey = "run";
cd.basicAttackKey = "attack1";
```

결합 검색어는 이제 `ashe_idle1`이며 `skinned_mesh_ashe_idle1.wanim`과 일치한다.

## 2. Bush 원본 PNG 색 복구

### 원인

기존 `Mesh3D.hlsl`은 모든 map submesh를 다음 공통 단계로 통과시켰다.

```text
sRGB 근사 역변환 -> stylized diffuse -> point light -> AO -> sRGB 재변환
```

foliage의 얇은 alpha-cutout mesh에도 이 계산이 적용되어 `sru_brush.png`의 녹색/채도가 회백색으로 손실됐다. 텍스처 파일 자체를 다시 칠하거나 전체 map 조명을 제거하면 일반 지형과 나무까지 회귀하므로, 정확히 foliage material hash에 해당하는 803개 submesh만 분리했다.

### 실질 코드: CPU/GPU 상수 계약

파일: `Engine/Public/Renderer/FxShaderConstants.h`

```cpp
struct CBPerObject
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 worldInvTranspose;
    DirectX::XMFLOAT4 materialOverrideColor = { 1.f, 1.f, 1.f, 1.f };
    DirectX::XMFLOAT4 vMaterialOverrideParams = { 0.f, 0.f, 0.f, 0.f };
    u32_t bUnlitTexture = 0u;
    u32_t _padding0[3] = {};
};
static_assert(sizeof(CBPerObject) % 16 == 0);
```

파일: `Shaders/Mesh3D.hlsl` 및 `Shaders/Skinned3D.hlsl`

```hlsl
cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matWorldInvTranspose;
    float4 g_vMaterialOverrideColor;
    float4 g_vMaterialOverrideParams;
    uint g_bUnlitTexture;
    uint3 g_vObjectPad;
};

float4 PS(PS_INPUT input) : SV_TARGET
{
    const float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    clip(texColor.a - 0.05f);
    if (g_vMaterialOverrideColor.a >= 0.999f)
        return float4(g_vMaterialOverrideColor.rgb, texColor.a);
    if (g_bUnlitTexture != 0u)
        return texColor;

    const float ao = SampleScreenAO(input.vPosition);
    const float3 baseLinear = SrgbToLinearApprox(texColor.rgb);
    const float3 colorLinear = ApplyStylizedDiffuse(baseLinear, normalize(input.vNormal),
        input.vWorldPos, ao);
    // 일반 material은 기존 조명 경로를 계속 사용한다.
}
```

alpha clip은 그대로 유지하고, unlit foliage만 sampled diffuse를 그대로 반환한다. 일반 지형/나무의 stylized 조명은 변경하지 않았다.

### 실질 코드: foliage material 선택

파일: `Engine/Private/Renderer/ModelRenderer.cpp`

```cpp
void ModelRenderer::SetUnlitMaterialByName(const std::string& materialName)
{
    if (!m_pImpl || !m_pImpl->pSharedModel || materialName.empty())
        return;

    const u64_t materialHash = Winters::Asset::FNV1a(materialName.c_str());
    bool_t bMatched = false;
    const auto& submeshes = m_pImpl->pSharedModel->GetSubmeshInfos();
    for (u32_t i = 0; i < static_cast<u32_t>(submeshes.size()); ++i)
    {
        if (submeshes[i].materialHash != materialHash)
            continue;

        SetSubmeshVisible(m_pImpl->unlitMaterialMask, i, true);
        bMatched = true;
    }
    m_pImpl->bHasUnlitMaterials = m_pImpl->bHasUnlitMaterials || bMatched;

#if defined(_DEBUG)
    if (!bMatched)
    {
        OutputDebugStringA(("[ModelRenderer] Unlit material not found: " +
            materialName + "\n").c_str());
    }
#endif
}
```

동일 파일의 실제 render split은 아래처럼 lit/unlit mask를 만들고 같은 geometry를 두 번 중복하지 않고 각 submesh를 정확히 한 pass에만 보낸다.

```cpp
const u32_t submeshCount = m_pImpl->pSharedModel->GetMeshCount();
for (u32_t i = 0; i < submeshCount; ++i)
{
    if (!IsSubmeshVisible(mask, i))
        continue;

    const bool_t bUnlit = IsSubmeshVisible(m_pImpl->unlitMaterialMask, i);
    SetSubmeshVisible(bUnlit ? unlitMask : litMask, i, true);
    bHasUnlitSubmeshes = bHasUnlitSubmeshes || bUnlit;
    bHasLitSubmeshes = bHasLitSubmeshes || !bUnlit;
}

if (bHasLitSubmeshes)
{
    UpdateObjectConstants(false);
    m_pImpl->cbPerObject.Bind(pContext, 1);
    m_pImpl->pSharedModel->RenderWithMask(pDevice, litMask);
}

UpdateObjectConstants(true);
m_pImpl->cbPerObject.Bind(pContext, 1);
m_pImpl->pSharedModel->RenderWithMask(pDevice, unlitMask);
```

### 실질 코드: InGame/Editor 동일 적용

파일: `Client/Private/Scene/Scene_InGameLifecycle.cpp`

```cpp
bMapInit = m_Map.Initialize(GetSelectedMapMeshPath(), L"Shaders/Mesh3D.hlsl");
if (bMapInit)
{
    m_Map.SetUnlitMaterialByName(
        "Maps/KitPieces/SRX/Base/Models/LevelProp/Materials/VertexDeform_inst");
}
```

파일: `Client/Private/Scene/Scene_Editor.cpp`

```cpp
m_Map.Initialize("Texture/MAP/output/sr_base_flip.wmesh",
    L"Shaders/Mesh3D.hlsl");
m_Map.SetUnlitMaterialByName(
    "Maps/KitPieces/SRX/Base/Models/LevelProp/Materials/VertexDeform_inst");
```

중요하게도 display material 이름인 `.../VertexDeform`이 아니라 WMesh에 실제 저장된 Assimp source name `.../VertexDeform_inst`를 hash한다. audit에서 이 hash가 foliage 803개와 정확히 일치하고 일반 map 277개에는 일치하지 않음을 확인했다.

## 3. 애쉬 기본 공격/W Yaw 복구

### 실제 visual 구조

기본 공격과 W 모두 화살 본체는 PNG billboard가 아니다.

```json
{
  "render_type": "MeshParticle",
  "model": "Client/Bin/Resource/Texture/Character/Ashe/particles/fbx/ashe_base_aa_arrow.fbx",
  "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_aa_arrowtext.png"
}
```

두 번째 emitter인 trail만 `Billboard`다. 따라서 사용자가 W에서 FBX인지 PNG인지 구분하기 어려웠던 직접 원인은 화살 mesh가 `-90°` 반대 방향을 보고 있었기 때문이다.

### 실질 코드

파일: `Data/LoL/FX/Champions/Ashe/ba_arrow.wfx`

```json
{
  "name": "ba_arrow_mesh",
  "render_type": "MeshParticle",
  "model": "Client/Bin/Resource/Texture/Character/Ashe/particles/fbx/ashe_base_aa_arrow.fbx",
  "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_aa_arrowtext.png",
  "scale": [0.021, 0.021, 0.021],
  "rotation": [0.0, 1.4234, 0.0]
}
```

파일: `Data/LoL/FX/Champions/Ashe/w_arrow.wfx`

```json
{
  "name": "w_arrow_mesh",
  "render_type": "MeshParticle",
  "model": "Client/Bin/Resource/Texture/Character/Ashe/particles/fbx/ashe_base_aa_arrow.fbx",
  "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_aa_arrowtext.png",
  "scale": [0.021, 0.021, 0.021],
  "rotation": [0.0, 1.4234, 0.0]
}
```

여기서 `1.4234`는 사용자가 지정한 기존 보정 상수 그대로다. 수학적인 `pi/2` 값으로 재해석하거나 반올림하지 않았다.

## 회귀 위험 검토

| 영역 | 위험 | 통제/판정 |
|---|---|---|
| 애니메이션 | 다른 챔피언 idle 검색 변화 | Ashe 등록 문자열 한 줄만 변경. 공용 검색 로직은 미변경. 낮음 |
| BA/W 회전 | R 또는 다른 projectile 회전 변화 | BA/W WFX 두 파일만 변경. `r_arrow.wfx`의 기존 `-1.57079632679`는 유지. 낮음 |
| 일반 map 색/조명 | 지형·돌·나무가 unlit이 될 위험 | exact material hash `0x3E947741946CB83F`만 선택. 803 foliage만 일치, 277 일반 submesh 불일치. 낮음 |
| Bush alpha | 풀 외곽이 사각형으로 보일 위험 | 기존 `clip(texColor.a - 0.05f)`를 unlit return보다 먼저 유지. 낮음 |
| 렌더 상태 누수 | foliage 이후 다른 model이 unlit으로 남을 위험 | lit pass와 no-unlit path에서 `UpdateObjectConstants(false)`로 명시 복구. 낮음 |
| 성능 | map draw 호출 증가 | geometry 수는 동일하고 map main-color가 material 기준 최대 2 pass로 분리된다. 매 frame 최대 1,080 submesh mask 분류가 추가되는 제한된 비용. 낮음~보통 |
| DX12/RHI | backend 간 색 차이 | 현재 RHI snapshot map path는 이미 raw `albedo * tint` 계열이다. 이번 변경은 회백색이 발생한 DX11 `ModelRenderer` 경로를 맞춘다. 낮음 |
| 서버/GameSim | gameplay 판정 변화 | animation key와 client visual/WFX/renderer만 변경. 서버 권위, 투사체 판정, nav/surface mesh는 미변경. 없음 |

## 자동 검증 결과

### Build

```powershell
MSBuild Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal /nologo
MSBuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal /nologo
```

- Engine Debug x64: PASS
- Client Debug x64: PASS
- 산출물: `Client/Bin/Debug/WintersGame.exe`
- 기존 DLL export/interface warning은 남아 있으나 새 error는 없다.

### Shader

`fxc` shader model 5.0으로 아래 네 entry 조합을 독립 컴파일했다.

- `Mesh3D.hlsl`: VS PASS, PS PASS
- `Skinned3D.hlsl`: VS PASS, PS PASS
- 빌드 후 `Client/Bin/Debug/Shaders` runtime copy와 source hash 일치

### Asset/상수 검사

```text
PASS visual=S1080/V729060/I1759152 foliage=803 span=(13461.4,13449.7) surface=S277/V375147/I774927 stage=v5/B0
PASS unlit hash=0x3E947741946CB83F matched=803 lit=277
PASS Ashe BA/W yaw=1.4234, R unchanged=-1.57079632679
PASS Ashe keyword ashe_idle1 matches skinned_mesh_ashe_idle1.wanim
```

`git diff --check`도 대상 파일 기준 PASS다. 출력된 LF/CRLF 메시지는 저장소 line-ending 경고이며 whitespace error가 아니다.

## 사용자 눈검증 체크리스트

서버가 준비된 뒤 다음 순서로 판정하면 된다.

1. 애쉬를 생성하고 입력 없이 정지했을 때 idle이 재생되는지 본다.
2. 이동해 run을 재생한 뒤 멈췄을 때 즉시 idle로 복귀하는지 본다.
3. 기본 공격 화살 FBX의 머리가 실제 이동/타격 방향을 향하는지 본다.
4. W의 각 화살 FBX가 퍼지는 진행 방향을 향하는지 본다.
5. R은 이번 변경 전과 같은 회전을 유지하는지 본다.
6. 부쉬가 회백색이 아니라 `sru_brush.png`의 녹색/채도로 보이는지 본다.
7. 부쉬 alpha-cutout 외곽에 사각형 배경이 생기지 않았는지 본다.
8. 지형·돌·일반 나무의 기존 stylized 조명과 AO가 그대로인지 본다.

## 최종 판정 기준

- 위 1~4와 6~8이 통과하면 세 버그는 종료한다.
- 부쉬 geometry 위치/형태가 다시 틀린 경우에만 map cook/원본 mapgeo를 재조사한다.
- 색만 틀린 경우 FBX/Obsidian 재추출로 돌아가지 않는다. 그 경우에는 `sru_brush.png` 자체와 sampler/color-space를 캡처 비교하는 것이 다음 진단 단계다.
