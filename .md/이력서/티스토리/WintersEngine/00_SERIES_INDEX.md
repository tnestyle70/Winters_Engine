# Winters Engine 해부기 - 시리즈 인덱스

이 시리즈의 목적은 Winters Engine의 기술 스택을 단순히 나열하는 것이 아니다.

핵심은 다음 질문에 답하는 것이다.

> 기존 학습용/레거시 엔진의 한계를 어떻게 문제로 정의했고, Winters Engine에서는 그 문제를 어떤 코드 경계와 검증 파이프라인으로 해결했는가?

## 시리즈 전체 관점

Winters Engine은 LoL형 MOBA와 Elden형 Action RPG를 하나의 C++ 런타임 위에 분리해 올리기 위한 자체 엔진 프로젝트다.

중심 구조:

```text
WintersEngine.dll
├─ WintersLOL.exe      // MOBA product client
└─ WintersElden.exe    // Action RPG product client

Client Input
-> GameCommand
-> Server GameSim
-> Snapshot/Event
-> Client Visual
```

이 구조를 통해 보여주고 싶은 역량은 다음이다.

- 기능 구현 이전에 구조적 병목을 문제로 정의하는 능력
- Engine / Client / Shared GameSim / Server / Tools 책임 분리
- DX11 concrete renderer를 RHI와 RenderWorldSnapshot으로 분리하는 설계
- 서버 권위 gameplay와 Client presentation의 경계 설계
- JSON authoring과 runtime definition pack의 데이터 소유권 분리
- audit, build, deterministic regression을 묶은 검증 루프 운영

## 글 목록

1. `01_왜_자체_엔진을_만들었나.md`
2. `02_Product_Client_Separation.md`
3. `03_Engine_Runtime.md`
4. `04_RHI.md`
5. `05_RenderWorldSnapshot.md`
6. `06_ECS_Runtime_Systems.md`
7. `07_Server_Authority_GameSim.md`
8. `08_Snapshot_Replication.md`
9. `09_DataDriven_DefinitionPack.md`
10. `10_Champion_Skill_AI.md`
11. `11_FX_UI_Debug_Observability.md`
12. `12_Asset_Resource_Pipeline.md`
13. `13_WorldPartition_Elden_Direction.md`
14. `14_Verification_Pipeline.md`
15. `15_Collaboration_Documentation_Pipeline.md`

## 글 하나의 고정 구조

각 글은 다음 흐름으로 작성한다.

```text
문제 정의
-> 왜 단순 구현으로는 부족한가
-> Winters의 구조적 접근
-> 실제 코드 근거
-> 검증 방법
-> 이력서 문장
```

## 공개 전 체크리스트

- 원본 상용 에셋 경로, 비공개 리소스, 개인 토큰이 노출되지 않았는가?
- 아직 미완성인 기능을 완료형으로 과장하지 않았는가?
- 코드 전체 복붙보다 핵심 타입/흐름만 보여주는가?
- 이 글을 이력서 bullet 하나로 압축할 수 있는가?
- "무엇을 만들었다"보다 "무엇을 문제로 봤다"가 먼저 보이는가?

