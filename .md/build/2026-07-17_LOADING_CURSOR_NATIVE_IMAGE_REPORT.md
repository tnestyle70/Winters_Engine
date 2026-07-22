# 2026-07-17 로딩 씬 네이티브 커서 이미지 교체 리포트

## 1. 증상과 진단

- 증상: MainMenu/Lobby에서는 설정한 LoL 커서(hover_precise.png)가 보이는데, Loading/MatchLoading에서는 기본 Windows 화살표가 보인다.
- 진단: 버그가 아니라 **의도된 코드**다. `CGameInstance::SetLoadingCursorMode(true)`(GameInstance.cpp)가 로딩 씬 진입 시 소프트웨어(텍스처) 커서를 끄고 OS 네이티브 커서를 켠다. 텍스처 커서는 프레임이 렌더될 때만 움직이는데, InGame 프리로드는 메인스레드 스텝식 로드(스텝당 모델 1개, ms 예산 없음)라 한 스텝이 프레임을 수백 ms~수 초 멈추기 때문이다. "백그라운드 로딩 미비 → 프레임 스톨"이라는 가설은 사실이나, 커서 전환은 연동 단절이 아니라 그 스톨에 대한 **설계된 완화책**이다.
- 실제 GPU 자산(모델/텍스처/맵/FX)은 전부 메인스레드 로드다. 워커 스레드 로드는 맵 표면 CPU 파싱뿐(Loader.cpp `StartInGameCpuLoad`). 이는 b4d2237(2026-06-24) CRHIResourceTable 렌더스레드 소유권 규칙의 의도된 결과이며, 완전 비동기화(워커 CPU 준비 → 메인 GPU finalize 큐)는 S005 gap G5로 남아 있는 미실행 과제다.

## 2. 왜 여러 번 고쳐도 회귀했나 (정책 플립플롭)

| 시점 | 정책 | 근거 문서 |
|---|---|---|
| 2026-07-11 S004 | 로딩 중 **네이티브 커서 표시** (응답성 증명: "응답 없음" 금지 게이트) | `Plan/S004_..._RESULT_20260711.md` |
| 2026-07-13 | "커서가 InGame 이후에만 나온다"를 버그로 보고 `SetLoadingCursorMode` **전면 제거** (LoL 커서 단일 수명) | `.md/build/2026-07-13_EZREAL_W_CURSOR_SHOP_REGRESSION_REPORT.md` |
| 그 이후 | `SetLoadingCursorMode` **문서 없이 재도입** (현재 코드) | 코드 주석만 존재 |

두 요구(스톨 중 커서 응답성 vs 커스텀 커서 연속성)가 서로를 계속 되돌렸다. 이번 수정은 둘을 동시에 만족시킨다.

## 3. 수정 내용

핵심: `SetLoadingCursorMode`의 on/off 수명은 그대로 두고, 로딩 중 보이는 **네이티브 커서의 이미지 자체를 게임 커서 PNG로 교체**한다. 하드웨어 커서는 OS가 그리므로 프레임이 완전히 멈춰도 커스텀 이미지로 계속 움직인다.

- `Engine/Public/Platform/CWin32Window.h` (+EngineSDK 미러): `SetCursorImageFromFile(const wchar_t*)` API, `HCURSOR m_hCursorImage` 멤버 추가.
- `Engine/Private/Platform/CWin32Window.cpp`: WIC 디코드(RHITextureLoader.cpp 관용구 재사용) → 32×32 Fant 스케일 → straight-alpha BGRA DIB → `CreateIconIndirect`(핫스팟 0,0 = 소프트웨어 커서와 동일 앵커/크기) → `SetClassLongPtrW(GCLP_HCURSOR)` 설치. `SetSystemCursorVisible(true)` 시 `::SetCursor`로 즉시 반영. `Destroy()`에서 `DestroyCursor`.
- `Engine/Private/Manager/UI/UI_Manager.cpp`: `Initialize`에서 커서 SRV 로드 직후 `CEngineApp::Get().GetWindow().SetCursorImageFromFile(kPathCursorDefault)` 1회 설치(경로 단일 소스 유지). 실패 시 화살표 폴백 + 1회 트레이스.

동작 근거: WndProc은 WM_SETCURSOR을 처리하지 않아 DefWindowProc이 클래스 커서를 적용하고, ImGui는 `ImGuiConfigFlags_NoMouseCursorChange`(ImGuiLayer.cpp)라 개입하지 않는다. 로딩 중에도 프레임 루프 + 모델 로드 내부 중첩 펌프(Preload yield, budget 32)가 WM_SETCURSOR을 계속 소화한다.

## 4. 검증 상태

- Engine Debug x64 빌드 PASS (경고는 기존 C4251뿐). 변경 표면은 Engine DLL 내부에 완결 — Client는 `CEngineApp`/`CWin32Window`를 include하지 않아 Client측 컴파일/ABI 영향 없음.
- Client 전체 빌드는 병행 세션의 Data-Driven 진행 중 편집(`Minion_Manager.cpp`/`SnapshotApplier.cpp`에서 `MinionCombatDef.h` include 제거 상태, `kGameSimMinionRoleTibbers` C2065)으로 **본 수정과 무관하게** 실패 중. 해당 세션 완료 후 재빌드 필요.
- **인게임 시각 게이트: 대기.** Client 빌드가 복구되면: ① 게임 실행 → 로딩 씬에서 커서가 LoL 커서 이미지로 움직이는지, ② InGame 진입 후 텍스처 커서 1개만 보이는지, ③ MainMenu/Lobby 기존 동작 불변인지 확인. 보조 실측: `probe_class_cursor.ps1`(세션 스크래치패드) — 실행 중인 게임 창의 `GetClassLongPtr(GCLP_HCURSOR)`가 IDC_ARROW와 다른 핸들이면 설치 성공.

## 5. 남긴 것 (본 수정 범위 밖)

- 로딩 프레임 스톨 자체의 해소는 S005 gap G5(워커 CPU 준비 → 메인 GPU finalize 큐) 그대로 미해결.
- `CScene_InGame::OnEnter`가 조기 실패 return을 얻으면 loading-cursor mode가 켜진 채 누수되는 단일점 불변(현재는 도달 불가) — OnEnter에 실패 가드를 추가하는 사람은 `SetLoadingCursorMode(false)`를 먼저 호출할 것.
