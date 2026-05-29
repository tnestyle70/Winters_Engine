Session - 디자이너가 저장한 WFX를 서버 cue 단일 소스 경로로 게임에 연결한다.

1. 반영해야 하는 코드

성공 기준:
- `.wfx`에서 저장한 cue name이 `CFxCuePlayer::FindCue`와 visual hook 경로에서 그대로 사용된다.
- 서버 이벤트/FX cue 하나가 클라이언트에서 FX 한 번으로 이어진다.
- legacy local hook과 network visual hook이 같은 효과를 중복 재생하지 않는다.
- 툴에서 저장한 파일을 재시작 없이 reload하고, 실제 champion skill cue에서 확인할 수 있다.

작업 파일:
- `C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxCuePlayer.cpp`
- `C:/Users/user/Desktop/Winters/Client/Public/GameObject/FX/FxCuePlayer.h`
- `C:/Users/user/Desktop/Winters/Client/Private/GamePlay/VisualHookRegistry.cpp`
- `C:/Users/user/Desktop/Winters/Client/Public/GamePlay/VisualHookRegistry.h`
- `C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxLegacyManifest.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/UI/WfxEffectToolPanel.cpp`

구현 방향:
- `CFxCuePlayer::PreloadDirectory`와 `FindCue`의 default load path는 유지하되, 툴에서 reload 요청을 보낼 수 있는 명시 API를 둔다.
- registry reload는 cue name 기준 replace를 기본으로 한다. handle generation이 바뀌어도 다음 spawn이 새 asset을 잡아야 한다.
- VisualHookRegistry에서 champion skill FX는 raw C++ preset 생성보다 cue name 호출을 우선한다.
- 기존 C++ preset은 fallback 또는 legacy smoke로 남기되, 정상 경로에서는 cue catalog가 우선한다.
- network authoritative gameplay에서 FX는 server event/cue가 client visual path로 들어온 뒤 재생되어야 한다.
- tool preview는 명확히 preview path로만 재생한다. preview에서 성공했다고 server cue 경로가 성공한 것으로 간주하지 않는다.
- debug log는 `OutputDebugStringA/W`를 사용한다.
  - cue found
  - cue missing
  - cue reload
  - skipped unsupported emitter
  - duplicate cue

비범위:
- FlatBuffer schema 변경은 이 세션의 기본 범위가 아니다. 현재 `EffectTriggerEvent`로 충분한지 먼저 확인한다.
- 새로운 gameplay result를 추가하지 않는다.
- server GameSim에서 FX를 직접 렌더링하거나 client-only gameplay truth를 만들지 않는다.

2. 검증

검증 명령:
- `git diff --check -- Client/Private/GameObject/FX/FxCuePlayer.cpp Client/Public/GameObject/FX/FxCuePlayer.h Client/Private/GamePlay/VisualHookRegistry.cpp Client/Public/GamePlay/VisualHookRegistry.h Client/Private/GameObject/FX/FxLegacyManifest.cpp Client/Private/UI/WfxEffectToolPanel.cpp`
- `"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nodeReuse:false`
- 서버 cue 경로를 건드린 경우 `"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nodeReuse:false`

수동 확인:
- 툴에서 `Annie.Q.Fireball` 색을 바꾸고 저장, reload 후 실제 Annie Q visual cue에서 변경이 보인다.
- 서버 authoritative 모드에서 skill input 후 server event/cue emission, client cue application, actual rendering 로그를 구분해 확인한다.
- 같은 skill 한 번에 FX가 두 번 겹쳐 보이지 않는다.
- cue missing 상태에서는 fallback이 있으면 fallback 이름이 로그에 보이고, 없으면 조용히 실패하지 않는다.
- preview path와 server cue path의 로그 prefix가 구분된다.

확인 필요:
- champion별 VisualHookRegistry가 현재 어떤 cue name을 쓰는지 세션 시작 시 `rg \"PlayAll|FindCue|EffectTrigger|VisualHook\"`로 다시 확인한다.
