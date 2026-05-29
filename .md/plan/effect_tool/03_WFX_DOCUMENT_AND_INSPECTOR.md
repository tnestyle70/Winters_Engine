Session - Lifetime, Color, Duration 계열 속성을 안정적으로 편집하는 WFX 문서 모델과 Inspector를 만든다.

1. 반영해야 하는 코드

성공 기준:
- `.wfx`를 열면 emitter 목록, 선택 emitter, dirty state, validation state가 분리되어 관리된다.
- Lifetime, Start Delay, Fade In, Fade Out, Color, Width, Height, Duration 계열 값이 의도한 범위 안에서 편집된다.
- emitter 추가, 복제, 삭제, 이름 변경, 순서 이동을 할 수 있다.
- 저장한 파일을 다시 로드해도 값이 손실되지 않는다.
- 현재 `FxAsset.cpp`의 단순 문자열 JSON 파서 때문에 놓칠 수 있는 필드는 툴 쪽 검증에서 드러난다.

작업 파일:
- `C:/Users/user/Desktop/Winters/Client/Public/GameObject/FX/WfxDocument.h`
- `C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/WfxDocument.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/UI/WfxEffectToolPanel.cpp`
- `C:/Users/user/Desktop/Winters/Engine/Public/FX/FxAsset.h`
- `C:/Users/user/Desktop/Winters/Engine/Private/FX/FxAsset.cpp`

구현 방향:
- `CWfxDocument`에 dirty flag와 selected emitter index를 넣을지, UI state에 둘지 먼저 정한다. 추천은 document에는 dirty/validation만 두고 selection은 UI state에 둔다.
- `CWfxDocument`에 최소 조작 API를 추가한다.
  - `MarkDirty`
  - `ClearDirty`
  - `Validate`
  - `AddEmitter`
  - `DuplicateEmitter`
  - `RemoveEmitter`
  - `MoveEmitter`
- Inspector는 섹션을 고정 순서로 둔다.
  - Identity: cue name, emitter name, render type
  - Timing: lifetime, start delay, fade in, fade out, grow duration
  - Shape: width, height, radius, thickness, scale, rotation, offsets
  - Material: texture, erode texture, color, blend, depth, alpha clip, erode threshold
  - Animation: atlas cols, rows, frame count, fps, loop, UV scroll
  - Gameplay metadata: wind wall blockable 같은 visual/gameplay boundary 값
- `Duration`이라는 이름은 runtime 구조체에 없으므로, UI에서는 `Lifetime`을 기본 표시명으로 쓰고 필요하면 tooltip 또는 alias로만 제공한다.
- 숫자 입력은 slider만 쓰지 않는다. 미세 조정이 필요한 값은 `DragFloat`, 정확한 값은 input field를 같이 제공한다.
- 범위 clamp는 저장 전에 한 번 더 한다. 예: lifetime은 0보다 커야 하고, atlas cols/rows/frame count는 1 이상이어야 한다.

구조화 JSON 결정:
- `Client/Public/Network/Backend/json.hpp`에 nlohmann json이 있지만, Engine이 Client에 의존하면 안 된다.
- Session 3에서는 `CWfxDocument` 저장 안정성과 validation을 먼저 잡고, Engine loader를 nlohmann으로 바꾸는 작업은 별도 확인 후 진행한다.
- Engine loader를 바꾸려면 JSON header를 Engine 또는 Shared 쪽으로 이동하는 계획을 먼저 세운다.

2. 검증

검증 명령:
- `git diff --check -- Client/Public/GameObject/FX/WfxDocument.h Client/Private/GameObject/FX/WfxDocument.cpp Client/Private/UI/WfxEffectToolPanel.cpp Engine/Public/FX/FxAsset.h Engine/Private/FX/FxAsset.cpp`
- `"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nodeReuse:false`

수동 확인:
- `q_fireball.wfx` 로드 후 color, lifetime, atlas fps를 바꾸고 저장한 뒤 다시 로드하면 값이 유지된다.
- emitter를 복제하고 이름을 바꿔 저장하면 emitter count와 이름이 유지된다.
- emitter 삭제 후 저장했을 때 비어 있는 `.wfx`는 저장되지 않고 validation error가 뜬다.
- lifetime을 0 이하로 입력하려 하면 clamp되거나 validation error가 보인다.
- missing texture, duplicate emitter name, invalid atlas frame count가 UI에 표시된다.

확인 필요:
- Engine loader를 구조화 JSON으로 교체할지, Client tool serializer만 먼저 안정화할지 결정.
