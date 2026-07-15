Session - S004(로딩 응답성·HUD 복구·부쉬 폐기) 독립 감사 결과를 고정하고, 인게임 검증 게이트·잔여 갭 핸드오프·Claude/Codex 레인 분리를 박제한다.

작성: Claude 레인 (조사 에이전트 4 + 교차검증 에이전트 3, 인용 앵커 전수 rg/git 재검증)
작성일: 2026-07-11
선행: `Plan/S004_LOADING_RESPONSIVENESS_HUD_BUSH_RETIREMENT_SESSION_20260711.md`, 동 `_RESULT_20260711.md`
관련 packet: `.md/collab/work-packets/2026-07-11_s005_verification_gate_lane_split.md`

---

## 0. Current sequence

1. [완료] 독립 감사 — 로딩/HUD/부쉬 3개 이슈 원인 확정 -> verify: 교차검증 3건 전부 CONFIRMED (§2)
2. [완료] S004 적용분과 감사 결과 대조 -> verify: 방향 일치 + 잔여 갭 5건 식별 (§3, §4)
3. [Codex] 잔여 갭 G1 적용 (SaveLayout 파일명 불일치, 코드 2곳) -> verify: Engine/Client Debug x64 빌드 PASS + UpdateLib.bat (§5, §7)
4. [사용자] 인게임 검증 게이트 -> verify: §6 체크리스트 전 항목 통과
5. [Codex] 게이트 통과 직후 checkpoint commit (G3) -> verify: `git status` 대상 파일 전부 커밋, 보고서는 새 파일
6. [후속 세션] G5 완전 비동기 로딩 슬라이스 (§4-G5)

## 1. Goal / Non-goals / Why this order

**Goal**
- S004가 고친 것과 남긴 것을 코드 근거로 고정하고, 사용자가 인게임에서 무엇을 어떤 순서로 확인해야 하는지 단일 체크리스트로 만든다.
- Claude/Codex가 같은 트리에서 충돌 없이 협업하도록 레인을 `.md/collab/**`에 분리한다 (§9).

**Non-goals**
- 로딩의 완전 비동기화(worker CPU prepare -> GPU finalize queue)는 이번 범위가 아니다. 다음 슬라이스다 (§4-G5).
- 부쉬 재도입 설계 없음. S004의 폐기 결정을 그대로 인정한다.
- UI_Manager.cpp 전체 한글 주석 인코딩 정규화 없음 (G2는 결정 대기).

**Why this order**
- S004는 이미 적용·빌드 PASS 상태지만 전체가 미커밋이다. 인게임 게이트 전에 코드를 더 얹으면 실패 시 원인 분리가 안 된다. 예외는 G1 하나 — 2줄짜리 파일명 불일치로, 지금 안 고치면 인게임 검증 중 레이아웃 튜너 저장이 조용히 죽은 파일로 간다.

## 2. 검증된 현재 상태 (독립 감사 — 전 항목 교차검증 CONFIRMED)

### 2-1. 로딩 — "실수 원복"이 아니라 의도된 안전조치의 부작용

- 이력: 초기 커밋 `e6ded62`부터 `1813b00`까지 `CLoader::Create`는 InGame 포함 전 씬을 CJobSystem worker에 `RunLoadJob`으로 submit했다 (마우스 살아있던 시절의 실체).
- `b4d2237`(2026-06-24)에서 InGame만 메인스레드 단계식 로드로 **의도적으로** 전환. 이유: worker에서 모델 preload 시 RHI 핸들 테이블의 렌더스레드 소유권 assert가 발화 —
  `Engine/Public/RHI/CRHIResourceTable.h:128` `assert(std::this_thread::get_id() == m_RenderThreadId ...)`.
  결정 문서: `.md/plan/2026-06-24_IOCP_RHI_THREAD_OWNERSHIP_FIX_HANDOFF_PLAN.md` (invariant는 옳다고 명시, "worker decode -> owner thread GPU 등록"을 후속 처방).
- 증상 메커니즘: `CLoader::TickMainThreadLoad`가 프레임당 LoadStep 1개를 실행하는데, 맵 스텝 하나가 42,130,852바이트 `sr_base_flip.wmesh` 파싱 + material 텍스처 전수 WIC 디코드(구 코드는 `CTexture::Create`+`RHI_CreateTextureFromFile` 이중 디코드)를 포함했다. 메시지 펌프는 `CEngineApp::Run` 프레임당 1회(`CWin32Window::PumpMessages`, 유일한 펌프)라 수 초 스톨 -> "응답 없음".
- 즉 D3D11 디바이스 자체는 free-threaded(SINGLETHREADED 플래그 없음)이고, 제약은 엔진 자체의 핸들 테이블 소유권 규칙이다. **assert를 완화하는 방향은 금지** — Release에서는 조용한 race가 된다.

### 2-2. HUD — f9d4d5c 상수 리네임 vs untracked 데이터 파일

- HEAD `f9d4d5c`(Champion->Actor 일반화)가 `UI_Manager.cpp`의 레이아웃/텍스처 경로 상수를 `actor_hud_layout.json`, `ActorHUD_Default.png` 등 존재하지 않는 이름으로 변경. 실제 파일(`hud_irelia_layout.json` 등)은 **git untracked**라 커밋이 함께 리네임할 수 없었다.
- 레이아웃 로드 2회 모두 실패 -> `CActorHudPanel::UseDefaultLayout()` 폴백. 폴백에는 `stats.panel`+스탯 텍스트 8종 없음(스탯창 소실), `shape:"circle"`+`portrait.frame` 없음(원형 초상화 소실), `skill.q` x=291(JSON은 350.5)이라 Q 왼쪽에 앵커되는 패시브 아이콘이 사각 초상화 rect 안으로 밀림 + 슬롯 프레임 스프라이트 부재(패시브 소실처럼 보임 — 그려지긴 하나 초상화에 겹침).
- 같은 커밋이 미니언/터렛 HP바 텍스처 4종 등 8종 상수도 파일 없이 리네임했고 `상점1.png` 리터럴을 인코딩 파손시켰다(한글 주석 다수도 파손, C4828 경고로 잔존).

### 2-3. 부쉬 — crossed-card 구조가 평면 PNG로 보인 이유 (폐기 근거)

- S003의 `map11_bush_cluster.wmesh` = windgrass foliage 카드 1장을 0/60/120도로 교차 복제(210 verts/864 indices)한 crossed-card. 텍스처 `sru_brush.png`(256x256 RGBA)는 **전 픽셀 alpha=255** — `Mesh3D.hlsl`의 `clip(texColor.a - 0.05f)`가 무력화되어 불투명 교차 평면으로 렌더. 실루엣/체적/잎 깊이 전무 = 스크린샷 증상 그대로. (알파 마스크는 셰이더가 아니라 텍스처에 없었다 — 재도입 시 아트가 선결.)
- 서버는 `StageData::bushes`를 어디서도 소비하지 않고 스폰 엔티티를 전부 visible-to-all로 찍는다(`GameRoomSpawn.cpp` `BuildServerVisibleToAll`). 부쉬 은폐는 클라 표현(FoW/상호가시성) 전용이었으므로, **Stage v4 원복으로 은폐 볼륨 64개가 사라져도 서버 게임플레이 손실은 없다.**

## 3. S004 적용분 대조 (Codex 구현 vs 독립 감사)

| 이슈 | S004 선택 | 감사 판정 |
| --- | --- | --- |
| 로딩 | cooperative pump(모델 로드 경계 yield에서 펌프) + `WM_QUIT` latch(`CWin32Window.cpp` `m_bQuitRequested`) + CPU-only `CMapSurfaceSampler`만 worker + 모델 로컬 텍스처 dedupe | 일치. CRHIResourceTable invariant 침범 없음. "안전한 중간 단계"라는 S004 자평도 정확 — 완전 비동기는 G5로 |
| HUD | 상수를 실제 파일명으로 원복 (감사의 대안 중 코드-원복 방향) + `상점1.png` 리터럴 복구 | 일치. 단 G1 잔존: SaveLayout과 실패 로그 문자열이 여전히 옛 이름 (§5) |
| 부쉬 | Stage1.dat v4/B0 원복 + 생성 도구/산출물 삭제 = full retirement. Bush_Manager mesh 경로는 휴면 코드로 유지 | 일치. 감사의 대안(v5 유지+시각만 차단)보다 사용자 "폐기하자" 의도에 부합. 휴면 코드 처리는 G4 |

## 4. 잔여 갭 (우선순위순)

- **G1 (P0, 코드 2곳)** — SaveLayout/로그 파일명 불일치. 로더는 `hud_irelia_layout.json`을 읽지만(`UI_Manager.cpp:63-64`), 인게임 레이아웃 튜너의 `CActorHudPanel::SaveLayout`은 `..\Resource\UI\actor_hud_layout.json`에 쓴다(`ActorHUDPanel.cpp:110`) — 저장해도 다시 읽히지 않는다. 실패 로그 문자열(`UI_Manager.cpp:797`)도 옛 이름을 출력해 채증을 오도한다. 교체 블록 §5.
- **G2 (P1, 결정 대기)** — `UI_Manager.cpp`의 f9d4d5c발 한글 주석 인코딩 파손(C4828) 잔존. `git show f9d4d5c~1` 기준 복원 가능. CONFIRM_NEEDED: 이번에 복원할지, 별도 인코딩 정규화 세션으로 미룰지.
- **G3 (P0, 절차)** — S004 전체가 미커밋이며 이 dirty set을 소유한 packet이 없었다(§9에서 등록함). 인게임 게이트 통과 직후 checkpoint commit. 이때 **untracked인 `Client/Bin/Resource/UI/hud_irelia_layout.json`과 `hud_atlas_manifest.json`을 git 추적에 추가**할 것 — 이번 사고의 재발 방지(코드 리네임이 데이터 파일을 조용히 고아로 만드는 패턴)의 핵심이다.
- **G4 (P2, 결정)** — Bush_Manager의 휴면 mesh 경로(`Bush_Manager.cpp:214` `AppendRenderSnapshotMeshes`, `:421` 등 `Sync_MeshRenderer` 호출, `Scene_InGameRender.cpp`의 부쉬 submit 2곳). S004는 "미래 실제 3D bush asset용 유지"를 선택 — 이견 없음. 삭제는 새 부쉬 접근이 확정된 뒤 별도 커밋으로.
- **G5 (P2, 다음 슬라이스)** — 완전 비동기 로딩: worker CPU prepare(WMesh/WMat/WAnim 파싱 + 텍스처 픽셀 디코드) -> bounded main-thread GPU finalize queue(프레임당 시간 예산으로 등록). 금지: `CModel::Create` 전체 worker 복귀, CRHIResourceTable assert 완화. `CFxAssetRegistry::LoadDirectory`는 global registry를 mutate하므로 worker parse / main publish 분리가 선결. `ResourceCache`의 단일 mutex가 로드 전체를 잡는 구조(`ResourceCache.cpp` lock 범위)도 이 슬라이스에서 조회/삽입만 잡도록 좁힌다.

## 5. G1 교체 블록 (Codex 직접 적용용)

### 5-1. `C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/ActorHUDPanel.cpp`

`TryBuildSaveLayoutPath` 내부 (line 109 근처):

기존 코드:
```cpp
            std::wstring candidate = std::wstring(exePath) +
                L"..\\Resource\\UI\\actor_hud_layout.json";
```

아래로 교체:
```cpp
            std::wstring candidate = std::wstring(exePath) +
                L"..\\Resource\\UI\\hud_irelia_layout.json";
```

주의: 이 교체 후 레이아웃 튜너 저장은 로더가 읽는 원본 JSON을 직접 덮어쓴다(로드/세이브 왕복 복원 = 원래 의도). G3에서 이 파일을 git 추적에 넣어야 잘못된 튜닝 저장을 diff로 되돌릴 수 있다.

### 5-2. `C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp`

`LoadLayout` 실패 분기 (line 797 근처):

기존 코드:
```cpp
        OutputDebugStringA("[UI_Manager] actor_hud_layout.json load failed - using built-in HUD layout\n");
```

아래로 교체:
```cpp
        OutputDebugStringA("[UI_Manager] hud_irelia_layout.json load failed - using built-in HUD layout\n");
```

확인 필요: 새 파일 없음 -> vcxproj/filters 변경 없음. Engine 수정이므로 빌드 후 `UpdateLib.bat` 후속 동기화 실행 필요 (헤더 변경은 없어 SDK diff는 없어야 정상).

## 6. 인게임 검증 절차 (사용자 게이트)

기반: S004 RESULT §5. 아래는 감사에서 추가된 관측 포인트를 합친 통합 체크리스트. 실행 파일: `Client/Bin/Debug/WintersGame.exe` (Debug x64, 디버거 attach 상태로 Output 창 관찰).

**A. 로딩 응답성**
1. BanPick -> Irelia 선택 -> match loading 진입. 로딩 내내 마우스로 원 그리기.
2. PASS 기준: OS 커서 끊김 없음 / 타이틀 "응답 없음" 미발생 / 창 이동 가능 / 인게임 전환 후 게임 커서 1개만 표시.
3. 별도 1회: 로딩 도중 창 닫기 -> 파괴된 HWND로 다음 프레임 진행 없이 정상 종료.

**B. HUD**
1. 1920x1080 Irelia 인게임 하단 중앙 확인.
2. PASS 기준: HUD base/frame 표시 / 원형 초상화 face+frame / 패시브 아이콘이 초상화·Q에 겹치지 않고 Q 왼쪽 슬롯에 / QWER+rank pip+HP/MP+스탯창(AD/AP/방/마저/공속/스킬가속/치명/이속 8종 텍스트)+인벤/골드 정위치.
3. Output 창에 아래 로그가 **없어야** PASS:
   - `[UI_Manager] hud_irelia_layout.json load failed`(G1 적용 후 문자열) 또는 `actor_hud_layout.json load failed`(적용 전)
   - `ActorHUD_Default.png load failed` / `UnitBlueHPBar.png load failed` / `skill rank pip texture load failed`
   - `[RHITextureLoader] FAIL` 계열
4. 1280x720로 변경 -> center anchor 유지. Viego/Yasuo 재진입 -> 초상화/패시브/스킬 아이콘만 바뀌고 공통 프레임 유지.

**C. 부쉬/앰비언트**
1. normal F5 인게임에서 기지/레인/정글 이동.
2. PASS 기준: crossed PNG card 부쉬 0개 / Stage bush count 로그 0 / 새·오리·firefly·곤충 앰비언트 유지.
3. Editor에서 Stage1 load -> `B:0` 확인, 저장 왕복 후에도 부쉬 0 유지.

**실패 시 채증 규칙**: 남은 로그 라인 원문 + 스크린샷을 그대로 전달. 로그가 가리키는 파일 경로가 코드 상수면 Codex 레인(구현), 원인 불명이면 Claude 레인(감사)으로 배정.

## 7. 검증 명령

```powershell
# G1 적용 확인 (두 옛 이름이 코드에서 사라졌는지 — SaveLayout/로그까지)
rg -n "actor_hud_layout" Engine/ Client/ --glob "!*.md"
# 기대: 0건

# 빌드 (S004와 동일 경로: 솔루션 타깃이 아니라 프로젝트 직접)
msbuild Engine/Include/Engine.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64
msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64

git diff --check
```

## 8. 롤백 범위

- G1: 위 두 파일의 두 블록만 원복하면 S004 직후 상태로 복귀.
- S004 전체: 현재 미커밋이므로 G3 checkpoint commit 이후 `git revert`로 되돌린다. `git checkout -- <file>` 방식 금지 (`.md/collab/GIT_SYNC_RULES.md` 금지 규칙, dirty 파일 혼재 상태에서 위험).
- 부쉬 재도입: `Tools/cook_map11_brush_volumes.py`는 research-only(WBRUSH 좌표 산출)로 남아 Stage를 오염시키지 못한다. 재도입은 alpha-authored 아트 확보 후 새 세션.

## 9. 레인 분리 (이 세션에서 반영 완료)

- `.md/collab/OWNERSHIP_MATRIX.md`: `## Agent 레인 (Claude / Codex)` 섹션 추가 — 장비 축과 독립인 agent 축, 규칙 4개.
- `.md/collab/ACTIVE_WORK_PACKETS.md`: packet에 `Agent` 필드 추가, S004(Codex)/S005(Claude) packet 등록으로 현재 dirty set 소유 명시.
- `.md/collab/GIT_SYNC_RULES.md`: `claude/*` 브랜치 prefix 추가.
- 규약 요지: **Plan/S{NNN} 시퀀스는 공유하되 상대 문서에 append 금지(응답은 새 번호)** / Active packet의 owned paths는 상대 agent에게 read-only / 코드 파일 동시 수정 금지 / Claude의 코드 수정은 사용자 명시 지시 + packet 등록 시에만.

## 10. 다음 슬라이스

G1+G3 완료·게이트 통과 후: G5 완전 비동기 로딩 설계 세션(S006 후보). 입력: 이 문서 §4-G5, S004 RESULT §6, `.md/plan/2026-06-24_IOCP_RHI_THREAD_OWNERSHIP_FIX_HANDOFF_PLAN.md` §5.5.
