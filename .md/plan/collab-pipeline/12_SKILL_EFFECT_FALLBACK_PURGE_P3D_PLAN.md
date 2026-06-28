Session - audit 카운트를 "진짜 gameplay 원자"만 세도록 정리하고, 잔여 gameplay 상수를 원자화한 뒤, 모든 실행 경로의 pDefinitions 보장을 재확인하고 constexpr fallback을 삭제해 skillEffectHardcodeCandidates를 0으로 만든다(P3 종료).

검토 반영(2026-06-28): 초판의 전제가 틀려 전면 교정함.
- (정정) SimLab은 `tc.pDefinitions`를 이미 설정한다(`Tools/SimLab/main.cpp:311, 462` = `&ServerData::GetLoLGameplayDefinitionPack()`). 따라서 "SimLab pDefinitions가 nullptr"는 거짓이고, 별도 주입 작업은 필요 없다. 남은 일은 Client local 경로의 pDefinitions 보장 재확인뿐이다.
- (정정) 09의 summon 정책 원자 분리는 이미 완료다: `SkillAtomData.h`에 `eSummonPolicyParamId`/`SummonPolicySpec`/`ResolveSummonPolicyParam`, `SkillGameplayDef.h:20`에 `summonPolicy{}`, `GameplayDefinitionQuery.h:57-64`에 `ResolveSummonPolicyParam`, Annie R 전환 + JSON `summonPolicy` 존재. 이 계획서는 그것을 다시 만들지 않는다.
- (정정) 대부분 챔피언은 hook로 이미 전환됨. "8 미전환"은 stale. 실제 잔여는 소수 상수 + audit 분류 + Ezreal/Garen 확인이다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Tools/LoLData/Collect-LoLLegacyDataAudit.ps1

audit 정규식(177-179)이 `constexpr f32_t k*(Damage|Range|Radius|Speed|DurationSec|...)` 전부를 candidate로 세어, gameplay 수치가 아닌 구조/연출 상수까지 156에 포함한다. "진짜 gameplay 원자"만 세도록 분류한다.

```text
- gameplay candidate로 인정: 09의 eSkillEffectParamId / eSummonPolicyParamId 원자 목록에 대응되는 값(damage/range/radius/cc duration/dash/shield/summon 등).
- 제외(코드에 남김, 데이터화 강제 안 함): stage tag(u8_t, 예 kZedRSourceShadowStage), local 연출 타이밍(예 kYasuoQHitDelaySec), passive 메커니즘 구조값(예 kSylasPassiveMaxStacks).
- 제외는 임의가 아니라 generated 제외(07 I6)처럼 명시 목록 + 사유를 audit 출력에 남긴다.
```

확인 필요: 분류 후 "진짜 gameplay" candidate의 정확한 수를 audit로 재집계한다(156은 비-gameplay 포함값이므로 의미 없음).

1-2. 잔여 gameplay 상수 원자화 (있는 것만, 챔피언별 slice)

검토로 확인된 실제 잔여 gameplay 후보(파일:라인 작업 직전 rg 재확인). 대부분은 이미 hook 전환됨; 아래만 남음.

```text
- Viego  ViegoGameSim.cpp:82  possession duration fallback 5.f  -> SummonPolicy/PassivePolicy 원자 후보
- Yasuo  YasuoGameSim.cpp:27  kYasuoAirborneLift 2.1f           -> 신규 eSkillEffectParamId(AirborneLift) 후보
- Yasuo  YasuoGameSim.cpp:533 airborne 탐색 radius 14.0f        -> Radius 원자
- Sylas  SylasGameSim.cpp:225 E range fallback 6.0f             -> 기존 Range 원자 사용
```

각 값은 byte-identical로 `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`에 기입 -> generator emit -> `Resolve*Param` reader 전환 -> SimLab 해시 불변 확인.

확인 필요:
- `kYasuoQHitDelaySec`(0.25), `kZedRSourceShadowStage`(3), `kSylasPassiveMaxStacks`(3), `kSylasPassiveWindowSec`(4.0)은 gameplay balance가 아니라 메커니즘/연출 구조값이다. 1-1 분류로 candidate에서 빼고 코드에 둔다(데이터화 강제 안 함). 단 PassiveWindowSec를 balance로 본다면 별도 PassivePolicy 원자로(09 P3a 후속).

1-3. Ezreal / Garen 실행 경로 확정

`Shared/GameSim/Champions/{Ezreal,Garen}` 디렉터리는 없지만, **generated pack(`Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`)에는 두 챔피언 정의가 풀 스킬 로드아웃으로 존재한다**(검토 확인). 즉 데이터는 있고 server-side sim hook 실행 코드가 없다.

```text
- 두 챔피언의 스킬 실행이 어디서 일어나는지 확정: 클라 로컬 hook 전용인가, 공용 generic 처리인가, 미구현인가.
- gameplay 하드코딩이 클라 _Registration/_Skills에 있다면 그것이 컷오버 대상(P3 B 도메인). 없으면 P3 대상 아님으로 표시.
```

확인 필요: rg "Ezreal|Garen" Shared Server Client 로 실행 경로와 잔여 하드코딩을 먼저 확정한 뒤 slice 여부 결정.

1-4. fallback 삭제 서브페이즈 (모든 atomize/분류 완료 후 마지막)

`Resolve*SkillEffectParam(..., fallbackValue)`의 constexpr fallback 인자를 제거하고 선언을 삭제한다. pDefinitions는 Server(`GameRoomTick.cpp:92-101`)와 SimLab(`main.cpp:311,462`)에서 이미 보장됨 -> Client local 경로만 재확인 후 삭제.

확인 필요: Client local smoke 경로(예 `Client/Private/Scene/Scene_InGameLocalSkills.cpp`)가 pDefinitions를 설정하는지 확인. 설정 안 하면 그 경로만 먼저 주입한 뒤 삭제(아니면 클라 로컬에서 0값으로 떨어져 동작 변경).

2. 검증 (07 §6)

미검증:
- audit 재분류 미반영, 잔여 원자화/ Ezreal·Garen 확정/ fallback 삭제 미반영

검증 명령:
- python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1

통과 기준:
- G4 SimLab same-seed 해시: 잔여 원자화/fallback 삭제 후에도 불변(현재 baseline 해시 기준; pDefinitions가 이미 살아 있으므로 fallback이 load-bearing이 아니어야 정상).
- G3 audit: 재분류된 "진짜 gameplay" candidate가 원자화+fallback 삭제 후 0.
- G1 Build PASS, G5 git diff --check PASS.

확인 필요:
- 1-1(분류)을 먼저 해야 "무엇이 진짜 남았는지"가 정해진다. 그다음 1-2/1-3/1-4 순서.
- Client local 경로 pDefinitions 보장이 fallback 삭제의 유일한 선행 조건(Server/SimLab은 이미 충족).
