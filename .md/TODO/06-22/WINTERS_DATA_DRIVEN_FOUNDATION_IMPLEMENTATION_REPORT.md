# Winters DataDriven Foundation 구현 결과

작성일: 2026-06-22

## 1. 결론

이번 반영은 **DataDriven 전체 리라이트의 첫 수직 슬라이스**를 완료했다.

- PASS: 소유권별 JSON 분리
- PASS: stable `DefinitionKey`와 pack-local `DefId` 분리
- PASS: 검증된 immutable C++ pack 생성
- PASS: ServerPrivate gameplay 값이 Client 생성물에 포함되지 않는 빌드 경계
- PASS: 서버 champion spawn과 dirty stat recompute의 Def pack 전환
- PASS: Client yaw reader의 ClientPublic visual pack 전환
- PASS: Debug x64 전체 빌드와 SimLab 결정론 회귀
- NOT COMPLETE: CommandExecutor, 네트워크 schema, legacy table 전체 삭제

따라서 현재 상태를 "DataDriven 완료"라고 부르지 않는다. 정확한 상태는 **DataDriven Foundation PASS, legacy cutover 진행 중**이다.

## 2. 원자 식별자

### `DefinitionKey`

- JSON, manifest, network/save에서 사용할 영속 식별자다.
- canonical string을 FNV-1a 32-bit로 cook한다.
- 이번 pack의 key 103개에서 collision은 0개다.

### `ChampionDefId`, `SkillDefId`, `SummonerSpellDefId`

- 한 번 cook된 pack 내부의 1-based dense index다.
- 네트워크, 저장 파일, JSON에는 영속 식별자로 쓰지 않는다.
- runtime hot path는 문자열 대신 이 ID로 배열을 직접 읽는다.

### `EntityHandle`

- 현재 프로세스 안에서 entity index와 generation을 묶는 생명주기 식별자다.
- JSON, network, save로 보내지 않는다.
- 서버 champion spawn은 `CreateEntityHandle()`로 생성하고 기존 시스템 호환을 위해 index를 사용한다.

세 식별자는 서로 대체 관계가 아니다.

## 3. 데이터 소유권

### ServerPrivate

`Data/LoL/ServerPrivate/Gameplay`에 다음을 생성한다.

- `ChampionGameplayDefs.json`
- `SkillGameplayDefs.json`
- `SummonerSpellGameplayDefs.json`

실제 C++ 값은 `Server/Private/Data/Generated`에 생성되고 Server 프로젝트에서만 컴파일된다.

### ClientPublic

`Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json`에는 다음만 둔다.

- model yaw offset
- animation playback speed
- cast/recovery frame
- replicated cue ID

동일 값의 C++ pack은 `Client/Private/Data/Generated`에 생성된다.

### SharedContract

`Data/LoL/SharedContract/DefinitionManifest.json`에는 type, canonical key, `DefinitionKey`만 둔다.

## 4. 런타임 반영

서버 champion spawn 흐름:

```text
Lobby eChampion compatibility value
-> ServerPrivate immutable pack lookup
-> ChampionDefId
-> EntityHandle 생성
-> ChampionDefinitionComponent
-> SkillLoadoutComponent[5]
-> StatComponent / Health / Spatial / Vision 호환 출력
```

dirty stat recompute 흐름:

```text
EntityID
-> ChampionDefinitionComponent
-> ChampionDefId
-> immutable pack dense lookup
-> base stat 재구성
-> item/buff/rune modifier 적용
```

JSON parsing과 string lookup은 프레임 안에서 일어나지 않는다. JSON은 authoring/cook 입력이고 런타임은 정적 배열만 읽는다.

Client의 yaw 보정은 더 이상 혼합 `ChampionGameDataDB`를 읽지 않고 Client 전용 visual pack을 읽는다.

## 5. 생성 검증

`Build-LoLDefinitionPack.py --check` 결과:

| 항목 | 결과 |
|---|---:|
| build hash | `0x58678ADB` |
| champion | 17 |
| skill | 85 |
| skill stage | 94 |
| summoner spell | 1 |
| stable definition key | 103 |
| key collision | 0 |
| ServerPrivate animation/yaw 위반 | 0 |
| visual timing mismatch | 0 |

## 6. 빌드 및 회귀

`Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1` 최종 결과: PASS

| 검증 | 결과 |
|---|---|
| generated pack freshness | PASS |
| legacy ownership audit | PASS |
| Client visual timing parity | PASS |
| GameSim Debug x64 | PASS |
| Server Debug x64 | PASS |
| Client Debug x64 | PASS |
| SimLab Debug x64 | PASS |
| `git diff --check` | PASS |

SimLab:

- seed 42 same-seed hash: `67F2A97563B8DB04`
- seed 43 hash: `5DA19645E291A29B`
- same-seed replay: PASS
- seed sensitivity: PASS

기존 C4828/C4251/C4275 경고는 남아 있으나 이번 반영으로 추가된 빌드 오류는 없다.

## 7. 아직 남은 본질적 부채

legacy audit 결과:

- `SkillDef` 관련 238줄
- `ChampionDef` 관련 188줄
- skill registration 파일 17개
- champion registration 파일 12개
- 의심스러운 Shared/legacy visual-authoritative 지점 905개
- server object hardcode 43개

특히 `CommandExecutor.cpp`는 아직 `ChampionGameDataDB`에서 cooldown, range, stage, Kalista dash, summoner spell과 visual yaw를 읽는다. Client `Scene_InGame.cpp`도 network action duration과 약한 예측 값 일부를 혼합 DB에서 읽는다.

이 값들을 이번에 강제 삭제하지 않은 이유:

1. CommandExecutor 파일에 기존 비 UTF-8 바이트가 있어 안전한 `apply_patch`가 불가능했다.
2. server gameplay가 visual yaw/animation timing에 기대는 좌표 계약이 남아 있다.
3. Client가 필요한 prediction 공개 계약을 network snapshot/event가 아직 전달하지 않는다.
4. old reader가 남은 상태에서 table부터 삭제하면 즉시 기능 회귀가 발생한다.

## 8. 다음 삭제 순서

1. `CommandExecutor.cpp`를 동작 변경 없이 UTF-8로 정규화하고 byte/compile diff를 검증한다.
2. immutable pack을 `TickContext` 또는 executor constructor로 주입한다.
3. cooldown/range/stage/passive dash/summoner spell reader를 `SkillDefId` 기반으로 전환한다.
4. gameplay yaw를 model yaw와 분리하고 snapshot pose 계약을 gameplay yaw 기준으로 고정한다.
5. Client prediction에 필요한 공개 값만 SharedContract/network event로 전달한다.
6. network의 `ubyte championId`를 stable `DefinitionKey:uint`로 교체한다.
7. legacy reader count가 0이 된 뒤 `SkillTable`, `ChampionTable`, `ChampionGameDataDB`, old generated table을 삭제한다.
8. 그 다음 Hot Reload와 Perforce workflow를 시작한다.

삭제 조건은 "새 구조가 존재한다"가 아니라 **old reader 0 + build PASS + SimLab hash PASS + Client/Server smoke PASS**다.
