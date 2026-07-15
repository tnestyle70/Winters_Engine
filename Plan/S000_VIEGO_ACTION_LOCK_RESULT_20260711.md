Session - Viego 원혼/빙의와 서버 권위 행동 잠금 회귀를 구현·빌드·결정론 검증한 결과를 박제한다.

1. 반영한 코드

권위 흐름은 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`로 고정했다. 클라이언트 애니메이션 길이는 더 이상 이동 가능 여부의 truth가 아니며, 서버가 승인한 action의 `sourceChampion/sourceSlot/stage/movePolicy/lockEndTick/commandSequence`만 이동 정책을 결정한다.

Viego 원혼과 빙의는 다음 상태로 반영했다.

```text
Viego가 직접 적 챔피언 처치
-> killer 전용 원혼 1개 생성
-> 피해/일반 타게팅 면역 상태로 5초 유지
-> 해당 Viego의 우클릭만 섭취 또는 NavGrid 추격
-> 0.72초 StationaryChannel 후 피해 챔피언 visual + BA/QWE identity 적용
-> R은 override mask 밖의 Viego R로 해석
-> 빌린 runtime 취소 + Viego QWE bank 복원 + Viego R 실행
```

`ViegoSoulComponent`에는 `eligibleViego`와 피해자의 QWE rank를 저장한다. 다른 아군이 처치하거나 다른 Viego가 우클릭한 경우 원혼을 만들거나 먹지 못한다. 소비·만료 시 `EntityIdMap::Unbind`를 거쳐 stale NetID도 남기지 않는다. 원혼은 `DamagePipeline`과 일반 target query에서 제외하되, owner Viego의 `AttackChase` 상호작용만 별도 허용했다.

빙의 중 base truth인 `ChampionComponent.id == VIEGO`는 유지한다. `FormOverrideComponent`가 visual/skill champion과 `BA|Q|W|E` mask만 보유하고 R은 Viego로 남는다. 명시적인 stolen champion query가 entity의 Viego loadout보다 우선하도록 `GameplayDefinitionQuery`를 분리했고, 기본 공격도 `ResolveBasicAttack`과 `CombatActionComponent.eSourceChampion`을 통해 피해 챔피언의 range/timing/effect identity를 사용한다.

피해자의 QWE rank를 적용하는 동안 Viego의 원래 QWE rank/runtime은 별도 bank에 보관한다. 원래 cooldown과 stage window는 빙의 중에도 background tick되어 오래된 W2 같은 단계가 복귀 후 되살아나지 않는다. 빙의 중 QWE 레벨 업은 borrowed rank가 아니라 원래 Viego progression bank에 반영한다. 폼 종료 시 Fiora/Jax/Yasuo/Yone의 private dash를 포함해 공개된 borrowed champion runtime을 취소하고, Zed mark/vanish와 Lee Sin/Sylas/Kalista transient도 정리한다.

클라이언트는 피해 챔피언 renderer를 attach하고, 원혼을 초록 tint의 실제 alpha blend로 그린다. 원혼은 모든 opaque object/contact shadow 뒤, Fog/FX 앞의 far-to-near transparent pass에서 렌더한다. DX11 blend/depth 상태는 draw 범위 RAII로 복구한다. 조기 섭취와 scene exit 모두 SoulIdle FX tracking을 정리한다. 로컬 플레이어 R 복귀는 `Attach -> Bind -> Retry R` 순서로 실행해 Idle이 R 애니메이션을 덮지 않는다.

행동 이동 정책은 다음처럼 서버에서 명시했다. 현재 hook이 없는 스킬은 허위 dash를 만들지 않도록 Allow 또는 짧은 Queue fallback을 사용한다.

| 챔피언 | Q | W | E | R |
|---|---|---|---|---|
| Annie | Queue | Queue | Allow | Queue |
| Ashe | Allow | Queue | Allow | Queue |
| Ezreal | Queue | Queue | Forced | Queue |
| Fiora | Forced | Stationary | Allow | Allow |
| Garen | Allow | Allow | Allow | Queue |
| Irelia | Forced | S1 Stationary / S2 Queue | Allow | Queue |
| Jax | Forced | Allow | Allow | Allow |
| Kalista | Queue | Queue | Allow | Queue |
| Kindred | Allow | Allow | Allow | Allow |
| Lee Sin | Q1 Queue / Q2 Forced | W1 Forced / W2 Allow | Allow | Queue |
| Master Yi | Allow | Allow | Allow | Allow |
| Riven | Queue | Queue | Allow | Allow |
| Sylas | Queue | Allow | Forced | Queue |
| Viego | Queue | W1 Allow / W2 Forced | Allow | Forced |
| Yasuo | Queue | Allow | Forced | Forced |
| Yone | Queue | Queue | Forced | Forced |
| Zed | Queue | W1 Allow / W2 Forced | Allow | Forced |

`Allow` cast는 기존 `MoveTarget`을 지우지 않는다. `QueueUntilUnlock`의 입력 잠금은 full animation 길이가 아니라 최대 8 simulation ticks로 제한하며 마지막 Move 입력만 보관한다. `StationaryChannel`과 `ForcedMotion`은 lock 중 다른 공격/cast/flash를 막되 동일 슬롯의 stage-2 release는 허용한다. W2와 raw Jax E2/Yone E2는 이전 lock의 `max()`를 사용하지 않고 새 action tick으로 교체하며 queued move를 보존한다. 서버가 거절한 cast는 ActionState sequence를 만들지 않고, 클라이언트도 로컬 선행 lock을 만들지 않는다.

Snapshot/Event schema 끝에는 다음 권위 필드를 추가하고 C++/Go generated 파일을 재생성했다.

```text
actionSourceChampionId
actionSourceSlot
actionMovePolicy
actionLockEndTick
actionCommandSeq
```

LAN 실행 경계는 이미 서버가 `INADDR_ANY`, 즉 `0.0.0.0:9000`으로 listen한다. 같은 Wi-Fi의 다른 PC에서는 서버 PC의 IPv4 주소를 사용한다.

```powershell
# 서버 PC
Server\Bin\Debug\WintersServer.exe

# 클라이언트 PC
Client\Bin\Debug\WintersGame.exe --server-host=<서버_PC_IPv4> --server-port=9000
```

Windows 방화벽 인바운드 TCP 9000 허용은 배포 PC에서 별도로 확인해야 한다. 서버/클라이언트는 동일 commit의 schema와 `Client/Bin/Resource`를 사용해야 한다.

콘텐츠 완성도 경계도 남긴다. BA/QWE identity routing 자체는 전 챔피언 공통으로 동작하지만, 현재 서버 hook이 완전한 roster는 Annie/Ashe/Fiora/Irelia/Jax/Lee Sin/Viego/Yasuo/Yone/Zed 중심이다. Kalista/Kindred/Riven/Sylas/Ezreal은 부분 구현이며 Master Yi/Garen은 전용 server hook이 불완전하다. 이 챔피언은 routing 버그가 아니라 skill content 구현 세션이 추가로 필요하다.

2. 검증

`Tools/SimLab/main.cpp`에 다음 회귀 프로브를 추가했다.

```text
[Viego]
- non-Viego kill은 soul 0
- killer owner/champion/QWE rank 일치
- 일반 damage 0, non-owner consume 거절
- 먼 거리 우클릭 chase 유지 후 consume
- consume NetID unbind, 0.72초 pending command gate
- BA/QWE override와 무기한 form
- 원래 cooldown background tick, level-up bank 분리
- R 복귀, Jax state 취소
- borrowed Yone E runtime을 form exit에서 취소하고 이후 강제 귀환 없음
- 5초 soul 만료와 NetID 정리

[ActionLock]
- Allow cast가 기존 MoveTarget 보존
- Queue cast lock <= 8 ticks
- Move A/Move B 중 B만 unlock 후 실행
- rejected cast가 action sequence를 만들지 않음
- Viego W2가 임의로 길게 남긴 W1 lock을 교체
```

최종 실행 결과:

```text
[SimLab][Viego] PASS: owner soul, chase, possession, QWE/BA, R restore
[SimLab][ActionLock] PASS: Allow/Queue/Forced, last-input, reject, W2 replace
[SimLab] same-seed replay OK: hash=70CF741D5D501315
[SimLab] seed sensitivity OK: seed+1 hash=FFEFADA355D178E8
[SimLab] PASS
```

빌드/정적 검증:

```text
Build-LoLDefinitionPack.py --check -> PASS, hash 0x94A01989
Shared/Schemas/run_codegen.bat -> PASS
Check-SharedBoundary.ps1 -> PASS
Tools/SimLab/SimLab.vcxproj Debug x64 -> PASS
Server/Include/Server.vcxproj Debug x64 -> PASS
Client/Include/Client.vcxproj Debug x64 -> PASS
WintersServer.exe --smoke-seconds=3 -> PASS, listening 0.0.0.0:9000
git diff --check -> PASS
```

두 대의 실제 PC에서 mesh/alpha/R animation을 눈으로 확인하는 최종 LAN 캡처는 이 로컬 세션에서 대신할 수 없으므로 위 실행 명령으로 수동 확인 항목을 남긴다. 빌드와 서버 listen, schema, 서버 권위 결정론 경로까지는 자동 검증 완료다.
