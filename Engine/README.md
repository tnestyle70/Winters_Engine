# Engine

원자: EngineCapability

Engine은 제품을 모르는 실행 능력을 소유한다.

소유:
- platform, window, frame loop
- RHI, renderer, resource runtime
- input, audio, UI primitive
- profiler, debug, capture primitive
- 제품 중립 runtime primitive

소유하지 않음:
- champion, skill, item, quest, boss, minion, turret
- 제품 HUD, 제품 UI rule
- server authority
- 기획 수치
- QA, marketing, live ops 산출물

구조 규칙:
- 기존 `Engine/` 폴더명과 구조를 유지한다.
- 게임 명사가 필요하면 Engine 밖의 ProductPresentation, GameplayTruth, GameDesignSource로 보낸다.
- Engine public header 변경 후에만 SDK 동기화를 검토한다.
