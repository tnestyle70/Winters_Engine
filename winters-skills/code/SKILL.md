---
name: code
description: >
  WintersEngine 의 코드 작성 + 리뷰 사이클 통합 가이드. 기존 인프라 식별 → 데이터 형태부터 정의 →
  DLL 경계 보수 → 검증 결정 포인트 → 최소 수정 → 엣지 케이스 검토 순.
  "구현해줘", "리뷰해줘", "이거 어떻게 작성", "검토 부탁" 등에 트리거.
  Codex 의 코드 작성/리뷰 패턴을 흡수해 코드 추론 함정 회피.
---

# Skill: Code — WintersEngine

코드 작성 (FX/시스템/매니저 등) 과 코드 리뷰 (PR/패치) 양쪽에 적용. 디버깅 사이클은 별도 [debug-pipeline](../debug-pipeline/SKILL.md) 참조.

## 핵심 원칙

**기존 인프라 식별 → 데이터 형태 정의 → DLL 경계 보수 → 검증 결정 포인트 → 최소 수정 → 엣지 케이스 검토**

코드 흐름 추론으로 가설만 누적하지 말고 **실제 코드/데이터 직접 측정**.

---

## A. 코드 작성 (creation) — 6 단계

### 1. 기존 인프라 식별 우선 (★ 중복 방지)

새로 만들기 전에 **`Engine/Public/Resource/`, `Engine/Public/Core/`, `Engine/Public/Framework/`, `Engine/Public/Renderer/`, `Engine/Public/RHI/` 폴더 전수 grep**:
- 같은 책임의 매니저/유틸/enum 이미 있는지 (예: `BlendStateCache` 의 Additive/Premultiplied 가 이미 존재 — `eFxBlendMode` 신설 회피)
- 기존 패턴 (Mesh3D 셰이더 초기화, Pimpl, ResourceCache lookup) 그대로 복사

체크리스트:
- [ ] 4 폴더 grep 으로 동일 책임 클래스 검색
- [ ] CLAUDE.md 의 Gotchas 검색 — 같은 영역 함정 있는지
- [ ] memory/feedback_*.md 검색 — 도메인 지식 (LoL FX 텍스처 함정 등)

### 2. 데이터 형태 정의가 셰이더/렌더러보다 먼저 (자연 순서)

**컴포넌트 → 셰이더 → 렌더러** 순. 거꾸로 가면 셰이더 두 번 손댐.

이유: cbuffer 는 컴포넌트가 보낼 데이터 형태에 의존. 컴포넌트 필드 확정 → cbuffer 설계 → 셰이더 작성.

예시 (FX-1~FX-6):
- ❌ FxSprite.hlsl 먼저 → FxBillboardComponent 변경 → 셰이더 cbuffer 재조정
- ✅ FxBillboardComponent (vColor/atlas/scroll) 정의 → CBFxParams (b2) 매핑 → FxSprite.hlsl 작성

### 3. DLL 경계 보수 (★ Engine public API)

Engine public API 에 STL/Client enum 노출 금지:
- ❌ `void DrawMesh(const std::string& path, const Mat4&)`
- ✅ `void DrawMesh(const char* path, const FxMeshDrawParams& params)` (POD + const char*)

enum 통과는 정수로:
- ❌ `eFxBlendMode iBlendMode` (Client 전용 enum 노출)
- ✅ `u32_t iBlendPreset` (정수, 내부에서 `static_cast<eBlendPreset>`)

POD 구조체 강제:
```cpp
struct FxMeshDrawParams
{
    Mat4  matWorld;
    Vec4  vTint           { 1.f, 1.f, 1.f, 1.f };
    f32_t fAlphaClip      = 0.05f;
    u32_t iBlendPreset    = 1;       // 정수 enum
    bool  bDepthWrite     = true;
};
```

NO_COPY 강제 — raw ID3D11* 멤버를 가진 클래스는 복사 시 double-Release 크래시:
```cpp
DX11Pipeline(const DX11Pipeline&) = delete;
DX11Pipeline& operator=(const DX11Pipeline&) = delete;
```

### 4. Source of truth 전환은 보류 (호환 유지)

기존 caller 가 직접 세팅하는 필드 (예: `bBillboard = true/false`) 는 **신규 enum 도입 시 즉시 source 로 만들지 X**. 모든 caller 검증 후 별도 단계.

이유: facingMode 같은 신규 enum 기본값이 기존 bBillboard 를 무시하면 R 같은 케이스에서 회귀.

### 5. 검증 결정 포인트 명시 (★ Step 단위 진입 판단)

각 Phase 의 핵심 시각/동작 결과로 인프라 검증:
- W stage1 atlas 4프레임 12fps = sprite 인프라 검증
- E beam Additive 광원 = mesh 재질 인프라 검증
- AttackRange 무파손 = 기존 PlaneRenderer 호환 검증

검증 결정 포인트 통과 후 다음 Step 진입. 안 통과면 보강 단계.

### 6. 최소 수정 (큰 패치 회피)

원인 확정 전:
- ❌ 신규 로더 분기, aiProcess 옵션 신설, 셰이더 신규 작성, ResourceCache 우회
- ✅ 텍스처 1개 교체, 멤버 1개 추가, 슬라이더 클램프 확장, 경로 1개 정정

원인 확정 후 인프라 변경 검토.

---

## B. 코드 리뷰 (review) — 4 단계

### 1. 빌드 차단 vs 동작 차단 분리

차단 우선순위:
- 🔴 **컴파일 에러** (괄호 / 시그니처 / 미정의) — 즉시 수정
- 🔴 **링크 에러** (DLL export 누락 / NO_COPY 미선언 시 vtable mismatch) — 즉시 수정
- 🔴 **동작 차단** (셰이더 SEMANTIC 오타 / cbuffer slot 충돌 / 잘못된 shader bind) — 빌드 통과해도 화면 0
- 🟠 **런타임 엣지 케이스** (div0 / 범위 초과 / 댕글링 포인터) — 특정 입력 시 크래시
- 🟡 **코드 컨벤션** (필드명 / include 순서) — 빌드 통과 시 무관

체크리스트:
- [ ] 모든 신규 멤버가 nullptr/기본값 초기화됐나
- [ ] DLL export 클래스의 raw 포인터 멤버에 NO_COPY 있나
- [ ] 셰이더 SEMANTIC 이 InputLayout 과 일치하나
- [ ] cbuffer slot (b0=Frame, b1=Object, b2=Fx) 충돌 없나

### 2. 엣지 케이스 검토 (★ Codex 가 강한 영역)

런타임 엣지 케이스 - 프로젝트 특성:
- div0 가드 — atlas iAtlasCols/Rows 가 0 일 때
- range overflow — frameCount > cols * rows
- nullptr 체크 — wrapper 의 m_pImpl, optional 텍스처
- 중복 Release — DX11 raw 포인터 double-free
- 컴포넌트 ForEach 중 DestroyEntity (UB)
- 셰이더 alpha clip 으로 전 픽셀 버려짐
- FBX UV 도메인 vs 텍스처 alpha bbox 불일치

### 3. 코드 컨벤션 일관성 (디테일도 짚기)

- 필드명 통일 (fUVScrollU vs fUvScrollU — 한 패턴)
- include 순서 (Defines.h 우선, FxMeshComponent.h 패턴)
- 한글 주석 인코딩 (mojibake 검사)
- 네이밍 (파일 C 없이, 클래스 C 붙임)
- 타입 alias (f32_t / u32_t / bool_t — 신규 코드)

CLAUDE.md 의 ★ 4 폴더 grep 규칙 + WINTERS_ENGINE_CONVENTIONS.md 점검.

### 4. 이미 적용된 부분 식별 — 깨진 부분만 패치

리뷰 시 모든 항목 매트릭스로 분류:
| 항목 | 상태 | 위치 | 조치 |
|------|------|------|------|
| ✅ 적용 OK | — | 파일:라인 | 무관 |
| ⚠️ 부분 적용 / 버그 | 차단 # | 파일:라인 | Before/After |
| ❌ 미적용 | — | — | 진행 여부 사용자 결정 |

수정 계획서는 **차분만** — 이미 OK 항목 다시 작성 X.

---

## C. 사용자에게 위임 전 자체 시도 (★ Phase 1 교훈)

read-only 권한 안에서도 다음은 가능:
- Bash → Python 한 줄 (PIL alpha bbox, struct hex dump)
- PowerShell `Select-String -Encoding byte` (FBX 바이너리 키워드)
- AssetConverter / 프로젝트 내장 도구
- 셰이더 파일 Read
- DIAG 로그 grep
- FBX 파일 크기 / 수정 시간 검사

"BP 찍어주세요" / "슬라이더 시도해주세요" 권장 **전에** 위 도구로 자체 진단 1회 시도. 사용자 디버깅 위임은 자체 시도 후 막힌 지점부터.

---

## D. 사이클 종료 후 (★ 의무)

원인 확정/구현 완료 후 **사고 흐름 갱신**:

1. **CLAUDE.md Gotchas** 한 줄 추가 — 도메인 사실 (재발 방지)
2. **memory/feedback_*.md** 신규 작성 — 구체 사례 + 도메인 지식
3. **놓친 사고 흐름 발견 시** 본 SKILL 또는 [debug-pipeline/SKILL.md](../debug-pipeline/SKILL.md) 보강 — **사이클 자체 개선**
4. **MEMORY.md 인덱스** 1줄 추가

→ 다음 세션 자동 로드. 같은 함정 재발 회피 + 사이클 시간 갈수록 단축.

---

## E. 사례 (Phase D ~ Phase FX 수확)

### 1. eBlendPreset 재사용 vs eFxBlendMode 신설
v4 가 eFxBlendMode 신설 → Codex 가 BlendStateCache 의 기존 enum 재사용 짚음.
**교훈**: 기존 인프라 grep 우선. 동일 책임 enum 중복 금지.

### 2. DX11Pipeline NO_COPY 미선언 → double-Release 크래시
DX11Shader 는 NO_COPY 있는데 DX11Pipeline 은 없음 → 복사 시 raw ID3D11* 댕글링.
**교훈**: raw 포인터 멤버 클래스는 NO_COPY 의무. CLAUDE.md Gotcha 등록됨 (2026-04-23 unique_ptr 멤버 + WINTERS_ENGINE dllexport 패턴).

### 3. atlas div0 가드 (Codex 가 짚은 엣지)
`frame % iAtlasCols` 가 iAtlasCols=0 일 때 크래시. 내가 놓친 부분.
**교훈**: 사용자 입력 (또는 컴포넌트 default-init) 으로 0 가능한 모든 division 에 가드.

### 4. render/PNG vs mesh diffuse (도메인 지식)
LoL 추출 PNG 가 sprite 캡처 — mesh UV 가 알파 0 영역 가리킴 → clip 으로 전 픽셀 버려짐.
**교훈**: CPU 디버거로 못 잡는 픽셀 단계 버그. 데이터 직접 계측 필요. [debug-pipeline](../debug-pipeline/SKILL.md) 호출.

### 5. POSITIONT 오타 (셰이더 SEMANTIC)
빌드 통과 + 런타임에서 InputLayout 매칭 실패 → 정점 위치 0 → 화면 0.
**교훈**: 셰이더는 빌드 검증 약함. SEMANTIC 이름 / cbuffer slot / 정점 포맷 매뉴얼 점검.

### 6. CEngineApp Pipeline::Create 인자 없음
v4.1 의 `DX11Pipeline::Create(/*VTX Mesh InputLayout 재사용*/)` 주석만. 빌드 차단.
**교훈**: 신규 코드 작성 시 같은 패턴 (Mesh3D Pipeline 호출부) 그대로 복사. 주석으로 대체 X.

### 7. 컴포넌트 → 셰이더 순서 (Codex 가 짚음)
v4 가 셰이더 먼저 → 컴포넌트 변경 시 cbuffer 재조정. 거꾸로.
**교훈**: 데이터 형태 (caller 가 보낼 것) 가 먼저. 셰이더는 그걸 받아서 처리.

---

## 슬래시 명령어

`/code` 로 명시 호출 가능 — `.claude/commands/code.md` 가 본 SKILL 의 단계별 절차를 즉시 컨텍스트에 로드.

연관 스킬:
- [debug-pipeline](../debug-pipeline/SKILL.md) — 버그 추적 사이클 (셰이더 우선 Read + 데이터 계측)
- [code-scaffolding](../code-scaffolding/SKILL.md) — 클래스 스캐폴딩 (네이밍/팩토리 패턴)
- [code-review](../code-review/SKILL.md) — 리뷰 체크리스트
