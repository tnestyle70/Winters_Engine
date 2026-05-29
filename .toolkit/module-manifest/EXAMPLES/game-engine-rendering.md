# Renderer Module

> **EXAMPLE** — 게임 엔진 (DX11 기반) 의 렌더링 모듈 예시.
> Winters Engine 의 `Engine/Public/Renderer/` 를 기반으로 작성.
> 회사 코드베이스의 동급 모듈 (UE5 의 Renderer / Unity 의 SRP / 자체 엔진의 Renderer) 에 비슷하게 적용 가능.

---

## 책임 (Responsibility)

DX11 기반 mesh / skinned mesh / UI / particle 렌더링 사이클.
RenderGraph (Phase 2 도입 예정) 진입 전까지 직접 Input Assembler / Pixel Shader bind 패턴.
캡슐화 단위: `ModelRenderer` (인스턴스 생성), `FxSystem` (particle), `PlaneRenderer` (지면 quad), `BillboardSystem` (카메라 facing).

---

## 진입점 (Entry Points)

- `CModelRenderer::Create(...)` at `ModelRenderer.h:54` — 챔프/미니언 instance 생성 시
- `CModelRenderer::PlayAnimationByName(name)` at `ModelRenderer.h:72` — 애니 키 전환
- `CModelRenderer::Render(ctx)` at `ModelRenderer.h:88` — 매 프레임 호출
- `CFxSystem::Spawn(desc)` at `FxSystem.h:31` — particle / FX 스폰
- `CFxSystem::Render(ctx)` at `FxSystem.h:42` — particle 렌더 (post-mesh)
- `CPlaneRenderer::Create(...)` at `PlaneRenderer.h:25` — 지면 quad (AttackRange / FX 빌보드)
- `CPlaneRenderer::Render(ctx)` at `PlaneRenderer.h:38` — quad 렌더

---

## 의존성 (Dependencies)

### Public (헤더 노출)

- `RHI` — `IBuffer`, `CDX11Device`, `DX11Pipeline` 인터페이스 (헤더 멤버로 노출)
- `Resource` — `CModel`, `CTexture`, `CAnimator` (renderer 가 직접 보유)
- `ECS` — `RenderComponent`, `TransformComponent`, `FxBillboardComponent` (시스템 통신)

### Private (구현만)

- `Profiler` — `WINTERS_PROFILE_SCOPE` 매크로 (.cpp 안)
- `Core/Logging` — 디버그 로그 출력
- `Manager/Resource` — 셰이더 cache 조회

### Forward-Decl Only

- `Editor/ImGuiLayer` — class CImGuiLayer; (DebugDraw 시 포인터만)
- `Scene/Scene_Manager` — class CScene_Manager; (callback 등록용)

---

## 의존받음 (Depended By)

- `Client/Scene/Scene_InGame` — 매 프레임 Render 호출
- `Client/Manager/Champions/Yasuo` — Yasuo 전용 FX preset
- `Client/Manager/Champions/Garen` — Garen Q sweep FX
- (모든 챔프 매니저)

---

## Common Tasks (AI 매핑)

- "신규 셰이더 추가" → `Shaders/{Name}.hlsl` 작성 + `DX11Shader::Create(path)` ResourceCache 등록
- "FX 빌보드 신규" → `FxSystem::Spawn(desc)` + `FxBillboardComponent` 패턴 미러
- "신규 메시 타입" → `CModel::LoadModel` 분기 + `RenderComponent::bAnimated` flag 추가
- "지면 quad (AOE)" → `CPlaneRenderer::Create + Render` 호출, yaw 회전은 `XMMatrixRotationY` 적용
- "데미지 팝업 폰트" → `CDamageFontRenderer` (Phase C-2 신규) 진입

---

## 함정 (Gotchas)

- **HLSL 수정 후 OutDir 동기화 필수** — MSBuild incremental 가 .hlsl 변경 미감지 → OutDir 옛 .hlsl 그대로 → `D3DCompileFromFile` 실패 + `__debugbreak`
  / 해결: ① `cp Shaders/*.hlsl Client/Bin/Debug/Shaders/` 직접 ② Rebuild Solution ③ vcxproj PostBuild 의 xcopy 에 `/Y` 강제
- **Mesh3D.hlsl `clip(texColor.a - 0.05)`** — alpha 0 영역 픽셀 버려짐. LoL render/*.png 같은 sprite 캡처가 mesh material 로 사용되면 화면 0 픽셀
  / 해결: 진짜 material texture (`*_texture.png`) 를 `SetMeshTexture` 로 바인딩. render/*.png 는 빌보드 전용
- **Skinned 정점 76B layout 변경 금지** — `BLENDINDICES uint32×4 @ 44 + BLENDWEIGHT @ 60` byte offset. POD 변경 시 IL 동시 갱신 안 하면 vertex 가 garbage 로 collapse → 메시 소리없이 사라짐
  / 해결: vertex 포맷 추가 시 IL 부터 read off 후 POD 설계
- **CPlaneRenderer 기본 CULL_BACK** — yaw 회전 quad 가 카메라 각도 따라 사라짐
  / 해결: `D3D11_CULL_NONE` TwoSided RasterizerState 생성 + Render 에 backup/bind/restore

---

## 외부 노출 API (DLL boundary)

- **노출** (`WINTERS_ENGINE` 마크): `CModelRenderer`, `CPlaneRenderer`, `CFxSystem`, `CBillboardSystem`
- **비노출** (내부 구현): `BlendStateCache`, `SamplerStateCache`, RHI raw pointer 캐시 — `CGameInstance::Get_*` 게터 경유만

---

## Plugin 메타 (Phase G 이후)

- **소속 Plugin**: Engine Core (필수, 항상 로드)
- **LoadingPhase**: `Default`
- **EnabledByDefault**: `true`

---

## 핵심 파일 (Top 5)

1. `ModelRenderer.h/.cpp` — 인스턴스 생성 + Animate + Render 사이클
2. `FxSystem.h/.cpp` — particle spawn / update / render
3. `PlaneRenderer.h/.cpp` — 지면 quad 렌더
4. `BillboardSystem.h/.cpp` — 카메라 facing 계산
5. `Shaders/Mesh3D.hlsl`, `Shaders/Skinned3D.hlsl` — 핵심 셰이더

---

## 관련 계획서 / 문서

- `.md/plan/engine/RENDERGRAPH_PLAN.md` — Phase 2 RenderGraph 도입 (예정)
- `.md/architecture/WINTERS_GAMEPLAY_ARCHITECTURE.md §렌더` — 7-layer 구조
- `.md/guide/CHAMPION_WMESH_PIPELINE_GUIDE.md` — 챔프 메시 변환

---

## 성능 특성

- 100 instance × Skinned3D: ~3ms / frame (1 worker)
- AnimUpdate 병렬화 (Phase 5-B 후): -2ms 예상
- 셰이더 hot reload: ~50ms (D3DCompileFromFile)
