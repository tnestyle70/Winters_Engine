# C++ 면접 대비 — Winters로 증명하기 (INDEX)

> 목적: 게임업계 C++ 면접(신입~주니어)에서 **개념을 정확히 설명하고, 내가 만든 Winters 엔진 코드로 증명**한다.
> 모든 챕터는 ① 한 줄 본질 → ② 기본 개념 → ③ 심화(꼬리질문) → ④ Winters 적용(path:line 인용) → ⑤ 면접 Q&A → ⑥ 흔한 오답 구조를 따른다.

## 챕터 목록

| # | 챕터 | 핵심 |
|---|------|------|
| 01 | [C++의 본질과 철학](01_cpp_essence.md) | 제로 오버헤드 추상화 · 결정적 소멸 · 값 의미론 세 축. "C++을 어떻게 이해하나"라는 첫 질문의 완성 답안 — 팬텀 타입 핸들, ComPtr vs raw, POD 컴포넌트, /fp:precise 결정론으로 증명 |
| 02 | [컴파일 모델 · 링크 · DLL 경계](02_compile_link_dll.md) | 번역 단위 · ODR · 링커 동작부터 dllexport/import lib/C4251까지. Engine DLL 경계의 실제 사고(unique_ptr copy-delete, stale lib)로 증명 |
| 03 | [메모리 · 객체 수명 · RAII](03_memory_lifetime_raii.md) | 가상 주소 공간, storage duration, placement new, 정렬, 소멸 순서. GPU 리소스·COM·스레드 수명을 RAII로 묶는 법 |
| 04 | [포인터 · 참조 · 스마트 포인터](04_pointers_smart_pointers.md) | 소유(unique/shared)와 관찰(raw)을 **타입으로 설계**하는 법. 리소스 캐시·팩토리·pimpl·IOCP 세션·CHttpClient this 캡처 수명 사고 |
| 05 | [클래스 설계 · 값 의미론 · 복사/이동](05_class_design_value_semantics.md) | 특수 멤버 6종, Rule of 0/3/5, 이동 의미론, RVO. dllexport+unique_ptr 복사 delete 규칙, 가짜 이동 생성자 함정 |
| 06 | [다형성 · 가상 함수 · 인터페이스 설계](06_polymorphism_virtual.md) | vtable/vptr 메커니즘과 비용, 가상 소멸자, CRTP. "경계에는 virtual, 데이터에는 배제" — IScene/RHI 어댑터/챔피언 훅 테이블 |
| 07 | [템플릿 · 제네릭 프로그래밍](07_templates_generic.md) | 인스턴스화 모델(왜 헤더에), SFINAE→if constexpr→concepts, perfect forwarding. CWorld DLL 경계 분할, RHIHandle 팬텀 타입 |
| 08 | [STL 컨테이너 · 캐시 · 데이터 지향 설계](08_stl_containers_cache.md) | "빅오가 아니라 캐시가 컨테이너 선택 기준". vector 내부, 반복자 무효화, false sharing, AoS/SoA — sparse-set, 비트팩 NavGrid, alignas(64) deque |
| 09 | [동시성 · 멀티스레딩 · 비동기](09_concurrency.md) | 데이터 레이스 정의, memory ordering 4단계, Chase-Lev work-stealing, future 소멸자 함정, IOCP. 직접 겪은 동시성 사고 3건과 해법 |
| 10 | [에러 처리 전략](10_error_handling.md) | 예외 메커니즘·안전성 3단계·noexcept와, 게임 엔진이 반환값 모델을 택하는 이유. Create() 팩토리, ePathFindResult, verify bounded trace |
| 11 | [아키텍처 · OOP vs ECS · 레이어 경계](11_architecture_ecs.md) | 상속 트리의 조합 폭발을 ECS로 푸는 법, Phase 실행 순서=데이터 흐름, Shared→Engine 어댑터 절단, 서버 권위 결정론 스냅샷 |
| 12 | [네트워크 · 직렬화 · C++](12_network_serialization.md) | 신뢰 불가 바이트가 C++ 객체가 되는 경계 — TCP 프레이밍, 패딩/엔디안/포인터 함정, FlatBuffers verify, Move 코얼레싱 |
| 13 | [면접 질문 은행 — Winters로 답하기](13_interview_qa_bank.md) | 기초 32문 + 심화 꼬리질문 27문 + STAR 경험담 12건 = **71문항**, 전부 검증된 path:line 인용 |

## 학습 로드맵

- **1회독 (개념 정립)**: 01 → 03 → 04 → 05 → 06 순서. C++의 척추(수명·소유권·다형성)를 먼저 세운다.
- **2회독 (엔진 프로그래머 차별화)**: 08 → 09 → 02 → 07. 캐시·동시성·빌드 시스템은 "게임 엔진 만들어 본 사람"만 깊게 답할 수 있는 영역.
- **3회독 (아키텍처 연결)**: 10 → 11 → 12. 언어 기능이 아키텍처 결정으로 이어지는 서사를 만든다.
- **면접 전날**: 13 질문 은행만 빠르게 회독. 각 답변의 첫 문장(= 각 챕터 "한 줄 본질")을 소리 내어 말해볼 것.

## 짝 문서

엔진 도메인/구조/협업/문제해결 관점은 [`../engine/00_INDEX.md`](../engine/00_INDEX.md) 세트가 담당한다.
이 세트(cpp)는 **언어**를, engine 세트는 **시스템 설계와 경험 서사**를 증명한다. 면접에서는 둘을 교차 인용하라.
