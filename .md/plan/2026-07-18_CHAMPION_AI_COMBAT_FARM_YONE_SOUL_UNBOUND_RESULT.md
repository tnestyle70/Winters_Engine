Session - 전 챔피언 AI 전투·파밍 기반 개선과 요네 영혼해방 권위 구현 결과

## 1. 완료 판정

이번 세션은 기존의 3초 단위 단발 스킬 선택을 공통 데이터 기반 전투 상태 머신으로 교체하고, 요네의 `E → Q → BA → W → BA → Q → E2`를 실제 명령 승인 순서와 위험 기반 복귀 판단까지 연결했다.

자동 검증 기준에서는 완료다. 사용자 인게임 게이트에는 챔피언별 콤보 체감, 요네 E 표식의 크기·위치, 실제 라인전에서 포탑 진입 직전 E2 복귀 타이밍이 남는다.

## 2. 공통 AI 반영

- 17개 챔피언 profile에 Q/W/E/R 우선순위와 명시적 combo plan을 연결했다.
- 공통 콤보 상태 머신은 기본 공격 주기와 스킬 쿨다운 중 현재 단계를 유지하며, 승인된 명령 뒤에만 다음 단계로 진행한다.
- 신규 콤보 시작은 공통 cast interval을 지키고, 요네 전용 경로도 `comboTarget/comboStep`을 선점하지 않는다.
- 현재 자원으로 지불할 수 없는 스킬은 명령 후보에서 제외한다. 재시전 단계는 추가 비용 없이 기존 stage 계약을 따른다.
- AI는 profile의 `skillRules`로 레벨 포인트를 사용하고, 막타 가능한 미니언을 기본 공격 또는 스킬 피해로 우선 선택한다.
- 원거리 profile은 `preferredRange`와 `kiteBias`를 실제 이동 목표에 사용한다.
- 공통 위협항은 적의 준비된 BA와 Q/W/E/R의 데이터 피해를 방어력 이후 체력 비율로 환산한다. 자원 부족, 사거리, Self/OnHit, BA 준비, 2단계 recast 상태를 구분한다.
- 위협항은 전 챔피언의 Fight/Retreat 점수에 함께 들어가며, 요네 E2도 같은 Retreat 우세 판정을 소비한다.

## 3. 요네 E 반영

- 기본 콤보는 `E → Q → BA → W → BA → Q`이며 마지막 E2 데이터 단계는 즉시 시전이 아닌 복귀 대기 게이트다.
- E2는 남은 E 시간이 0.75초 이하이거나, 공통 Retreat 점수가 Fight 점수보다 우세하거나, 교전 대상이 사라졌을 때 발행한다.
- E1 시점의 E 랭크를 스냅샷하고, E 동안 요네가 적 챔피언에게 준 실제 물리·마법 체력 피해를 대상별로 저장한다.
- 기본 공격 및 Q의 아이템 온힛 피해는 저장량에서 제외한다. 보호막 흡수분도 저장하지 않는다.
- 복귀 시 저장 피해의 `25 / 27.5 / 30 / 32.5 / 35%`를 고정 피해로 한 번 적용하며 echo 자체는 재누적하지 않는다.
- CC 중에도 E 타이머가 흐르고, SoulReturn은 기존 `ForcedMotionComponent`를 제거해 완료 뒤 앵커가 다시 덮이지 않게 한다.
- 대상 사망, 시전자 사망, 런타임 취소, 정상 복귀에서 표식을 정리한다. 취소 경로는 TickContext를 전달해 stage 4 제거 cue를 한 번 발행한다.
- 서버 stage 3/4 cue로 `Yone.E.SoulMark` WFX를 생성·제거하고, 씬 종료 시 정적 FX handle 저장소도 비운다.

## 4. 에이전트 비평 반영

별도 비평 에이전트의 1차·2차 리뷰를 모두 반영했다.

- P0: 사망 수명주기, 아이템 피해 혼입, 쿨다운 중 단계 건너뜀, 적 예상 콤보 피해 누락을 수정했다.
- P1: 요네 전용 시작 쿨다운 우회, CC 타이머 정지, 대상 사망, E 랭크 변경, 거절 E2 정리 순서, 무조건 E2를 수정했다.
- 2차 P1: 자원 없는 스킬·사거리 0 강화기의 위협 과대평가, 강제 이동의 앵커 재덮기, 취소 cue 누락을 수정하고 자동 회귀를 추가했다.
- P2: 씬 교체 시 클라이언트 표식 handle을 정리했다.
- 현재 고유 챔피언 로스터 밖의 미러 요네 2명이 동일 대상에 동시에 거는 복수 표식은 단일 `YoneSoulMarkComponent` 계약으로 지원하지 않는다. 멀티소스 컨테이너가 필요한 별도 확장이다.

## 5. 생성·빌드·검증 실측

### 데이터 생성 계약

```text
ChampionGameData build hash: 0x6BACE85B
LoL definition pack hash:    0x76B43B5B
Champion count:              17
Skill count:                 85
Ordered contract:            0x0C098CDBB67CC155
두 생성기 --check:           PASS
git diff --check:            PASS (개행 변환 경고만 존재)
```

### 전체 빌드

```text
MSBuild Winters.sln /m:1 /p:Configuration=Debug /p:Platform=x64
exit 0
Engine / GameSim / Server / Client / SimLab / EldenRingClient PASS
기존 C4251/C4275 DLL interface warning은 남아 있으나 신규 compile/link error 없음
```

### SimLab

```text
Tools\Bin\Debug\SimLab.exe 600 1234
[SimLab][ChampionAI] PASS
[SimLab][ChampionAI][MidDefense] PASS: D91E971C9DAEA9A9
[SimLab][YoneE] PASS: exact combo, resource/stage threat gate,
forced return, item/shield exclusion, echo=25/35,
target/caster death cleanup
[SimLab][DataContract] PASS: 17 champions, 85 skills, 98 stages
[SimLab] same-seed replay OK: FEA5AEDF274DE383
[SimLab] PASS
exit 0
```

요네 회귀는 실제 `CChampionAISystem → GameCommand → CDefaultCommandExecutor → YoneGameSim` 경로를 최대 360틱 실행한다. 승인 순서, R 미개입, E2 즉시 복귀 금지, 적 위협 증가 후 복귀, 자원·stage 위협 필터, rank 1/5 echo, 마법·보호막, 아이템 제외, 강제 이동, 대상/시전자 사망, 취소 cue를 단언한다.

## 6. 주요 변경 표면

- AI 데이터·생성: `Data/LoL/ServerPrivate/AI/ChampionAIGameplayDefs.json`, `Tools/LoLData/Build-LoLDefinitionPack.py`, 생성된 policy/definition pack
- 공통 AI: `ChampionAIPolicy`, `ChampionAIPerception`, `ChampionAIValuation`, `ChampionAISystem`, `ChampionGameplayAssembly`
- 요네 권위: `YoneSimComponent`, `YoneGameSim`, `DamageQueueSystem`, `DamagePipeline`, `DamageTypes`, `WorldKeyframe`
- 클라이언트 시각: `Yone_Skills.cpp/.h`, `Scene_InGameLifecycle.cpp`, `Data/LoL/FX/Champions/Yone/e_soul_mark.wfx`
- 자동 검증: `Tools/SimLab/main.cpp`

## 7. 인게임 확인 항목

1. 요네가 실제 라인전에서 `E-Q-평-W-평-Q`를 순서대로 사용하고 공격 주기 동안 멈춰 기다리는지 확인한다.
2. 체력 열세, 적의 준비된 콤보 피해 증가, 적 포탑 위험 진입, E 만료 임박 각각에서 E2 복귀 타이밍을 확인한다.
3. E 표식이 적 머리 위에 한 번 나타나고 복귀·사망·취소 때 사라지는지 확인한다.
4. 다른 16개 챔피언은 Q/W/E/R 도달성, 막타, 카이팅, 고유 2단계 콤보를 교차 확인한다.
