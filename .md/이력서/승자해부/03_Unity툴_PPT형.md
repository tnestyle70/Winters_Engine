# 승자 해부 03 — Unity 툴 개발자 / PPT형 (#Unity #Tool #C++ #PPT)

> 구성 원칙: **목차로 면접관이 읽고 싶은 것만 골라 읽게** — "면접관의 시간을 최대한 덜 뺏겠다".

## 복원 — 구조와 내용

### 목차 (페이지 번호 명시)
1. **RTD** — 3D 로그라이크 조립식 타워디펜스 (Unity/C#) 인벤토리, Nested class inspector, 각종 에디터 툴 — p.3~17
2. **Packit List** — 3D 물리기반 퍼즐 (Unity/C#) 메쉬 생성기, 카메라 컨트롤러 — p.18~34
3. **SuperJellyMaker** — 2D 물리기반 머지 (Unity/C#) 물리 이슈 해결
4. **ASH** — 2D 퍼즐 플랫포머 (Unity/C#) Level graph 에디터, Input Manager, 그래픽 효과
5. **Ramensoup** — 3D GUI 게임 엔진 (C++/OpenGL) EventQueue, Relationship component — p.36~41
6. **RamenNetworking** — 서버/클라 라이브러리 (C++/winsock) 소켓 추상화, lock-free 메시지 큐 — p.42~46
7. 기타 이력

### 프로젝트 헤더 블록 규격 (RTD 예)
플랫폼 / 장르 / **개발 인원(2)** / 기간(2024.02~진행중) / 도구 / **영상 링크** / 요약 / **담당 내용** (C# event 기반 전 UI, 중첩클래스 인스펙터·구글 스프레드시트 로더·메쉬 슬라이서 툴, 장비 부착 외형 변화 시스템)

### 항목 서술 구조 (개발 배경 → 해결 → 한계)
- **ObservableDenseInventory**: List 관리 → 조립창 복잡화로 ObservableList 이벤트 기반 전환 → Add/Remove만으로는 부족(첫 획득/소진 시 UI 생성·삭제 분기) → 코드 + 코드 옆 설명(구독/이벤트별 UI 변화)
- **한계 및 개선점을 정직하게 기재**: "List<Entry> 전체 순회 → Dictionary 인덱스 필요", "Sorting 시 Observable 콜백 미지원" — 쓰기 불편하거나 대응 안 되는 예외를 스스로 적음
- **Mesh Slicing Tool**: 2인 팀이라 외부 에셋 사용 → 한 메쉬에 장갑/신발/갑옷 합쳐짐 → Sphere/Box로 정점 선택, Normal/UV/Bone 보존, **잘린 구멍을 한 번만 사용된 edge의 fan triangle로 메움** — 그래픽스 지식으로 해결했음을 명시
- **Ramensoup (자체 엔진)**: premake+git submodule 구성 / OpenGL 추상화 렌더 파이프라인 + assimp / glfw+imgui GUI 에디터 / **entt ECS + yaml-cpp Scene serialization**. 에디터 스크린샷(Scene View/Game View/Hierarchy/Inspector/TimeProfiler)
- **EventQueue**: 콜백 대신 이벤트큐 — 매 프레임 생성·삭제라 **동적 할당 없이 연속 공간 보관** (템플릿 push/Pop 코드) — 언어 숙련도+CS 이해 어필
- **기타 이력**: GMTK GameJam 상위 20%, 넥슨 대학생 게임잼 우수상, 캡스톤, 덱빌딩 개인작 — 다양성 어필

### 명시된 포인트 4개
1. 좋아하고 잘하는 것을 다양한 개발 경험으로 소개
2. 목차로 면접관이 관심 주제를 빠르게 확인
3. 프로젝트 개요에 개발 인원·수행 역할 필수
4. 지원 프로젝트의 엔진 경험이 없으면 기반 지식 숙련도 강조

## 분석

### 장점 (훔칠 것)
- **목차 + 페이지 번호** — 면접관 존중이 구조로 표현됨
- **한계 및 개선점 섹션** — 정직함이 신뢰가 됨 (아는 것과 모르는 것의 경계를 스스로 그음)
- 헤더 블록 규격 (인원/기간/역할/영상)
- 자체 엔진(Ramensoup)을 "API 디자인·추상화·책임 분할 공부"로 프레이밍 — 엔진 = 학습의 증거
- 툴 개발 비중 — "생산성을 높이는 개발자" 신호

### 단점 (배울 것 — 사용자 주석)
- **PPT의 영상 링크는 눌러야 재생** — Notion처럼 하이라이트 영상이 바로 보이는 형식이 우위 → "Notion이 가장 끌린다"
- 수치 데이터 부재 (형식은 최고, 증거는 약함)

### 내 포폴 적용 (사용자 주석 반영)
- 목차 + 페이지(섹션) 구조 채택, 각 프로젝트 헤더 블록 규격화
- **한계 및 개선점을 전 프로젝트에 넣는다** — 예: Winters "JobSystem은 Chase-Lev race 수정 전 비활성 상태", 스타 "전략 AI 매니저 부재" — 이 정직함이 곧 FAULT 없는 제출봇
- 자기소개 서사(사용자 초안): 문과 → 모바일 앱 중단 → **Liberation 1인 출시 (성공 믿음 → 실패 경험) → "다른 사람들과 같이 만들어야 한다" → 쥬신 아카데미 → WinAPI 스타 → DX9 팀협업(git 세팅부터 리소스 일정·갈등 조율) → Winters Engine** ("학원 프레임워크의 한계 → 세계 최고 기술을 결합해 UE/Unity와 객관 비교해도 경쟁력 있는 엔진") — 회고와 현재 선택을 동시에 제시. 기업가의 꿈은 솔직하게
