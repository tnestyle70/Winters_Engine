---
name: code-scaffolding
description: >
  WintersEngine 아키텍처 컨벤션에 맞는 C++20 클래스 자동 생성.
  "클래스 만들어줘", "Component 추가", "System 구현", "RHI 구현" 등에 트리거.
---

# Skill: Code Scaffolding — WintersEngine

## 실행 순서
1. `references/engine-api.md` → 레이어 인터페이스 확인
2. `references/module-list.md` → 모듈/등록 규칙 확인
3. `references/gotchas.md` → 해당 항목 체크
4. **[생각 흐름]** 출력 → 코드 생성 → **code-review** 자동 연계

## 생각 흐름 (반드시 출력)
```
1. 클래스 분류 (ECS Component/System, RHI, Platform, Core, Renderer, Editor, Network, Game)
2. 엔진 레이어 / 3. 의존 인터페이스 / 4. 소유권 모델
5. 스레드 모델 / 6. 데이터 설계 / 7. vcxproj 등록 / 8. gotchas 해당 번호
```

## 핵심 규칙

### 네이밍 (★ 엄격)
- **파일명: C 접두사 금지** — `Transform.h` ✅, `CTransform.h` ❌
- **클래스명: C 접두사 필수** — `class CTransform` ✅, `class Transform` ❌
- **struct (POD): C 접두사 금지** — `struct TransformComponent` ✅
- **인터페이스: I 접두사** — `IBuffer`, `ISystem`, `IWintersApp`
- 멤버: `m_PascalCase` (`m_LocalPosition`, `m_pBuffer`)
- 포인터: `m_p`, 불리언: `m_b`, float: `m_f`, Vec3: `m_v`
- static: `s_`, global: `g_`, 상수: `ALL_CAPS`

### 파일 / 선언 매핑 예시
| 파일 | 선언 |
|------|------|
| `Transform.h` | `class CTransform` |
| `DX11Device.h` | `class CDX11Device` |
| `TexturePool.h` | `class CTexturePool` |
| `IBuffer.h` | `class IBuffer` (인터페이스) |
| `TransformComponent.h` | `struct TransformComponent` (POD) |
| `DX11VertexBuffer.h` | `class CDX11VertexBuffer` |

### 타입 별칭 (★ 신규 코드 필수)
- `u32_t` / `i32_t` / `f32_t` / `f64_t` / `bool_t` / `wstring_t` 사용
- 금지: `uint32_t` / `int32_t` / `float` / `std::wstring` (신규 코드)
- 예외: Win32 (`HWND`/`DWORD`), DX11 (`ID3D11*`), DirectXMath (`XMFLOAT3`) 그대로

### STL 네임스페이스
- `Engine_Defines.h`에서 `using namespace std;` 선언됨 — **`std::` 생략 허용**
- 예: `unique_ptr<T>`, `shared_ptr<T>`, `function<void()>` 전부 `std::` 없이 OK
- 단, 헤더에서 `using namespace ...` 직접 선언 금지 (#7)

### 그 외
- RAII: `make_unique`/`ComPtr`, `new`/`delete` 금지 (#1)
- `[[nodiscard]]` 초기화 함수, `noexcept` 이동 연산 (#8, #9)
- `enum class` (bool 파라미터 금지), `constexpr` (#define 금지) (#10, #17)
- RHI 위에서 DX11 타입 직접 참조 금지 (#2)
- ECS Component에 로직 함수 금지 (#3)
- Create 팩토리 패턴: private ctor + `static unique_ptr<T> Create()` (CLAUDE.md §클래스 설계 원칙)

### ★ ImGui 적극 활용 (#14 정책)
**모든 새 시스템은 ImGui 튜닝 UI와 함께 작성** — 하드코딩 값 금지.

**의무 체크리스트**:
- [ ] 튜닝 가능한 파라미터(속도, 거리, 색, 각도, 계수)는 **ImGui 슬라이더/컬러픽커**로 노출
- [ ] 상태(state)는 **ImGui 읽기 전용 표시** — Physics ON/OFF, Animation State, Entity Count 등
- [ ] 디버그 토글(가시화/무력화)은 **ImGui 체크박스**로 제공
- [ ] 새 클래스 설계 시 `DrawDebugUI()` 또는 `OnInspectorGUI()` 멤버 함수 고려
- [ ] 활성화는 `#ifdef WINTERS_EDITOR`로 감싸서 Release에서 완전 제거

**ImGui 사용 권장 영역** (우선순위):
1. Entity Inspector — TransformComponent, Velocity, Health 등 전부 슬라이더
2. Material Editor — Albedo color, Roughness, Metallic 실시간
3. Profiler Overlay — 프레임 타임 플롯, 스레드 타임라인
4. Animation Blender — Blend weight 슬라이더, 본 회전 디버그
5. Physics Debug — 콜라이더 시각화 체크박스
6. Shader Reloader — HLSL 파일 저장 감지 → 재컴파일 버튼
7. Console — 런타임 명령 (`set_time_scale`, `reload_shaders`)
8. Network Debug — 패킷 로그, RTT 플롯 (LoL 연동 단계)

**원칙**: "빌드 1번으로 모든 값 튜닝" = 기획/디자인 이터레이션 속도 = 게임 품질
