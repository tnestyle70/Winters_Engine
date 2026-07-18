Session - 수업 Blueprint 파이프라인(소비자 소멸)과 StaticScene(호출부 0)을 은퇴시키고 챔피언 스폰을 데이터 주도 경로로 단일화한다.

```text
좌표: 신규 후보 "흔적 기관 은퇴" · 축: C8·C4
관련: 없음 (근거 = 2026-07-17 세션 전수 실측)
상태: 적용 완료 — 빌드/런타임 검증은 사용자 수행 대기
```

## 1. 결정 기록

```text
① Clone_Entity 호출 0건(Client·EldenRing·LoLEditor·Tools 전수), Add_Blueprint 1건(고아,
   Loader.cpp 구 667행 — hp 600 등 스탯 하드코딩으로 ChampionGameplayDefs.json과 이중 진실),
   Set_StaticScene 호출부 0건. 매 씬 전환마다 빈 Clear_Scene 호출 + 매 프레임 4개 죽은 분기.
② 순진한 해법 = "언젠가 쓸지 모르니 방치" — 소비자 소멸이 실측됐고, 대체 경로(데이터 주도
   정의 + 서버 권위 스폰)가 유일한 실전 경로. 방치 비용 = 이중 진실 + 인수 학습 소음.
③ 등록(Loader)·API(GameInstance)·레지스트리·타입 4계층 + StaticScene 멤버/분기/API를 한
   슬라이스로 제거. Clear_Resources는 씬 정리 심(seam)으로 보존(주석 갱신).
④ 판결 큐 방식 — OnShutdown의 Change_Scene(End, nullptr) 무효 호출 의혹은 조사 대기라 제외.
⑤ 잃는 것: 씬 스코프 템플릿 일괄 파기 + 영속 오버레이 씬 메커니즘. ResourceCache::Unload_Scene
   연계 또는 전역 UI 오버레이 씬이 필요해지는 시점이 이 판결이 틀려지는 지점 — 그때는
   Blueprint 부활이 아니라 그 시점 요구 기준의 재설계가 맞다.
```

## 2. 반영해야 하는 코드 (적용 완료 기록)

```text
Client/Private/Scene/Loader.cpp    — Register_Blueprints_InGame() 함수·호출·전용 include 3줄 제거
Client/Public/Scene/Loader.h       — 선언 제거
Engine/Include/GameInstance.h      — EntityBlueprint include·전방선언·Blueprint API 3종 선언·
                                     Set_StaticScene 선언·m_pBlueprintRegistry 멤버 제거
Engine/Private/GameInstance.cpp    — 레지스트리 생성/reset/include 제거, Blueprint API 구현 3종·
                                     Set_StaticScene 래퍼 제거, Clear_Resources를 빈 심으로 교체
Engine/Public/Scene/Scene_Manager.h — Set/Get_StaticScene·m_pStaticScene 제거
Engine/Private/Scene/Scene_Manager.cpp — 소멸자/Update/LateUpdate/Render/ImGui의 StaticScene
                                     분기·Set_StaticScene 구현 제거
Engine/Include/Engine.vcxproj / .filters — EntityBlueprint* 항목 4건 제거
파일 삭제(git rm): Engine/{Public,Private}/ECS/Systems/EntityBlueprint{,Registry}.{h,cpp}
```

## 3. 검증 — 예측을 먼저 쓴다

```text
예측:
- Engine·Client Debug|x64 빌드 성공. Shared/Server 무접촉 → SimLab 골든 해시 불변.
- 씬 전환 왕복 정상. Clear_Resources 트레이스포인트는 여전히 매 전환 발화(심 보존).
- 인게임 사일러스 스폰·스탯 무변화 (StatSystem.cpp:146 BP로 재확인 가능) — 스폰이
  데이터 주도 경로였음의 최종 증명.
- 깨지면 컴파일 에러로 깨진다 (런타임 침묵 실패 경로 없음). 잔존 참조 rg 스윕 = 0 확인 완료
  (EngineSDK/inc 사본 제외).

검증 명령 (사용자 수행):
- msbuild Winters.sln /t:Engine /p:Configuration=Debug /p:Platform=x64 → Client 순차
- UpdateLib.bat (Engine public header 변경 — EngineSDK/inc 동기화, 스테일 EntityBlueprint*.h
  사본 정리 여부 확인)
- 클라 실행: 로그인→메인메뉴→상점 왕복, 인게임 진입, 사일러스 스탯 확인
- Tools\Bin\Debug\SimLab.exe → same-seed replay OK + 기존 해시 동일

미검증:
- 빌드/링크/런타임 전부 (사용자 수행 예정)

확인 필요:
- Codex 병행 세션과의 충돌 — Scene_Manager.cpp의 기존 더티는 공백 1건뿐임을 diff로 확인했음.
  Codex 레인(Fiora/Zed FX, UIRenderer)과 본 슬라이스는 파일 교집합 없음(vcxproj.filters만
  서로 다른 라인). 커밋 전 git diff로 재확인.
```

RESULT 문서는 사용자 빌드·런타임 검증 후 작성 (예측 vs 실측 + 판결 + ⑤ 갱신).
