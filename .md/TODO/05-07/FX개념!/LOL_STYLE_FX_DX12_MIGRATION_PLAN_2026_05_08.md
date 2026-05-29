# LoL Style FX DX12 Migration Plan

작성일: 2026-05-08

## 0. 최종 목표

이 DX12 마이그레이션의 근본 목표는 "DX12 자체 완성"이 아니다.

목표는 Winters에서 리그오브레전드 느낌의 FX를 만들 수 있는 기반을 여는 것이다.

즉, 아래 조합이 실제 게임 씬에서 안정적으로 돌아가야 한다.

```txt
단순 메쉬/빌보드/리본/빔
+ grayscale noise / mask / erode texture
+ material instance parameter
+ stylized unlit shader
+ additive / alpha / premultiplied blend
+ depth read / depth no-write / overlay 선택
= LoL 스타일 마법 FX
```

DX12 마이그는 이 목표를 위한 기반 작업이다.
Renderer 전체를 한 번에 DX12로 끝내려는 작업이 아니라, FX 제작에 필요한 RHI 기능을 우선 완성하는 작업이다.

## 1. 현재 닫힌 부분

완료된 기반:
- DX12 device / queue / swapchain / RTV / clear / present.
- Debug-DX12 Engine + Client 빌드 구성.
- EngineApp에서 DX12 우선, DX11 legacy fallback 선택 구조.
- RHI command list의 pipeline / bind group / vertex buffer / index buffer / draw path.
- RHI shader / texture / pipeline / bind group 최소 구현.
- FX sprite, beam, ribbon의 DX12 RHI 렌더 경로.
- FX static FBX mesh의 DX12 RHI 렌더 경로.
- FX mesh diffuse texture + erode/mask texture 경로 전달.
- JSON/asset 기반 `FxEmitterDesc::strErodeTexturePath`.
- 같은 FBX에 다른 diffuse/erode 조합을 붙일 수 있는 cache key.

현재 기준으로 DX12는 "FX를 올려보기 시작할 수 있는 단계"까지 왔다.

## 2. 완료 기준 재정의

이번 목표의 완료 기준은 다음이다.

완료라고 부르는 시점:
1. DX12 Client에서 Irelia/Ezreal 대표 FX가 게임 씬에서 보인다.
2. 동일 FBX에 다른 grayscale/mask/erode texture variant를 붙여도 cache 충돌이 없다.
3. Shader가 texture color에 의존하지 않고 grayscale data + material parameter로 색을 만든다.
4. Sprite / beam / ribbon / static mesh FX가 같은 material parameter 모델을 공유한다.
5. Additive / AlphaBlend / Premultiplied blend가 DX11 legacy와 같은 의도로 선택된다.
6. Mesh FX가 depth test/read, depth no-write, overlay 중 최소 2개 모드를 선택할 수 있다.
7. FX JSON 또는 asset desc만 바꿔서 fire / ice / void / Irelia magic 계열 색감 차이를 만들 수 있다.
8. Debug-DX12 Client smoke가 8초 이상 유지되고, 대표 FX 스폰 로그가 실패 없이 통과한다.

아직 완료 기준이 아닌 것:
- 전체 ModelRenderer DX12 전환.
- ImGui DX12 backend 완성.
- 모든 챔피언/맵/포스트프로세스 DX12 parity.
- RenderGraph 완성.
- DXR, GPU particle, compute emitter.

위 항목은 장기 목표지만, LoL식 FX 1차 목표를 막는 조건으로 두지 않는다.

## 3. LoL Style FX의 핵심 모델

LoL 느낌의 FX는 PBR 재질이 아니라 stylized VFX material이다.

핵심 분리:

```txt
Texture = 데이터
Shader = 데이터 해석 함수
Material Instance = 함수 인자
Mesh = 형태
```

예시:

```txt
t0 main/brush grayscale: 기본 강도와 붓 모양
t1 erode/mask grayscale: dissolve, edge, alpha shaping
t2 noise/flow optional: UV distortion, flow, turbulence

BaseColor: 어두운 바탕색
EmissionColor: HDR 발광색
EdgeColor: dissolve edge 색
RimColor: fresnel/rim 색
Contrast: grayscale 대비
ErodeThreshold: 사라지는 경계
ScrollSpeed: 흐름
DistortStrength: 흔들림
EmissionIntensity: bloom으로 이어지는 강도
```

같은 mesh와 같은 grayscale texture라도 material instance 값만 바꾸면 fire, ice, void, arcane, Irelia magic으로 분화되어야 한다.

## 4. 진행 순서

### Stage FX-0. 빌드 안정화와 기준 씬 고정

목표:
- Debug-DX12 Client가 FX 테스트 루프로 계속 빌드 가능해야 한다.
- 실패한 빌드 때문에 shader/asset 작업이 멈추지 않아야 한다.

작업:
- `Client/Include/Client.vcxproj / Debug-DX12` 빌드 기준 유지.
- SDK header 동기화 실패 방지.
- 대표 테스트 명령을 문서화.
- Irelia 또는 Ezreal 하나를 FX test champion으로 고정.

완료 기준:
- Debug-DX12 Client 빌드 성공.
- `--rhi=dx12 --banpick-smoke --smoke-start --smoke-champion=irelia` 8초 유지.

### Stage FX-1. Neutral FX Material ABI 만들기

목표:
- DX11/DX12가 같은 FX material parameter 구조체를 본다.
- shader constant buffer ABI를 렌더러별로 흩뜨리지 않는다.

우선 추가할 파일 후보:

```txt
Engine/Public/FX/FxMaterialDesc.h
Engine/Public/Renderer/FxShaderConstants.h
```

핵심 구조:

```cpp
struct FxMaterialDesc
{
    Vec4 vBaseColor;
    Vec4 vEmissionColor;
    Vec4 vEdgeColor;
    Vec4 vRimColor;
    Vec4 vScrollA;
    Vec4 vScrollB;
    Vec4 vShape;
    Vec4 vDissolve;
    Vec4 vFresnel;
    f32_t fEmissionIntensity;
    f32_t fContrast;
    f32_t fDistortStrength;
    f32_t fSoftParticleDistance;
};
```

완료 기준:
- Sprite / beam / ribbon / mesh draw params가 같은 material desc를 참조한다.
- 기존 `CBFxParams`와 충돌 없이 HLSL packing이 16-byte align된다.

### Stage FX-2. LoL-style Master Shader 1차

목표:
- 지금의 단순 texture tint가 아니라 grayscale 기반 stylized shader를 쓴다.

우선 구현할 shader 기능:
- UV pan.
- grayscale contrast.
- erode/dissolve threshold.
- edge emission.
- HDR emission.
- fresnel/rim.
- per-life fade.
- optional texture fallback.

첫 master shader 후보:

```txt
Shaders/FX/M_VFX_Generic.hlsl
```

1차 pixel shader 개념:

```hlsl
float mainMask = MainTex.Sample(LinearWrap, uv).r;
float erode = ErodeTex.Sample(LinearWrap, uv).r;

mainMask = pow(saturate(mainMask), Contrast);

float dissolve = erode - ErodeThreshold;
float edge = 1.0 - smoothstep(0.0, EdgeWidth, dissolve);
float core = saturate(dissolve / max(EdgeWidth, 0.001));

float3 color =
    BaseColor.rgb * core +
    EmissionColor.rgb * core * EmissionIntensity +
    EdgeColor.rgb * edge * EdgeIntensity +
    RimColor.rgb * fresnel * RimIntensity;

return float4(color, alpha);
```

완료 기준:
- 흰색 diffuse texture가 아니라 grayscale mask만으로 색이 만들어진다.
- material parameter 값만 바꿔서 최소 4개 preset을 만든다.

### Stage FX-3. Asset / JSON Material Instance 확장

목표:
- 코드에서 직접 값을 박지 않고 asset desc에서 FX 색감과 셰이더 값을 조절한다.

추가할 JSON key 후보:

```json
{
  "material": "Texture/FX/Common/magic_brush_01.png",
  "erode_material": "Texture/FX/Common/magic_erode_01.png",
  "base_color": [0.1, 0.2, 0.4, 1.0],
  "emission_color": [0.5, 1.5, 4.0, 1.0],
  "edge_color": [3.0, 4.0, 5.0, 1.0],
  "rim_color": [0.8, 1.5, 3.0, 1.0],
  "emission_intensity": 6.0,
  "contrast": 2.5,
  "edge_width": 0.05,
  "distort_strength": 0.03,
  "scroll_a": [0.0, 0.5, 0.0, 0.0]
}
```

완료 기준:
- `FxAsset.cpp`가 material parameter를 읽는다.
- `LegacyFxAdapter`가 legacy component와 asset desc 사이 값을 보존한다.
- `FxLegacyAssetDumper`가 새 material 값을 다시 JSON으로 덤프한다.

### Stage FX-4. Renderer 연결 통합

목표:
- Sprite / beam / ribbon / mesh가 같은 material parameter path를 탄다.

작업:
- `CRHIFxSpriteRenderer`의 draw params 확장.
- `CFxStaticMeshRenderer`의 draw params 확장.
- `CFxBeamSystem` / `CFxMeshSystem`에서 `FxMaterialDesc` 채우기.
- DX11 legacy path는 깨지지 않게 기존 `CPlaneRenderer`와 `CTexture` 분기 유지.

완료 기준:
- 같은 JSON material desc로 billboard, beam, ribbon, mesh가 모두 비슷한 색감 규칙을 적용한다.

### Stage FX-5. Depth / Blend Mode 정리

목표:
- LoL식 FX의 겹침, 지면 밀착, 캐릭터 앞뒤 관계를 제어한다.

필요 모드:

```txt
DepthTestWriteOff: 지형/챔피언 뒤에 가려지지만 depth는 쓰지 않음
OverlayNoDepth: UI성 강조 FX, 항상 위
DepthWriteOn: 특수 opaque-ish mesh FX
SoftParticle: scene depth 기반 접촉부 fade
```

작업:
- `FxDepthMode` enum 추가.
- `RHIPipelineDesc` 또는 FX pipeline cache에 depth mode 반영.
- DX12 scene depth SRV 노출은 SoftParticle 단계에서 진행.

완료 기준:
- 지면 장판 / 빔 / 검기 mesh가 서로 다른 depth 의도를 가진다.
- FX가 챔피언 모델이나 지형과 어색하게 뚫리는 현상이 줄어든다.

### Stage FX-6. 대표 프리셋 제작

목표:
- 기술 기반이 실제 LoL 느낌으로 보이는지 확인한다.

프리셋:

```txt
MI_Irelia_MagicBlue
MI_Ezreal_ArcaneGold
MI_FireMagic
MI_IceMagic
MI_VoidMagic
```

필요 texture library:

```txt
Client/Bin/Resource/Texture/FX/Common/
  magic_brush_01.png
  magic_noise_01.png
  magic_erode_01.png
  magic_mask_pack_01.png
  magic_flow_01.png
```

완료 기준:
- 같은 shader + 같은 mesh + 다른 material parameter로 최소 4종 분위기가 나온다.
- 새 texture를 많이 만들지 않고도 변주가 가능하다.

### Stage FX-7. Irelia / Ezreal 실제 FX 적용

목표:
- 문서상 개념이 아니라 챔피언 스킬에서 직접 보인다.

우선순위:
1. Irelia E beam / stun mesh.
2. Irelia R ult wave / sword trail.
3. Irelia Q mark pulse.
4. Ezreal Q projectile.
5. Ezreal R wide projectile.

완료 기준:
- DX12에서 위 FX가 spawn/update/render/lifetime까지 유지된다.
- DX11 legacy와 DX12 RHI가 같은 gameplay event를 공유한다.
- texture load 실패 시 fallback은 보이되, 대표 preset에서는 fallback이 발생하지 않는다.

### Stage FX-8. ImGui Tuning Panel

목표:
- 빌드 없이 색, intensity, erode, edge, scroll 값을 조절한다.

작업:
- `FxMaterialTunerPanel` 추가.
- 선택한 active FX instance의 material parameter 표시.
- preset 저장은 후순위, 우선 runtime tuning.

완료 기준:
- `EmissionIntensity`, `ErodeThreshold`, `EdgeWidth`, `Contrast`, `ScrollSpeed`를 실시간 조절한다.

### Stage FX-9. Resource / Performance 정리

목표:
- 테스트용 dynamic buffer path를 장기 리소스 구조로 승격한다.

작업:
- RHI mesh buffer를 upload/default heap 구조로 전환.
- texture cache lifetime 정리.
- descriptor allocator를 frame-retired/free-list 모델로 확장.
- 같은 material instance / texture / mesh의 중복 생성 제거.

완료 기준:
- 대표 FX를 반복 스폰해도 GPU resource leak이 없다.
- cache key와 shutdown path가 명확하다.

### Stage FX-10. 고급 FX는 그 다음

1차 목표 이후:
- Flipbook / SubUV.
- Flowmap.
- Color over life 1D texture.
- 6-way lit smoke.
- VAT.
- GPU particle / compute emitter.
- DrawIndirect.
- FX graph editor.

이 단계는 LoL식 1차 FX가 안정화된 뒤 들어간다.

## 5. 바로 다음 코드 작업

가장 가까운 다음 코딩 순서:

1. `FxMaterialDesc` 중립 헤더 추가.
2. `FxEmitterDesc`에 LoL-style material parameter 필드 정리.
3. `FxMeshComponent`, `FxBillboardComponent`, `FxBeamComponent`, `FxRibbonComponent`에 공통 material desc 연결.
4. `FxAsset.cpp` JSON parser 확장.
5. `CRHIFxSpriteRenderer`와 `CFxStaticMeshRenderer` shader parameter binding 통합.
6. embedded shader를 `Shaders/FX/M_VFX_Generic.hlsl` 쪽으로 분리할지 결정.
7. Irelia/Ezreal 대표 preset JSON 하나 작성.
8. Debug-DX12 Client smoke에서 대표 FX 확인.

## 6. 판단 기준

이제부터 DX12 관련 작업은 아래 질문으로 우선순위를 정한다.

```txt
이 작업이 LoL-style FX를 더 잘 보이게 하는가?
이 작업이 grayscale/mask/material parameter workflow를 가능하게 하는가?
이 작업이 Irelia/Ezreal 대표 FX의 DX12 재생을 막는 병목인가?
```

셋 중 하나에 해당하면 지금 한다.
셋 다 아니면 이후 엔진 DX12 parity 단계로 미룬다.

## 7. 한 줄 결론

DX12 마이그레이션의 현재 목표는 "엔진 전체 DX12 완성"이 아니라,
`AAA_VFX_GRAYSCALE_AND_SHADER.md`의 개념을 실제 Winters Client에서 재생 가능한 LoL-style FX pipeline으로 바꾸는 것이다.

Related master flow:
- `.md/TODO/05-07/FX개념!/FX_FLOW_LOL_ELDEN_MASTER_PLAN_2026_05_08.md`
