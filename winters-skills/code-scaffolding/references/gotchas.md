# GOTCHAS — WintersEngine 프레임워크 함정 목록

코드 작성/리뷰 시 반드시 확인.

---

## 메모리 & 소유권
**#1** `new`/`delete` 금지 → `make_unique`, `make_shared`, `ComPtr` 사용
**#6** 전역 싱글톤 금지 → Engine이 소유, 참조로 전달 (DI 패턴)
**#12** 모든 멤버 변수 선언부에서 기본값 초기화 (`= 0.f`, `= nullptr` 등)

## RHI 아키텍처
**#2** RHI 위에서 `d3d11.h` include / `ID3D11*` 직접 참조 금지 → `Engine/RHI/DX11/` 내부에서만
**#11** Win32 타입(`HWND`, `MSG`)이 Platform 밖으로 노출 금지
**#13** 셰이더 컴파일 에러 반드시 처리 (HRESULT + errorBlob 출력 + `__debugbreak`)
**#20** 모든 DX11 API 호출 후 HRESULT 체크 필수

## ECS 컨벤션
**#3** Component = 순수 데이터. 로직 함수 금지 (const 헬퍼만 허용). Component 간 참조 금지
**#4** System 내 Entity 직접 삭제 금지 → CommandBuffer로 지연 처리
**#5** ComponentStore 순회 중 Remove 금지 (swap-and-pop으로 인덱스 꼬임)
**#15** SystemAccess의 reads/writes를 실제 접근 Component와 정확히 일치시킬 것

## Modern C++20
**#25** Line references in handoff output must be verified with `rg -n` or numbered local output. Do not cite a function start line as the replacement line; include a stable anchor snippet when line numbers may differ.
**#7** 헤더에서 `using namespace` 금지
**#8** 이동 생성자/대입에 `noexcept` 필수
**#9** 초기화/생성 함수에 `[[nodiscard]]` 적용
**#10** bool 파라미터 2개 이상이면 `enum class`로 대체
**#17** `#define` 대신 `constexpr` / `static constexpr`

## DX11 / 렌더링
**#14** [★ 정책 변경] **ImGui 적극 사용**. 엔진 내부 디버그/튜닝/에디터 UI는 ImGui로 작성.
  - `WINTERS_EDITOR` 매크로로 Debug/Editor 빌드에서만 활성화, Release 빌드에서 `#ifdef`로 완전 제거
  - Engine DLL에 ImGui 런타임(Begin/End/Render) 포함 허용
  - 새 시스템/클래스 작성 시 "튜닝 가능한 파라미터는 반드시 ImGui 노출" — 하드코딩 금지
  - 용도: Entity Inspector, Material Tuner, Profiler 뷰, Animation Blend, Physics Debug, Console, Shader Reloader, Network Debug
  - 기본 원칙: **"빌드 1번으로 모든 값을 튜닝할 수 있어야 한다"** — 기획/디자인 이터레이션 속도가 곧 게임 품질
**#16** `World::Query<T...>()` 매 프레임 호출 시 성능 주의 (10K+ Entity면 GetAll+Has 조합)
**#18** 게임 로직 스레드에서 GPU 직접 호출 금지
**#19** 셰이더/에셋 파일 경로 하드코딩(절대경로) 금지

## 네이밍 컨벤션
**#21** 파일명에 C 접두사 금지 (`Transform.h`, `DX11Device.h`), 클래스명에 C 접두사 필수 (`class CTransform`, `class CDX11Device`). struct(POD)는 클래스명도 C 금지 (`struct TransformComponent`). 인터페이스는 I 접두사 (`class IBuffer`).
**#22** 멤버 변수는 `m_PascalCase`, 포인터 `m_p`, 불리언 `m_b`, float `m_f`, Vec3 `m_v`. Engine_Defines.h의 `using namespace std;` 덕분에 `std::` 생략 가능 — 일관되게 생략 또는 명시 중 택1.
**#23** 신규 코드는 타입 alias 사용 필수: `u32_t`, `i32_t`, `f32_t`, `bool_t`, `wstring_t`. `uint32_t`, `float`, `std::wstring` 금지 (신규 코드). Win32/DX11/DirectXMath 타입은 예외.
**#24** C++ 코드 작성 시 반드시 먼저 `code-scaffolding/SKILL.md` + 이 문서 숙지. 코드 리뷰 시 `code-review/SKILL.md` 체크리스트 적용.
