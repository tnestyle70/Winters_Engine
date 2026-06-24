Session - 2026-06-24 Champion AI combo, LeeSin ward-hop result report.

1. 기준 요구사항

- `main`과 먼저 동기화한다.
- AI는 서버 권위 흐름을 깨지 않고 `GameCommand`만 발행한다.
- 리신은 `Q -> Q2 -> BA -> E -> BA -> E2 -> ward -> W ward-hop -> R` 기본 콤보를 시도한다.
- 전용 콤보가 없는 챔피언도 기본적인 스킬/평타 콤보를 시도한다.
- ward 설치, ward 시야, ward snapshot replication, 리신 ward/allied W dash가 동작해야 한다.
- 계획서와 결과 보고서를 남기고 코드 위생 및 서버/클라이언트 빌드를 확인한다.

2. 반영 상태

2-1. 동기화

- `git pull --ff-only origin main` 결과 `main`은 이미 `origin/main`과 동기화된 상태였다.

2-2. AI 콤보

- `ChampionAIComboPlan` 최대 step 수를 10으로 확장했다.
- 전용 콤보가 없는 챔피언의 기본 콤보를 `Q -> BA -> E -> BA -> R`로 정리했다.
- 리신 콤보를 `Q -> Q2 -> BA -> E -> BA -> E2 -> WardBehindTarget -> W(LastOwnWard) -> R`로 반영했다.
- Q2/E2는 일반 쿨다운이 아니라 stage2 window와 two-stage definition을 기준으로 판단한다.
- 리신 Q2는 대상에게 자기 Q mark가 있을 때만 발행한다.
- 배우지 않은 스킬 단계는 건너뛰어 저레벨 AI가 콤보에서 멈추지 않게 했다.

2-3. Ward / Vision / Replication

- `Shared/GameSim/Definitions/WardDefinitions.h`에 ward item id, 설치 사거리, 시야 범위, 지속 시간, spatial radius를 공용 상수로 추가했다.
- `UseItem` command에서 trinket ward를 서버 권위로 생성한다.
- 생성된 ward에는 transform, ward owner, spatial, vision source, visibility, targetable, net id component를 붙인다.
- 서버 tick에서 ward duration을 감소시키고 만료 시 net id 해제 후 destroy한다.
- snapshot builder는 ward를 `EntityKind::Ward`로 복제한다.
- client snapshot applier는 ward runtime tag, spatial, vision, visibility, targetable, 작은 marker billboard를 붙인다.

2-4. LeeSin W / Input

- 리신 W stage1 target mode를 `UnitTarget`으로 바꿨다.
- 서버 sim hook에서 W stage1이 아군 챔피언, 아군 미니언, 아군 ward로 dash하도록 했다.
- 클라이언트 hover query가 ward와 아군도 잡을 수 있게 했고, 공격 명령의 enemy filter는 유지했다.
- 리신 W cast command만 예외적으로 아군/ward unit target을 허용한다.
- 서버 권위 모드에서 `4` 키로 커서 위치에 trinket ward `UseItem` command를 보낼 수 있게 했다.

3. 검증 결과

3-1. 코드 위생

```powershell
git diff --check
```

- 통과.
- 공백 오류 없음.
- 일부 파일은 기존 줄끝 정책 때문에 `LF will be replaced by CRLF` 경고만 표시됨.

3-2. 서버 빌드

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' .\Server\Include\Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

- 성공.
- 기존 DLL interface/codepage 계열 경고는 남아 있음.

3-3. 클라이언트 빌드

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' .\Client\Include\Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

- 성공.
- 기존 DLL interface/codepage 계열 경고는 남아 있음.

4. 남은 확인

- 실제 서버/클라이언트 런타임에서 리신 AI가 ward를 적 뒤에 설치한 뒤 W로 타는지 화면으로 확인해야 한다.
- ward marker는 현재 작은 billboard marker로만 표시된다. 정식 ward 모델/텍스처가 정해지면 snapshot applier marker만 교체하면 된다.
- vision은 `VisionSourceComponent`로 연결했으므로, 팀별 시야 렌더링/미니맵 표시 정책은 기존 vision consumer 쪽에서 이어서 확인하면 된다.

