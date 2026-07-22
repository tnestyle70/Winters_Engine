# 1. 예측 vs 실측

- **적중 — F4가 실제 통합 튜너가 된다.** F4는 더 이상 Structure 전용 창을 열지 않고 `Skills`를 기본 카테고리로 하는 Balance Lab을 한 번만 렌더한다. Skills / Champion / Items / Units & Structures / Respawn / Runtime / All 분류와 서버 practice operation 배선은 `Test-F4BalanceContracts.py`에서 통과했다. F4 직접 실행 시의 창 조작성은 이번 세션에서 시각 검증하지 못했다.
- **적중 — 이렐리아·비에고의 약한 피해 원인은 착각이 아니었다.** 랭크별 flat/AD/AP/missing-HP 수치를 데이터에 반영했고, 충전 배율이 local flat에만 선적용된 뒤 definition resolve에서 사라지던 경로를 `DamageRequestComponent::skillDamageScale`로 보존했다. 실제 C++ `CDamageQueueSystem` probe에서 이렐리아 W(랭크 1, 배율 3, AD/AP 각 100)가 방어력 0 대상에게 300, 비에고 R(랭크 1, 배율 0.5, AD 100, 최대/현재 체력 2000/1000)이 120 피해를 주었다.
- **적중 — Classic Summoner's Rift 부활 규칙과 F4 override가 서버 권위로 동작한다.** 레벨 1~18 기본값 `10, 10, 12, 12, 14, 16, 20, 25, 28, 32.5, 35, 37.5, 40, 42.5, 45, 47.5, 50, 52.5`와 15/30/45분 이후 30초 단위 증가·50% cap을 공용 resolver로 만들었다. SimLab이 레벨 clamp, 900/930/1800/1830/2700/2730/3300초 경계, practice on/off, 레벨 18 최대 78.75초를 실제 함수 호출로 통과했다.
- **초안 빗나감 → 수정.** Bernoulli의 구현 후 재비평에서 문자열 중심 계약 검사와 Chrono Break keyframe의 부활 override 누락이 P1로 발견됐다. DamageQueue/FlatBuffers/respawn resolver 실행 assertion을 추가하고 `practiceRespawnSecondsByLevel`을 keyframe capture/restore에 포함한 뒤 재검증했다.
- **적중 — 사망 카운트다운과 골드 위치가 authored HUD에 연결된다.** Snapshot의 remaining/duration이 Client `RespawnComponent`와 HUD state를 거쳐 원형 초상화 중앙 `respawn.text`에 초 단위로 표시된다. 골드는 `[695, 137]`에서 `[707, 142]`로 오른쪽 12·아래 5 이동했고 built-in fallback도 같은 좌표다. 단, 실제 authored 파일 `Client/Bin/Resource/UI/hud_irelia_layout.json`은 기존 `.gitignore` 규칙에 걸려 이 작업트리에서는 동작하지만 Git 산출물에는 자동 포함되지 않는다.
- **검증 실측.** Champion/LoL pack 생성기 `--check`, Python compile, F4 계약 검사, `git diff --check`, `SimLab --f4-balance-only`, 전체 `SimLab 1 42`가 모두 통과했다. GameSim Debug, SimLab Debug, Server Debug/Release, Client Debug/Release, Engine Release 빌드는 오류 0으로 통과했다. 기존 동시 작업의 dirty 파일과 생성 산출물은 되돌리거나 정리하지 않았다.

# 2. 판결

**수정 반영** — 초안의 F4·피해·부활·HUD 방향은 유지하되, 서브 에이전트 P1 두 건을 실제 실행 probe와 keyframe 보존으로 교정했다. 자동 계약·전체 회귀·Debug/Release 빌드가 모두 통과했으므로 반영 상태로 닫는다.

# 3. ⑤ 갱신

- 카테고리 하나에서 많은 수치를 조절할 수 있게 된 대신 새 항목은 `UI → Command schema → Server operation → authoritative data/overlay → Snapshot` 전 구간을 함께 확장해야 한다. 이 중 하나라도 생략되면 다시 “창에는 보이지만 게임에는 적용되지 않는” 튜너가 된다.
- 부활 override는 room state이므로 reset뿐 아니라 Chrono Break capture/restore에도 포함해야 한다. 이후 room-scoped practice state가 추가될 때 keyframe 계약을 빠뜨리면 이 선택이 틀린다.
- 현재 표는 일반 Classic Summoner's Rift 기준이다. Swiftplay나 별도 모드를 추가하면 같은 표를 암묵적으로 공유하지 말고 mode별 정책으로 분리해야 한다.
- authored HUD JSON을 장기 배포 대상으로 삼으려면 `Client/Bin/Resource` ignore 정책을 바꾸거나 추적 가능한 원본에서 런타임 Resource로 생성하는 경로가 필요하다. 그렇지 않으면 fallback은 남아도 JSON 좌표·bind는 다른 작업트리에서 소실될 수 있다.
- 자동 검증은 계산·직렬화·빌드 회귀를 닫았지만 실제 F5 화면에서 F4 카테고리 조작성, 초상화 카운트다운의 대비, 골드 위치의 시각 균형은 수동 QA가 필요하다.

## 후속 세션 — 2026-07-19 본질형 F4 / AI Debug / UI Manager

# 1. 입력 vs 출력

- **입력 — 챔피언 피해와 성장치, 미니언 피해를 가장 단순한 화면에서 조절한다.** 출력은 F4 `Balance` 창의 `Champion Damage / Growth / Minions` 세 탭으로 축소했다. 챔피언 피해 탭은 `Base AD`와 Q/W/E/R `Flat Damage`, 성장 탭은 HP/Mana/AD/AP/Armor/MR/AS의 Base/Per Level, 미니언 탭은 역할별 Attack Damage만 노출한다.
- **입력 — 잡다한 버튼과 raw override 표를 제거한다.** 출력은 `Save & Apply`와 `Restore This Category` 두 동작만 남겼다. 저장·적용 시 화면에 허용된 Q/W/E/R flat damage, 챔피언 기본/성장치, 활성화한 미니언 역할 피해만 서버 명령으로 보낸다. 기존 raw JSON/고급 override 표는 런타임 표면에서 컴파일 제외했다.
- **입력 — 미니언 수치도 실제 게임에 반영한다.** 출력은 `UI -> PracticeControl Command -> Server GameRoom -> PracticeMinionAttackDamagePolicy -> 현재/신규 MinionStateComponent`의 서버 권위 경로를 추가했다. practice 해제·category restore 시 현재 미니언까지 definition pack의 시간 성장값으로 복구하며, Chrono Break keyframe에도 override를 보존한다.
- **입력 — AI Debug와 UI Manager도 본질만 남긴다.** 출력은 F9 AI Debug를 대상/현재 판단/강제 행동/Core Tuning/최근 결정으로, F8 UI Manager를 HUD/Health Bars/Cursor로 축소했다. HUD 편집기는 Image/Text 선택, 항목 선택, Position/Size 또는 Font Scale, `Save Layout`만 노출한다. F7 WFX는 수정하지 않았다.
- **계획 비평 반영.** Bernoulli의 사전 비평에 따라 기본 탭 enum을 `ChampionDamage`로 통일하고, 숨은 legacy JSON 값 전송을 allowlist로 차단했으며, practice 해제 시 이미 생성된 미니언 복구와 minion policy 실행 probe를 추가했다. `Base AD`와 QWER `Flat Damage`가 서로 다른 의미라는 라벨도 고정했다.
- **검증.** `Test-F4BalanceContracts.py`, `SimLab --f4-balance-only`, GameSim/SimLab/Engine/Server/Client Debug, Engine/GameSim/Server/Client Release, `Services/go test ./...`, `git diff --check`가 모두 통과했다. Release 최초 시도는 다른 Visual Studio 빌드와 PDB가 겹쳐 `LNK1201`이 났으나, 컴파일러·링커 종료 후 `/m:1 /nr:false` 단독 재시도에서 전 대상이 통과했다. 기존 C4251/C4275 DLL-interface 경고와 작업트리의 타 작업 변경은 건드리지 않았다.

# 2. 판결

**본질형 표면으로 수정 반영 완료.** F4는 단순 밸런스 패치 도구로 닫았고, F8/F9도 서로 한 창만 토글하도록 분리했다. 자동 계약·정책 probe·Debug/Release 빌드는 통과했다. 다만 practice control은 서버 코드상 Debug 방 호스트 전용이며, 실제 F5 화면에서 탭 배치와 클릭 흐름을 확인하는 수동 시각 QA는 이번 세션에서 수행하지 않았다.

# 3. 핵심 통찰

- Q/W/E/R의 `Flat Damage`는 방어력·계수·아이템 효과까지 반영된 최종 피해가 아니다. 기본 피해 항목만 빠르게 패치하고, AD/AP 계수나 특수 공식은 definition/전용 튜닝 경로에서 별도로 다뤄야 의미가 섞이지 않는다.
- F4에서 수치를 보이게 만드는 것만으로는 부족하다. 앞으로 항목을 추가할 때도 UI allowlist, command schema, 서버 적용/복구, 신규 spawn, keyframe, 실행 probe를 한 계약으로 묶어야 한다.
- 현재 사용자가 원한 일상 흐름은 `F4 -> 탭 선택 -> Custom 활성화/값 입력 -> Save & Apply`면 끝난다. 원복은 해당 탭에서 `Restore This Category`를 누른다.
- 골드 위치는 `F8 -> HUD -> Edit: Text -> gold.text -> Position -> Save Layout`에서 옮긴다. 오른쪽은 X 증가, 아래는 Y 증가다.

## 교정 세션 — 2026-07-19 전 챔피언 JSON 밸런스 편집 / 즉시 Hot Load

# 1. 실패 인정과 교정 결과

- 직전 “본질형 표면 완료” 판정은 잘못됐다. `본질만`을 잡다한 UI 제거가 아니라 필드 자체 축소로 오해해 Current champion과 Base AD 중심으로 남겼고, 사용자의 실제 목적이던 전 챔피언 선택·성장 수치·QWER 계수/쿨타임·미니언/타워 HP/AD를 누락했다. 서브 에이전트 비평이 축소를 요구한 것은 아니며 범위 해석 책임은 구현 세션에 있다.
- F4는 이제 `Champions / Skills / Minions / Towers` 네 탭이다. Champions/Skills에서 `champions.json`의 17명 전체를 선택하며 Current champion에 종속되지 않는다. Minions/Towers 탭에는 불필요한 챔피언 선택기를 표시하지 않는다.
- Champions는 HP, Mana/Energy, AD, AP, Armor, MR, Attack Speed의 Base/Per Level과 Attack Speed Ratio, Resource Regen/Sec를 제공한다.
- Skills는 Q/W/E/R의 rank별 Cooldown, Flat Damage, Total AD, Bonus AD, AP, Target Max HP, Missing HP ratio를 제공한다. Q/W/E는 5칸, R은 3칸이며 `Ratio 1.0 = 100%`다.
- Minions는 Melee/Ranged/Siege/Super의 Max Health/Attack Damage, Towers는 Turret Max Health/Attack Damage와 Nexus Turret Attack Damage를 제공한다.

# 2. 저장과 실제 반영 경로

- 편집 대상은 임시 override JSON이 아니라 `Data/Gameplay/ChampionGameData/champions.json`, `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`, `Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json` 세 진실 파일이다.
- `Save & Hot Load`는 champion/slot/effect-key bijection, rank 길이, 수치 범위를 전체 검증한다. 다른 Codex/에디터가 로드 후 파일을 바꿨으면 stale draft로 저장을 중단한다. dirty 파일만 temp와 backup으로 기록하고 세 파일 중 하나라도 replace에 실패하면 원본으로 롤백한다.
- Debug 방 호스트에서는 저장 성공 뒤 `SetEnabled + ReloadGameplayDefinitions`를 보낸다. 서버는 reload 성공 뒤 같은 tick에 챔피언 stat 재계산, 살아 있는 lane minion HP/AD, 살아 있는 일반/넥서스 타워 HP/AD를 새 pack으로 갱신한다. 죽은 엔티티와 role 4 소환물은 되살리거나 바꾸지 않는다.
- Yasuo Q, Kalista E, Lee Sin Q, Ezreal R와 basic-hit empower처럼 custom flat을 쓰는 경로는 일반 Flat 행이 실제 소비자인 척하지 않도록 구분했다. custom flat은 runtime params에서 조절하고, canonical AD/AP/HP ratio는 DamageQueue에서 병합해 반영한다.
- Release에서는 runtime Hot Load 버튼을 비활성화한다. F4가 저장한 JSON의 Release 영속화는 definition pack cook와 Release 빌드를 거친다.

# 3. 검증 실측

- `python Tools/LoLData/Test-F4BalanceContracts.py --root .` — PASS.
- `python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check` — PASS, pack `0x8B32D869`, champions 17, skills 85.
- `Tools/Bin/Debug/SimLab.exe --f4-balance-only` — PASS.
- `Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug` — 전체 프로젝트 빌드, 전체 SimLab, draft round-trip, schema/freshness, `git diff --check` PASS.
- `Verify-LoLDataDrivenPipeline.ps1 -Configuration Release` — GameSim/Server/Client/SimLab Release 빌드와 전체 SimLab PASS. 기존 SimLab PDB 형식 경고 `LNK4020`은 있었지만 링크와 실행은 성공했다.
- 마지막 UI 정리 후 Client Debug/Release를 다시 빌드해 둘 다 통과했다. 기존 C4251/C4275 경고와 타 작업 dirty 변경은 수정하지 않았다.
- 자동 계산·데이터·빌드 검증은 닫혔다. 실제 F5에서 F4를 열어 창 배치, 스크롤, 버튼 클릭과 인게임 수치 변화를 눈으로 보는 수동 시각 QA는 수행하지 않았다.

# 4. 판결

**교정 반영 완료.** 이전 완료 판정을 철회하고 사용자가 요구한 전 챔피언 스탯/성장·QWER 계수/쿨타임·미니언/타워 HP/AD의 JSON 저장과 Debug 즉시 Hot Load 경로를 구현·검증했다.

## 후속 교정 결과 — 2026-07-19 Reload 의미 / Hot Load 진단 / 스킬 슬라이더

# 1. 예측 vs 실측

- **원인 확정 — Release gate는 정상 동작이었다.** 사용자가 Release Client에서 실행했다고 교정했으며, `Save & Hot Load` 비활성은 의도된 동작이다. UI는 이제 Release Client, non-authoritative scene, command sender/network/snapshot 부재와 disconnect를 Client가 아는 범위에서 구분한다. 방장 여부·Server Release·서버 JSON parse 실패는 server-side라 전송 뒤 공통 reject 안내로만 표시한다.
- **적중 — 스킬 수치가 slider가 됐다.** Q/W/E/R의 rank별 cooldown은 0..300초, flat damage는 0..2000, AD/AP/target-HP ratio는 0..5 범위의 `SliderFloat + AlwaysClamp`를 사용한다. custom-hit의 Runtime Damage Params도 같은 slider 경로이며, Ctrl+click 정확 입력 안내와 155px rank 열/1180px 초기 창 폭을 적용했다.
- **적중 — Reload 의미와 손실 경계를 닫았다.** `Reload Draft`를 `Reload JSON`으로 바꾸고, 세 JSON을 다시 읽으며 자체적으로 서버 값을 바꾸지 않는다는 tooltip을 추가했다. dirty draft는 확인 모달 없이 버리지 않으며, authoritative ack 대기 중에는 Save/Reload를 모두 잠가 pending sequence를 보존한다.
- **검증 실측.** `Test-F4BalanceContracts.py`, definition pack `--check`(pack `0x8B32D869`, 17 champions/85 skills), scoped `git diff --check`, Client Debug 및 Release 빌드가 통과했다. 첫 빌드 명령의 오래된 `Client/Client.vcxproj` 경로는 실제 `Client/Include/Client.vcxproj`로 교정했다. 기존 C4251/C4275 DLL-interface 경고는 유지됐다.
- **미검증.** 실제 F5 Debug 방장 세션에서 slider drag/Ctrl+click, `Save & Hot Load`, server revision 성공 문구를 눈으로 확인하지 않았다.

# 2. 판결

**수정 반영.** Release 제한은 유지하고, 사용자가 혼동한 이유를 화면에서 판별 가능하게 만들었다. 스킬 수치 조절은 slider로 교체했으며 Reload/ack 중 데이터 손실 경계도 함께 막았다.

# 3. ⑤ 갱신

- F4 slider는 일상 밸런싱 범위만 제공한다. cooldown 300초, flat 2000, ratio 5.0을 넘거나 음수 공식을 편집해야 할 때는 임의로 범위를 키우지 말고 canonical field metadata 기반 범위로 승격한다.
- 현재 ack는 global `toolRevision >= before + 2`를 사용하므로 동시 tool command가 섞이는 환경에서는 전용 command-result correlation이 필요하다. 이번 UI 교정에서는 wire/schema를 확장하지 않았다.

## 후속 결함 교정 결과 — 2026-07-19 WFX식 DragFloat / 진행 중 쿨다운 Hot Load

# 1. 이전 판정 철회와 원인

- 위 후속 결과의 `SliderFloat + AlwaysClamp` 판정은 사용자 요구를 다시 잘못 해석한 것이므로 철회한다. 요구한 조작은 파란 thumb를 옮기는 slider가 아니라 WFX Effect Tool과 같은 회색 숫자 필드 전체를 좌우로 드래그하고, 더블클릭하면 정확한 숫자를 직접 입력하는 ImGui `DragFloat`였다.
- `Save & Hot Load` 뒤 이렐리아 Q가 바뀌지 않은 것처럼 보인 현상은 파일 저장 실패가 아니었다. 현재 `Data/Gameplay/ChampionGameData/champions.json`에는 사용자가 저장한 이렐리아 Q 값 `[2.200000047683716, 2.5, 2.200000047683716, 2.700000047683716, 6.0]`이 실제로 남아 있다.
- 이 배열에서 Rank 5는 여전히 `6.0`이다. 테스트 중인 이렐리아 Q가 5랭크라면 Rank 1~4를 줄여도 인게임 쿨다운은 6초라서 변경되지 않은 것처럼 보이는 것이 정상이다. F4에서 현재 스킬 랭크와 같은 열을 수정해야 한다.
- 서버는 다음 스킬 사용부터 새 정의를 조회했지만 이미 돌고 있던 `SkillSlotRuntime.cooldownRemaining/cooldownDuration`을 리로드 시점에 새 정의로 환산하지 않았다. 따라서 쿨다운 도중 저장하면 기존 타이머가 끝날 때까지 변경이 안 된 것처럼 보일 수 있었다.

# 2. 반영 결과

- Skills의 rank별 Cooldown/Flat Damage/AD·AP·HP Ratio와 Runtime Damage Params를 `DragFloat`로 교체했다. cooldown drag speed는 `0.1`, flat damage는 `1.0`, ratio는 `0.01`이며 직접 입력도 기존 범위로 clamp한다. 별도 raw `InputFloat`나 파란 `SliderFloat` 경로는 남기지 않았다.
- Debug 서버는 리로드 전에 각 살아 있거나 죽어 있는 챔피언의 Q/W/E/R 이전 definition cooldown을, 아직 practice override가 존재하는 순서에서 보관한다. 성공 리로드와 stale override 제거 뒤 새 팩의 동일 랭크 cooldown을 다시 구하고 진행 중 타이머를 새 기준으로 환산한다.
- 환산은 `remaining / duration` 진행률과 기존 `duration / previousDefinition`의 쿨다운 감소 배율을 모두 보존한다. 예를 들어 이전 정의 10초, 실제 duration 8초, remaining 6초에서 새 정의 4초로 바뀌면 duration 3.2초, remaining 2.4초가 된다. ready/NaN/음수 경계는 0으로 정규화한다.
- 독립 서브 에이전트 비평에서 지적된 잘못된 death 필드 가정, override 제거 전 이전값 보관 순서, 단순 수학 테스트 부족을 모두 받아들여 계획과 구현을 교정했다.

# 3. 검증 실측

- `Shared/GameSim/Include/GameSim.vcxproj` Debug — PASS.
- `Server/Include/Server.vcxproj` Debug — PASS.
- `Client/Include/Client.vcxproj` Debug — PASS. `ChampionTuner.cpp`의 WFX식 `DragFloat` 경로가 실제로 컴파일됐다.
- `Tools/SimLab/SimLab.vcxproj` Debug — PASS.
- `Tools/Bin/Debug/SimLab.exe --f4-balance-only` — PASS. `CooldownReload`가 rank 3, practice override 제거 순서, 진행률/쿨다운 감소 보존을 실제 world/component 경로로 검증했다.
- 라이브 draft를 유지한 source contract 검사 — PASS. Skills에 `DragFloat`가 있고 `SliderFloat`가 없으며, 서버 cache → reload → override 제거 → remap 순서를 확인했다.
- scoped `git diff --check` — PASS.
- 기본 `Test-F4BalanceContracts.py`와 definition pack `--check`는 현재 의도적으로 FAIL이다. 전자는 원본 이렐리아 Q `[10, 9, 8, 7, 6]` 대신 사용자가 저장한 라이브 값부터 발견했고, 후자는 Release용 생성팩이 그 라이브 JSON보다 stale임을 보고했다. 사용자 JSON을 되돌리거나 Release 생성팩을 임의로 cook하지 않았다.
- 실제 F5 Debug 방장 세션에서 마우스 드래그·더블클릭과 인게임 HUD/피해를 눈으로 확인하는 수동 QA는 수행하지 않았다.

# 4. 판결

**코드·자동 재현 기준 교정 완료.** 저장은 실제 JSON에 이뤄지고 있었으며, 누락됐던 진행 중 쿨다운의 서버 Hot Load 환산을 추가했다. 수치 입력은 WFX와 같은 `DragFloat` 조작으로 바뀌었다. 최종 수동 확인은 새 Debug Client와 Debug Server를 모두 완전히 재시작한 뒤 수행해야 한다.

## 후속 차단 결함 교정 — 2026-07-19 Item 3153 `cppFlags`

# 1. 실제 원인

- 사용자 서버 로그로 `Save & Hot Load` 명령이 도착했지만 `TryReloadRuntimeGameplayDefinitions` transaction이 `ItemGameplayDefs.json: items[3153].onHitDamage unknown field: cppFlags`에서 매번 실패한 사실을 확인했다. 따라서 앞서 추가한 cooldown remap까지 실행된 적이 없었다.
- tracked item 3153 damage object에는 `cppType`, `cppFlags`, `rankCount`가 존재한다. 이는 codegen이 `type`, `flags`, rank array에서 다시 만드는 파생 metadata이며 실제 runtime damage truth가 아니다.
- cook 정규화는 이 metadata를 입력 truth로 쓰지 않지만 Debug runtime parser만 unknown field로 거부해 동일 canonical JSON의 cook/runtime 수용 규약이 갈라져 있었다.

# 2. 반영 결과

- `TryParseDamageFormula` strict allowlist에 정확히 `cppType`, `cppFlags`, `rankCount`만 compatibility metadata로 추가했다. 다른 unknown field rejection은 유지했다.
- runtime parser는 세 metadata를 읽거나 신뢰하지 않는다. 기존처럼 `type`, `flags`, 여섯 rank array만 파싱하고 `rankCount`도 array 길이에서 다시 계산한다.
- 독립 비평의 P1 지적을 수용해 테스트는 key 존재뿐 아니라 세 metadata에 대한 `node[...]`, `node.value(...)`, `node.contains(...)` 접근이 없음을 고정한다. 3153 fixture의 Physical/empty flags/1-rank 계약도 함께 검증한다.

# 3. 검증 실측

- Item runtime compatibility source contract — PASS.
- `Server/Include/Server.vcxproj` Debug — PASS. 새 `Server/Bin/Debug/WintersServer.exe` 생성 시각은 2026-07-19 03:30:18이다.
- scoped `git diff --check` — PASS.
- full `Test-F4BalanceContracts.py`는 새 item/parser 검사를 통과한 뒤 사용자가 저장한 이렐리아 Q 라이브 값이 원본 `[10, 9, 8, 7, 6]`과 다른 기존 지점에서만 FAIL했다.
- definition pack `--check`는 같은 라이브 champion JSON 때문에 Release generated outputs가 stale이라 FAIL했다. 사용자 밸런스 값을 되돌리거나 Release pack을 임의로 cook하지 않았다.
- 기존 Debug `GameRoomBotMatchSoak` 통합 probe의 첫 authoritative command로 `ReloadGameplayDefinitions`를 추가했다. 실행 전 compiled base pack에 item 3153이 실제 존재함을 요구하고, 실행 뒤 runtime definition revision 정확히 `+1`, tool revision/replay command 정확히 `3`을 검사하므로 3153 parser가 건너뛰거나 reject되면 fail-close한다.
- 실제 Debug 서버 객체와 링크한 probe — PASS. `[Data] runtime definitions reloaded rev=1 statRefresh=10 skillCooldownRefresh=40 minionRefresh=0 turretRefresh=2`, `CONTROL_PROBE status=PASS ... tool_revision=3 replay_commands=3`, 최종 `RESULT status=PASS`를 확인했다.
- 기존 PowerShell 5.1 래퍼는 성공 진단이 `stderr`로 출력되자 `$ErrorActionPreference = "Stop"` 때문에 중단됐다. 동일 freshly-linked executable을 같은 인자로 직접 실행해 exit code 0과 위 PASS 증거를 확인했다. 이는 runtime/parser 실패가 아니라 검증 래퍼의 stderr 취급 문제다.
- 실제 새 Debug Client 화면에서 `Save & Hot Load` 버튼을 눌러 보는 수동 UI QA는 수행하지 않았다. authoritative server reload command와 parser/publish 경로는 자동 통합 probe로 검증했다.

# 4. 판결

**로그에 나타난 Item 3153 parser 차단은 교정 및 실제 서버 통합 검증 완료.** 새 Debug 서버에서는 `cppFlags` 다음의 `rankCount`까지 포함해 동일 원인으로 막히지 않으며, reload publish와 살아 있는 챔피언 Q/W/E/R 40개 refresh도 자동 probe에서 통과했다. 실제 F4 버튼의 수동 화면 확인만 사용자 재실행으로 남는다.
