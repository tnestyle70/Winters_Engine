# GameRoom WalkabilityAuthority 추출 보고서

- 날짜: 2026-06-23
- 범위: `14_CODEX_PROMPT_ATOMIC_DECOMPOSITION.md` / `SERVER_ATOMIZE`
- 대상: GameRoom Stage 2 `WalkabilityAuthority`

## 진행 요약

`CGameRoom`의 walkability override 계약은 유지하고, 내부 계산 일부를 `CWalkabilityAuthority`로 분리했다. 이번 조각은 Shared/GameSim과 서버 이동 로직이 자주 호출하는 query/resolve 계층, 이동 타겟 resolve DTO, move path build DTO, authored navgrid/wmesh path resolve, stage bounds coverage 판정을 분리했다.

새 Authority 책임:
- wmesh path resolve
- authored navgrid path resolve/load
- stage gameplay bounds 계산
- authored navgrid coverage 판정
- path/base navgrid 선택
- XZ walkable query
- XZ segment walkable query
- move segment clamp
- map surface height sample
- raw position을 nearest walkable position으로 보정
- move target resolve 결과 DTO 생성
- move path build 결과 DTO 생성

GameRoom에 유지한 책임:
- `ICommandExecutor` override 표면
- navgrid 생성/베이크/구조물 carve
- authored navgrid path/load 실패/reject 로그
- flow field rebuild trigger
- minion waypoint sanitize shell
- 이동 타겟 resolve/path resolve의 bounded debug log
- Shared/GameSim `ICommandExecutor` API에 맞춘 output waypoint buffer 복사

## 변경 파일

- `Server/Public/Game/WalkabilityAuthority.h`
- `Server/Private/Game/WalkabilityAuthority.cpp`
- `Server/Private/Game/GameRoomNav.cpp`
- `Server/Include/Server.vcxproj`
- `Server/Include/Server.vcxproj.filters`

## 보존한 동작

- navgrid가 없으면 walkable query/segment query는 기존처럼 `true`를 반환한다.
- clamp 시작점이 blocked인 경우 nearest walkable cell로 이동하고, height sample 실패 시 기존 y를 유지한다.
- `TrySampleHeight`는 기존처럼 surface sampler height에 `+0.05f`를 더한다.
- `TryResolveServerWalkablePosition`은 기존처럼 path grid 우선, 없으면 base grid를 사용한다.
- `TryResolveMoveTarget`은 기존처럼 blocked start 보정, out-of-bounds reject, BFS nearest reachable goal 보정, surface delta reject 로그 조건을 유지한다.
- `TryBuildMovePath`는 기존처럼 no-grid/direct/path-single/path trace mode, waypoint cap 실패 시 partial waypoint count, surface delta reject 로그 조건을 유지한다.
- authored navgrid stage bounds coverage 판정은 Authority가 수행하고, reject 로그는 GameRoom shell에서 출력한다.
- authored navgrid path/load 로그는 GameRoom shell에서 출력한다.
- 로그 출력은 새 Authority로 이동하지 않았다.

## 검증

```powershell
git diff --check -- Server\Public\Game\WalkabilityAuthority.h Server\Private\Game\WalkabilityAuthority.cpp Server\Private\Game\GameRoomNav.cpp Server\Include\Server.vcxproj Server\Include\Server.vcxproj.filters
```

- 결과: 통과
- 참고: 일부 파일 CRLF 변환 warning만 출력

```powershell
rg -n "#include .*(Client|Renderer|UI|ImGui|d3d|DX)|CSession|CPacket|WrapEnvelope|flatbuffers|PacketDispatcher|Session_Manager|ReplayRecorder|Send\(" Server\Private\Game\WalkabilityAuthority.cpp Server\Public\Game\WalkabilityAuthority.h
```

- 결과: 매치 없음

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /m:1 /nodeReuse:false /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /p:IntDir=C:\Users\tnest\Desktop\Winters\Server\Bin\Intermediate\CodexDebug\
```

- 결과: 성공
- 오류: 0
- 경고: 6
- 참고: 기존 계열 `MSB8028`, `C4275` warning 유지. 이번 빌드에서는 `GameRoomProjectiles.cpp`도 재컴파일되어 `C4275` warning이 추가로 반복 출력됐고, 최종 빌드/링크는 성공.

```powershell
.\Server\Bin\Debug\WintersServer.exe --smoke-seconds=10
```

- 결과: 성공
- 종료 코드: 0

## 다음 후보

- fallback bake 쪽은 stage bounds/seeds/surface sampler와 얽혀 있으므로 별도 조각으로 분리
- flow field rebuild trigger와 minion waypoint sanitize는 side effect 경계가 있으므로 별도 세션에서 처리
- `GameRoomNav.cpp`에 남은 fallback bake seed/build helper를 `WalkabilityAuthority` 또는 별도 `ServerNavBootstrap`로 나눌지 검토
