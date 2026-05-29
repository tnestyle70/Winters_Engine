Session - 디자이너가 협업해서 쓸 수 있는 WFX 이펙트 툴을 세션 단위로 완성한다.

1. 반영해야 하는 코드

목표:
- 기존 인게임 `WFX Effect Tool` 뼈대를 버리지 않고, 에셋 브라우저, 문서 편집, 프리뷰, 저장, 검증, 런타임 cue 연결까지 단계별로 끌어올린다.
- 툴은 게임플레이 진실을 만들지 않는다. 서버 권위 흐름은 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual` 그대로 두고, 툴은 클라이언트 시각 편집과 데이터 제작에 집중한다.
- 첫 완성 형태는 인게임 ImGui 툴이다. 독립 실행형 에디터는 툴 데이터 모델과 프리뷰 경로가 안정된 뒤 별도 세션으로 분리한다.

현재 확인한 기반:
- `C:/Users/user/Desktop/Winters/Client/Private/UI/WfxEffectToolPanel.cpp`
- `C:/Users/user/Desktop/Winters/Client/Public/UI/WfxEffectToolPanel.h`
- `C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/WfxDocument.cpp`
- `C:/Users/user/Desktop/Winters/Client/Public/GameObject/FX/WfxDocument.h`
- `C:/Users/user/Desktop/Winters/Engine/Private/FX/FxAsset.cpp`
- `C:/Users/user/Desktop/Winters/Engine/Public/FX/FxAsset.h`
- `C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxCuePlayer.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp`

세션 순서:
- `01_ASSET_LOADING_AND_CATALOG.md`: `.wfx`와 텍스처/모델 에셋을 검색, 분류, 로드하는 카탈로그를 만든다.
- `02_CLEAN_TOOL_SHELL_AND_UI.md`: 현재 하드코딩 패널을 디자이너용 3열 툴 UI로 정리한다.
- `03_WFX_DOCUMENT_AND_INSPECTOR.md`: Lifetime, Color, Duration 계열 속성을 안정적으로 편집하고 저장하는 문서 모델과 Inspector를 만든다.
- `04_PREVIEW_SANDBOX_AND_TIMELINE.md`: 게임플레이를 건드리지 않는 프리뷰 샌드박스, 재생 컨트롤, 스폰 정리를 만든다.
- `05_DESIGNER_ASSET_WORKFLOW.md`: 저장, 백업, 유효성 검사, 경로 검증, 썸네일/레퍼런스 흐름을 만든다.
- `06_RUNTIME_CUE_INTEGRATION.md`: 디자이너가 저장한 `.wfx`가 서버 cue 단일 소스 경로로 게임에 들어가게 만든다.
- `07_PORTFOLIO_DEMO_POLISH.md`: 롤/엘든링 포트폴리오 시연 가치가 보이도록 샘플, 성능, 데모 체크리스트를 완성한다.

공통 원칙:
- Runtime 리소스는 `Client/Bin/Resource` 기준으로 해석한다. `Client/Bin/Debug*/Resource`나 `Client/Bin/Release*/Resource` 복사본에 의존하지 않는다.
- `.wfx` authoring 데이터는 `Data/LoL/FX`를 우선 루트로 삼고, 실제 렌더 텍스처/모델은 runtime resource path를 명확히 검증한다.
- cue 이름은 C++ 하드코딩보다 데이터에서 발견 가능해야 한다.
- FX 재생은 `CFxCuePlayer`와 client visual path에서 1회만 일어난다. legacy hook과 visual hook이 같은 FX를 중복 재생하지 않게 한다.
- 디자이너 UI는 “무엇을 고쳐야 하는지”가 즉시 보이는 검증 상태를 가져야 한다. 실패를 조용히 삼키지 않는다.

2. 검증

문서 검증:
- 각 세션 문서가 독립적으로 읽혀도 목표, 작업 파일, 완료 기준, 검증 방법을 알 수 있어야 한다.
- 세션 1부터 순서대로 진행하면, 이전 세션의 산출물이 다음 세션의 전제 조건이 되어야 한다.

구현 공통 검증 명령:
- `git diff --check`
- `"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nodeReuse:false`

수동 공통 확인:
- F5 인게임 진입 후 툴 창이 정상 표시된다.
- `.wfx` 로드 실패, 텍스처 누락, 모델 누락, cue 누락이 UI와 `OutputDebugStringA/W`에서 확인 가능하다.
- 서버 권위 게임플레이 결과가 툴 조작으로 바뀌지 않는다.
- FX cue는 서버 이벤트 또는 툴 프리뷰 중 하나의 명확한 경로로만 재생된다.
