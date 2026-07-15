# 기술면접 대비 문서 인덱스

목적함수: **게임회사 취업 (클라이언트/서버 프로그래머) → 이후 창업.**
전 문서 한국어, 총 **12개 문서 / 598문항**. 모든 답변은 "정의·원리 → 왜/트레이드오프 → 내 프로젝트(Winters 등) 실전 연결 → 함정/꼬리질문" 구조.

## 문서 목록

| 문서 | 질문 수 | 핵심 내용 |
|---|---|---|
| [cpp.md](cpp.md) | 34 | 컴파일/링크 모델, vtable, RAII, 스마트포인터 내부, 이동 의미론, STL 무효화, DLL 경계 (dllexport+unique_ptr gotcha 실전) |
| [os-systems-programming.md](os-systems-programming.md) | 39 | 프로세스/스레드, 동기화 전 계열, 데드락, atomic/memory_order, 가상메모리, IOCP, Fiber, thread_local race 실전 (Profiler 2-pass) |
| [computer-architecture.md](computer-architecture.md) | 37 | 파이프라인/분기예측, 캐시 계층/false sharing, TLB, SIMD(DirectXMath), IEEE754와 /fp:precise 결정론, ECS-DOD 연결 |
| [data-structures-algorithms.md](data-structures-algorithms.md) | 35 | 복잡도와 캐시 상수항, 해시/트리/힙, introsort, BFS/DFS/다익스트라/A*(octile 휴리스틱 실코드), Chase-Lev deque race 실전, 코테 전략 |
| [network.md](network.md) | 35 | TCP/UDP, handshake, 혼잡제어, Nagle, IOCP 모델 비교, 패킷 프레이밍, FlatBuffers verify, 서버 권위/스냅샷/예측/보간, lag compensation |
| [database.md](database.md) | 35 | SQL/조인, B+tree 인덱스, ACID/격리수준, 락 vs MVCC, Redis 랭킹, 게임 DB 스키마/트랜잭션/샤딩 |
| [graphics-dx11.md](graphics-dx11.md) | 35 | DX11 전 스테이지, 리소스/뷰/USAGE, 상수버퍼 전략, 드로우콜/인스턴싱, Z-fighting 실전, 17.8ms→9ms 최적화 실전, RHI 추상화 |
| [math-graphics-physics.md](math-graphics-physics.md) | 183 | 게임 수학 전 영역 11개 도메인: 선형대수/회전(쿼터니언·yaw 규약)/변환 파이프라인/셰이딩/텍스처/스키닝/A*·스티어링/스냅샷 보간/MCTS·UCB1/충돌·피킹/시야 판정 |
| [winters-engine.md](winters-engine.md) | 35 | 포트폴리오 스토리텔링: 5계층 아키텍처, 서버 권위(GameCommand 프로듀서 — Bot AI는 게임플레이 진실을 직접 변경하지 않음), ECS Phase, 3분/10분 발표 시나리오, 공격 질문 35 |
| [tool-development.md](tool-development.md) | 35 | ImGui 즉시모드, 에디터-런타임 분리, 에셋 쿠킹(.wmesh), 바이너리 직렬화/버전, Undo/Redo, 이펙트 툴 설계, UE 에디터 비교 |
| [experience-projects.md](experience-projects.md) | 62 | 마크 던전스 협업 / 스타 WinAPI / Liberation 출시 답변 프레임 + 행동면접(STAR) — 본인 사례 채움 슬롯 46곳 |
| [resume.md](resume.md) | 33 | 이력서/자소서/포트폴리오 전략 + 실측 수치 기반 초안 전문 — 채움 슬롯 28곳 |

## 추천 학습 순서 (면접까지 시간 역순 배분)

1. **winters-engine.md** — 내 이야기가 먼저다. 3분/10분 발표가 모든 면접의 축.
2. **cpp.md + os-systems-programming.md** — 게임사 기술면접 최다 출제 축.
3. **math-graphics-physics.md + graphics-dx11.md** — 클라이언트 직군 핵심.
4. **network.md** — 서버 직군 핵심 (클라 지원이어도 서버 권위 구조는 필수).
5. **data-structures-algorithms.md + computer-architecture.md** — 코테/CS 기본기.
6. **database.md** — 서버 지원 시 우선순위 상향.
7. **experience-projects.md + resume.md** — CONFIRM_NEEDED 슬롯을 본인 사실로 채우는 작업 필요 (아래).

## 채워야 할 것 (CONFIRM_NEEDED)

experience-projects.md(46곳)와 resume.md(28곳)에 본인 확인이 필요한 슬롯이 있다. 핵심 8개:

1. 마크 던전스 모작 — 개발 기간 / 팀 규모 / 본인 담당 파트 / 사용 엔진 / 핵심 구현
2. 스타크래프트 WinAPI 모작 — 기간 / 구현 범위(길찾기·유닛 AI 등) / 코드 보존 여부
3. Liberation — 팀/개인 여부, 장르, 사용 기술, 본인 기여, 성과
4. Winters 총 개발 기간 (이력서 기간 표기)
5. 교육 이력 (기관/전공/기간)
6. GitHub 저장소 URL 및 공개 범위
7. 포트폴리오 영상 URL
8. 1지망 직군 확정 (클라이언트 vs 서버)
