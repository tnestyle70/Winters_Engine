# Test Patterns — WintersEngine

assertion 목록. 분류별 해당 항목 전부 실행. FAIL → 수정 → 재실행.

---

## [Engine] — RHI / Core / Platform

| ID | 검증 내용 |
|----|-----------|
| E01 | `new`/`delete` 없음 → smart ptr / ComPtr |
| E02 | 멤버 변수 기본값 초기화 |
| E03 | 헤더에서 `using namespace` 없음 |
| E04 | 이동 연산에 `noexcept` |
| E05 | 초기화 함수에 `[[nodiscard]]` |
| E06 | bool 파라미터 2개 이상이면 `enum class` |
| E07 | `#define` 대신 `constexpr` |
| E08 | COM Release 후 `= nullptr` |
| E09 | 전역 싱글톤 없음 |
| E10 | 파일 경로 하드코딩 없음 |
| R01 | `d3d11.h` include가 RHI/DX11 내부에서만 |
| R02 | DX11 API 호출 후 HRESULT 체크 |
| R03 | 셰이더 컴파일 실패 시 에러 블롭 출력 |
| R04 | Buffer ByteWidth > 0 체크 |
| R05 | ConstantBuffer 16B 정렬 static_assert |
| R06 | Map/Unmap WRITE_DISCARD |
| PL01 | Win32 타입이 Include/에 미노출 |
| PL02 | WndProc 입력 이벤트 → CInput 전달 |
| PL03 | PumpMessages WM_QUIT → false |

## [ECS] — Component / System / World

| ID | 검증 내용 |
|----|-----------|
| C01 | Component에 로직 함수 없음 |
| C02 | Component 간 직접 참조 없음 |
| C03 | include는 표준 타입 + Math만 |
| C04 | 멤버 기본값 초기화 |
| S01 | ISystem 상속 + GetAccess + Update override |
| S02 | GetAccess reads/writes 정확 |
| S03 | Update에서 Entity 직접 삭제 없음 → CommandBuffer |
| S04 | 순회 중 Remove 없음 |
| S05 | 게임 로직에서 GPU 직접 호출 없음 |
| W01 | Query 결과에 조건 미충족 Entity 없음 |
| W02 | CommandBuffer Flush는 프레임 끝 메인 스레드 |
| W03 | Entity 삭제 후 Get → nullptr |

## [Renderer]

| ID | 검증 내용 |
|----|-----------|
| RD01 | pImpl로 DX11 타입 Include/ 미노출 |
| RD02 | Init/Create 실패 시 false + 로그 |
| RD03 | Shutdown에서 모든 GPU 리소스 해제 |
| RD04 | InputLayout 시멘틱 = HLSL 일치 |
| RD05 | ConstantBuffer 슬롯 = HLSL register(bN) 일치 |
| RD06 | Draw 전 IASetPrimitiveTopology 호출 |
| RD07 | Render 후 Unbind |

## [PublicAPI]

| ID | 검증 내용 |
|----|-----------|
| P01 | Include/에 `d3d11.h` 없음 |
| P02 | WINTERS_API 매크로 적용 |
| P03 | `#pragma once` 존재 |
| P04 | pImpl → struct Impl 전방선언만 |
| P05 | pImpl → 생성자/소멸자 선언 |

## [Shader]

| ID | 검증 내용 |
|----|-----------|
| SH01 | cbuffer register(bN) = C++ Bind(slot) 일치 |
| SH02 | VS 입력 시멘틱 = InputLayout 일치 |
| SH03 | VS 출력에 SV_POSITION |
| SH04 | PS 출력에 SV_TARGET |
| SH05 | 행렬 곱 DX 규칙 `mul(vector, matrix)` |

---

## 판정: ✅ PASS / ❌ FAIL (수정→재실행) / ⚠️ WARN (권고) / N/A (스킵)
