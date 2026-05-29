Session - 게임플레이를 건드리지 않는 프리뷰 샌드박스와 재생 타임라인을 만든다.

1. 반영해야 하는 코드

성공 기준:
- Preview 버튼을 누를 때마다 이전 preview entity를 정리하거나 명확히 누적 모드를 선택할 수 있다.
- play, pause, restart, loop, scrub에 해당하는 최소 타임라인 조작이 가능하다.
- preview는 client visual world에서만 일어나며 server GameSim, damage, cooldown, projectile truth를 변경하지 않는다.
- attach-to-player, world position, forward, end position, lifetime override, size override를 명확히 제어한다.

작업 파일:
- `C:/Users/user/Desktop/Winters/Client/Public/UI/WfxPreviewSession.h`
- `C:/Users/user/Desktop/Winters/Client/Private/UI/WfxPreviewSession.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/UI/WfxEffectToolPanel.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxCuePlayer.cpp`
- `C:/Users/user/Desktop/Winters/Client/Public/GameObject/FX/FxCuePlayer.h`

구현 방향:
- 새 `WfxPreviewSession`은 preview로 생성한 `EntityID` 목록을 소유한다.
- `CFxCuePlayer::PlayAll`의 `pOutSpawned`를 사용해 spawned entity를 회수하고, restart/clear 때 `CWorld::DestroyEntity`로 정리한다.
- preview context를 UI 상태로 분리한다.
  - spawn mode: player forward, fixed world, attach to player
  - distance
  - end distance
  - lifetime override
  - size override
  - loop
  - accumulate
- pause/scrub은 현재 FX systems가 전역 update tick으로 움직이므로 1차에서는 `restart`, `loop`, `clear`를 완성 목표로 삼는다.
- 실제 time scrub이 필요하면 Session 4 후반에서 preview-only world 또는 effect local time override를 추가한다.
- preview spawn은 edited document를 `CFxSystem::GetAssetRegistry().RegisterOrReplaceByName`으로 반영한 뒤 `CFxCuePlayer::PlayAll`을 사용한다.
- preview 실패는 cue missing, asset missing, unsupported emitter type, texture/model load fail을 구분한다.

비범위:
- 서버 이벤트 재생 시뮬레이션은 Session 6에서 한다.
- GPU capture/profiler는 Session 7에서 한다.
- 독립 preview viewport는 이번 세션에서 만들지 않고, 현재 인게임 카메라를 활용한다.

2. 검증

검증 명령:
- `git diff --check -- Client/Public/UI/WfxPreviewSession.h Client/Private/UI/WfxPreviewSession.cpp Client/Private/UI/WfxEffectToolPanel.cpp Client/Private/GameObject/FX/FxCuePlayer.cpp Client/Public/GameObject/FX/FxCuePlayer.h`
- `"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nodeReuse:false`

수동 확인:
- Preview를 10번 연속 눌러도 이전 preview가 의도 없이 계속 쌓이지 않는다.
- Clear Preview가 preview entity를 즉시 제거한다.
- Attach To Player를 켜면 player 이동을 따라가고, 끄면 world position에 남는다.
- Beam/Ribbon처럼 end position이 필요한 emitter가 preview end distance를 따른다.
- MeshParticle emitter는 `pFxMeshRenderer`가 없으면 실패 이유가 표시되고, 있으면 정상 spawn된다.
- preview 도중 서버 HP, cooldown, projectile simulation 상태가 바뀌지 않는다.
