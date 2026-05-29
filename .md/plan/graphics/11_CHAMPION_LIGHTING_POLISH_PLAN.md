# 챔피언 렌더링 찰흙빛 수정 계획

> 기준일: 2026-05-06  
> 목표: 맵 submesh 복구 이후, 챔피언이 회색/찰흙/플라스틱처럼 보이는 문제를 조명, 컬러 스페이스, 머티리얼 데이터 관점에서 순서대로 줄인다.

---

## 0. 현재 코드 기준 진단

### 확인된 사실

- PBR 셰이더는 `Shaders/Mesh3D_PBR.hlsl`, `Shaders/Skinned3D_PBR.hlsl`에 있다.
- PBR 재질 기본값은 `CBPerMaterial` 기준 metallic 0.0, roughness 0.5, AO 1.0이다.
- 조명 값은 `ModelRenderer::UpdateCamera()` 안에 하드코딩되어 있다.
  - directional: `lightDirWorld = {-0.45, -1.0, 0.30}`, intensity 2.0
  - point light 4개가 고정 위치와 색으로 항상 들어간다.
- PBR 픽셀 셰이더는 albedo를 그대로 사용하고, 마지막에 tone map + gamma encode를 수행한다.
- 현재 챔피언 등록은 PBR/Unlit이 섞여 있다.
  - Yone/legacy 다수: `Mesh3D_PBR.hlsl`
  - Ezreal/Fiora/Jax 등 일부 pure ECS: `Mesh3D.hlsl`
- normal/metallic/roughness/AO 텍스처는 아직 실질적으로 머티리얼 단위로 붙지 않는다.

### 증상 후보

| 후보 | 가능성 | 이유 |
|---|---:|---|
| 컬러 스페이스 불일치 | 높음 | PNG albedo가 sRGB인데 shader/loader 정책이 명시되어 있지 않다. |
| 재질 파라미터 단일화 | 높음 | 모든 PBR 챔피언이 같은 roughness/metallic/AO 기본값으로 보인다. |
| 하드코딩 전역 조명 | 높음 | 테스트용 point light 4개와 강한 directional이 챔피언별 룩을 평평하게 만들 수 있다. |
| normal map 부재 | 중간 | 피부/천/금속의 표면 결이 없어 찰흙처럼 보인다. |
| IBL/hemisphere ambient 부재 | 중간 | 그림자 영역에 실제 환경광이 없어 단조롭다. |
| PBR/Unlit 혼재 | 중간 | 챔피언마다 밝기와 색 재현이 다른 기준으로 나온다. |

---

## 1. 수정 방향

### Phase R0. 렌더 디버그 시야 확보

먼저 눈대중 튜닝을 막는다.

- ImGui `Lighting Debug` 패널 추가
- 토글:
  - Final
  - Albedo Only
  - NdotL
  - Ambient Only
  - Direct Only
  - AO Only
  - Roughness/Metallic Preview
- 전역 조명 슬라이더:
  - directional intensity
  - directional color
  - ambient intensity
  - point light enable

합격 기준:
- 같은 카메라, 같은 챔피언으로 Final/Albedo/Direct/Ambient 비교 스크린샷을 남길 수 있다.

### Phase R1. 컬러 스페이스 정책 고정

albedo는 sRGB, normal/MR/AO는 linear로 고정한다.

선택지:
- A안: Texture loader에서 albedo SRV를 `_SRGB` 포맷으로 생성한다.
- B안: shader에서 `SRGBToLinear()` 후 마지막에 `LinearToSRGB()`를 수행한다.

권장:
- 단기: B안으로 shader 명시 변환. 현재 `CTexture::Create()` 호출부가 텍스처 용도를 모르기 때문이다.
- 중기: `eTextureUsage { Albedo, Normal, MetallicRoughness, AO, UI, FX }` 추가 후 SRV 포맷을 loader에서 결정.

합격 기준:
- `Albedo Only` 출력이 기존 unlit 텍스처 색과 눈에 띄게 어긋나지 않는다.

### Phase R2. 테스트용 point light 제거 및 MOBA 기본 라이트 리그

`ModelRenderer::UpdateCamera()`의 테스트 point light 4개를 기본 OFF로 내리고, 씬 단위 라이트 리그로 이동한다.

기본값 제안:

```cpp
directionalDir = normalize({ -0.35f, -1.0f, 0.25f });
directionalColor = { 1.0f, 0.96f, 0.88f };
directionalIntensity = 1.2f;
ambientColor = { 0.42f, 0.48f, 0.56f };
ambientIntensity = 0.08f;
```

합격 기준:
- 챔피언 피부/천 영역이 과하게 회색으로 뭉치지 않는다.
- 스킬 FX가 없을 때는 point light가 0개여야 한다.

### Phase R3. ChampionDef에 렌더 머티리얼 추가

챔피언별 기본 재질을 등록 데이터로 이동한다.

```cpp
struct ChampionRenderMaterial
{
    f32_t metallic = 0.0f;
    f32_t roughness = 0.62f;
    f32_t ambientOcclusion = 1.0f;
    Vec3 albedoTint = { 1.0f, 1.0f, 1.0f };
    Vec3 emissiveTint = { 0.0f, 0.0f, 0.0f };
    f32_t emissiveIntensity = 0.0f;
};
```

우선순위:
- Yone: 검/몸/장식 submesh별 roughness 분리
- Ezreal/Fiora/Jax: PBR opt-in 후 기본 재질 맞춤
- Annie/Ashe: 피부/천/금속 분리

합격 기준:
- 모든 pure ECS 챔피언이 같은 기본 roughness로 묶이지 않는다.

### Phase R4. Normal/MR/AO 텍스처 경로 확장

현재는 color map 중심이다. 다음 단계에서 per-submesh 텍스처를 확장한다.

- `texturePath[]` 유지
- `normalPath[]`
- `metallicRoughnessPath[]`
- `aoPath[]`

합격 기준:
- normal map 없는 챔피언도 fallback으로 정상 렌더링된다.
- normal/MR/AO가 있는 챔피언은 `Lighting Debug`에서 채널 확인 가능하다.

### Phase R5. IBL은 후순위

전역 조명 문제로 보이지만, 바로 IBL로 뛰면 디버깅 변수가 너무 많다.

순서:
1. 컬러 스페이스
2. 라이트 리그
3. 챔피언별 material
4. normal/MR/AO
5. IBL/hemisphere 확장

---

## 2. 이번 맵 submesh 수정과의 관계

맵 `sr_base_flip.wmesh`는 `submesh_count=1268`이다. 기존 visibility mask와 `.wmesh` loader 방어값은 256 기준이라 큰 맵에 부족했다.

이번 수정 기준:
- visibility mask 한도: 256 → 2048
- all-visible mask는 빠르게 전체 렌더 경로로 우회
- `.wmesh` loader submesh 방어값: 256 → 2048

주의:
- 맵은 1268 draw call 규모라 전체 submesh 복구 후 프레임 비용이 증가할 수 있다.
- 후속 최적화는 map chunk 병합, static batching, visibility culling, `.wmesh` material table 정리 순서로 간다.

---

## 3. 완료 기준

- 맵 가장자리 벽, 소환사의 제단, 포탑/억제기/넥서스 배치가 다시 보인다.
- Yone/Fiora/Ezreal 중 최소 1체씩 Final/Albedo/Direct/Ambient 비교가 가능하다.
- PBR 챔피언이 회색 찰흙 느낌이 아니라, texture color와 재질 차이를 유지한다.
- 렌더 수정은 서버/ECS/스킬 로직에 영향이 없어야 한다.
