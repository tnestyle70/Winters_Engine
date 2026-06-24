Session - 2026-06-24 Sylas AI combo, passive BA, hijack ultimate pipeline.

1. 기준 요구사항

- 사일러스 봇은 기본 교전에서 Q를 사용한다.
- 사일러스 봇은 E1으로 거리를 좁히고, E2로 사슬을 맞히는 콤보를 시도한다.
- 사일러스 봇은 W를 사용한다.
- 사일러스 봇은 R로 적 궁극기를 강탈하고, 강탈한 궁극기를 다시 R로 사용한다.
- 사일러스가 스킬을 시전한 뒤 일정 시간 안에 BA를 시전하면 패시브 BA로 처리한다.
- 패시브 BA는 일반 BA와 다른 애니메이션을 재생하고, 원형테 계열 패시브 BA 이펙트를 재생한다.
- 권위 흐름은 `Client Input or AI -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`을 유지한다.

2. 설계 원칙

2-1. AI

- AI는 직접 피해, 이동, 이펙트, 쿨다운 결과를 만들지 않는다.
- AI는 `ChampionAIComboPlan`에 정의된 순서대로 `GameCommand`만 발행한다.
- 쿨다운, 사거리, stage window, target 유효성은 기존 `ChampionAISystem`의 step gate에서 검증한다.
- 사일러스 전용 콤보는 기존 범용 콤보를 덮어쓰되, 다른 챔피언의 기본 콤보 흐름은 유지한다.

2-2. 사일러스 패시브 BA

- 스킬 cast가 서버에서 accept되는 시점에 사일러스 패시브 스택과 지속 시간을 서버 GameSim에 기록한다.
- BA command가 accept될 때 패시브 스택과 시간 창이 유효하면 패시브 BA로 소비한다.
- 패시브 BA 여부는 `CombatActionComponent`의 action stage/flag를 통해 replication 가능한 전투 액션 상태로 전달한다.
- 클라이언트는 action stage를 읽어 일반 BA와 패시브 BA 애니메이션 및 FX cue를 분기한다.

2-3. R 강탈과 사용

- R 강탈 step은 적 target에 대해 `SylasGameSim::CanHijackUltimate`가 참일 때만 발행한다.
- 이미 강탈한 궁극기가 활성화되어 있으면 R 강탈 step은 막는다.
- 강탈 궁극기 사용 step은 `SpellbookOverrideComponent`가 활성화되어 있을 때만 발행한다.
- 실제 강탈과 강탈 궁극기 실행 로직은 기존 `SylasGameSim` 및 CommandExecutor 경로를 재사용한다.

3. 반영 대상 코드

3-1. Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h

- `eChampionAIComboTargetMode`에 사일러스 R 강탈 target mode와 강탈 궁극기 사용 target mode를 추가한다.

3-2. Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp

- 사일러스 전용 combo plan을 추가한다.
- 순서는 `Q -> E1 -> E2 -> BA -> W -> BA -> R steal -> R stolen ultimate`로 둔다.
- E2는 기존 two-stage 규약인 `itemId == 2`를 사용한다.

3-3. Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

- 사일러스 R 강탈 step은 R 준비, override 미활성, target 궁극기 강탈 가능 상태를 모두 확인한다.
- 사일러스 강탈 궁극기 사용 step은 R 준비, override 활성 상태를 확인한다.
- 조건이 맞지 않는 step은 건너뛰어 AI가 콤보 중간에서 정지하지 않도록 한다.

3-4. Shared/GameSim/Components/SylasSimComponent.h

- 사일러스 패시브 BA 스택과 남은 시간을 서버 GameSim 컴포넌트로 보관한다.

3-5. Shared/GameSim/Champions/Sylas/SylasGameSim.h / .cpp

- 스킬 시전 성공 시 패시브를 arm하는 함수와 BA 시 패시브를 consume하는 함수를 추가한다.
- tick에서 패시브 유지 시간을 감소시키고 만료 시 스택을 제거한다.

3-6. Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

- 사일러스 스킬 cast accept 시 패시브 BA window를 연다.
- 사일러스 BA accept 시 패시브 스택을 소비하고 BA action stage를 패시브 stage로 기록한다.
- 패시브 BA flag를 action에 함께 기록해 추후 디버그와 확장 여지를 남긴다.

3-7. Shared/GameSim/Components/CombatActionComponent.h

- `CombatActionFlags::SylasPassive`를 추가한다.

3-8. Shared/GameSim/Systems/Combat/CombatActionSystem.cpp

- BA impact FX trigger에 action stage를 포함해 클라이언트 visual hook이 패시브 BA를 구분할 수 있게 한다.

3-9. Client/Public/GameObject/Champion/Sylas/SylasSkills.h
3-10. Client/Private/GameObject/Champion/Sylas/SylasSkills.cpp
3-11. Client/Private/GameObject/Champion/Sylas/SylasRegistration.cpp

- 사일러스 BA cast frame visual hook을 추가한다.
- 일반 BA는 일반 hit cue를 재생한다.
- stage 2 이상 BA는 패시브 BA cue를 재생한다.

3-12. Client/Private/Network/Client/EventApplier.cpp
3-13. Client/Private/Scene/Scene_InGameNetwork.cpp

- replicated basic attack stage가 패시브 stage이면 `skinned_mesh_sylas_attack_passive` 애니메이션을 선택한다.

3-14. Data/LoL/FX/Champions/Sylas/ba_hit.wfx
3-15. Data/LoL/FX/Champions/Sylas/passive_ba.wfx
3-16. Client/Private/GameObject/FX/FxLegacyManifest.cpp

- 사일러스 일반 BA hit cue와 패시브 BA hit cue를 WFX 파일로 추가한다.
- 패시브 BA는 기존 리소스의 torus, passive swipe, highlight 텍스처를 조합해 원형테 계열 이펙트로 구성한다.

4. 검증 계획

4-1. 코드 위생

```powershell
git diff --check
```

4-2. 서버 빌드

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' .\Server\Include\Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

4-3. 클라이언트 빌드

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' .\Client\Include\Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

4-4. 런타임 스모크 확인

- 사일러스 봇이 Q, E1, E2, BA, W, BA, R 강탈, 강탈 R 사용 순서를 시도하는지 확인한다.
- 스킬 직후 BA에서 패시브 BA 애니메이션과 원형테 FX가 보이는지 확인한다.
- R 강탈 전에는 enemy ultimate target을 향하고, 강탈 이후에는 override 궁극기를 사용하는지 확인한다.
