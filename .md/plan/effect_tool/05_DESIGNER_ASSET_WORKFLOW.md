Session - 디자이너가 에셋을 고르고 저장하고 검증하는 협업 워크플로를 만든다.

1. 반영해야 하는 코드

성공 기준:
- 디자이너가 `.wfx`를 열고, texture/model path를 고르고, 저장 전 누락 리소스를 확인할 수 있다.
- 저장 전 자동 백업이 남고, 저장 실패 시 원본이 망가지지 않는다.
- authoring path와 runtime resource path의 차이가 UI에서 명확하다.
- 자주 쓰는 챔피언/스킬별 필터와 최근 파일 목록이 있다.
- 포트폴리오용 효과 pack을 만들 때 cue naming과 파일 위치가 흔들리지 않는다.

작업 파일:
- `C:/Users/user/Desktop/Winters/Client/Public/UI/WfxAssetCatalog.h`
- `C:/Users/user/Desktop/Winters/Client/Private/UI/WfxAssetCatalog.cpp`
- `C:/Users/user/Desktop/Winters/Client/Public/GameObject/FX/WfxDocument.h`
- `C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/WfxDocument.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/UI/WfxEffectToolPanel.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxLegacyManifest.cpp`

구현 방향:
- 저장은 atomic에 가깝게 처리한다.
  - `.bak` 생성
  - temp file write
  - temp file 성공 시 replace
  - 실패 시 원본 유지
- 경로 검증은 두 층으로 나눈다.
  - `.wfx` 파일 위치 검증: `Data/LoL/FX`
  - 렌더 리소스 위치 검증: `Client/Bin/Resource`
- runtime resource policy를 UI validation에 반영한다. `Client/Bin/Debug*/Resource`나 `Client/Bin/Release*/Resource` 경로는 warning 또는 error로 처리한다.
- Texture picker는 처음에는 catalog 검색 기반으로 만든다. OS file dialog는 나중에 붙인다.
- 최근 파일 목록은 `imgui.ini`에 기대지 말고 툴 상태 파일을 따로 둔다. 예: `Data/ToolState/WfxEffectToolRecent.json`.
- cue naming convention을 고정한다.
  - `Champion.Skill.Part`
  - 예: `Annie.Q.Fireball`, `Irelia.E.Connect`, `Elden.Boss.SwordTrail`
- `FxLegacyManifest`와 cue catalog가 서로 다른 이름을 말하지 않도록, 최소한 validation에서 mismatched cue를 잡는다.

비범위:
- 원본 Riot/외부 에셋 import 자동 변환은 이번 세션에 넣지 않는다.
- Cooker/DDC와 연결한 배포 파이프라인은 별도 tooling 세션에서 다룬다.
- 멀티유저 동시 편집 lock은 이번 세션에 넣지 않는다.

2. 검증

검증 명령:
- `git diff --check -- Client/Public/UI/WfxAssetCatalog.h Client/Private/UI/WfxAssetCatalog.cpp Client/Public/GameObject/FX/WfxDocument.h Client/Private/GameObject/FX/WfxDocument.cpp Client/Private/UI/WfxEffectToolPanel.cpp Client/Private/GameObject/FX/FxLegacyManifest.cpp`
- `"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nodeReuse:false`

수동 확인:
- 저장 전 `.bak`이 생성되고, 저장 실패를 강제로 만들어도 원본 `.wfx`가 유지된다.
- `Client/Bin/Debug/Resource/...` 경로를 넣으면 validation warning이 뜬다.
- `Client/Bin/Resource/Texture/FX/...`의 texture를 선택하면 missing resource warning이 사라진다.
- 최근에 연 `.wfx`가 툴 재실행 후에도 목록에 남는다.
- `Champion.Skill.Part` convention에서 벗어난 cue name은 warning으로 표시된다.
