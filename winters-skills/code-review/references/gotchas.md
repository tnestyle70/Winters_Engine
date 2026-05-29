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
**#7** 헤더에서 `using namespace` 금지
**#8** 이동 생성자/대입에 `noexcept` 필수
**#9** 초기화/생성 함수에 `[[nodiscard]]` 적용
**#10** bool 파라미터 2개 이상이면 `enum class`로 대체
**#17** `#define` 대신 `constexpr` / `static constexpr`

## DX11 / 렌더링
**#14** ImGui 코드는 Tools 전용 — Engine DLL에 포함 금지
**#16** `World::Query<T...>()` 매 프레임 호출 시 성능 주의 (10K+ Entity면 GetAll+Has 조합)
**#18** 게임 로직 스레드에서 GPU 직접 호출 금지
**#19** 셰이더/에셋 파일 경로 하드코딩(절대경로) 금지
