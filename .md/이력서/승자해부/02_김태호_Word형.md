# 승자 해부 02 — 김태호 / Word형 (#UnrealEngine #Multiplay #Word)

> 구성 원칙: **단일 주제 딥다이브** — "궁금해서 만들고 싶은 기능 → 만드는 과정 → 접근/기술 선택 이유 → 객관적 결과". 대주제와 개발 의도를 최상단 배치.

## 복원 — 구조와 내용

### 대주제
**[리플리케이션 그래프를 통한 최적화된 치팅 방지 시스템 (전장의 안개)]** (코드: Network / S1ReplicationGraph.h)

- 노드 그래프 다이어그램 직접 그림: S1 Replication Graph → Grid Spatialization 2D / Always Relevant(All) / Always Relevant(Connection) / DynamicSpatialFrequency_VisibilityCheck(Connection)

### 개발 의도 (최상단)
FPS 월핵 방지 시스템을 만들고 싶었는데 기존 Replicated 시스템으로는 문제가 있어 Replication Graph로 구현. "구현 방식 → 발생한 문제 → 해결 과정" 순서 예고.

### 1차 시도 — VisibilityCheck_ForConnection Node
- Replication Graph 선택 이유: 복제 조건 커스터마이징 가능 + 성능 고점
- 벽 뒤 액터 복제 차단은 동작했으나 서버 프레임 하락:
  1. N개 커넥션 상호 Tick당 4회 Raycast = **N×(N-1)×4회/프레임**
  2. Relevancy 변경마다 무거운 액터 Destroy/Spawn 부하
- 접근 전환의 사고: "시야에 없는 후방 액터까지 매번 검사할 필요가 있나?" → 스킵하면 유의미한 최적화

### 2차 시도 — DynamicSpatialFrequency_VisibilityCheck Node
- NetFrequency 소개 장면에서 힌트 → 내부 코드 분석·수정으로 자체 노드 제작
- **Dot Product로 후방 7~8 / 측면 5~8 / 전방 3~8 프레임마다 복제** — 복제될 프레임이 되어서야 Raycast → Raycast·복제 횟수 유동 감소, 플레이어 체감 없음
- Destroy/Spawn 오버헤드 → **Pause Replication** 활용: 복제되어도 패킷 미전송 = 실질 위치 노출 차단

### 결과 (객관 수치 — 이 포폴의 백미)
| | Raycast | 평균 프레임 |
|---|---|---|
| 기존 | 초당 4,000회 | 30 |
| 변경 후 | 초당 120~480회 | **36** |

### 엣지 케이스 2건 (문제 발견 → CS 지식 기반 해석 → 해결)
1. **코너 피킹**: 고지연에서 벽에서 빠르게 나오는 적이 허공에서 등장 / 늦게 등장 → 레이턴시 Factor 3개(서버 DeltaSeconds, 대상 속력, 네트워크 지연)로 **Bounding Box 확장 후 4끝점 Raycast** → 300ms + 대쉬에도 벽 뒤부터 미리 복제 시작. 코드: LatencyOffset, BoundingBox 4점
2. **컬링된 적의 MulticastRPC 가시성**: 벽 뒤 컬링된 적이 공격할 때 0.1초 보였다 사라짐 → "MulticastRPC는 NetCullDistance 밖이면 실행 안 된다"는 규칙을 가시성 컬링에도 확장해야 한다고 판단 → Raycast 후 상호 가시성 관계를 해시맵 기록, **ProcessRemoteFunction() override**로 컬링한 커넥션에는 Actor Channel을 열지 않아 RPC 차단

### 면접 연결
면접관이 "이 접근의 역효과는?"을 물었음 — 정답 없는 즉석 질문이라 **기술의 완벽한 이해 기반 trade-off 설명**이 요구됨.

### 명시된 포인트 5개
1. 하나의 주제 선정, 개발 과정을 논리적으로 소개
2. 결과 단계에서 개선 수치 제시로 신뢰도 확보
3. 자소서 내용의 객관적 근거가 되도록 구성
4. 역질문 대비 — 사용 기술의 장단점 파악
5. 개발에 정답 없음 — 기술 선택의 본인만의 논리

## 분석

### 장점 (훔칠 것)
- **전 포폴 중 유일하게 Before/After 수치 표** — 4000→120~480 Raycast, 30→36fps. 신뢰도의 원천
- 문제 발견을 CS 지식으로 해석하는 구조 (MulticastRPC 규칙 → 커스텀 라우팅)
- 엔진 내부 코드 분석·수정 능력의 증명 (ProcessRemoteFunction override)
- "클라 지망인데 서버 집중" — 사용자 판단대로 프로그래머의 증명 지점은 최적화·서버 문제 해결

### 단점 (배울 것 — 사용자 주석)
- **"서버 프레임이 꽤 내려갔고", "적지 않은 부하"** — 최종 결과 표 외의 중간 서술이 주관 표현. 면접관 입장에서 매우 거슬림 → **모든 문제 서술 단계부터 수치로** (내 포폴 원칙: 계측 없으면 주장 없음)
- 단일 주제라 폭이 없음 — 다른 프로젝트가 안 보임

### 내 포폴 적용 (사용자 주석 반영)
- Winters의 동형 자산이 이미 있음: **팀 시야(FOW) 기반 스냅샷 필터링 + Lag Compensation + 클라 예측 보호** — 김태호가 UE 위에서 만든 것을 나는 엔진 바닥부터 만들었다는 프레이밍
- 나의 Before/After 수치 후보: 17.8ms→9ms(계측 근거 있음), 300틱 해시(무회귀 증명). **추가 확보 대상**: FOW 필터로 준 스냅샷 바이트/패킷 수 비교 — 계측만 하면 김태호식 표가 나옴
- ECS·JobSystem·Fiber 서술 계획은 **구현·계측 완료분만** (JobSystem race 미해결/Fiber 미착수 상태에서 기재 금지 — FAULT 원칙)
