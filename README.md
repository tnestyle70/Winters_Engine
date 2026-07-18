# Winters Engine

**서버 권위(server-authoritative) 멀티플레이 MOBA를 바닥부터 돌리는 자체 C++20 게임 엔진.**
LoL 구조의 5:5 게임이 자체 DX11 렌더러, IOCP 서버, Go 백엔드 위에서 실제로 플레이됩니다 — 계정 가입부터 챔피언 구매, 밴픽, 인게임 전투, 전적·리플레이 저장까지 한 바퀴 전부.

[GIF 슬롯: 클라 2분할 + 서버 콘솔 — 서버 권위 동기화]
[GIF 슬롯: 인게임 전투 하이라이트]

▶ **포트폴리오 영상 (2분)**: [유튜브 링크]
▶ **포트폴리오 (Notion)**: [링크]

---

## 핵심 설계 — 클라이언트를 신뢰하지 않는다

```text
Client Input → GameCommand → Server GameSim (30Hz 고정 틱)
            → FlatBuffers Snapshot/Event → Client 보간·연출
```

모든 게임플레이 진실은 서버만 만듭니다. 클라이언트가 어떤 값을 보내든 서버가 커맨드 입구에서 검증(`CanMove/CanCast`, 시퀀스 윈도우)하고 자기 시뮬레이션 결과만 배포합니다. 봇 AI도 GameCommand 생산자로 통일되어 진실을 직접 수정하는 경로 자체가 없습니다. 클라 예측·서버 보정 충돌은 예측 보호 규약(최대 12스냅샷)으로, 고핑 판정은 200ms 위치 히스토리 지연 보상으로 풉니다.

## 도메인 지도

| 도메인 | 내용 |
|---|---|
| 엔진 코어 | ECS(세대 핸들 + sparse-set + Phase 스케줄러) · Chase-Lev work-stealing JobSystem(+Fiber 모드, 10만 잡 벤치 워커 처리량 +25%) · RHI 추상화(DX11 기본 + DX12 백엔드) |
| 서버 권위 심 | 30Hz 고정 틱 · 챔피언 17종 시뮬 · IOCP 워커4+틱1 · 300틱 결정성 해시 회귀(SimLab) · .wrpl 리플레이 기록/재생 · 시뮬 되감기(90초 키프레임 창) |
| 데이터 & 툴 | JSON 저작→쿡 검증→C++ 팩(서버 런타임 파싱 0회)+핫리로드 · FBX 27종→자체 .wmesh(60MB→1.2MB, ~50배) · 이펙트 툴/챔피언·구조물 튜너/Model&Anim Lab(전부 ImGui) · 독립 에디터 exe(LoLEditor) |
| 백엔드 | Go 마이크로서비스(인증/프로필/상점) + PostgreSQL/Redis/Kafka, docker-compose — 가입→RP→구매→매치→전적/리플레이 E2E |
| 품질 & 성능 | 최적화 빌드 상시 계측(GPU 패스 타임스탬프·파이프라인 통계·통합 드로우 카운터) · 리플레이 무인 캡처 CLI · 실측 원장 · 예외 경로 전수 감사 |

## 실측 수치 (전부 재현 가능한 조건과 함께 기록)

- **애니메이션 지연 평가 최적화**: 프로파일러가 지목한 1위 병목(화면 밖 캐릭터 포즈 계산, 프레임의 25%)을 소비 지점 지연 평가로 전환 → **Update 페이즈 0.72→0.17ms(-76%)**, 프레임 중앙값 1.69→1.1~1.3ms — Release /O2, 동일 리플레이 3회 재현, GPU·드로우콜 불변 (`.md/plan/performance/PROFILING_LEDGER.md`)
- **결정성**: 108파일 리팩터링 전후 300틱 시뮬 해시 동일 — 무회귀를 기계가 증명
- **에셋 파이프라인**: FBX/GLB 27종 전수 변환 FAIL 0, 대표 메시 60MB→1.2MB
- **서버 안정성**: 30분 5v5 봇 매치 soak, p99 틱 3.4ms (30Hz 예산 33.3ms)

## 개발 방식 — CS + AI 에이전트

이 저장소는 AI 에이전트 하네스로 개발됩니다: 사람이 문제를 정의하고 구조를 설계하면, 두 개의 AI(Claude Code, Codex)가 병렬 레인에서 구현하고, **빌드·스모크·300틱 결정성 해시 게이트를 통과해야만 머지**됩니다. 컨벤션(`CLAUDE.md`/`AGENTS.md`), 재발 방지 로그(`.claude/gotchas.md`, 30+ 항목), 세션 인수인계 문서가 저장소 안에 실물로 있습니다 — "AI가 짰다"가 아니라 "게이트를 통과했다"가 기준입니다.

## 저장소 구조

```text
Engine/    엔진 DLL — ECS, RHI(DX11/DX12), JobSystem/Fiber, 프로파일러, 리소스, ImGui 툴
Client/    게임 클라이언트 — 씬 플로우(로그인→로비→밴픽→로딩→인게임), 챔피언/FX, HUD
Server/    전용 서버 — IOCP, 30Hz GameRoom, 커맨드 검증, 스냅샷/이벤트, 리플레이 기록
Shared/    GameSim — 서버·SimLab이 공유하는 결정론 시뮬 코어 (Engine 비의존)
Services/  Go 백엔드 — auth/profile/shop + docker-compose (PostgreSQL/Redis/Kafka)
Tools/     SimLab(결정성 하니스), 프로파일러 분석기, 에셋 컨버터, 하네스 스크립트
LoLEditor/ 독립 에디터 실행 파일 (클라이언트와 빌드 분리)
```

## 빌드 & 실행 (Windows, VS2022)

```powershell
# 1) 백엔드 (선택 — 계정/상점 E2E용)
powershell -File Services\StartBackend.ps1

# 2) 엔진 → 클라이언트 → 서버 빌드 (vswhere 경유 MSBuild, x64)
msbuild Engine\Include\Engine.vcxproj  /t:Build /p:Configuration=Release /p:Platform=x64 /m:1
msbuild Client\Include\Client.vcxproj  /t:Build /p:Configuration=Release /p:Platform=x64 /m:1
msbuild Server\Include\Server.vcxproj  /t:Build /p:Configuration=Release /p:Platform=x64 /m:1

# 3) 실행
Server\Bin\Release\WintersServer.exe            # 0.0.0.0:9000
Client\Bin\Release\WintersGame.exe              # 127.0.0.1:9000 접속

# 결정성 회귀 (골든 해시)
Tools\Bin\Release\SimLab.exe

# 프로파일링 원커맨드 (빌드→리플레이 무인 캡처→분석→원장 기록)
powershell -File Tools\Profiler\run_profile_session.ps1
```

인게임 키: `F3` 프로파일러 오버레이 · `F12` 프로파일 JSON 캡처 · `F4`/`7`/`8`/`9` 튜너·랩 패널(Debug).

## 에셋 고지

이 저장소는 학습·포트폴리오 목적의 비상업 프로젝트입니다. 서드파티 게임 에셋(모델/텍스처/사운드)은 저장소에 포함하지 않습니다.
