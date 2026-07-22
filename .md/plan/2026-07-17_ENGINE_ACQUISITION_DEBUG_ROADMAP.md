Session - Winters Engine 전체를 브레이크포인트·디버깅 검증으로 인수(⓪)하고, 좌표 42장을 "수정 결과를 예측할 수 있는" 방어 가능 상태로 만든다.

이 문서는 코드 변경 계획이 아니라 인수(引受) 로드맵이다. 산출물은 코드가 아니라 (1) 백지 재현물 (2) ENDORSE/CONTEST 판결 로그 (3) 예측-실측 divergence 기록이다.

## 0. 원칙과 완료 기준

- 이해의 정의 = 예측. 읽고 끄덕이면 인식, 수정 결과를 미리 맞히면 획득.
- 대상 = Winters 좌표 42장(05 답안집 W01~W22 + S01~S20). 풀 ⓪(predict-modify-run) ~15장, 라이트 ⓪(읽기+수직추적) ~27장. 통독 금지.
- 전체 완료 기준: 42장 방어 + 백지 뼈대(사다리) 재현 + CONTEST 목록 존재 + closed-book 재구현 3건 diff.
- 채점지(기존 해설 문서)는 독립 판독이 끝난 뒤에만 연다:
  `.md/plan/refactor/05_ECS_JOB_FIBER_ATOMIC_ESSENCE_PLAN.md`, `.md/interview/engine/04_ecs_gameobject.md`, `.md/plan/2026-07-10_ENGINE_DNA_HANDS_ON_SESSION_01.md`
- CONTEST 확정 전 반드시 시간축 감정(git blame → 커밋/세션 문서)으로 "의도인가 실수인가"를 가른다 (Chesterton's Fence).

## 1. 상비 장비 (모든 주차 공통)

함수 브레이크포인트 사다리 — 중단점 창 → 새로 만들기 → 함수 중단점. 심볼 기반이라 줄 이동에 안 썩는다. 세트 구성 후 XML 내보내기(`Tools/Debug/ladder_frame.xml` 권장, 위치는 자유):

```text
wWinMain                       (Client/Private/main.cpp)
WintersRun                     (Engine/Private/WintersEngine.cpp)
CEngineApp::Run                (Engine/Private/Framework/CEngineApp.cpp)
CEngineApp::Update
CSystemScheduler::Execute      (Engine/Private/ECS/SystemScheduler.cpp)
CEntityManager::TryResolve     (Engine/Public/ECS/Entity.h)
```

Present는 심볼명 미확정 — `Engine/Private/RHI/DX11/CDX11Device.cpp`의 `m_pSwapChain->Present` 줄(line 1019 근처)에 일반 BP. (확인 필요: 소속 메서드명 확인 후 함수 BP로 승격)

계측기:
- `Tools\Bin\Debug\SimLab.exe` — 기본 1800틱/seed 42. 기대 로그: `same-seed replay OK: hash=%016llX` + 프로브 23종 PASS 체인. 모든 ⓪ 실험의 1차 관측 장비.
- 인게임: ImGui 튜너(9키 Practice, F4, F5, RenderDebug), bounded OutputDebugString.
- GPU 의심 시 RenderDoc 우선 (CPU 디버거로 못 잡는 패턴).

위생 규칙:
- 브랜치 `autopsy/wNN` 또는 `autopsy/sNN`에서만 파괴 실험. 커밋 금지, 종료 시 `git checkout -- .` 원복.
- BP·수정은 항상 `Engine/Public|Private` 원본에. `EngineSDK/inc`는 UpdateLib.bat 산출물이라 금지.
- 로그는 log.csv 한 줄: `날짜,좌표,판정(ENDORSE|CONTEST|보류),막힌 축,divergence 한 줄`.

## 2. 경로 1 — 엔티티의 일생 (주 1) : W01 · W02 · W14

추적: 스폰 호출부 → `CWorld` CreateEntity → `EntityHandle` 발급 → `CComponentStore<T>` Add → `ForEach` 순회 → 파괴 → free list 재활용 → stale 핸들 `TryResolve` 거절.

읽기(선언부 먼저): `Engine/Public/ECS/Entity.h` → `ComponentStore.h` → `World.h`.

풀 ⓪ 실험:
- 값 보존: 컨테이너 pre-reserve 등 → 예측 "SimLab 골든 해시 불변" → 실행 → 대조.
- 고의 파괴: `TryResolve` generation 검사 무력화 → "프로브 23종 중 무엇이 우는가" 선예측 → 실행. 아무것도 안 울면 그 자체가 검증 구멍 발견(CONTEST 후보, C8).
- 연습문제(판정 비공개): `Engine/Public/ECS/Components/TransformComponent.h`를 열기 전에 — W01의 ①에서 출발해 "계층 부모 참조가 어떤 타입이면 어떤 시나리오에서 무엇이 터지는가"를 먼저 유도하고, 그다음 파일을 열어 현재 코드와 대조. CONTEST 여부는 blame 감정까지 포함.

완료 판정: W01/W02/W14 5칸 구두 방어 + 판결 3건 + divergence 기록.

## 3. 경로 2 — 프레임의 일생 (주 2) : W03 · W13 · W07 · W08 · W11

추적: `CEngineApp::Run` 루프 → dt → `OnUpdate` → `CSystemScheduler::Execute`(병렬 배치) → `OnRender`(Scene_InGameRender.cpp: NormalPass → GTAO CS → 포워드 → PostFx) → Present.

풀 ⓪ 실험 후보:
- `kParallelThreshold`(16)을 1로 낮춰 병렬 강제 → 예측: 어떤 시스템이 워커로 넘어가고 무엇이 그대로인가 → JobSystem 통계 카운터로 관측.
- PostFx `SetEnabled` 토글(RenderDebug 체크박스) → 예측: 블룸/비네트 시각 변화와 프레임타임 방향.
- 고의 파괴: WorkStealingDeque 용량 초과 유도 또는 전역 큐 폴백 경로 강제 → 예측: 어느 카운터가 튀는가.
- GTAO 파라미터(radius 1.25) 극단값 → RenderDoc 1캡처로 AO 버퍼 확인.

주의: `CWorld::ForEach`는 템플릿이라 함수 BP가 안 박힌다 — 인스턴스화 호출부(파일:줄)에 일반 BP.

## 4. 경로 3 — 명령의 일생 (주 3) : S01 · S03 · S04 · S07 · S08 · S18

3A 인프로세스(먼저): SimLab은 심을 단일 프로세스로 돌린다 — 네트워크 없이 CommandExecutor/틱/키프레임을 스테핑할 수 있는 최단 경로. SimLab.exe를 디버거로 실행해 `Tools/SimLab/main.cpp`의 시나리오 프로브에서 진입.

3B 실 클라-서버: 서버는 별도 프로세스 — 솔루션 다중 시작 프로젝트(Server+Client) 또는 실행 후 어태치. TCP 포트 9000, IOCP 워커 4. 백엔드 필요 시 `Services/StartBackend.ps1` (WINTERS_DEV_AUTH_ENABLED=true) + 비회원 로그인.

추적: 우클릭 → Move 커맨드 직렬화(`GameSessionClient`) → 서버 수신 → CommandExecutor → `GameRoomTick.cpp` 페이싱(33,333µs, 배속 클램프 0.1~8x) → `SnapshotBuilder::Build`(세션별 yourNetId/lastAcked만 개인화) → 클라 `SnapshotApplier`(yaw 보호 최대 12스냅샷).

풀 ⓪ 실험 후보:
- 배속을 8x 초과로 요청 → 예측: 클램프 지점과 실제 period 값.
- yaw 보호: 이동 직후 스냅샷 몇 장 동안 로컬 yaw가 유지되는지 세어 예측 → 관측.
- 고의 파괴: FlatBuffers verify 실패 유도(패킷 1바이트 변조는 3A에서 불가하므로 확인 필요: UdpLoopbackHarness/chaos 경로 활용) → bounded trace가 찍히는지.

계약: Bot AI는 GameCommand 생산자다 — 관찰 중 봇 경로가 게임플레이 진실을 직접 쓰는 지점이 보이면 그 자체가 CONTEST다.

## 5. 경로 4 — 정의값의 일생 (주 4) : W12 · W15 · S13

추적: 인게임 9키 → op25(ReloadGameplayDefinitions) → `Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp` 파싱·검증 → atomic 팩 스왑(실패 시 활성 팩 불변, 구세대 보존) → CommandExecutor 값 참조.

풀 ⓪ 실험 후보:
- JSON 수치 하나 수정 → 리로드 → 예측: 반영 시점(즉시인가, 다음 스폰인가 — SpawnObject는 복사 의미론).
- 쿨다운 튜닝 → 예측: 반영 안 됨 — `CommandExecutor.cpp` line 711 근처 dev 오버라이드 `return 3.0f`가 마스킹. 실측으로 확인 후 이 줄 자체를 판결(의도된 튜닝 게이트인가, 제거 대상인가 — blame 감정).
- 깨진 JSON 리로드 → 예측: 활성 팩 불변 + 실패 로그.
- 키프레임: WKF1 save→restore→save 바이트 동일성 — SimLab 프로브로 이미 게이트됨을 확인하고, 미등록 컴포넌트 하나를 추가해 하드실패가 뜨는지(W15의 완전성 계약) 파괴 실험.

## 6. 라이트 ⓪ (나머지 ~27장, 주차 사이 30분 단위)

읽기+수직추적+5칸 방어만 (파괴 실험 생략): W04(Fiber 3모드) W05 W06 W09 W10 W16~W22, S02 S05 S06 S09~S17 S19 S20. 프로세스/문서 좌표(W16 W20 W21)는 해당 스크립트·정책 문서 확인으로 대체.

## 7. 시험 — closed-book 재구현 (해당 좌표 ⓪ 완료 후)

레포 밖 `Desktop\rewrite-exam\`에서, 원본 닫고: W01 resolve 경로(~50줄) → W02 add/remove/iterate(~100줄) → (선택) W03 덱(~150줄). 원본과 diff — diff의 각 줄은 "내가 못 본 것(②③ 학습 목록)" 또는 "원본이 이상한 것(CONTEST 후보)".

## 8. 디버깅 함정 (이 로드맵 한정)

- Debug|x64 필수. Release는 인라인·최적화로 스텝 불가, 디버그 패턴값(0xCC/0xDD)도 없음.
- 함수 BP는 템플릿·인라인에 안 박힌다 → 호출부 일반 BP로.
- 조건부 BP를 틱 루프 안에 걸지 말 것(히트마다 트랩 — 수백 배 감속). 데이터 BP는 하드웨어 4개 한계.
- 서버 스테핑 중 클라는 타임아웃될 수 있다 — 프로토콜 관찰은 3A(SimLab)가 기본, 3B는 확인용.
- /fp:precise 전제의 결정론 계약 — Shared/GameSim 접촉 실험은 SimLab 골든으로만 판정하고 절대 커밋하지 않는다.

## 2. 검증

미검증:
- 이 문서의 주차 배정은 계획이며, 실측 소요는 log.csv 누적으로 갱신한다.

검증 명령:
- 매 ⓪ 세션 종료 시: `Tools\Bin\Debug\SimLab.exe` → `same-seed replay OK` + 프로브 PASS 확인 후 원복 상태로 종료.
- `git status`로 autopsy 브랜치 외 오염 없음 확인.

확인 필요:
- CDX11Device Present 소속 메서드명 (함수 BP 승격용).
- 경로 3 패킷 변조 실험의 안전한 주입 지점 (UdpLoopbackHarness vs chaos 파이프).

완료 판정(주차별):
- 주 N 종료 시 해당 좌표들의 5칸 구두 방어 + log.csv 판결 + divergence ≥ 1건 기록.
- 최종: 42장 방어 + 백지 사다리 재현 + CONTEST 목록 + 재구현 diff 3건.
