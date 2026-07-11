# 이력서 — [이름 CONFIRM_NEEDED]

**게임 클라이언트 / 서버 프로그래머 (신입)**

- 연락처: [전화 CONFIRM_NEEDED]
- 이메일: [CONFIRM_NEEDED — tnestyle70@gmail.com 또는 cidcid8564@gmail.com 중 확정. 기존 초안(resume.md §10)은 cidcid8564로 기재됨]
- GitHub: https://github.com/tnestyle70
- 포트폴리오 영상 (2분): [URL — video/2분영상_스크립트.md 로 제작 후 기입]
- Steam 출시작: https://store.steampowered.com/app/2014540/Liberation/

---

## 요약

C++20 자체 DX11 엔진 위에 **서버 권위(authoritative) 멀티플레이 MOBA**를 혼자 설계·구현하고 있는 신입 프로그래머입니다. 5인 팀 프로젝트에서 IOCP 서버와 20TPS 스냅샷 동기화를 담당(전체 커밋의 64%)했고, 이를 개인 프로젝트에서 30Hz 고정 틱·커맨드 검증·클라 예측·결정성 해시 검증까지 확장했습니다. 2023년 Steam에 공포 게임을 상용 출시(개발 명의 WINTERS)한 완성 경험이 있습니다.

## 기술 스택 (숙련도 — 근거 프로젝트)

- **C++20**: 주력. 자체 엔진 2종(DX11/DX9 DLL 구조), WinAPI 게임 — Winters(5만+줄), MC Dungeons(12만 줄 중 64% 기여), Starcraft(5.4만 줄)
- **네트워크**: IOCP 비동기 서버, TCP, 커스텀 바이너리 프로토콜, FlatBuffers, 서버 권위 커맨드/스냅샷 동기화, 랙 컴펜세이션 — MC Dungeons(20TPS), Winters(30Hz)
- **그래픽스**: DirectX 11(자체 RHI 추상화, DX12 실험 경로), DirectX 9, HLSL(포스트프로세스), GDI 소프트웨어 합성 — Winters, MC Dungeons, Starcraft
- **엔진 시스템**: ECS(sparse-set, Phase 스케줄러), 자체 바이너리 에셋 포맷+컨버터, ImGui 툴링(에디터/프로파일러), FMOD — Winters
- **협업/품질**: Git PR 협업(135건 머지 관리), 결정성 해시 회귀 검증, CPU 프로파일러 계측 기반 최적화, 경계 lint 스크립트
- **기타**: Go 마이크로서비스(PostgreSQL/Redis/JWT), Python

## 프로젝트

### Winters — 자체 C++20/DX11 엔진 + LoL 구조 MOBA (개인, 2026.04 ~ 진행 중)

- LoL 구조의 5:5 MOBA를 **서버 권위 파이프라인**(Client Input → GameCommand → Server GameSim → FlatBuffers Snapshot/Event → Client Visual)으로 구현 — 챔피언 15종 서버 시뮬레이션, 30Hz 고정 틱, IOCP 워커 4+틱 1 스레드
- 스키닝 갱신이 프레임의 90%(16ms)임을 **자체 CPU 프로파일러로 계측 확정** 후 제거 → 프레임 17.8ms → 9ms (~110fps)
- 108개 파일 리팩터링을 **300틱 시뮬레이션 해시 비교**로 무회귀 증명 (리플레이·결정성 하니스 직접 제작)
- Assimp 기반 오프라인 컨버터로 FBX/GLB 27종 → 자체 `.wmesh` 바이너리 전수 변환(FAIL 0), 대표 메시 60MB → 1.2MB(~50배), POD 고정 레이아웃 zero-copy GPU 업로드
- 클라 예측-서버 보정 충돌을 예측 보호 규약(스냅샷 12개 한도)으로 해소, 봇 AI까지 GameCommand 생산자로 통일해 권위 단일화
- Go 마이크로서비스 백엔드(인증/매치메이킹/전적 — PostgreSQL/Redis/JWT), ImGui 에디터·이펙트 튜너·F3 프로파일러 툴 체인

### Minecraft Dungeons 모작 — 5인 팀, 자체 DX9 엔진 (2026.03 ~ 04, 4주)

- 자체 DX9 엔진(DLL)+클라이언트+전용 서버+UI 에디터 4프로젝트 구조(약 12만 줄), **GitHub PR 135건 기반 협업 — 본인 220/342 커밋(64%), 레포 소유·머지 관리**
- **[담당] IOCP 비동기 게임 서버 + 20TPS 고정 틱 스냅샷 브로드캐스트** — 커스텀 바이너리 패킷 20여 종(스폰/입력/상태/보스 동기화/사망 동기화)으로 멀티플레이 구현
- [담당] 엔더드래곤 보스전(패턴·연출·전용 동기화 패킷), ImGui 인게임 맵 에디터(멀티 스테이지, 트리거 기반 스폰/씬 전환), 파티클 시스템, HLSL 포스트프로세스 6종, 쿼드트리 프러스텀 컬링

### Starcraft 모작 — WinAPI/GDI, 라이브러리 없이 (개인, 2025.12 ~ 2026.01)

- RTS 코어를 엔진 없이 구현(5.4만 줄): **Octile A\*(코너컷 방지·타일 비용) + Separation/Cohesion 스티어링 결합 군집 이동** — 전역 경로탐색과 지역 조향의 역할 분리
- 3상태 전장의 안개를 dirty-flag 캐시 DC로 최적화, GDI 비트연산(SRCAND/SRCPAINT)+AlphaBlend 3단 합성으로 팀컬러/반투명을 셰이더 없이 구현
- `deque<Order>` 명령 큐 + Commandable 인터페이스로 유닛(32종)·건물(40종) 공용 커맨드 계약, 인게임 맵 에디터 → 바이너리 저장 → 내비 그리드 재생성 파이프라인

### Liberation — Steam 상용 출시 공포 게임 (2023.06 출시)

- 전략 생존 공포 게임을 Steam에 상용 출시(₩5,600, 개발/배급 명의 WINTERS) — 스토어 심사·빌드 패키징·출시 운영 경험 [스택/기간/역할 CONFIRM_NEEDED]

## 교육

- [CONFIRM_NEEDED — 학력/교육과정. 게임 아카데미 과정이면 과정명·기간 표기]

---
---

# (제출 전 삭제) 운영 부록

## A. 제출 전 확정 체크리스트 — 이것만 채우면 제출 가능

1. [ ] 이름 / 전화 / 이메일 확정 (이메일 2개 중 택1 — GitHub 계정과 일치 권장)
2. [ ] 교육 이력 1줄
3. [ ] Winters 기간 표기 방식 ("2026.04~" 확인됨, 시작점 본인 확정)
4. [ ] MC Dungeons 역할 공식 명칭 ("서버/네트워크·보스전 담당" — git 근거 있음, 본인 확정만)
5. [ ] Liberation 스택·기간·역할 (전 항목 본인 기억 의존 — weapons/01 참조)
6. [ ] 2분 영상 제작 → URL 기입 (video/ 스크립트)
7. [ ] GitHub 30분 정리 (pages/index.md 체크리스트 — 핀 4개·프로필 README·레포 설명)

## B. 직무 3변형 가이드 (같은 본문, 요약·순서만 조정)

- **클라이언트 지원**: 요약에 "자체 엔진·렌더링" 선행. Winters 불릿에서 프로파일러 최적화·에셋 파이프라인·RHI를 위로, 서버 불릿은 유지(차별화 요소)
- **서버 지원**: 요약에 "IOCP·서버 권위" 선행. MC Dungeons를 두 번째 유지하되 [담당] 서버 불릿을 첫 줄로, Winters 30Hz 틱/랙컴펜/결정성 해시 강조
- **엔진/툴 지원**: ECS·RHI·에셋 포맷·에디터 툴 불릿 선행, UE 5.7 소스 매핑 문장 추가 가능

## C. 작성 원칙 (합격 패턴 리서치 반영 — 근거는 워크플로 market 보고서)

- 모든 불릿 = 문제→해결→수치 구조 (합격 후기 공통 패턴)
- 연봉 정보·가족사항·주민번호 기재 금지 (네오위즈 명시 규정)
- 기술 나열 금지 — 반드시 프로젝트 근거 병기 ("깊이 없는 나열"이 감점 1순위)
- 과장 0: 모든 주장은 weapons/ 카드의 코드 경로·URL로 역추적 가능 (FAULT 없는 제출봇)
