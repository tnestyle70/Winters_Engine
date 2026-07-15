# S009 결과 - 150 챔피언 데이터 방향과 서버 권위 연습 도구

## 결론

연습 모드는 별도 Scene으로 분리하지 않았다. 정상 `Scene_InGame`의 F10 ImGui 위에서 명령만 만들고, Debug Server의 room host가 연습 세션을 명시적으로 켠 뒤 GameRoom tick에서 gameplay truth를 바꾸도록 구현했다.

```text
F10 Practice Tool / Balance Lab
-> PracticeControl FlatBuffer
-> CommandIngress / ordered room queue
-> Debug + host + enabled policy
-> Server GameRoom mutation
-> Shared/GameSim definition overlay
-> existing Snapshot/Event
-> Client observed state
```

이 구조는 별도 연습 Scene이 실제 경기의 네트워크, AI, navmesh, snapshot, UI 경로와 달라지는 문제를 피한다. Release Server는 `PracticeControl`을 소비하고 아무 gameplay state도 변경하지 않는다.

## 실제 반영

- `CommandKind::PracticeControl=12`와 typed operation/value/flags를 append-only schema로 추가했다.
- `BuildServerCommand`가 source session을 보존해 GameRoom이 host 권한을 검증한다.
- 서버 연습 세션은 `_DEBUG + lobby host + SetEnabled` 세 조건으로 열린다.
- 서버 권위 기능:
  - 자동 체력 회복, 마나 무한, 스킬/소환사 주문 쿨다운 제거, 골드 무한
  - 즉시 HP/마나 회복, 쿨다운 초기화, 골드 추가, 정확한 1~18 레벨 설정
  - navmesh resolve 후 순간이동 및 move/chase/combat/recall/CC 정리
  - Blue/Red, Melee/Ranged/Siege/Super, Top/Mid/Bot 미니언 생성
  - room당 연습 spawn 100개 상한 및 net-id unbind 후 일괄 정리
- `PracticeSkillEffectOverrideComponent`는 현재 플레이어의 slot+param scalar를 32개까지 임시 저장한다.
- `GameplayDefinitionQuery::ResolveSkillEffectParam`이 임시 overlay를 먼저 읽고 Clear 후 immutable pack 값으로 돌아간다.
- F10 `ChampionTuner` placeholder를 `Practice Tool / Balance Lab`으로 교체했다.
- 런타임 Resource JSON의 Load/Edit/Save/Apply/Clear를 구현했다.
- JSON Apply는 이전 서버 override를 먼저 지워 삭제된 행이 남지 않게 했다.
- 일반 챔피언 snapshot이 current mana 대신 max mana를 보내던 문제를 수정했다.
- generator는 bool/NaN/Infinity와 skill-effect 의미 범위 오류를 거절한다.
- architecture compass에 normal-match practice path와 temporary overlay 원칙을 추가했다.

## JSON 범위

현재 JSON은 전체 gameplay pack hot reload가 아니다. 다음 scalar effect만 안전한 세션 overlay로 적용한다.

```text
BaseDamage, DamagePerRank, Range, Speed, MoveSpeedMul,
StunDurationSec, SlowDurationSec, AirborneDurationSec,
DashDistance, DashDurationSec, Radius, EffectDurationSec,
BonusAd, BonusAttackSpeed
```

적용 파일:

`C:/Users/user/Desktop/Winters/Client/Bin/Resource/Config/Practice/practice_balance_overrides.json`

승인된 값은 이 preset에만 남겨 출시하면 안 된다. canonical authoring JSON/sheet에 옮긴 뒤 validator, cook, SimLab, build를 다시 통과해야 한다.

## 자동 검증 결과

| 검증 | 결과 |
|---|---|
| FlatBuffers C++/Go codegen | PASS |
| LoL definition pack freshness | PASS, `0x0566A4D2` |
| 현재 pack 수 | 17 champions / 85 skills / 1 summoner spell |
| Shared dependency boundary | PASS |
| GameSim Debug x64 | PASS |
| SimLab Debug x64 build | PASS |
| SimLab full deterministic run | PASS |
| 신규 Practice command/overlay/clear probe | PASS |
| 기존 StatusCC/SylasR/YoneE/Viego/ActionLock/NavGrid probes | PASS |
| Server Debug x64 | PASS |
| Client Debug x64 | PASS |
| Server Release x64 | PASS |
| Practice JSON parse | PASS |
| `git diff --check` | PASS; line-ending warning only |

빌드에는 기존 DLL interface 경고(C4251/C4275)와 `ChampionSpawnService.cpp`의 기존 `sprintf_s` format 경고가 남아 있으나 신규 연습 도구의 compile/link 오류는 없다.

## 사용자가 확인할 순서

1. `Server/Bin/Debug/WintersServer.exe`를 실행하고 host Client로 정상 경기에 입장한다.
2. F10을 눌러 `Practice Tool / Balance Lab`을 연다.
3. `Enable Practice Session`을 먼저 누른다.
4. Player Options의 네 옵션을 선택하고 `Apply Options`를 누른다.
5. 피격, 연속 스킬, 구매, Level Up/18, Teleport를 확인한다.
6. Minion Spawn에서 team/role/lane/position을 바꾸어 생성하고 Clear를 확인한다.
7. JSON `Load -> 값 편집 -> Save -> Apply` 후 동일 target에 같은 스킬을 반복 적중한다.
8. `Clear Server Overrides` 후 원래 피해량으로 복구되는지 확인한다.
9. 동일 Wi-Fi의 두 번째 Client가 같은 snapshot 결과를 보는지, 비-host의 명령이 Server Debug output에서 `host-required`로 거절되는지 확인한다.
10. Disable 또는 Server 재시작 뒤 option/override/spawn이 남지 않는지 확인한다.

## 남은 구조적 작업

- 현재 `practice_tool` lobby metadata는 Server ruleset으로 전달되지 않는다. 정식 내부 QA 빌드에는 Lobby/Hello의 server-owned mode/capability가 필요하다.
- command별 accepted/reject reason은 아직 전용 network result event가 아니라 bounded Server Debug trace와 다음 snapshot으로 확인한다.
- 현재 JSON overlay는 effect scalar만 지원한다. cooldown, mana cost, target shape, cast/impact/recovery, movement policy는 다음 versioned definition patch 계약으로 확장해야 한다.
- `ResolveSkillEffectParam`을 사용하지 않는 legacy hardcode는 overlay가 바꾸지 못한다. reader parity를 0까지 줄여야 한다.
- 150 챔피언 완료의 본체는 common skill atom executor와 generated per-champion deterministic test matrix다. 이번 S009는 이를 검증할 권위 경로와 디자이너 수직 슬라이스를 만든 단계다.
