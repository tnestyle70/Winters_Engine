Session - 소비자 0인 Engine CStatusEffectSystem을 은퇴시킨다 (상태 시스템 3벌 → 2벌 정상화).

```text
좌표: 흔적 기관 은퇴 #2 · 축: C8·C7
관련: 2026-07-17_LEGACY_SCENE_PIPELINE_RETIREMENT_PLAN.md (같은 판결 큐)
상태: 적용 완료 — 빌드/런타임 검증은 사용자 수행 대기
```

## 1. 결정 기록

```text
① 상태 시스템이 3벌 존재: 진실=Shared/GameSim/Systems/StatusEffect(서버 틱·SimLab 게이트),
   표현=Client CLocalStatusEffectSystem(무조건 등록), Engine CStatusEffectSystem=생성 호출부 0.
② 순진한 해법 = 방치. 비용 = 아키텍처 설명 시 "상태는 어디서 도나"의 3중 답변 혼동 +
   클라 6개 TU의 스테일 include가 잘못된 의존 신호.
③ blame 감정: 최초 커밋(e6ded62 "Initial Clean Winters Engine") 출생, 이후 무변경 —
   수업 프레임워크 시절 원형. Chesterton 통과(사연 없음).
④ Shared·Client 상태 시스템은 무접촉 — 이 슬라이스는 순수 Engine 유물 제거.
⑤ 잃는 것: 없음(실행된 적 없는 코드). 틀리는 순간: Engine 범용 상태 시스템이 필요한
   신제품(Elden)이 생기면 — 그때는 이 유물 부활이 아니라 요구 기준 신설이 맞다.
```

## 2. 반영해야 하는 코드 (적용 완료 기록)

```text
파일 삭제(git rm): Engine/Public/ECS/Systems/StatusEffectSystem.h
                   Engine/Private/ECS/Systems/StatusEffectSystem.cpp
Engine/Include/Engine.vcxproj          — ClCompile/ClInclude 항목 2건 제거
Engine/Include/Engine.vcxproj.filters  — 필터 블록 2건(6줄) 제거
클라 스테일 include 1줄씩 제거 (사용 0 검증됨):
  Client/Private/Scene/Scene_InGame.cpp · Scene_InGameInput.cpp · Scene_InGameLocalSkills.cpp
  · Scene_InGameMapNav.cpp · Scene_InGameNetwork.cpp · Scene_InGameRender.cpp
```

## 3. 검증 — 예측을 먼저 쓴다

```text
예측:
- Engine·Client Debug|x64 빌드 성공 (참조 잔존 0 스윕 완료 — 깨지면 컴파일로 깨진다).
- Engine 빌드 후 UpdateLib.bat이 PostBuildEvent로 자동 실행되어 EngineSDK/inc의
  StatusEffectSystem.h 사본 처리 — 수동 실행 불필요 (Engine.vcxproj:74-77 확인).
- 런타임 무변화: 클라 상태 표현은 CLocalStatusEffectSystem, 서버 진실은 Shared 쪽이
  계속 담당 — 스턴/버프 동작 동일. SimLab 골든 불변(Shared 무접촉).

검증 명령 (사용자):
- Engine → Client 빌드, 인게임 스턴/버프 1회 육안, SimLab.exe 해시 동일 확인.

확인 필요:
- UpdateLib.bat이 EngineSDK/inc의 삭제 파일 사본을 지우는지(복사 전용이면 스테일 사본
  잔존 — 무해하나 다음 정리 때 삭제).
```
