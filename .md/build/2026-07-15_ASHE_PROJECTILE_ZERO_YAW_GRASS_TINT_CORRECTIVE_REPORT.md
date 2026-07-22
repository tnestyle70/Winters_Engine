# Session - Ashe Projectile Zero Yaw / Map11 GrassTint Corrective Fix

> FAILED / SUPERSEDED (2026-07-16): the `yaw = 0` conclusion and success claim in this report are invalid. The replicated presentation path overwrote WFX rotation from `ProjectileVisualCatalog::fYawOffset`, and Ashe's raw arrow FBX requires `-PI/2`. The corrective implementation and verification gates are recorded in `.md/plan/2026-07-16_PROJECTILE_YAW_AND_CHAMPION_WFX_PLAN.md`. A successful build is not visual proof.

Date: 2026-07-15

## 결론

사용자 시각 검증에서 실패한 두 변경을 성공으로 취급하지 않고 원인부터 다시 추적했다.

- Ashe idle `idle1` 수정은 기존 상태 그대로 유지한다. 이 항목은 사용자 시각 검증을 통과했다.
- BA/W/E/R의 authored yaw offset을 모두 `0`으로 통일했다.
- 크기는 BA `0.021`, W `0.021`, E `0.042`(BA 2배), R `0.063`(BA 3배)로 고정했다.
- E의 기존 PNG billboard를 제거하고 BA arrow FBX를 사용하는 `MeshParticle` cue로 교체했다.
- Map11 bush는 803개 genuine foliage mesh를 유지한다. FBX/Obsidian 재추출이나 billboard 재도입은 필요하지 않다.
- bush 색 문제의 근본 원인은 SSAO나 render exclusion이 아니다. asset cook이 Riot `VertexDeform_inst` 재질을 단일 diffuse로 축약하면서 `USE_GRASS_TINT_MAP` / `GrassTint_SRX` 공간 색상 단계를 버린 것이다.
- raw/unlit 출력은 저채도인 `sru_brush.png`를 그대로 보여줄 뿐이므로 폐기했다. 803개 foliage에만 `GrassTint_SRX`를 곱한 뒤 기존 stylized diffuse, point light, AO를 그대로 통과시킨다.

Engine/Client Debug x64 및 Mesh3D/Skinned3D VS/PS 컴파일은 통과했다. 사용자 요청에 따라 게임과 서버는 실행하지 않았다. 최종 화면 판정은 사용자 인게임 검증 gate로 남긴다.

## 이전 시도의 실패를 확정하는 근거

### 1. WFX yaw만 바꾼 수정이 반영되지 않은 이유

WFX의 `rotation.y`는 spawn 시점 초기값이다. replicated BA/W/R은 이후 매 snapshot마다 다음 코드가 최종 회전을 다시 쓴다.

파일: `Client/Private/Network/Client/EventApplier.cpp`

```cpp
const ProjectileVisualDesc& visual =
    ProjectileVisualCatalog::Resolve(uProjectileKind);
const f32_t yaw =
    WintersMath::YawFromDirectionXZ(direction) + visual.fYawOffset;

TransformComponent& transform = world.GetComponent<TransformComponent>(entity);
transform.SetPosition(vPosition);
const Vec3 rotation = transform.GetRotation();
transform.SetRotation({ rotation.x, yaw, rotation.z });
```

따라서 `ba_arrow.wfx`와 `w_arrow.wfx`를 `1.4234`로 바꾸는 것만으로는 최종 화면을 바꿀 수 없다. 최종 owner인 `ProjectileVisualDesc::fYawOffset`을 고쳐야 한다.

### 2. raw/unlit bush가 색을 복구하지 못한 이유

로컬 Riot material binary에는 다음 문자열이 함께 존재한다.

```text
VertexDeform_inst
DiffuseTexture
ASSETS/Maps/KitPieces/SRX/textures/SRU_Brush.tex
TintColor
USE_GRASS_TINT_MAP
```

Map11 shipping binary는 다음 authored map texture를 연결한다.

```text
ASSETS/Maps/Info/Map11/GrassTint_SRX.tex
```

즉 `SRU_Brush`는 최종 색이 아니라 foliage detail/alpha diffuse이며, 위치별 최종 색은 `GrassTint_SRX` 단계에서 만들어진다. 현재 cooked WMat은 diffuse path만 보존해 이 단계가 탈락했다. foliage는 이미 main color, normal, AO 경로에 포함되어 있으므로 SSAO 전처리나 렌더 제외 문제가 아니다.

## 실제 반영 코드

### 1. Snapshot 최종 소유 경로의 BA/W/R yaw offset = 0

파일: `Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp`

```cpp
constexpr ProjectileVisualDesc kAsheBasicAttackVisual{
    "Ashe.BA.Arrow", nullptr, "Ashe.BA.Hit", nullptr, nullptr, nullptr, 0.f
};

constexpr ProjectileVisualDesc kAsheVolleyArrowVisual{
    "Ashe.W.Arrow", "Ashe.W.Hit", nullptr, nullptr, nullptr, nullptr, 0.f
};

constexpr ProjectileVisualDesc kAsheCrystalArrowVisual{
    "Ashe.R.Arrow", "Ashe.R.Hit", nullptr, nullptr, nullptr, nullptr, 0.f
};
```

legacy local smoke도 같은 축 규칙을 사용하도록 다음 상수를 `0.f`로 맞췄다.

파일: `Client/Private/GameObject/Champion/Ashe/Ashe_FxPresets.cpp`

```cpp
constexpr f32_t kArrowMeshYawOffset = 0.f;
const Vec3 kBAArrowMeshScale{ 0.021f, 0.021f, 0.021f };
```

### 2. BA/W/E/R WFX rotation과 크기

네 cue의 MeshParticle 값은 다음과 같다.

| Cue | Scale | Rotation |
|---|---:|---:|
| `Ashe.BA.Arrow` | `0.021` | `[0, 0, 0]` |
| `Ashe.W.Arrow` | `0.021` | `[0, 0, 0]` |
| `Ashe.E.Hawkshot` | `0.042` | `[0, 0, 0]` |
| `Ashe.R.Arrow` | `0.063` | `[0, 0, 0]` |

새 파일: `Data/LoL/FX/Champions/Ashe/e_hawkshot.wfx`

```json
{
  "name": "Ashe.E.Hawkshot",
  "emitters": [
    {
      "name": "e_hawkshot_mesh",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Ashe/particles/fbx/ashe_base_aa_arrow.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_aa_arrowtext.png",
      "lifetime": 0.70,
      "scale": [0.042, 0.042, 0.042],
      "rotation": [0.0, 0.0, 0.0],
      "color": [0.78, 1.16, 1.42, 0.95],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.18,
      "blockable_by_wind_wall": true
    }
  ]
}
```

### 3. E billboard 제거 및 authoritative mesh 이동

파일: `Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp`

```cpp
void OnCastFrame_E_Visual(VisualHookContext& ctx)
{
    if (!ctx.bAuthoritativeEvent || !ctx.pWorld || !ctx.pCommand) return;
    if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

    const Vec3 forward = ResolveVisualForward(
        *ctx.pWorld, ctx.casterEntity, ctx.pCommand);
    const f32_t range = (ctx.pDef && ctx.pDef->rangeMax > 0.f)
        ? ctx.pDef->rangeMax
        : 25.f;
    const Vec3 origin = ResolveVisualPosition(*ctx.pWorld, ctx.casterEntity);

    FxCueContext fx{};
    fx.vWorldPos = {
        origin.x + forward.x * 0.8f,
        origin.y + 1.0f,
        origin.z + forward.z * 0.8f
    };
    fx.vForward = forward;
    fx.vVelocity = {
        forward.x * range / VisualCue::kEHawkshotTravelSeconds,
        0.f,
        forward.z * range / VisualCue::kEHawkshotTravelSeconds
    };
    fx.attachTo = NULL_ENTITY;
    fx.pFxMeshRenderer = ctx.pFxMeshRenderer;
    fx.bOverrideVelocity = true;
    fx.bOverrideLifetime = true;
    fx.fLifetimeOverride = VisualCue::kEHawkshotTravelSeconds;
    CFxCuePlayer::PlayAll(
        *ctx.pWorld, VisualCue::kEHawkshot, fx, nullptr);
}
```

이 경로는 server EffectTrigger가 전달한 방향을 사용하는 client visual이다. E damage, vision, reveal 같은 gameplay authority를 새로 만들지 않는다. predicted hook에서는 spawn하지 않아 동일 cue의 이중 생성을 막는다.

### 4. 803개 foliage만 GrassTint 재질로 분리

파일: `Engine/Private/Renderer/ModelRenderer.cpp`

```cpp
bool_t ModelRenderer::SetGrassTintMaterialByName(
    const std::string& materialName,
    const std::wstring& grassTintTexturePath)
{
    if (!m_pImpl || !m_pImpl->pSharedModel || materialName.empty() ||
        grassTintTexturePath.empty())
    {
        return false;
    }

    const u64_t materialHash = Winters::Asset::FNV1a(materialName.c_str());
    bool_t bMatched = false;
    const auto& submeshes = m_pImpl->pSharedModel->GetSubmeshInfos();
    for (u32_t i = 0; i < static_cast<u32_t>(submeshes.size()); ++i)
    {
        if (submeshes[i].materialHash != materialHash)
            continue;

        SetSubmeshVisible(m_pImpl->grassTintMaterialMask, i, true);
        bMatched = true;
    }
    if (!bMatched)
        return false;

    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
    auto pGrassTintTexture = CTexture::Create(
        pDevice,
        grassTintTexturePath,
        eTexSamplerMode::Clamp,
        eTexColorSpace::ShaderLocalSRGB);
    if (!pGrassTintTexture)
    {
        m_pImpl->grassTintMaterialMask.fill(0ull);
        return false;
    }

    m_pImpl->pGrassTintTexture = move(pGrassTintTexture);
    m_pImpl->bHasGrassTintMaterials = true;
    return true;
}
```

호출은 InGame과 Editor에서 동일하다.

```cpp
m_Map.SetGrassTintMaterialByName(
    "Maps/KitPieces/SRX/Base/Models/LevelProp/Materials/VertexDeform_inst",
    L"Texture/MAP/output/textures/assets/maps/info/map11/grasstint_srx.png");
```

exact material hash가 일치하는 genuine foliage 803개만 tint mask에 들어가며 일반 지형·나무 277개는 standard mask에 남는다.

### 5. GrassTint 후 기존 조명/AO 유지

파일: `Shaders/Mesh3D.hlsl`, `Shaders/Skinned3D.hlsl`

```hlsl
float2 ResolveGrassTintUV(float3 localPosition)
{
    // Riot Map11 frame: X=[0,15000], Z=[-15000,0].
    return saturate(float2(localPosition.x, -localPosition.z) / 15000.f);
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    if (g_bUseGrassTint != 0u)
    {
        const float3 grassTint =
            g_GrassTintMap.Sample(g_GrassTintSampler, input.vGrassTintUV).rgb;
        texColor.rgb = saturate(texColor.rgb * grassTint * 2.f);
    }
    clip(texColor.a - 0.05f);

    const float ao = SampleScreenAO(input.vPosition);
    const float3 baseLinear = SrgbToLinearApprox(texColor.rgb);
    const float3 colorLinear = ApplyStylizedDiffuse(
        baseLinear, normalize(input.vNormal), input.vWorldPos, ao);
    // 기존 출력 경로 계속 사용
}
```

중요한 차이는 raw/unlit return을 제거했다는 점이다. GrassTint는 diffuse 색 복원 단계이고, 이후 alpha clip, stylized diffuse, point light, SSAO는 기존과 동일하다.

## 회귀 위험

| 영역 | 위험 | 현재 방어선 / 남은 gate |
|---|---|---|
| 일반 지형·나무 | 전체 map 색이 바뀔 위험 | exact `VertexDeform_inst` hash만 선택한다. 감사 결과 foliage 803, standard 277이다. |
| bush alpha | 풀 가장자리가 사각형이 될 위험 | 기존 `clip(texColor.a - 0.05f)`를 유지한다. |
| bush lighting | 색을 살리며 입체 조명이 사라질 위험 | unlit return을 제거하고 기존 stylized light/AO를 계속 사용한다. |
| atlas UV | map-space 축/방향이 틀릴 위험 | cooked foliage X/Z span과 Riot Map11 0..15000 frame을 사용했다. 최종 위치별 tint는 사용자 화면 gate가 필요하다. |
| 성능 | map geometry를 두 번 전부 그릴 위험 | visibility mask로 standard/tint submesh를 배타 분리한다. geometry 중복은 없고 각 submesh는 한 pass에만 들어간다. |
| DX12/RHI-only | DX11과 색 차이가 날 위험 | 기본 F5 DX11 `ModelRenderer`에는 반영됐다. RHI snapshot path에는 GrassTint texture slot이 아직 없어 backend parity는 후속 위험으로 남는다. |
| BA/W/R yaw | WFX가 다시 덮이는 위험 | snapshot 최종 owner인 catalog offset과 WFX rotation을 모두 0으로 맞췄다. |
| E visual | local prediction과 authoritative cue가 중복될 위험 | authoritative EffectTrigger에서만 E mesh를 생성한다. |
| gameplay authority | 시각 수정이 피해/판정에 영향 줄 위험 | Shared/GameSim, server projectile 결과, hit/damage를 변경하지 않았다. |

## 자동 검증 결과

### Definition pack

```text
Checked LoL definition pack 0x10774DA5
Champions: 17, skills: 85, summoner spells: 1
```

### Map foliage audit

```text
PASS visual=S1080/V729060/I1759152 foliage=803 grassTint=256x256 span=(13461.4,13449.7) surface=S277/V375147/I774927 stage=v5/B0
```

감사 스크립트는 다음을 함께 검사한다.

- visual map에 genuine `VertexDeform` foliage가 존재할 것
- foliage hash가 runtime GrassTint selector와 정확히 일치할 것
- `GrassTint_SRX` PNG가 존재하고 256x256일 것
- surface mesh에는 foliage가 없을 것
- Stage billboard/mesh bush entry가 `B0`일 것

### WFX 정적 검증

```text
Ashe.BA.Arrow: scale=0.021,0.021,0.021 rotation=0,0,0
Ashe.W.Arrow: scale=0.021,0.021,0.021 rotation=0,0,0
Ashe.E.Hawkshot: scale=0.042,0.042,0.042 rotation=0,0,0
Ashe.R.Arrow: scale=0.063,0.063,0.063 rotation=0,0,0
```

### Shader / build

```text
Mesh3D.hlsl VS_5_0: PASS
Mesh3D.hlsl PS_5_0: PASS
Skinned3D.hlsl VS_5_0: PASS
Skinned3D.hlsl PS_5_0: PASS
Engine Debug x64: PASS
Client Debug x64: PASS
Output: Client/Bin/Debug/WintersGame.exe
```

기존 DLL interface 및 `sprintf_s` warning은 남아 있으나 이번 변경의 compile/link error는 없다. targeted `git diff --check`도 whitespace error 없이 통과했다.

## 사용자 시각 검증 체크리스트

1. BA와 W의 FBX 머리가 발사 진행 방향을 향하는지 확인한다.
2. E가 billboard가 아니라 BA와 동일 계열의 FBX이고 BA 정확히 2배 크기인지 확인한다.
3. R이 BA 정확히 3배 크기이며 진행 방향을 향하는지 확인한다.
4. bush가 회색 raw diffuse가 아니라 위치별 `GrassTint_SRX`의 녹색 계열을 회복했는지 확인한다.
5. bush alpha 가장자리, 음영, SSAO가 유지되는지 확인한다.
6. 일반 지형과 나무의 기존 색/조명이 변하지 않았는지 확인한다.

이 여섯 항목의 화면 gate 전까지는 “빌드 및 경로 반영 완료”이며 “시각 판정 완료”로 기록하지 않는다.
