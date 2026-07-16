Session - WintersLoLEditor 독립 CMake exe를 신설하고 Stage 라이터를 Shared로 내려 로드→세이브 바이트 동일 왕복 게이트를 세운다.

# LoL 에디터 분리 슬라이스 1 — 적용 기록 (2026-07-15)

상용 엔진 대응 구도: 에디터 = 엔진 SDK를 소비하는 별도 실행 파일(UE `TargetType.Editor`/Unity UnityEditor.dll 분리에 대응), 런타임 계약 포맷(Stage{N}.dat)은 Shared에서 로더+라이터 양방향, 렌더러는 Engine DLL 공유(에디터는 소비자 레이어).

## 반영된 코드 (전부 적용·빌드 PASS)

수정 2:
- `CMakeLists.txt` — `add_subdirectory(LoLEditor)` (EldenRingEditor 아래)
- `Shared/GameSim/Definitions/StageData.h` — `WriteStageBlockCount`/`WriteStageEntries`/`SaveStageDataToFile` 추가. 로더와 대칭: header 보존(왕복 바이트 동일), 블록 존재는 `header.version` 기준(≥4 waypoint, ≥5 bush). 헤더가 무효일 때만 magic/VERSION 5로 정규화(새 문서 경로).

신규 8 (`LoLEditor/`):
- `CMakeLists.txt` — `add_executable(WintersLoLEditor WIN32)` + `Winters::Engine` 링크 + POST_BUILD 3종(Engine DLL/서드파티 DLL/Shaders 복사) + `VS_DEBUGGER_WORKING_DIRECTORY=루트`. EldenRingEditor와 동형.
- `Private/main.cpp` — `wWinMain` → `EngineConfig`(1600×900, 기본 RHI=DX11, `--rhi=` 오버라이드) → `WintersRun`.
- `Public/LoLEditorApp.h` / `Private/LoLEditorApp.cpp` — `IWintersApp` 구현, `OnInit`→`Change_Scene(0, CLoLMapEditorScene)`. 창 상수 `LoLEditor::kWindowWidth/Height` (g_iWinSizeX는 Client 전역이라 미사용).
- `Public/World/LoLStageDocument.h` / `Private/World/LoLStageDocument.cpp` — 문서 모델. 매니저 4종 비의존, `StageData` 직접 소유. 경로 해석 = Winters.sln+Data\ 워크스페이스 루트 워크(Client `CMapDataIO` 이식; `WintersResolveContentPath`는 존재 파일 전용이라 신규 저장 부적합). `VerifyRoundtrip` = 원본 vs 재직렬화 바이트 비교.
- `Public/LoLMapEditorScene.h` / `Private/LoLMapEditorScene.cpp` — `IScene` 구현. 카메라 = Engine `CCamera` 서브클래스(`CLoLEditorCamera`, protected ctor 개방; 기본 가상 Update가 WASD 프리캠). 맵 메시 = `sr_base_flip.wmesh` + Mesh3D.hlsl, scale {-0.01,0.01,0.01}, rotY -135°(CScene_Editor 동일 상수). 피킹 = `CInput::GetMouseWorldRay` y=0 평면. 팔레트/하이어라키/인스펙터 = 문서 벡터 직결. NavGrid 페인트/오버레이 = CScene_Editor 이식(WNVG v1). 마커 = ImGui 배경 드로리스트(메시 프리뷰는 다음 슬라이스).

## 검증 결과 (2026-07-15)

- 빌드: `cmake --preset msvc-ninja` + `cmake --build out/build/msvc-ninja --config Debug --target WintersLoLEditor` → PASS (vcvars64 환경 필수).
- **roundtrip 게이트: PASS** — 콘솔 하네스로 Data/Stage1.dat(v5, S=30 J=26 W=27 B=0) 로드→세이브 → **6868 bytes 동일**.
- 스모크: exe 단독 실행 8초 생존(OnInit 성공, ~294MB) 후 정상 kill. Data/Stage1.dat 무변경.
- 빌드 함정 2건 실측: ①Ninja 빌드는 vcvars64 환경 필요 ②Engine 헤더의 debug `new` 매크로가 imgui.h를 깨므로 scene cpp에서 `push_macro("new")`/`undef`/`pop_macro` 필수(Client Scene_Editor.h와 동일 패턴).

수동 확인 잔여:
- 인게임 시각 확인(마커/맵/피킹 체감), 에디터 내 Verify Roundtrip 메뉴, 저장 후 클라/서버 로드 회귀.
- OS 창 크기와 kWindowWidth/Height 불일치 시 피킹 오차 여부.

## 다음 슬라이스

1. ObjectVisualDefs.json 기반 구조물/정글 wmesh 프리뷰 (visibilityStates 반영)
2. undo — EldenRingEditor `CEditorTransaction` 공유 승격 + Add/Delete/Transform 커맨드
3. y=0 평면 → MapSurfaceSampler 지형 높이 피킹
4. 클라 CScene_Editor / 'M'키 은퇴 (UE WITH_EDITOR 소거 대응)
5. 팩 파이프라인 통합 (Stage.dat=소스, LoLDefinitionPack=쿡 결과물)

주의: CMake 전용 타깃 — sln 워크플로에서 빌드되지 않음(EldenRingEditor와 동일 rot 리스크). `.md/collab/HARNESS_RULES.md` 빌드/스모크 타깃에 `WintersLoLEditor` 등재 필요.

경계: 이 슬라이스는 GameSim 런타임 로직 무접촉(순수 직렬화 함수만 추가). Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다는 규칙 불변.
