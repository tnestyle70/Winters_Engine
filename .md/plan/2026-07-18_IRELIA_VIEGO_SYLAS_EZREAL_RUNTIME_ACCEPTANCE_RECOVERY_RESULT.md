Session - Irelia E · Viego W/R · Sylas Passive BA · Ezreal R 회귀 복구 구현 및 빌드 검증 결과
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성 · C8 검증이 병목
관련 계획: `2026-07-18_IRELIA_VIEGO_SYLAS_EZREAL_RUNTIME_ACCEPTANCE_RECOVERY_PLAN.md`

# 1. 결론

계획서에서 확정한 코드·데이터 수정과 자동 검증을 반영했고, Debug/Release의 완전한 Data-Driven 파이프라인을 모두 닫았다.

- ChampionGameData 생성 해시: `0x3DAED90B`
- LoL Definition Pack 해시: `0x940E8A30`
- Data-Driven 목표: `12/12 complete`
- 정렬된 스킬 계약: `17 champions / 85 skills / 98 gameplay stages`
- 정렬 계약 해시: `c2ee91f37a8e34b0`
- Client visual timing: `17 champions / 101 stages / mismatch 0`
- Debug 전체 파이프라인: PASS
- Release 전체 파이프라인: PASS
- 결정론 same-seed hash: `CD67433376465495`
- seed+1 hash: `82DF7E84CF9F979C`

이번 결과에서 “빌드 검증”은 완료다. 다만 FBX가 실제 화면에 보이는지, 애니메이션의 체감 방향이 맞는지 같은 렌더 결과는 자동 빌드가 픽셀 단위로 증명할 수 없으므로 마지막 인게임 acceptance 항목으로 분리한다.

# 2. 실제 반영 내용

## 2-1. Irelia E blade/mark visual 조기 종료 제거

파일: `Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp`

`OnCastAccepted_E`에서 authoritative `EffectTrigger`를 다시 legacy `resolvedTargetMode`로 검사하던 분기를 제거했다.

기존에는 서버가 E1/E2를 정상 실행해 기절과 피해를 적용해도, Client `SkillRegistry` 재구성 결과에 따라 `resolvedTargetMode`가 기대값이 아니면 다음 코드 전체가 실행되지 않았다.

- `CIreliaBladeSystem::SpawnPlaced`
- E2 blade 연결
- 적중 대상 mark visual

이제 visual hook은 이미 서버가 확정해 보낸 slot, stage, ground position을 사용한다. 서버 피해·기절 로직과 R/Q mark 경로는 건드리지 않았다.

Debug에서 다음 제한형 로그도 활성화했다.

```text
[IreliaReplayCue][Client] slot=... stage=... effect=... def=... visual=... pos=...
```

## 2-2. Viego W 기획 수치 Data-Driven 조정

파일:

- `Data/Gameplay/ChampionGameData/champions.json`
- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`

Viego W의 canonical 수치를 다음과 같이 변경했다.

| 수치 | 이전 | 현재 |
|---|---:|---:|
| W `rangeMax` | 4.0 | 5.0 |
| charge 최소 기절 | 0.25초 | 0.25초 |
| charge 최대 기절 | 0.75초 | 2.0초 |
| fallback/parity 기절 | 0.75초 | 2.0초 |

`rangeScale=[0.5, 1.0]`은 유지했으므로 실제 tap/full 중심 사거리는 `2.5 / 5.0`이다. 모든 차징 단계가 2초가 된 것이 아니라, 차징 비율에 따라 `0.25초 → 2.0초`로 보간된다. midpoint 계약값은 `1.125초`다.

canonical JSON 변경 후 생성물을 다시 만들었고 freshness check가 통과했다. Yasuo W 등 다른 챔피언의 range는 변경되지 않았다.

## 2-3. Viego R 착지점·피해·슬로우 중심 통일

파일: `Shared/GameSim/Champions/Viego/ViegoGameSim.cpp`

기존 `OnR`은 GroundTarget 명령을 받으면서도 실제 endpoint를 항상 `origin + direction * maxRange`로 계산했다. 가까운 지점을 클릭해도 대시와 원형 피해·슬로우가 최대 사거리 지점을 기준으로 실행될 수 있었다.

`ResolveGroundDashEnd`를 추가해 다음 하나의 `end`를 만들고, 모든 결과가 그 좌표를 공유하도록 수정했다.

1. command의 `groundPos`를 우선 사용한다.
2. 최대 R 사거리 밖이면 max range로 clamp한다.
3. walkable authority가 있으면 caster radius를 사용해 이동 선분을 clamp한다.
4. 대시 endpoint, 원형 피해 중심, 원형 slow 중심이 동일한 `end`를 사용한다.
5. 회전 방향도 최종 착지점을 향하도록 맞춘다.

피해 발생 시점을 arrival event로 바꾸는 확장 작업은 하지 않았다. 이번 범위는 잘못된 중심 좌표를 복구하는 최소 수정이다.

새 SimLab probe는 3m 클릭 대상과 과거 6m 최대 사거리 대상의 위치를 분리해, 클릭 대상만 R 피해와 slow를 받는지 검사한다.

```text
[SimLab][ViegoRLanding] PASS: damage/slow centered on requested landing point
```

## 2-4. Sylas passive BA 권위 경로 계측

파일:

- `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`
- `Client/Private/Network/Client/EventApplier.cpp`

정적 조사와 SimLab에서 이미 다음 경로가 정상임을 확인했기 때문에, 근거 없이 gameplay 수치나 stage를 다시 덮어쓰지 않았다.

- JSON `maxStacks=3`, `stackWindowSec=5`
- accepted skill 이후 passive stack 적립
- BA에서 stack 소비
- 강화 BA `actionStage=2`
- `CombatActionFlags::SylasPassive`
- stage 2 EffectTrigger 한 번
- Client stage2 animation key와 `.wanim` 자산 존재

대신 Debug에서 실제 단절 지점을 바로 확인할 수 있도록 제한형 trace를 넣었다.

```text
[SylasPassive][ServerBA] consumed=... stage=... flags=... remaining=... seq=...
[SylasPassive][ClientAction] stage=... def=... stage2Key=... selected=... played=...
[SylasPassive][ClientCue] stage=... effect=... visual=... authoritative=...
```

서버 trace는 최대 64회, Client action/cue trace는 각각 최대 32회다. Release gameplay에는 영향을 주지 않는다.

집중 자동 검증 결과:

```text
[SimLab][SylasPassive] PASS: JSON 3-stack/5s, consume, BA stage 1/2, one cue
```

따라서 남은 화면 문제가 있다면 trace의 첫 불일치 지점만 수정하면 된다. 현재 자동 증거만으로는 stack 수치나 서버 stage를 다시 고칠 근거가 없다.

## 2-5. Ezreal R 최종 projectile yaw 보정

파일: `Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp`

최종 replicated projectile presentation이 사용하는 catalog yaw를 다음과 같이 바꿨다.

```text
PI → PI / 2
```

관찰 이력은 offset 0에서 -90도, PI에서 +90도였으므로 두 값의 중간 보정인 PI/2를 적용했다. projectile의 서버 이동 벡터와 gameplay 판정은 변경하지 않았다.

# 3. 자동 검증 결과

## 3-1. 스키마와 생성물

실행 명령:

```powershell
python Tools/LoLData/Test-ChampionGameDataSchema.py
python Tools/ChampionData/build_champion_game_data.py
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/ChampionData/build_champion_game_data.py --check
python Tools/LoLData/Build-LoLDefinitionPack.py --check
```

결과:

```text
[ChampionSchemaContract] PASS: canonical + ByRank accepted; nested/stage/charge/target mutations rejected
Check passed: generated files match champions.json (hash 0x3DAED90B)
Checked LoL definition pack 0x940E8A30
Champions: 17, skills: 85, summoner spells: 1
```

## 3-2. Debug 집중 빌드와 probe

실행 명령:

```powershell
MSBuild Tools/SimLab/SimLab.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
Tools/Bin/Debug/SimLab.exe --stage-input-only
Tools/Bin/Debug/SimLab.exe --sylas-passive-only
```

최종 결과:

```text
SimLab.vcxproj -> Tools/Bin/Debug/SimLab.exe
[SimLab][DataContract] PASS: 17 champions, 85 skills, 98 stages, orderedContract=c2ee91f37a8e34b0
[SimLab][IreliaWRelease] PASS: W1 blocks E, W2 accepts, move+E accept same tick
[SimLab][ChargeContract] PASS: curves, max-hold release, CC cancellation
[SimLab][ViegoRLanding] PASS: damage/slow centered on requested landing point
[SimLab][SylasPassive] PASS: JSON 3-stack/5s, consume, BA stage 1/2, one cue
[SimLab] PASS
```

집중 빌드 도중 새 검증 코드에서 드러난 두 컴파일 문제도 즉시 수정하고 재검증했다.

- Sylas trace에서 `SylasSimComponent` 선언 헤더 누락
- SimLab에서 `SkillGameplayDef::rangeMax` 대신 실제 구조인 `range.rangeMax`를 사용해야 하는 문제

수정 후 같은 빌드가 error 0으로 통과했다.

## 3-3. Debug 완전 파이프라인

실행 명령:

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug -RequireComplete
```

포함된 주요 게이트:

- definition pack freshness
- recursive schema mutation contract
- legacy ownership/authoritative reader audit
- draft round-trip과 stale hash reject
- Data-Driven 목표 12/12
- Shared dependency boundary
- GameSim Debug build
- Server Debug build/link
- Client Debug build/link
- SimLab Debug build
- 1800-tick deterministic regression
- whitespace validation

최종 결과:

```text
[LoLDataDriven] PASS
```

## 3-4. Release 완전 파이프라인

실행 명령:

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Release -RequireComplete
```

주요 산출물:

```text
Shared/GameSim/Bin/Release/WintersGameSim.lib
Server/Bin/Release/WintersServer.exe
Client/Bin/Release/WintersGame.exe
Tools/Bin/Release/SimLab.exe
```

결정론 결과:

```text
[SimLab] same-seed replay OK: hash=CD67433376465495
[SimLab] seed sensitivity OK: seed+1 hash=82DF7E84CF9F979C
[SimLab] PASS
[LoLDataDriven] PASS
```

## 3-5. diff 검사

이번 slice 대상 파일에 `git diff --check`를 실행했고 오류는 없었다. 출력된 내용은 기존 Windows worktree의 LF→CRLF 변환 예고뿐이다.

# 4. 빌드로 닫힌 범위와 인게임 acceptance 경계

## 자동 검증으로 닫힌 범위

- Irelia W hold/release 후 이동과 E command 계약
- Viego W canonical range/charge stun 값과 생성물 일치
- Viego R의 대시·피해·slow 중심 좌표 일치
- Sylas passive 3 stack/5초, 소비, 강화 BA stage 2, cue 1회
- Debug/Release GameSim, Server, Client, SimLab 컴파일·링크
- 17/85/98 gameplay contract와 101 visual timing stage
- same-seed 결정론

## 인게임에서 마지막으로 확인할 범위

1. Irelia E1 blade FBX, E2 blade/connect, 적중 대상 mark가 실제 화면에 보이는가.
2. Viego W tap/full 사거리와 `0.25초 → 2.0초` 기절 체감이 원하는가.
3. Viego R을 2m/4m/최대 사거리에 사용했을 때 화면의 착지·원형 피해·slow 중심이 같은가.
4. Sylas trace가 `Server stage=2 → Client played=1 → Client visual=1` 순서인지, passive animation/FX가 화면에 보이는가.
5. Ezreal R을 ±X, ±Z, 대각선에서 발사했을 때 mesh가 이동 방향과 평행한가.

위 항목은 빌드 실패 가능성 때문에 남긴 것이 아니라, 렌더·애니메이션의 최종 시각 acceptance이기 때문에 남긴 것이다. 실패하면 이제 해당 trace/최종 writer 한 구간만 되돌아가면 되며 Data-Driven gameplay 계약 전체를 다시 흔들 필요가 없다.

# 5. 최종 상태

이번 요청의 “구현 후 빌드 검증까지”는 완료했다.

- 구현: 완료
- schema/codegen freshness: 완료
- focused regression: 완료
- Debug 전체 파이프라인: 완료
- Release 전체 파이프라인: 완료
- RESULT 문서화: 완료
- 인게임 시각 acceptance: 사용자 실행 경계로 명시
