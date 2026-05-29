# EFX-1 .wfx/.wmi 스키마와 Round-trip

작성일: 2026-05-07
상태: 구현 계획
의존:
- `02_EFX0_LEGACY_BRIDGE_AND_ASSETIZATION.md`
- `.md/plan/EffectTool/18_ASSET_LAYER_BAKE.md`
- `.md/plan/EffectTool/27_AAA_VFX_INSIGHTS_AND_MASTER_MATERIALS_BAKE.md`

목적:
- 현재 수동 string parser 기반 `.wfx` 로더를 structured JSON reader/writer로 교체한다.
- Master Material 3종과 Material Instance `.wmi`를 1차 워크플로우로 만든다.
- Load -> Save -> Load 의미 동일성을 검증한다.

---

## 1. 현재 문제

현재 파일:

```txt
Engine/Private/FX/FxAsset.cpp
```

현재 parser 특성:

```txt
1. `"key"` 검색 후 뒤쪽 colon/string/number를 찾는다.
2. emitter block은 brace depth를 손으로 추적한다.
3. nested object, escaped string, bool/null, canonical writer가 없다.
4. schema validation과 error list가 없다.
5. material instance와 master material parameter layout이 없다.
```

따라서 EFX-1에서는 기존 parser 확장이 아니라 별도 structured JSON reader/writer를 만든다.

---

## 2. 신규 파일

```txt
Engine/Public/FX/v2/Json/FxJsonValue.h
Engine/Public/FX/v2/Json/FxJsonReader.h
Engine/Public/FX/v2/Json/FxJsonWriter.h

Engine/Private/FX/v2/Json/FxJsonReader.cpp
Engine/Private/FX/v2/Json/FxJsonWriter.cpp

Engine/Public/FX/v2/Asset/FxSystemAsset.h
Engine/Public/FX/v2/Asset/FxEmitterAsset.h
Engine/Public/FX/v2/Asset/FxAssetJsonLoader.h

Engine/Private/FX/v2/Asset/FxSystemAsset.cpp
Engine/Private/FX/v2/Asset/FxAssetJsonLoader.cpp

Engine/Public/FX/v2/Material/FxMaterialInstance.h
Engine/Public/FX/v2/Material/FxMaterialInstanceJsonLoader.h
Engine/Public/FX/v2/Material/FxMasterMaterialRegistry.h

Engine/Private/FX/v2/Material/FxMaterialInstance.cpp
Engine/Private/FX/v2/Material/FxMaterialInstanceJsonLoader.cpp
Engine/Private/FX/v2/Material/FxMasterMaterialRegistry.cpp
```

주의:

```txt
Engine/Public/FX/v2는 Engine public header다.
subdirectory include는 반드시 "FX/v2/..." 경로를 포함한다.
ID3D11/ID3D12 include 금지.
```

---

## 3. JSON reader 범위

지원 범위:

```txt
object
array
string
number
bool
null
escaped quote/backslash/basic unicode pass-through
```

지원하지 않는 것:

```txt
comments
trailing comma
NaN/Infinity
binary blob inline
```

이유:

```txt
1. C++ 표준에는 JSON이 없다.
2. 신규 third-party 도입은 THIRDPARTY 절차가 필요하다.
3. EffectTool asset은 우리가 작성하는 JSON이므로 작은 recursive descent parser로 충분하다.
```

---

## 4. .wfx v1 스키마

System asset:

```json
{
  "schema": "WintersFxSystem",
  "version": 1,
  "name": "Irelia_Q_Trail",
  "domain": "LoL",
  "deterministic": false,
  "duration": 1.0,
  "emitters": [
    {
      "name": "TrailSprite",
      "enabled": true,
      "simulation": "CPU",
      "maxParticles": 1,
      "spawn": {
        "mode": "Burst",
        "count": 1,
        "rate": 0.0
      },
      "renderer": {
        "type": "Sprite",
        "materialInstance": "Client/Bin/Resource/FX/LoL/Irelia/MI_Irelia_Q_Trail.wmi",
        "texture": "Client/Bin/Resource/Texture/FX/Irelia/q_mark_pulse_erode.png",
        "blend": "Additive",
        "depthWrite": false,
        "billboard": true
      },
      "lifetime": 0.35,
      "size": [1.0, 1.0],
      "color": [1.0, 1.0, 1.0, 1.0],
      "uvScroll": [0.0, 0.0],
      "atlas": {
        "cols": 1,
        "rows": 1,
        "frames": 1,
        "fps": 0.0,
        "loop": true
      }
    }
  ]
}
```

필수 key:

```txt
schema
version
name
emitters
emitters[].name
emitters[].renderer.type
emitters[].renderer.materialInstance
emitters[].lifetime
```

선택 key:

```txt
domain
deterministic
duration
spawn
renderer.texture
renderer.model
renderer.blend
renderer.depthWrite
atlas
```

---

## 5. .wmi v1 스키마

Material Instance:

```json
{
  "schema": "WintersFxMaterialInstance",
  "version": 1,
  "name": "MI_Irelia_Q_Trail",
  "master": "M_VFX_Particle_Generic",
  "lightingModel": "UnlitAdditive",
  "parameters": {
    "TintColor": [0.8, 1.2, 2.4, 1.0],
    "EmissionColor": [0.6, 1.0, 3.5, 1.0],
    "EmissionIntensity": 8.0,
    "Contrast": 2.2,
    "DissolveThreshold": 0.0,
    "EdgeWidth": 0.08,
    "FresnelPower": 3.0,
    "UvScrollA": [0.0, 0.5],
    "DistortionStrength": 0.03,
    "SoftParticleDistance": 1.0
  },
  "textures": {
    "MainMask": "Client/Bin/Resource/Texture/FX/Irelia/q_mark_pulse_erode.png",
    "NoiseA": "Client/Bin/Resource/Texture/FX/Common/noise_soft_01.png",
    "NoiseB": "Client/Bin/Resource/Texture/FX/Common/noise_warp_02.png"
  }
}
```

핵심 파라미터 그룹:

```txt
Core
  TintColor
  EmissionColor
  EmissionIntensity
  Contrast
  DissolveThreshold
  UvScrollA
  DistortionStrength
  FresnelPower

Advanced
  EdgeWidth
  EdgeColor
  CenterMaskPower
  SoftParticleDistance
  AtlasBlend
  RandomVariation
  PolarRotation
  SixWayLightingScale
```

---

## 6. Master Material registry

Master 3종:

```txt
M_VFX_Particle_Generic
  Sprite, billboard, ground decal, shockwave ring.
  LoL FX 1차 default.

M_VFX_Trail
  Ribbon, beam, slash mesh strip.
  Yasuo/Irelia/Ezreal projectile trail 1차.

M_VFX_Volumetric
  Smoke, fog, spell cloud, Elden boss volume.
  EFX-5 이후 강화.
```

EFX-1에서는 registry가 HLSL을 compile하지 않는다. descriptor와 parameter layout만 제공한다.

---

## 7. Canonical writer 규칙

```txt
1. slash는 `/`로 쓴다.
2. object key order는 schema order를 따른다.
3. unknown key는 load result warning으로 모으고 canonical save에서는 제거한다.
4. float는 기본 `%.6g` 수준으로 저장한다.
5. color는 배열 4개로 저장한다.
6. path는 SolutionDir 기준 상대 경로를 기본으로 한다.
7. 빈 optional field는 저장하지 않는다.
```

Semantic equality:

```txt
Load A
Save canonical B
Load B
Compare semantic fields
Save canonical C
Compare B text == C text
```

raw source A와 canonical B의 text equality는 요구하지 않는다.

---

## 8. 구현 단계

### EFX1-1. JSON value/parser/writer

완료 기준:

```txt
[ ] object/array/string/number/bool/null parse
[ ] parse error line/column
[ ] writer canonical key order 지원
[ ] unit smoke JSON 10개 parse/save
```

### EFX1-2. .wmi loader

완료 기준:

```txt
[ ] master name resolve
[ ] scalar/vector/texture parameter parse
[ ] unknown parameter warning
[ ] byte layout pack은 EFX-3 전까지 descriptor level로 유지
```

### EFX1-3. .wfx loader

완료 기준:

```txt
[ ] system name/version parse
[ ] emitter array parse
[ ] renderer type Sprite/Mesh/Ribbon/Beam parse
[ ] materialInstance path resolve
[ ] v1 FxAsset compatibility adapter 생성
```

### EFX1-4. 기존 registry 연결

`CFxAssetRegistry::LoadFromFile`은 v1 호환을 위해 유지한다. v2 loader는 별도 path로 시작한다.

권장 API:

```cpp
namespace Winters::FX::v2
{
    class CFxAssetJsonLoader
    {
    public:
        static FxSystemAssetLoadResult LoadSystemFromFile(const wstring_t& path);
        static std::string SaveSystemToString(const CFxSystemAsset& asset);
    };
}
```

완료 기준:

```txt
[ ] v1 CFxAssetRegistry load path 유지
[ ] v2 loader 별도 추가
[ ] EFX-0 dump asset을 v2 loader로 load 가능
[ ] canonical round-trip 통과
```

---

## 9. 검증 명령

```powershell
rg "ExtractString|ExtractNumber|ExtractEmitterBlocks" Engine/Private/FX/v2
rg "미결정|추측값|빈 구현" .md/TODO/05-07/EffectTool/03_EFX1_WFX_WMI_SCHEMA_AND_ROUNDTRIP.md
rg "ID3D11|ID3D12|CDX11Device" Engine/Public/FX/v2
```

기대:

```txt
1. Engine/Private/FX/v2에서 기존 수동 parser helper 이름 0 hit.
2. Engine/Public/FX/v2에서 native graphics API 0 hit.
3. 본 문서의 결정 영역에 미결정 항목 0.
```

완료 기준:

```txt
[ ] .wfx 3개 load/save/load 통과
[ ] .wmi 3개 load/save/load 통과
[ ] Irelia/Yasuo/Ezreal EFX-0 dump 산출물 canonical 저장
[ ] 기존 Client 빌드 영향 없음
```
