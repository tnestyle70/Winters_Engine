# GameRoom ServerProjectileAuthority 추출 보고서

- 날짜: 2026-06-23
- 범위: `14_CODEX_PROMPT_ATOMIC_DECOMPOSITION.md` / `SERVER_ATOMIZE`
- 대상: GameRoom Stage 2 `ServerProjectileAuthority`

## 진행 요약

`CGameRoom::Phase_ServerProjectiles` 안에 있던 투사체 판정/DTO 생성 책임을 `CServerProjectileAuthority`로 분리했다. GameRoom은 여전히 실제 사이드 이펙트 실행 셸로 남긴다.

새 Authority 책임:
- 미니언 원거리 투사체 판별
- 터렛 투사체 kind 상수와 미니언 원거리 타겟 높이 상수 소유
- 스킬 투사체 히트 타겟 탐색
- 투사체 spawn/hit replicated event DTO 생성
- 터렛/스킬 투사체 damage request DTO 생성

GameRoom에 유지한 책임:
- `EnqueueReplicatedEvent`
- `EnqueueDamageRequest`
- 상태이상/챔피언별 히트 효과 적용
- projectile entity destroy
- net id 발급/해제
- transform 갱신
- 디버그 로그

## 변경 파일

- `Server/Public/Game/ServerProjectileAuthority.h`
- `Server/Private/Game/ServerProjectileAuthority.cpp`
- `Server/Private/Game/GameRoomProjectiles.cpp`
- `Server/Include/Server.vcxproj`
- `Server/Include/Server.vcxproj.filters`

## 보존한 동작

- 타겟 추적 투사체 명중은 기존처럼 `Physical` damage type을 사용한다.
- 비타겟 스킬 투사체 명중은 기존처럼 `SylasChain`만 `Magic`, 그 외는 `Physical`을 사용한다.
- 투사체 event enqueue, damage queue enqueue, 상태효과 적용, 엔티티 삭제 순서는 기존 흐름을 유지했다.
- 새 Authority에는 Session/Packet/FlatBuffers/ReplayRecorder/Send 계층 의존성을 넣지 않았다.

## 검증

```powershell
git diff --check -- Server\Public\Game\ServerProjectileAuthority.h Server\Private\Game\ServerProjectileAuthority.cpp Server\Private\Game\GameRoomProjectiles.cpp Server\Include\Server.vcxproj Server\Include\Server.vcxproj.filters
```

- 결과: 통과
- 참고: 일부 파일 CRLF 변환 warning만 출력

```powershell
rg -n "#include .*(Client|Renderer|UI|ImGui|d3d|DX)|CSession|CPacket|WrapEnvelope|flatbuffers|PacketDispatcher|Session_Manager|ReplayRecorder|Send\(" Server\Private\Game\ServerProjectileAuthority.cpp Server\Public\Game\ServerProjectileAuthority.h
```

- 결과: 매치 없음

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /m:1 /nodeReuse:false /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /p:IntDir=C:\Users\tnest\Desktop\Winters\Server\Bin\Intermediate\CodexDebug\
```

- 결과: 성공
- 오류: 0
- 경고: 4
- 참고: 기존 계열 `MSB8028`, `C4275` warning 유지. PreBuildEvent에서 EngineSDK 복사 공유 위반 메시지가 한 줄 있었지만 최종 빌드/링크는 성공.

```powershell
.\Server\Bin\Debug\WintersServer.exe --smoke-seconds=10
```

- 결과: 성공
- 종료 코드: 0

## 다음 후보

- GameRoom Stage 2 남은 owner 추출 후보 재확인: `WalkabilityAuthority` 또는 tick/lifecycle 셸 축소
- `GameRoomProjectiles`의 남은 side effect 순서를 유지한 상태에서 더 작게 쪼갤 수 있는지 확인
