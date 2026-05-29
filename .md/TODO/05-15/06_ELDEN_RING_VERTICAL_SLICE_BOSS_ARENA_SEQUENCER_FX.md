# Elden Ring Vertical Slice Boss Arena Sequencer FX

Session - Elden Ring류 boss arena vertical slice를 world streaming, sequencer, camera, montage/notify, FX graph로 증명한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Engine/Public/World/WorldPartition.h

새 파일:
- small-scale WorldPartition MVP를 만든다.

반영:
- cell grid, cell state, streaming source, load range, priority를 둔다.
- MVP는 boss arena 주변 3x3 또는 5x5 cell만 사용한다.
- HLOD는 full system 대신 proxy mesh placeholder와 load/unload debug draw로 시작한다.

### 1-2. C:/Users/user/Desktop/Winters/Engine/Private/World/WorldPartition.cpp

새 파일:
- streaming source 기준으로 desired cell set을 계산한다.

반영:
- player camera, boss arena trigger, cinematic camera를 streaming source로 등록할 수 있게 한다.
- cell load/unload는 synchronous placeholder로 시작하고 async loader는 후속으로 둔다.
- debug overlay에 loaded/activated/unloaded cell count를 표시한다.

### 1-3. C:/Users/user/Desktop/Winters/Engine/Public/Cinematic/Sequence.h

새 파일:
- sequencer data model MVP를 만든다.

반영:
- `CSequence`, `ITrack`, `CTransformTrack`, `CCameraTrack`, `CEventTrack`, `CFxTrack`, `CMontageTrack`을 둔다.
- frame rate는 fixed tick resolution으로 둔다.
- binding은 runtime entity id 또는 named binding으로 시작한다.

### 1-4. C:/Users/user/Desktop/Winters/Engine/Private/Cinematic/SequencePlayer.cpp

새 파일:
- sequence player가 track을 frame time 기준으로 evaluate한다.

반영:
- play, pause, stop, seek를 지원한다.
- event track은 한 번만 fire되도록 last evaluated frame을 저장한다.
- server-replicated cinematic은 후속으로 두고 MVP는 client deterministic playback으로 시작한다.

### 1-5. C:/Users/user/Desktop/Winters/Engine/Public/Animation/AnimMontage.h

새 파일:
- boss attack와 player dodge/parry timing을 montage/notify로 표현한다.

반영:
- montage section, notify track, event name, frame time을 둔다.
- gameplay hit frame은 server/shared event로 연결할 수 있게 data화한다.
- visual-only notify는 client FX/sound cue로 연결한다.

### 1-6. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_EldenBossArena.cpp

새 파일:
- boss arena vertical slice scene을 만든다.

반영:
- arena load, player spawn, boss spawn, lock-on camera, boss phase trigger를 포함한다.
- boss intro sequence가 camera, animation, FX, UI blackout/fade를 제어한다.
- gameplay truth는 server/shared GameSim 확장 전까지 lab path로 명시한다.

### 1-7. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Boss

새 파일:
- boss actor, boss AI state, boss attack pattern data를 둔다.

반영:
- pattern은 sequence/montage/FX asset을 참조한다.
- hit validation은 capsule/sphere primitive로 시작한다.
- phase 2 진입 시 sequence와 FX graph를 동시에 trigger한다.

### 1-8. C:/Users/user/Desktop/Winters/Client/Private/UI/SequencerPanel.cpp

새 파일:
- ImGui timeline preview panel을 만든다.

반영:
- track list, playhead, keyframe list, play/pause/seek를 MVP로 둔다.
- curve editor는 후속으로 둔다.
- sequence asset save/load는 JSON으로 시작한다.

### 1-9. C:/Users/user/Desktop/Winters/Engine/Public/FX/FxAsset.h

목표:
- Elden boss spell FX가 FX Tool MVP asset을 재사용한다.

반영:
- ground rune, beam, ribbon trail, burst를 `.wfx` emitter 조합으로 표현한다.
- GPU compute 대량 particle은 vertical slice MVP 필수에서 제외하고 후속 milestone으로 둔다.

## 2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

런타임 검증:
- boss arena scene 진입.
- world cell debug overlay에서 arena 주변 cell load 상태 확인.
- boss intro sequence가 camera -> montage -> FX -> gameplay event 순서로 재생.
- boss phase 2 trigger 후 FX graph와 camera shake가 동시에 재생.

합격 기준:
- 첫 화면에서 boss arena vertical slice가 바로 플레이 가능하다.
- sequence event가 중복 fire되지 않는다.
- visual lab path가 normal LoL F5 server authority path를 숨기거나 우회하지 않는다.
