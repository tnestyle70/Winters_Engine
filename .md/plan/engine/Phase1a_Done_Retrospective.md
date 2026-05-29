# Phase 1a — Transform/Buffer 추상화 정리 회고

> **기간**: 2026-04-15 ~ 2026-04-16
> **상태**: ✅ 부분 완료 (Texture 추상화는 취소 — 기존 ResourceCache로 충분)
> **다음**: ImGui 전면 연동 (2026-04-17~)

---

## 산출물 (실제 머지된 것)

### ECS Transform 계층화
- `Engine/Public/ECS/Components/TransformComponent.h` — POD struct, 부모/자식 + Dirty 2단계 (`m_bLocalDirty`/`m_bWorldDirty`), 헬퍼 setter
- `Engine/Public/ECS/Systems/TransformSystem.h` — `class CTransformSystem : ISystem` (정확한 시그니처: `Execute(CWorld&, float)`)
- `Engine/Private/ECS/Systems/TransformSystem.cpp` — root → child DFS, `CWorld::ForEach<TransformComponent>` 활용, 매트릭스 곱셈은 dirty 체인만

### Buffer 추상화
- `Engine/Public/RHI/IBuffer.h` — `BufferType { Vertex, Index, Constant, Structured }`, `BufferUsage { Immutable, Dynamic, Default, Staging }`
- `Engine/Public/RHI/DX11/DX11VertexBuffer.h` + `Engine/Private/RHI/DX11/DX11VertexBuffer.cpp` — 복사 금지/이동 허용, Immutable+Dynamic 지원
- `Engine/Public/RHI/DX11/DX11IndexBuffer.h` + `Engine/Private/RHI/DX11/DX11IndexBuffer.cpp` — 16/32bit 인덱스
- `Engine/Public/RHI/DX11/DX11StructuredBuffer.h` (헤더 전용 템플릿) — Map/Unmap, SRV 자동 생성, `BoneMatrix` 구조체 + `CDX11BonePaletteBuffer` alias

### Sampler 추상화
- `Engine/Public/RHI/DX11/SamplerStateCache.h` + `Engine/Private/RHI/DX11/SamplerStateCache.cpp` — Point/Linear/Anisotropic × Wrap/Clamp 6프리셋, Meyer's singleton, `BindAllPS` 일괄 바인딩

---

## 취소된 것 (왜)

### `RHI/DX11/Texture.h/.cpp` — 신규 생성 취소
- 사유: `Engine/Public/Resource/Texture.h`에 이미 `Engine::CTexture`가 존재
- `Engine_Defines.h` L45 `using namespace Engine;`로 인해 cpp의 `CTexture` 심볼이 강제로 `Engine::CTexture`로 해결됨 → 이름 충돌 → "멤버 아님" 컴파일 에러 다수
- **결정**: 신규 파일 4개 전부 삭제, 기존 `Resource/Texture` 그대로 유지

### `RHI/DX11/TexturePool.h/.cpp` — 신규 생성 취소
- 사유: `Engine/Public/Resource/ResourceCache.h`의 `CResourceCache::LoadTexture(path)`가 이미 동일 역할 + 더 우수
- ResourceCache는 `NormalizePath`(슬래시/대소문자 정규화)도 보유 — 내가 만든 풀보다 정교
- **결정**: 신규 풀 폐기, 기존 ResourceCache 유지

---

## 발생했던 컴파일 에러 (디버깅 기록)

| # | 에러 | 원인 | 해결 |
|---|------|------|------|
| 1 | `D3D11_BUFFER_DESC desc = {};` 중복 선언 | DX11VertexBuffer.cpp 복붙 실수 5줄 | 중복 5줄 삭제 |
| 2 | `desc.Usgae = dynamic ? D3D11_BIND_SHADER_RESOURCE;` | 오타 + 삼항 false branch 누락 + Usage에 BindFlags 값 + BindFlags 라인 누락 | Usage/BindFlags 분리 + `D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT` |
| 3 | `device` vs `pDevice` 파라미터 불일치 | 함수 시그니처는 `pDevice`, 본문에서 `device` 사용 | `device` → `pDevice` 일괄 |
| 4 | `operator(const T&)` 등호 누락 | Texture.h L19 오타 | `operator=(...)` |
| 5 | TexturePool.h L38 `};` 세미콜론 누락 | 클래스 끝 세미콜론 누락 | `}` → `};` |
| 6 | TransformSystem `Update` vs ISystem `Execute` 시그니처 불일치 | 계획서가 ISystem.h를 확인 안 함 | `Update(World&, float)` → `Execute(CWorld&, float)`, `World::ForEach`/`HasComponent`/`GetComponent` 사용 |
| 7 | `Resource/Texture.cpp` dead stub (CreateFromFile/BindPS/BindVS) | RHI/DX11/Texture.cpp 코드가 잘못 머지/복붙됨, 헤더 미동기화 | dead stub 10줄 삭제 |
| 8 | `'WICTextureLoader.h': No such file` | RHI/DX11/Texture.cpp의 path-less include `<WICTextureLoader.h>` (vcpkg 표준은 `<directxtk/...>`) | 파일 자체 삭제로 해결 |
| 9 | `IBuffer.cpp` / `DX11StructuredBuffer.cpp` 빈 파일 vcxproj 참조 | 빈 파일 삭제 후 vcxproj/filters 동기화 미실시 | vcxproj + filters 양쪽 참조 제거 |

---

## 학습 (다음 계획서부터 적용)

### 1. 기존 코드 전수 스캔 의무 (★ 최우선)
**Phase 1a 사고의 근본 원인**: 새 추상화 만들기 전 `Resource/Texture` + `ResourceCache` 존재를 확인 안 함.

**규칙**: 새 기능 계획 시 **먼저** 다음 4폴더를 grep:
- `Engine/Public/Resource/`
- `Engine/Public/Core/`
- `Engine/Public/Framework/`
- `Engine/Public/Renderer/`

`grep -rn "class.*Name"` 한 번이면 중복 발견. 이걸 안 하면 또 같은 사고 반복.

### 2. 인터페이스/시그니처 먼저 확인
ISystem의 메서드가 `Update`인지 `Execute`인지, 파라미터가 `World`인지 `CWorld`인지 — 이런 건 **계획서 쓰기 전**에 헤더 1개만 읽으면 즉시 확인 가능. 안 하고 가정으로 계획서 쓰면 사용자가 그대로 작성 → 빌드 깨짐 → 다시 작성. 시간 낭비.

### 3. 컴파일 에러 디버깅 5단계 절차
1. **에러 메시지의 함수명/심볼명을 grep** — 정의/호출/선언 위치 동시 식별
2. **에러난 파일 전체 read** — 부분 read 금지 (특히 첫 만남)
3. **MSBuild 출력에서 실제 컴파일 경로 확인** — 캐시/다른 트리 가능성 배제
4. **헤더-cpp 시그니처 1:1 대조** — `grep "ClassName::"` 으로 정의 vs 헤더 선언 비교
5. **에러가 여러 줄이면 file별로 분리** — IDE가 묶어 보여줘도 origin 구분

### 4. 파일 삭제 시 vcxproj + filters 동시 동기화
파일 rm 후 반드시:
- `Engine.vcxproj`에서 `<ClCompile>` / `<ClInclude>` 항목 제거
- `Engine.vcxproj.filters`에서도 동일 항목 제거
- 누락 시 "소스 파일을 열 수 없습니다" 에러 발생

### 5. 컨벤션 강제 장치 (winters-skills 업데이트 완료)
- `code-scaffolding/SKILL.md`: 네이밍 4분법 (파일 C금지/클래스 C필수/POD C금지/인터페이스 I), 타입 alias, ImGui 적극 활용
- `code-scaffolding/references/gotchas.md`: #21~#24 추가, #14 ImGui 정책 반전
- `code-review/SKILL.md`: 체크리스트 + 빈번한 컴파일 에러 패턴 5종

---

## 다음 단계

### Phase 1b: ImGui 전면 연동 (2026-04-17~)
- 상세: `IMGUI_INTEGRATION_PLAN.md`
- 우선순위: Entity Inspector → Profiler Overlay → Material Tuner → Animation Blender → Console

### Phase 1c: JobSystem 강화 (ImGui 이후)
- 상세: `Phase1b_JobSystem_Enhancement.md` (이전 작성)
- Counter API + Work-stealing deque + Thread 백엔드 → Fiber 교체 가능 인터페이스
- 첫 병렬화: TransformSystem 서브트리 + 캐릭터 애니메이션

### Phase 2: RenderGraph + Deferred (Phase 1c 이후)
- G-Buffer + Clustered Lighting + CSM + PostFX
- 이때 Texture 추상화가 GBuffer MRT 컨텍스트에서 자연스럽게 다시 등장 — 그때 CResourceCache vs RHI texture wrapper 통합 결정

---

## 마무리

Phase 1a는 **부분 완료**지만 핵심 산출물(Transform 계층 + Buffer 추상화 + Sampler 캐시)은 전부 머지됨. Texture 부분은 기존 구조의 우수성 때문에 유지. 회고에서 도출한 **"기존 코드 전수 스캔" + "헤더 시그니처 사전 확인"** 두 가지 규칙이 이 작업의 가장 큰 자산.

내일부터 ImGui 적극 활용 시작 — 5명 캐릭터 Transform 슬라이더부터.
