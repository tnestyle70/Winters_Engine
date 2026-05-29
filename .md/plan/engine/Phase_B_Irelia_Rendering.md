# Winters Engine — Irelia FBX 렌더링 + 엔진 코어 구축 계획

## Context

DX9 마인크래프트 던전스 프로젝트에서 블럭을 띄우고 움직였던 것처럼, 이번에는 DX11 Winters Engine에서 LoL 이렐리아 캐릭터를 FBX로 로드해서 스키닝 애니메이션까지 화면에 띄우는 것이 목표. 기존 DX9의 Management > Scene > GameObject > Component 아키텍처를 DX11에 맞게 재설계한다.

---

## 핵심 의사결정 4가지

### 1. Fiber JobSystem 먼저? → **아니요, 렌더링 먼저**
- CEngineApp 게임 루프가 이미 동작 중이고 싱글스레드로 충분
- 이렐리아 한 캐릭터 렌더링에 병렬화 불필요
- Fiber는 이렐리아 화면 출력 이후 별도 Phase로 진행

### 2. CBase AddRef/Release(DX9) vs smart pointer? → **smart pointer**
- CLAUDE.md 컨벤션: private 생성자 + `Create()` → `unique_ptr` 반환
- 기존 Network SDK 전체가 이미 이 패턴
- `Safe_Release`는 COM 객체(ID3D11*)에만 사용

### 3. ECS vs GameObject/Component? → **단계적 접근**
- Phase B: `ModelRenderer` pImpl로 빠르게 이렐리아 화면 출력 (기존 CubeRenderer 패턴)
- Phase C (이후): 본격 GameObject/Component/Scene 시스템 구축
- 기존 ECS(CWorld, ComponentStore)는 향후 게임 로직용으로 공존

### 4. Assimp 설치? → **vcpkg**
- `vcpkg install assimp:x64-windows` — 종속성(zlib, minizip) 자동 해결
- `vcpkg install directxtk:x64-windows` — WICTextureLoader로 PNG/DDS 로드

---

## Phase B-1: 정적 메시 렌더링 (T-pose) — 1~2일

**결과물**: 이렐리아 T-pose + 디퓨즈 텍스처 화면 출력

### 파일

| 동작 | 파일 | 내용 |
|------|------|------|
| 수정 | `Engine/Public/Engine_Struct.h` | VTXMESH(44B), VTXANIM(76B) 정점 구조체 |
| 수정 | `Engine/Public/RHI/DX11/DX11Pipeline.h/.cpp` | CreateMesh(), CreateSkinnedMesh() 입력 레이아웃 |
| 생성 | `Engine/Public/Resource/CMesh.h` + `.cpp` | VB/IB 래핑, 서브메시, Render() |
| 생성 | `Engine/Public/Resource/CTexture.h` + `.cpp` | SRV + Sampler, WIC/DDS 로드 |
| 생성 | `Engine/Public/Resource/CModel.h` + `.cpp` | Assimp FBX 로더, 재귀 노드 순회 |
| 생성 | `Shaders/Mesh3D.hlsl` | Position+Normal+UV, 디렉셔널 라이트 |
| 생성 | `Engine/Include/ModelRenderer.h` + `.cpp` | pImpl, Init/Render |
| 수정 | `Client/Public/CGameApp.h` + `.cpp` | ModelRenderer 멤버 추가 |

### 검증
```
[ ] Assimp vcpkg 설치 + Engine 링크 성공
[ ] 이렐리아 FBX 로드 → 정점/인덱스 추출 로그 확인
[ ] 화면에 T-pose 메시 출력 (텍스처 O)
[ ] 카메라 회전으로 모델 확인
```

---

## Phase B-2: 스켈레톤 + GPU 스키닝 — 1~2일

**결과물**: 같은 T-pose지만 스키닝 파이프라인 통과 (본 가중치 검증)

### 파일

| 동작 | 파일 | 내용 |
|------|------|------|
| 생성 | `Engine/Public/Resource/CBone.h` | 본 데이터 (이름, 부모, 오프셋 행렬) |
| 생성 | `Engine/Public/Resource/CSkeleton.h` + `.cpp` | 본 계층구조, ComputeFinalTransforms() |
| 수정 | `Engine/Public/Resource/CModel.cpp` | ProcessMesh에서 aiBone 추출, VTXANIM 전환 |
| 수정 | `Engine/Public/RHI/DX11/DX11ConstantBuffer.h` | CBBoneMatrices { XMFLOAT4X4 bones[256]; } |
| 생성 | `Shaders/Skinned3D.hlsl` | cbuffer b2 본 행렬, 정점 스키닝 |
| 수정 | `Engine/Private/Renderer/ModelRenderer.cpp` | 스키닝 경로 분기, CBBoneMatrices 업로드 |

### 검증
```
[ ] 본 계층구조 로그 출력 (root → child 트리)
[ ] 스키닝 파이프라인 T-pose == Phase B-1 T-pose (동일해야 정상)
[ ] 본 가중치 합 == 1.0 검증
```

---

## Phase B-3: 애니메이션 재생 — 2~3일

**결과물**: 이렐리아 Idle 애니메이션 루프 재생

### 파일

| 동작 | 파일 | 내용 |
|------|------|------|
| 생성 | `Engine/Public/Resource/CAnimation.h` + `.cpp` | 키프레임 데이터, Evaluate() (Lerp/Slerp) |
| 생성 | `Engine/Public/Resource/CAnimator.h` + `.cpp` | 재생 상태, Update(dt), PlayAnimation() |
| 수정 | `Engine/Public/Resource/CModel.cpp` | aiScene::mAnimations 로드 |
| 수정 | `Engine/Include/ModelRenderer.h` | Update(dt), PlayAnimation(index) 추가 |
| 수정 | `Engine/Private/Renderer/ModelRenderer.cpp` | Animator 통합, 매 프레임 본 행렬 업로드 |
| 수정 | `Client/Private/CGameApp.cpp` | OnUpdate에서 m_Irelia.Update(dt) 호출 |

### 검증
```
[ ] 이렐리아 Idle 애니메이션 재생
[ ] 부드러운 루프 (끊김 없음)
[ ] 다른 애니메이션으로 전환 (PlayAnimation(1))
[ ] 안정적 프레임레이트 유지
```

---

## Phase B-4: 텍스처/머티리얼 완성 — 0.5일

| 동작 | 파일 | 내용 |
|------|------|------|
| 수정 | `Engine/Private/Resource/CTexture.cpp` | 폴백 텍스처 (1x1 white), aniso 필터링 |
| 수정 | `Engine/Private/Renderer/ModelRenderer.cpp` | 서브메시별 머티리얼 바인딩 |

---

## 이후 Phase (B 이후)

### Phase C: GameObject/Component/Scene 시스템
- CComponent 베이스 (smart pointer 기반)
- CGameObject: 컴포넌트 컨테이너
- CScene/CLayer: 오브젝트 관리
- CRenderer: 렌더 그룹 (NonAlpha, Alpha, UI)

### Phase D: Fiber JobSystem 통합
- 기존 CJobSystem을 Fiber 기반으로 업그레이드
- SystemScheduler와 통합
- 비동기 에셋 로딩

---

## 전체 파일 목록

### 신규 (17개)
```
Engine/Public/Resource/CMesh.h          + Private/Resource/CMesh.cpp
Engine/Public/Resource/CTexture.h       + Private/Resource/CTexture.cpp
Engine/Public/Resource/CModel.h         + Private/Resource/CModel.cpp
Engine/Public/Resource/CBone.h          (header-only)
Engine/Public/Resource/CSkeleton.h      + Private/Resource/CSkeleton.cpp
Engine/Public/Resource/CAnimation.h     + Private/Resource/CAnimation.cpp
Engine/Public/Resource/CAnimator.h      + Private/Resource/CAnimator.cpp
Engine/Include/ModelRenderer.h          + Private/Renderer/ModelRenderer.cpp
Shaders/Mesh3D.hlsl
Shaders/Skinned3D.hlsl
```

### 수정 (6개)
```
Engine/Public/Engine_Struct.h           — VTXMESH, VTXANIM 추가
Engine/Public/RHI/DX11/DX11Pipeline.h   — CreateMesh/CreateSkinnedMesh 추가
Engine/Private/RHI/DX11/DX11Pipeline.cpp
Engine/Public/RHI/DX11/DX11ConstantBuffer.h — CBBoneMatrices 추가
Client/Public/CGameApp.h               — ModelRenderer 멤버
Client/Private/CGameApp.cpp            — Init/Update/Render 통합
```

---

## 주의사항

1. **Assimp aiMatrix4x4는 column-major** → XMFLOAT4X4 복사 시 전치 필요
2. **aiBone::mOffsetMatrix**는 inverse bind pose → `final[i] = offset[i] * worldTransform[i]`
3. **mTicksPerSecond == 0이면 25.0으로 기본값** 사용
4. **VTXMESH(44B) vs VTXANIM(76B)** stride 불일치 시 GPU 행 발생
5. **CBBoneMatrices(256 × 64B = 16KB)** — DX11 cbuffer 64KB 제한 이내
6. **`#define new DBG_NEW`** 충돌 — Assimp 헤더 include 시 `#pragma push_macro("new") / #undef new` 필요 (json.hpp와 동일 패턴)
