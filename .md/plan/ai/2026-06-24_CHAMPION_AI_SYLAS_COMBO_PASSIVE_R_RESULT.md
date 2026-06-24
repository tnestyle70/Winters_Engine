Session - 2026-06-24 Sylas AI combo, passive BA, hijack ultimate result report.

1. 기준 문서 대비 반영 상태

- 사일러스 봇 Q 사용: 반영.
- E1으로 거리 좁히기: 반영. 기존 two-stage skill 흐름의 stage1 cast를 사용한다.
- E2 사슬 맞히기: 반영. `itemId == 2` stage2 step으로 발행한다.
- W 사용: 반영.
- R 궁극기 강탈: 반영. `SylasGameSim::CanHijackUltimate`와 spellbook override 상태로 gate한다.
- 강탈한 궁극기 사용: 반영. active override가 있을 때 R step을 다시 발행한다.
- 스킬 직후 패시브 BA: 반영. 서버 GameSim에서 스킬 cast accept 시 패시브 window를 열고 BA accept 시 소비한다.
- 패시브 BA 애니메이션: 반영. replicated BA stage 2 이상이면 `skinned_mesh_sylas_attack_passive`를 선택한다.
- 패시브 BA 원형테 FX: 반영. `Sylas.PassiveBA.Hit` cue와 `passive_ba.wfx`를 추가했다.
- 서버 권위 흐름: 유지. AI와 client visual은 결과를 직접 만들지 않고 command와 replicated action/event를 따른다.

2. 주요 반영 파일

2-1. AI combo

- `Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h`
- `Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp`
- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`

사일러스 전용 콤보를 `Q -> E1 -> E2 -> BA -> W -> BA -> R steal -> R stolen ultimate`로 추가했다. R 강탈 step과 강탈 궁극기 사용 step은 서로 다른 target mode로 나누어 spellbook override 상태에 따라 발행된다.

2-2. Passive BA simulation

- `Shared/GameSim/Components/SylasSimComponent.h`
- `Shared/GameSim/Champions/Sylas/SylasGameSim.h`
- `Shared/GameSim/Champions/Sylas/SylasGameSim.cpp`
- `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`
- `Shared/GameSim/Components/CombatActionComponent.h`
- `Shared/GameSim/Systems/Combat/CombatActionSystem.cpp`

사일러스 스킬 cast accept 시 패시브 스택과 window를 갱신하고, BA accept 시 유효한 패시브 스택을 소비해 action stage 2로 기록한다. combat impact trigger에는 stage 정보가 포함되어 client visual hook까지 전달된다.

2-3. Client visual

- `Client/Public/GameObject/Champion/Sylas/SylasSkills.h`
- `Client/Private/GameObject/Champion/Sylas/SylasSkills.cpp`
- `Client/Private/GameObject/Champion/Sylas/SylasRegistration.cpp`
- `Client/Private/Network/Client/EventApplier.cpp`
- `Client/Private/Scene/Scene_InGameNetwork.cpp`

사일러스 BA visual hook을 등록했다. 일반 BA는 일반 hit cue를, 패시브 BA stage는 패시브 hit cue를 재생한다. replicated action playback에서도 stage 2 이상이면 패시브 BA 애니메이션을 선택한다.

2-4. FX data

- `Data/LoL/FX/Champions/Sylas/ba_hit.wfx`
- `Data/LoL/FX/Champions/Sylas/passive_ba.wfx`
- `Client/Private/GameObject/FX/FxLegacyManifest.cpp`

기존 사일러스 리소스의 swipe, torus, highlight 계열 asset을 사용해 일반 BA와 패시브 BA cue를 등록했다.

3. 검증 결과

3-1. 코드 위생

```powershell
git diff --check
```

- 통과.
- 공백 오류 없음.
- 일부 파일은 기존 줄끝 정규화 때문에 `LF will be replaced by CRLF` 경고만 표시된다.

3-2. 서버 빌드

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' .\Server\Include\Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

- 통과.
- 산출물: `Server/Bin/Debug/WintersServer.exe`
- 기존 `CommandExecutor.cpp` codepage 경고와 DLL interface 계열 경고는 남아 있다.

3-3. 클라이언트 빌드

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' .\Client\Include\Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

- 통과.
- 산출물: `Client/Bin/Debug/WintersGame.exe`
- 기존 `Scene_InGameInput.cpp` codepage 경고와 DLL interface 계열 경고는 남아 있다.

4. 남은 런타임 확인

- 실제 서버/클라이언트 스모크에서 사일러스 봇의 combo step 진행 순서를 눈으로 확인해야 한다.
- 패시브 BA stage가 들어온 프레임에 `skinned_mesh_sylas_attack_passive`와 `Sylas.PassiveBA.Hit`가 같이 보이는지 확인해야 한다.
- 강탈 가능한 적 궁극기가 있는 상황에서 R 강탈 후 다시 R 사용으로 넘어가는지 확인해야 한다.

5. 기준 문서 대비 진행률

- 코드 반영: 완료.
- 서버 빌드 검증: 완료.
- 클라이언트 빌드 검증: 완료.
- 문서화: 완료.
- 런타임 화면 검증: 대기.

현재 상태는 빌드 기준으로 반영 완료이며, 남은 단계는 실제 인게임 스모크에서 사일러스 콤보와 패시브 BA FX를 확인하는 것이다.
