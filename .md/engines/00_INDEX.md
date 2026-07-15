# .md/engines — 엔진 본질·비교·마켓·최적화 참조 문서군

작성일: 2026-07-10. Winters(자체 DX11 엔진)를 축으로 Unreal Engine 5.7 / Unity 6를 대조하는 4부작 참조 문서. 기술면접 대비(.md/interview/)와 3축 툴 포트폴리오(UE+Unity+Winters 어빌리티 타임라인 에디터, Fab/Asset Store 출시)의 공통 기반이다. 웹 검증 사실은 출처 도메인 병기, 일반 공학 지식은 (일반 지식), 미확인은 (미검증) 표기 규칙을 4부 공통으로 따른다.

## 읽는 순서

1. [01_GAME_ENGINE_ESSENCE_BOTTOM_UP.md](01_GAME_ENGINE_ESSENCE_BOTTOM_UP.md) — 게임 엔진의 본질을 L0(플랫폼/메인루프/메모리)부터 L10(배포/백엔드)까지 층층이 쌓아 올리며, 각 층마다 Winters 실물 경로 + UE/Unity 대응물을 매핑. Winters 도메인(Client/Server/Engine/EngineSDK/Shared GameSim/Tools/Services/docker) ↔ UE ↔ Unity 총괄표 포함.
2. [02_THREE_ENGINES_CHARACTERISTICS.md](02_THREE_ENGINES_CHARACTERISTICS.md) — 세 엔진의 개별 정체성과 설계 철학: UE(UObject 리플렉션/모듈시스템/Slate/Blueprint/Niagara), Unity(C++코어+C#래퍼/YAML+GUID/ScriptableObject/DOTS/SRP), Winters(30Hz 결정론 GameSim/trivially-copyable ECS/.w* cook+Validator). 각 엔진이 "포기한 것"으로 마무리.
3. [03_MARKETPLACES_TOOLS_COLLABORATION.md](03_MARKETPLACES_TOOLS_COLLABORATION.md) — Fab(88/12, 소스 제출→Epic 버전별 컴파일)과 Unity Asset Store(70/30, ~10영업일 심사)의 원리·출판 절차·업데이트 계약·협업 규율, 이중 스토어 전략과 어빌리티 타임라인 에디터의 시장 포지션.
4. [04_OPTIMIZATION_ESSENCE_VFX_ANIMATION.md](04_OPTIMIZATION_ESSENCE_VFX_ANIMATION.md) — 최적화의 본질(계측→범인 확정→최소 수정, 예산 사고, 데이터 형태)과 Niagara/VFX·애니메이션 최적화, 3엔진 프로파일링 도구 지도. Winters 17.8ms→9ms 실증 사례 축.

## 자매 문서

- `.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` — UE/Fab 툴 생태계의 Winters 수용 설계(원본 방향 문서)
- `.md/interview/tool-development.md` — 툴/에디터 기술면접 대비(실물 경로 근거)
- `.md/plan/EffectTool/` — WFX 노드 그래프 이펙트 시스템 29부작 설계