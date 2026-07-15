# Winters 엔진 도메인/구조 면접 대비 (INDEX)

> 목적: 내가 만든 Winters 엔진의 **모든 도메인 구조, 설계 결정과 트레이드오프, 구조적 에러 처리, 툴·협업 프로세스, 어려웠던 문제와 해결 전략, 향후 확장 구조**를 면접에서 설명하는 대본이자 지식 베이스.
> 각 도메인 챕터는 ① 도메인 한 줄 정의 → ② 구조·데이터 흐름 → ③ 설계 결정과 트레이드오프(왜→대안→선택→비용) → ④ 어려웠던 점과 해결 → ⑤ 향후 개선 → ⑥ 면접 Q&A 구조를 따른다.

## 챕터 목록

### 진입 — 소개와 뼈대
| # | 챕터 | 핵심 |
|---|------|------|
| 01 | [엔진 전체 개관 — 포트폴리오 소개](01_engine_overview.md) | 5계층 지도, 한 프레임의 여정, 설계 철학 5개, **1분/3분/10분 소개 스크립트**. "프로젝트 소개해주세요"의 완성 답안 |
| 02 | [레이어 아키텍처 · 의존성 경계](02_architecture_layers.md) | truth/presentation 이분법의 5계층, ProjectReference 비대칭·SDK 헤더 purge·PreBuild lint로 경계를 **빌드 수준에서 강제**, Phase 7F 어댑터 절단 서사 |

### 도메인 각론
| # | 챕터 | 핵심 |
|---|------|------|
| 03 | [렌더링 파이프라인 (RHI · DX11 · FX)](03_rendering_pipeline.md) | IRHIDevice 추상화 + DX12 parity 트랙, RenderWorldSnapshot 데이터 계약, generational handle, 본 팔레트 SRV, FX 이중 트랙 |
| 04 | [ECS · 게임 오브젝트 모델](04_ecs_gameobject.md) | sparse set ECS × OOP 씬 하이브리드, Phase=데이터 흐름 계약, 접근권 기반 병렬 배칭, Phase swap 사고의 교훈 |
| 05 | [리소스 · 에셋 파이프라인](05_resource_asset_pipeline.md) | FBX→.wmesh/.wskel/.wanim/.wmat 오프라인 쿠킹, 신뢰 경계 로더, skelHash 가드, 이원 수명 캐시, 17.8ms→9ms 회복 사례 |
| 06 | [씬 시스템 · 게임 루프](06_scene_gameloop.md) | IScene 계약과 이중 슬롯 Scene_Manager, CEngineApp::Run 프레임 구조, 결정적 씬 전환·fail-fast, 에디터 씬 분리 |
| 07 | [네트워크 · 서버 권위 · 복제](07_network_replication.md) | IOCP TCP 30Hz 고정 틱, 프레이밍/verify 신뢰 경계, Move 코얼레싱, full-snapshot vs edge 이벤트 이원화, TCP MVP→UDP 로드맵 |
| 08 | [GameSim · 챔피언 시뮬레이션 · 결정론](08_gamesim_champions.md) | 고정 tick·주입 RNG·정렬 순회 3대 축, 256×256 챔피언 훅 테이블, 데이터 3팩 분할 — **150챔프 스케일 업데이트 구조** |
| 09 | [AI · 내비게이션 · 이동](09_ai_navigation.md) | 비트팩 NavGrid A*·reachability 캐시, silent fail 금지 경로 API, 미니언 4단 폴백, MCTS/BT 2계층 봇 |
| 10 | [동시성 구조 · JobSystem](10_concurrency_jobsystem.md) | Chase-Lev work-stealing 3층 구조, Submit race·Profiler race 사고, **"병렬화를 끄는 의사결정"** — 동시성 버그를 구조로 배제하는 방법론 |
| 11 | [UI · 툴 · 에디터](11_ui_tools_editor.md) | ImGui를 RHI 뒤로 래핑, view-data 전용 HUD, Stage 왕복 맵 에디터, tune-then-bake 튜닝 툴, 툴 투자 기준 |

### 관점 챕터 — 질문의 축을 미리 장악
| # | 챕터 | 핵심 |
|---|------|------|
| 12 | [구조적 에러 처리 · 복원력 설계](12_error_resilience.md) | 예외 없는 반환값 모델, 신뢰 경계 verify, bounded trace, dead diagnostics 금지, 폴백 사다리. "에러 처리를 어떻게 설계했나"의 완성 답안 |
| 13 | [협업 · 프로세스 · 개발 문화](13_collaboration_process.md) | **"규칙은 문서에, 강제는 빌드에"** — gotchas 조직 기억, plan-first 훅, Codex 교차 리뷰 20건 반영, 2-머신 Work Packet 운영 |
| 14 | [어려웠던 문제들 — 문제 해결 전략 사례집](14_hard_problems_war_stories.md) | yaw saga · 미니언 stuck · Chase-Lev race · 가짜 async 등 **8개 실전 사고를 STAR+교훈** 골격으로. 관통 방법론: 관측 우선·데이터 계측·CPU/GPU 경계 분기 |
| 15 | [향후 업데이트 구조 · 기술 부채 · 확장 설계](15_future_roadmap_techdebt.md) | 실측 부채 대장 12건 + 상환 계획표, 150챔프 콘텐츠 추가 구조, Fiber 서버·MCTS/RL·에디터 로드맵. "다시 만든다면?" 대비 |
| 16 | [엔진/아키텍처 면접 질문 은행](16_interview_qa_bank_engine.md) | 소개 11 + 설계 24 + 트러블슈팅 17 + 압박·약점 12 = **64문항**, "첫 문장 결론 + 근거 file:line + 꼬리질문 대비" 골격 |

## 학습 로드맵

- **뼈대 먼저**: 01 → 02. 소개 스크립트와 레이어 지도를 몸에 새긴 뒤 각론으로.
- **지원 직군별 우선순위**:
  - 클라이언트: 03 → 04 → 05 → 06 → 11
  - 서버: 07 → 08 → 10 → 12
  - 공통(반드시): 09, 14
- **관점 챕터(12~15)는 늦게 읽지 말 것** — "에러 처리·협업·부채" 질문은 경력·인성 면접에서도 나오는 축이라, 각론보다 먼저 훑어도 좋다.
- **면접 전날**: 16 질문 은행 + 01의 소개 스크립트 + 14의 war story 2개(가장 자신 있는 것)를 소리 내어 리허설.

## 짝 문서

C++ 언어 자체(수명·소유권·다형성·템플릿·동시성 프리미티브)는 [`../cpp/00_INDEX.md`](../cpp/00_INDEX.md) 세트가 담당한다.
engine 세트의 모든 설계 서사는 cpp 세트의 언어 근거 위에 서 있다 — 면접에서 "왜?"가 두 번 이상 반복되면 cpp 세트의 해당 챕터로 내려가서 답하라.
