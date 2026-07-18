Session - S040 후속, LoL Data Driven 65%→100% 컷오버 완료 결과

# 1. 결론

계획서의 P3~P9 완료 게이트 12개를 모두 통과했다. 현재 definition pack build hash는 `0xFC99B7EE`이며, gameplay tuning/fallback/client gameplay truth/AI hardcode/object-wave hardcode/구형 wire reader/legacy value-owner reader는 모두 0이다.

이번 “100%”는 측정 가능한 완료 정의를 뜻한다. 17개 구현 챔피언의 model/UI/AI profile coverage, canonical source 11개의 schema와 Debug reload 경로, hash-guarded draft round-trip을 모두 갖추고 `Verify-LoLDataDrivenPipeline.ps1 -RequireComplete`가 Debug와 Release에서 통과한다.

# 2. 완료 게이트

| Gate | 최종값 | 판정 |
|---|---:|---|
| P3 gameplay tuning literal | 0 | PASS |
| P3 pack-miss legacy fallback | 0 | PASS |
| P4 Client gameplay/duplicate visual scalar | 0 | PASS |
| P4 champion model/UI coverage | 17/17 | PASS |
| P5 AI policy/combo hardcode | 0 | PASS |
| P5 AI profile coverage | 17/17 | PASS |
| P6 object/minion/wave hardcode | 0 | PASS |
| P7 old network identity reader | 0 | PASS |
| P8 legacy value-owner reader | 0 | PASS |
| P9 JSON Schema coverage | 11/11 | PASS |
| P9 validated runtime reload coverage | 11/11 | PASS |
| P9 draft round-trip failure | 0 | PASS |

# 3. 적용 결과

## 3-1. 단일 데이터 소유권

- Champion/skill/effect/economy/item/rune/spawn-object/AI 정의는 canonical JSON → 검증/codegen → immutable pack 경로로 통일했다.
- pack miss는 예전 DB/registry/constexpr 값으로 돌아가지 않고 명시적으로 실패한다.
- `ChampionGameDataDB`, `ChampionStatsRegistry`, `ServerMinionTuning`과 해당 project 항목을 제거했다.
- Client champion registration은 hook/callback만 소유하고 gameplay truth는 generated ClientPublic/ServerPrivate 정의가 소유한다.
- AI 17개 profile과 combo plan은 `ChampionAIGameplayDefs.json`에서 생성되고 Debug reload 시 검증 후 한 번에 publish된다.
- minion behavior/wave formation/structure/jungle tuning은 `SpawnObjectGameplayDefs.json`에서 공급된다.

## 3-2. 안정된 네트워크 identity

- Hello/LobbyCommand/LobbySlot/EntitySnapshot의 런타임 identity를 `DefinitionKey`로 전환했다.
- Client와 Server의 런타임 reader는 구형 champion byte/id를 gameplay identity로 읽지 않는다.
- server-authoritative 흐름 `Client Input → GameCommand → Server GameSim → Snapshot/Event → Client Visual`은 유지했다.

## 3-3. 기획자 workflow

- canonical source 11개에 draft-2020-12 JSON Schema를 연결하고 generator의 domain validation과 함께 실행한다.
- 일반 codegen과 `--check`는 canonical source를 쓰지 않는다. source write는 `--apply-draft`에만 허용한다.
- draft는 `baseBuildHash` 일치, canonical source allow-list, 기존 JSON pointer/definition row/visual timing override, domain validation을 통과해야 한다.
- apply는 canonical source를 atomic replace한 뒤 derived pack을 재생성하고 새 build hash를 동기화한다.
- round-trip 테스트는 임시 workspace에서 실제 valid apply → 새 hash 생성 → stale draft 거부 → workspace 원본 불변까지 확인한다.
- Server gameplay/AI/rune과 Client champion timing/model/UI/object visual은 Debug reload에서 전체 parse 성공 후에만 active generation을 바꾼다.

# 4. 검증 결과

다음 명령이 모두 PASS했다.

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --check
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Test-LoLDataDrivenDraftRoundTrip.ps1 -Root . -NoWrite
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1 -Root . -NoWrite -FailWhenIncomplete
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug -RequireComplete
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Release -RequireComplete
git diff --check
```

- Debug/Release: GameSim, Server, Client, SimLab 빌드 PASS.
- Debug/Release SimLab 전체 PASS.
- same-seed deterministic hash: `E16C6B35D8D99A1A`.
- seed+1 sensitivity hash: `ABB16BD05761A991`.
- FormulaData: 17 champions, 85 skills PASS.
- ZedPassiveR RED는 passive request가 새 critical indicator flag를 갖는 현재 계약에 맞게 회귀식을 갱신해 PASS로 닫았다.
- Keyframe의 truncated/load-error 출력은 failure-atomicity 주입 케이스이며 최종 `KeyframeAtomic`과 replay 검증은 PASS다.

# 5. Debug와 Release 사용 원칙

- 기능 개발과 fail-closed/reload/validation 확인은 Debug에서 한다.
- 회귀는 Debug와 Release 모두 실행한다. 최적화 때문에 사라지는 assert, 초기화 차이, UB를 양쪽에서 잡기 위해서다.
- 성능 수치와 이력서 수치는 반드시 Release에서, 동일 장면·동일 seed·동일 해상도·동일 profiler 설정으로 수집한다.
- 즉, “Release만 검증”이 아니라 “Debug로 원인을 잡고 Release로 실제 비용과 최종 결과를 증명”하는 파이프라인이다.

# 6. 인수인계와 롤백

- Data Driven 65%→100% 과제는 완료 처리한다. 후속 튜닝은 새 canonical JSON draft와 독립 성능 세션으로 연다.
- 수동 F5 육안 검증은 데이터 소유권 완료 게이트와 분리한다. 신규 Fiora WFX를 포함한 visual capture 과제는 기존 S040 인수인계 순서를 따른다.
- 기존 worktree에는 다른 Claude/Codex/사용자 WIP가 함께 있으므로 이번 결과를 자동 커밋하지 않았다.
- 롤백은 `Data/LoL` canonical/schema/generated, `Tools/LoLData`, Shared GameSim definition/query/AI, Server runtime overlay/wire writer, Client generated/runtime visual reader, schema generated 파일 단위로 분리한다. 다른 Engine/FX/문서 WIP를 함께 되돌리지 않는다.
