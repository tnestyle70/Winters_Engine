# 2026-07-19 5V5 승리 스노우볼 경제·매크로·밸런스 RESULT

관련 계획: `2026-07-19_5V5_VICTORY_SNOWBALL_ECONOMY_MACRO_BALANCE_PLAN.md`

Session - 5V5 승리 스노우볼 경제·맵 핑 AI·밸런스 구현 및 검증

## 1. 전/후 비교

- 미니언 XP: 여러 챔피언이 범위 안에 있으면 각자 동일 XP를 복제하던 흐름을 제거했다. 1명은 `nearbyXP` 전부, 2명 이상은 `teamXP / 생존한 같은 팀 근처 챔피언 수`를 받는다. 4종 미니언 × 1/2/5명 프로브가 총 XP 풀 보존을 확인했다.
- 포탑 골드: 보상이 없던 구조물 처치에 파괴자 +1,500G, 같은 팀의 다른 챔피언 각각 +1,000G를 추가했다. Blue/Red에 대칭이며 적 팀은 받지 않는다.
- 맵 핑 AI: 기존 핑 휠 코드를 서버 권위 `TeamPing` command로 연결했다. Assist는 같은 팀 봇 전체에 180 tick(6초) 합류 목표를 주고, Danger는 핑 시점 12m 안의 봇만 90 tick(3초) 동안 각자의 `safeAnchor`로 이동시킨다. On My Way/Missing은 기록만 하고 AI를 바꾸지 않는다.
- AI 우선순위: 리콜·저체력·포탑 위험·진행 중 combo/dive가 핑보다 우선한다. 핑 만료는 decision cadence와 무관하게 정확한 tick에 상태를 지우고 즉시 재판단한다.
- 라인 매크로: 3 human + 2 allied bot 조합에서 두 봇이 모두 Bot으로 남아 있을 때만 Bot/Top으로 보정한다. 적 5봇 라인은 유지한다. 적 미드 외곽 포탑 파괴 후 적이 미드에 모였다는 이유로 사이드 봇이 자동 미드 집결하던 공격 매크로는 제거했고, 아군 구조물 방어 집결은 유지했다.
- F4 밸런스 툴: Total AD Ratio와 Bonus AD Ratio 입력 상한만 5에서 10으로 넓혔다. 기존 스킬 실제 계수는 일괄 변경하지 않았고 AP/HP 등 다른 ratio 상한은 5로 유지했다.
- 기본 공격: BA는 이미 `StatComponent.ad`를 읽는 경로이므로 별도 피해식을 만들지 않았다. BA 조절은 AD/base AD/아이템 AD를 통해 한다.
- 애쉬 R: authoritative `stunDurationSec`를 3.0초에서 2.0초로 변경했다.
- 시작 자원: 사용자 설정을 보존했다. Debug 연습 기본 10,000G/레벨 6과 Release factory 4,000G를 변경하지 않았다.

## 2. 자체 점검 및 검증 결과

PASS:

- 독립 계획 비평 최종 P0 0 / P1 0. P2 중 핑 만료 즉시 재판단과 기존 리플레이 사례 보존을 반영했다.
- `Build-LoLDefinitionPack.py --check`: pack `0xE562BD0D`, 17 champions / 85 skills.
- `Test-F4BalanceContracts.py`: PASS.
- `Check-SharedBoundary.ps1`: PASS.
- Debug SimLab 빌드 및 `SimLab.exe --victory-economy-only`: PASS.
- 전체 SimLab 중 이번 경로: `ChampionAI MidDefense` PASS, `VictoryEconomyPing` PASS.
- ReplayCommandContractProbe: 기존 ReorderItem + TeamPing payload, journal/resim 계약 PASS.
- GameRoomBotMatchSoak 120 ticks: TeamPing, 3-human lane preset, ACK/journal, checkpoint restore, Assist next-tick Move, Danger equivalent-Move 소비, 반경 이탈 후 TTL 유지, 정확한 만료, 전체 soak PASS.
- Debug Server 빌드: PASS.
- Release Server x64 빌드: PASS.
- Debug Client x64 빌드: 첫 링크에서 일시적 LNK1114 파일 잠금 후 재실행 PASS. TeamPing serializer와 인게임 입력 경로가 컴파일·링크됐다.
- `git diff --check`: PASS(기존 LF→CRLF 경고만 존재).

잔여 FAIL / 미검증:

- 전체 `Run-BotAiValidation.ps1`은 기존 `RunGameplayFormulaDataDrivenProbe`의 `Ezreal Q rank-3 request was not built from the pack` 한 건으로 FAIL했다. 이번 변경 전용 MidDefense/VictoryEconomyPing 프로브는 같은 실행에서 PASS했다. Ezreal 정의 계약은 이 슬라이스에서 수정하지 않았다.
- 실제 Release 서버 + 백엔드 + 3개 클라이언트의 장시간 승리 플레이, 포탑 골드 HUD ledger, 애쉬 R 60 tick 실측, 1920×1080/100% F4 화면 캡처는 headless 환경에서 미검증이다.
- FlatBuffers client packet → `AcceptCommandBatch`의 전용 TeamPing wire round-trip은 미추가다. generated schema, client serializer, server handler 컴파일과 GameRoom ingress 이후 경로는 검증했다.
- `run_codegen.bat`는 단독/단일 MSBuild에서 PASS하지만 병렬 `/m`에서는 기존 사전 빌드 훅 동시 실행 경쟁이 재현됐다. 검증은 `/m:1`로 닫았다.

## 3. 새로 얻은 점과 판단

- 문제의 본질은 적 봇의 콤보 성능 하나가 아니라, 실력 우위를 성장 격차로 보존하는 경제 규칙과 그 격차를 포탑·라인 압박으로 환전하는 팀 의사결정 계약이 없었던 것이다.
- XP 복제 제거는 뭉친 봇에게 기회비용을 만들고, 공격 자동 미드 집결 제거는 사이드 성장의 선택지를 되살린다. 포탑 보상은 그 성장 격차를 승리 가속으로 바꾸는 강한 규칙이다.
- 포탑 한 개당 팀 총 +5,500G는 공정한 LoL형 밸런스라기보다 명시적인 승리 보조 규칙이다. “상대는 포탑을 못 깬다”가 깨지면 대칭 적용으로 적도 같은 보상을 받으므로 실제 경기 ledger를 보고 수치를 재평가해야 한다.
- AD Ratio 10은 입력 가능 범위를 넓힌 것뿐이다. 실제 스킬 계수를 10으로 바꾸는 것과는 다르며, 후자는 TTK 전수 검증 없이는 적용하면 안 된다.
- 다음 외부 마감은 2026-07-21 실제 3-client 5v5 1판이다. 확인 항목은 시작 자원, Bot/Top 배치, 사이드 유지, Assist/Danger 반응, 포탑 +1500/+1000×4 ledger, 애쉬 R 2초, F4 10.00 저장/핫로드다.
