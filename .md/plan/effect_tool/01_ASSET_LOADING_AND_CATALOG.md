Session - WFX와 렌더 리소스를 검색, 분류, 로드하는 에셋 카탈로그를 만든다.

1. 반영해야 하는 코드

성공 기준:
- 하드코딩된 Annie preset 목록 없이 `Data/LoL/FX` 아래의 `.wfx`를 스캔해서 선택할 수 있다.
- 각 `.wfx`가 참조하는 texture, erode texture, model 경로가 존재하는지 표시한다.
- cue name, champion, skill, render type, emitter count, last load error가 한 화면에서 보인다.
- 로드한 asset은 기존 `CFxAssetRegistry`와 `CWfxDocument` 흐름으로 들어가며, preview와 runtime cue가 같은 이름을 본다.

작업 파일:
- `C:/Users/user/Desktop/Winters/Client/Public/UI/WfxAssetCatalog.h`
- `C:/Users/user/Desktop/Winters/Client/Private/UI/WfxAssetCatalog.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/UI/WfxEffectToolPanel.cpp`
- `C:/Users/user/Desktop/Winters/Engine/Public/FX/FxAsset.h`
- `C:/Users/user/Desktop/Winters/Engine/Private/FX/FxAsset.cpp`

구현 방향:
- 새 `WfxAssetCatalog`는 UI 전용 카탈로그로 둔다. Engine registry는 실제 asset handle 저장소, catalog는 검색/필터/검증 view model 역할만 맡는다.
- 스캔 루트는 우선순위로 나눈다.
  - `Data/LoL/FX`
  - `Client/Bin/Resource/FX`
  - 필요 시 사용자가 입력한 절대/상대 루트
- `.wfx` 파일 하나당 `WfxAssetEntry`를 만든다.
  - path
  - cue name
  - champion folder
  - skill token
  - emitter count
  - render type summary
  - missing resource list
  - load error
- `LoadFxAssetFromFile` 호출 결과를 catalog validation에도 재사용하되, UI 스캔 중 실패가 전체 툴을 멈추지 않게 한다.
- `CFxAssetRegistry`에 전체 asset을 열거하는 API가 필요하면 Session 1에서 최소 API만 추가한다. 단, Engine public header 변경 시 `EngineSDK/inc` 직접 수정은 하지 않고 검증 섹션에 `UpdateLib.bat` 동기화만 남긴다.

비범위:
- 썸네일 이미지는 Session 5에서 한다.
- 드래그 앤 드롭 import는 Session 5에서 한다.
- 독립 실행형 파일 브라우저는 이번 세션에 넣지 않는다.

2. 검증

검증 명령:
- `git diff --check -- Client/Public/UI/WfxAssetCatalog.h Client/Private/UI/WfxAssetCatalog.cpp Client/Private/UI/WfxEffectToolPanel.cpp Engine/Public/FX/FxAsset.h Engine/Private/FX/FxAsset.cpp`
- `"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nodeReuse:false`

수동 확인:
- `Data/LoL/FX/Champions/Annie/q_fireball.wfx`와 `Data/LoL/FX/Champions/Irelia/e_connect.wfx`가 catalog에 표시된다.
- 존재하지 않는 texture/model path가 있으면 목록에서 바로 보인다.
- 같은 cue name을 가진 `.wfx`가 여러 개면 duplicate 경고가 보인다.
- catalog에서 선택한 `.wfx`가 `CWfxDocument::LoadFromFile`로 열리고 기존 editor 값이 채워진다.

후속 동기화:
- `Engine/Public/FX/FxAsset.h`를 변경한 경우 `UpdateLib.bat` 실행 필요.
