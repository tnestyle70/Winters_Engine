Session - 현재 구현된 17개 챔피언·85개 슬롯의 피해/랭크/아이템 온힛과 96개 visual stage가 canonical data 경로를 계속 통과하는지 재검증하고, 미구현 제품 규칙을 완료 판정에서 분리한다.
좌표: 신규 좌표 후보 · 축: C7 · C8
관련: 2026-07-16_GAMEPLAY_FORMULA_DATA_DRIVEN_AUDIT_REPORT.md · 2026-07-16_GAMEPLAY_FORMULA_SKILL_ANIMATION_DATA_DRIVEN_REMEDIATION_RESULT.md

## 1. 결정 기록

① 문제·제약: roster 17명·BA/Q/W/E/R 85슬롯·아이템 34개·visual stage 96개에서 gameplay 값은 서버 권위, visual timing은 클라이언트 표현이어야 한다. 현재 pack hash는 `0x8E9EF70F`, champion hash는 `0x9D6886A7`이다.
② 순진한 해법의 실패: JSON·챔피언 C++·빈 scaling registry·Debug tuner를 동시에 진실로 두면 rank/온힛이 이중 계산되고, cutover 전 cast/recovery 0값을 우선하면 정상 애니메이션 타이밍이 사라진다.
③ 메커니즘: ServerPrivate/ClientPublic canonical JSON → validate/codegen → immutable definition → 공용 query/request/pipeline 한 경로로 고정하고, Debug overlay와 timing draft는 승인 전 임시 데이터로 격리한다.
④ 대조: 런타임 Data Asset 편집보다 빌드·SimLab 결정론을 우선한다. Winters의 현재 서버 권위/코드젠 구조에서는 cook된 pack과 typed practice command가 클라이언트 직접 gameplay mutation보다 안전하다.
⑤ 대가: cook·검증 때문에 즉시 수정 속도가 느리고 미확정 Fiora/Zed 규칙은 자동 완성되지 않는다. variant hook이 query를 우회하거나 mixed-type item passive·visual timing이 gameplay lock을 소유하면 이 선택은 깨진다.

## 2. 반영해야 하는 코드

없음. 이 계획의 코드 cutover는 2026-07-16에 이미 반영되었으므로 새 규칙에 따라 적용 완료 코드를 다시 계획으로 적지 않는다. 실제 변경 구조와 수식은 관련 `_RESULT` 및 상세 구현 보고서를 검증 근거로 사용한다.

## 3. 검증

예측:

- definition pack check는 hash `0x8E9EF70F`, champion 17, skill 85를 출력하고 champion data check는 hash `0x9D6886A7`을 출력한다.
- SimLab은 `FormulaData`, `BORK`, `SkillRank`, same-seed replay를 PASS하고 hash `D9B579C1425033BB`를 유지한다.
- 회귀가 생기면 85-slot coverage, BORK 1회 proc, Q/W/E 1·3·5·7·9와 R 6·11·16 gate, visual timing validator 중 하나가 먼저 실패해야 한다. variant hook 우회와 정상 F5 체감에는 완전한 자동 게이트가 없다.
- Bot AI는 GameCommand 생산자이며 damage/stat/rank truth를 직접 변경하지 않는다. gameplay 결과는 Server/GameSim의 동일 definition/query/pipeline을 통과한다.

검증 명령:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
python Tools/ChampionData/build_champion_game_data.py --root . --check
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/Check-SharedBoundary.ps1
msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
Tools/Bin/Debug/SimLab.exe
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
git diff --check
```

미검증:

- 2026-07-17 문서 개편 세션에서는 codegen 두 종과 기존 SimLab 실행만 재검증했다. Client/Server 재빌드와 서버를 띄운 정상 F5 visual timing 눈 검증은 다시 수행하지 않았다.
- Fiora 급소 패시브의 피해 기준·방향·보상, 별도 Zed passive, 혼합 피해 item passive는 제품 명세 또는 구현이 확정되지 않았으므로 현재 Data Driven 합격 범위 밖이다.

확인 필요:

- 새 variant skill을 추가할 때 중앙 formula 제외 목록과 별도 trigger가 같은 canonical key를 공유하는지 확인.
- mixed physical/magic/true item on-hit가 필요해지면 한 request 합산이 아니라 피해 타입별 request 분리를 별도 계획으로 작성.
- `ChampionVisualDefs.json` draft 병합 뒤 서버 action lock보다 visual recovery가 gameplay 입력을 먼저 열지 않는지 정상 F5에서 확인.
- schema-owning aggregate build의 병렬 codegen이 직렬화되기 전까지 Client/Server 검증은 `/m:1`을 유지.
- 작업 예산은 바닥 검증 70%, 천장 산출물 30%로 고정한다. 천장 산출물은 canonical JSON diff와 SimLab/F5 비교 캡처 한 세트이며 외부 공개 마감은 2026-07-18로 유지한다.
