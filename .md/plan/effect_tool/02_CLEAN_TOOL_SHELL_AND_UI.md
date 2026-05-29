Session - 현재 WFX 패널을 디자이너가 오래 써도 피곤하지 않은 깨끗한 툴 UI로 정리한다.

1. 반영해야 하는 코드

성공 기준:
- `WFX Effect Tool`이 항상 뜨는 실험 패널이 아니라, 명확한 토글과 레이아웃을 가진 작업 도구가 된다.
- 왼쪽은 asset browser, 가운데는 preview/playback, 오른쪽은 inspector가 된다.
- 상태 메시지는 임시 문자열 하나가 아니라 load/save/validation/preview 상태별로 구분된다.
- 디자이너가 자주 쓰는 값은 접힌 트리 깊숙한 곳이 아니라 상단 summary와 inspector 첫 화면에서 바로 만진다.

작업 파일:
- `C:/Users/user/Desktop/Winters/Client/Public/UI/WfxEffectToolPanel.h`
- `C:/Users/user/Desktop/Winters/Client/Private/UI/WfxEffectToolPanel.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp`
- `C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h`

구현 방향:
- `WfxEffectToolPanel.cpp` 안의 정적 상태를 `WfxEffectToolState`로 묶어 UI 상태와 document 상태를 분리한다.
- 패널 내부를 다음 함수 단위로 쪼갠다.
  - `RenderMainMenu`
  - `RenderAssetBrowser`
  - `RenderPreviewPanel`
  - `RenderInspector`
  - `RenderStatusBar`
- 창 토글 키를 추가한다. 추천은 `VK_F7`이다. 기존 `F8` UI tuner, `F9` AI, `F10` legacy debug와 충돌하지 않는다.
- `Scene_InGame::OnImGui`에서 `UI::CWfxEffectToolPanel::Render(this)`를 무조건 호출하지 않고, 툴 표시 플래그가 켜졌을 때만 호출한다.
- ImGui dockspace까지는 이번 세션에서 필수로 하지 않는다. 먼저 고정 3열 layout을 안정화한다.
- 버튼은 Load, Save, Preview Edited처럼 문자열만 늘어놓지 않고, 상태와 비활성 조건을 가진다.
- UI 텍스트는 기능 설명문을 길게 쓰지 않는다. 상태와 경고는 짧고 명확하게 표시한다.

비범위:
- undo/redo는 Session 3 이후로 미룬다.
- 썸네일, drag-drop import는 Session 5로 미룬다.
- standalone editor scene 전환은 별도 세션으로 미룬다.

2. 검증

검증 명령:
- `git diff --check -- Client/Public/UI/WfxEffectToolPanel.h Client/Private/UI/WfxEffectToolPanel.cpp Client/Private/Scene/Scene_InGame.cpp Client/Public/Scene/Scene_InGame.h`
- `"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nodeReuse:false`

수동 확인:
- F5 인게임 진입 후 기본 화면에서 툴이 시야를 가리지 않는다.
- `F7`로 툴 표시가 켜지고 꺼진다.
- 창 크기를 줄여도 asset browser, preview, inspector 텍스트가 겹치지 않는다.
- catalog에서 선택, load, save, preview 버튼의 enable/disable 상태가 현재 document 상태와 맞는다.
- 기존 `F8`, `F9`, `F10`, `F1` 디버그 토글 동작이 깨지지 않는다.
