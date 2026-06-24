Session - 2026-06-24 Champion AI combo, LeeSin Q2, ward vision, ward-hop pipeline.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h

- `ChampionAIComboPlan` capacity를 8에서 10으로 올린다.
- `eChampionAIComboTargetMode`에 `WardBehindTarget`, `LastOwnWard`를 추가한다.

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp

- 모든 챔피언 기본 콤보를 `Q -> BA -> E -> BA -> R`로 둔다.
- 리신 콤보를 `Q -> Q2 -> BA -> E -> BA -> E2 -> WardBehindTarget -> W(LastOwnWard) -> R`로 둔다.
- Q2/E2는 기존 `itemId == 2` stage2 규약을 사용한다.
- ward 설치는 기존 `UseItem` command와 `kTrinketWardItemId`를 사용한다.

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

- AI는 직접 데미지, 표식, 이동 판정을 만들지 않고 `GameCommand`만 만든다.
- 배우지 않은 스킬 단계는 현재 콤보에서 건너뛰어 저레벨 봇이 멈추지 않게 한다.
- Q2/E2는 일반 쿨다운 대신 `SkillStateComponent::currentStage`, `stageWindow`, `GameplayDefinitionQuery::IsSkillTwoStage`로 준비 판정한다.
- 리신 Q2는 대상에게 자기 표식 `LeeSinQMarkComponent`가 있을 때만 발행한다.
- 리신 ward step은 적 뒤쪽 지점을 계산해 `UseItem` ward command를 만든다.
- 리신 W step은 방금 설치한 `LeeSinWardOwnerComponent` ward를 찾아 target entity로 사용한다.

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/WardDefinitions.h

- ward item id, 설치 사거리, 시야 범위, 지속 시간, spatial radius를 공용 상수로 둔다.

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

- `UseItem` command를 switch에 연결한다.
- `kTrinketWardItemId`만 처리한다.
- 설치 위치는 caster 기준 사거리와 walkable query로 보정한다.
- 생성 ward에는 `TransformComponent`, `WardComponent`, `LeeSinWardOwnerComponent`, `SpatialAgentComponent(eSpatialKind::Ward)`, `VisionSourceComponent`, `VisibilityComponent`, `TargetableTag`, `NetEntityIdComponent`를 붙인다.

1-6. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/LeeSin/LeeSinGameSim.cpp

- 리신 W hook을 등록한다.
- W stage1은 아군 챔피언, 아군 미니언, 아군 ward만 target dash 대상으로 허용한다.
- W dash는 기존 `LeeSinDashComponent` 경로를 재사용한다.

1-7. C:/Users/tnest/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

- `WardComponent` entity를 `EntityKind::Ward`로 스냅샷에 싣는다.

1-8. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

- `WardComponent::remainingDuration`을 서버 틱에서 감소시키고 만료 시 destroy 및 net id unbind를 수행한다.

1-9. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

- `EntityKind::Ward` 수신 시 ward runtime tag, spatial, vision, visibility, targetable, small billboard marker를 붙인다.
- stale snapshot 제거 대상에 ward를 포함한다.

1-10. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGameInput.cpp

- 서버 권위 모드에서 `4` 키로 커서 위치에 trinket ward `UseItem` command를 보낸다.

1-11. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGameLocalSkills.cpp

- 리신 W stage1만 예외적으로 아군/ward hover target을 허용한다.
- 다른 `UnitTarget` 스킬은 기존 적 대상 규칙을 유지한다.

1-12. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/GameplayQuery.cpp

- hover target query가 아군과 ward도 잡을 수 있게 한다.
- 공격 명령은 기존 `IsValidAttackTarget` enemy filter를 계속 사용한다.

1-13. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp

- `SendUseItem`을 추가해 `UseItem`, `itemId`, ground position, direction을 서버로 보낸다.

2. 검증

2-1. 동기화

```powershell
git pull --ff-only origin main
git status --short --branch
```

결과:

- `main`은 `origin/main`과 동기화되어 있었다.

2-2. 코드 위생

```powershell
git diff --check
```

결과:

- 공백 오류 없음.
- 일부 파일은 기존 줄끝 정책 때문에 `LF will be replaced by CRLF` 경고만 표시됨.

2-3. 서버 빌드

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' .\Server\Include\Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

결과:

- 성공.
- 기존 DLL interface/codepage 계열 경고는 남아 있음.

2-4. 클라이언트 빌드

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' .\Client\Include\Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

결과:

- 성공.
- 기존 DLL interface/codepage 계열 경고는 남아 있음.

2-5. 런타임 스모크 기준

- 리신 AI가 Q를 발사한다.
- Q 표식이 붙은 대상에게만 Q2를 발행한다.
- Q2 이후 BA, E, BA, E2, ward 설치, W ward-hop, R 순서를 시도한다.
- 전용 콤보가 없는 챔피언도 Q, BA, E, BA, R 기본 순서를 시도한다.
- `4` 키로 ward를 설치하면 `EntityKind::Ward`로 복제되고 주변 시야 source가 생긴다.
- 리신 W는 hover된 아군 또는 ward target으로 서버 확정 dash를 수행한다.
