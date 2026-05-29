# Phase B-6 — 사일러스 호버 타겟팅 + 전투 입력 + 맵 회전 튜너

**기간**: 2026-04-17
**직전 Phase**: B-5 (ImGui + Scene 시스템)
**다음 Phase**: C-1 (CGameInstance 확장 + CRenderer 렌더 큐)

---

## 목표

Scene_InGame에서 마우스 호버 기반 전투 입력 UX를 추가하고, 맵 glb 축 정렬을 ImGui로 튜닝 가능하게 만든다.

- 사일러스를 테스트 타겟으로 세우고 **마우스 호버 판정**
- 호버 중 **우클릭 = 평타**, **Q/W/E/R = 스펠1~4** (호버 아닐 땐 무시)
- **맵 회전 3축 ImGui 슬라이더** + 44개 맵 오브젝트 동시 회전 옵션
- `CInput` **에지 트리거 API** (`IsKeyPressed` / `IsRButtonPressed`) 구현
- **액션 락**으로 공격/스펠 애니가 이동 블록에 덮이지 않도록 보호

쿨다운·데미지·피격·사거리 인디케이터는 Phase 4 (네트워크 + 게임 로직)로 이월.

---

## 주요 산출물

### 1. CInput 에지 트리거 API (`Engine/Public/Core/CInput.h` + `Engine/Private/Platform/CInput.cpp`)

- `IsKeyPressed(vKey)` / `IsKeyReleased(vKey)` — 이번 프레임에만 true
- `IsRButtonPressed()` / `IsRButtonReleased()` — 대칭 마우스 버전
- 내부: `m_pPrevKeys[256]` / `m_pPrevRButton`를 `EndFrame()`에서 스냅
- **기존 오타** `IsKeyRelease(uint8 vkey)` → `IsKeyReleased(uint8 vKey)` 수정
- `EngineSDK/inc/CInput.h` 바이트 단위 동기화 확인 (`fc /b` 무출력)

### 2. 사일러스 호버 판정 (`Client/Public,Private/Scene/Scene_InGame.*`)

- **히트 판정 형태**: Cylinder (반경·높이 2 파라미터, LoL 스타일, 캐릭터 회전 무관)
- `RayVsCylinder(ro, rd, base, r, h, outT)` — XZ 2D 원 교차 + Y 클램프, 수직 레이 엣지 케이스 포함
- `UpdateTargeting()` — `CInput::GetMouseWorldRay` → 실린더 교차 → `m_bSylasHovered` 갱신
- `ImGui::GetIO().WantCaptureMouse` 가드로 UI 위에선 호버 false

### 3. 전투 입력 (`UpdateCombatInput(outSkipGroundMove)`)

- Q/W/E/R: 호버 + `WantCaptureKeyboard` false + `IsKeyPressed` → `FirePlayerAction("spell1"~"spell4")`
- 우클릭: 호버 + `WantCaptureMouse` false + `IsRButtonPressed` → `FirePlayerAction("attack")` + `outSkipGroundMove = true`

### 4. FirePlayerAction — 챔프별 애니 이름 매핑

```cpp
// Irelia/Yasuo 네이밍 불일치 해소
if (strcmp(actionKey, "attack") == 0)
    key = (champ == IRELIA) ? "attack_01" : "attack1";
string animKey = prefix + key;   // "irelia_attack_01" / "yasuo_attack1"
m_pPlayerRenderer->PlayAnimationByName(animKey);
```

실제 LoL 추출 `.anm` 파일 이름:
- Irelia: `irelia_attack_01` / `_02` / `_03` (언더스코어+2자리)
- Yasuo: `yasuo_attack1`~`4` (깔끔)

`FindAnimationIndex`는 substring 매칭이라 키만 정확히 넣으면 됨.

### 5. 액션 락 — 이동 블록의 애니 덮어쓰기 차단 (핵심 버그 해결)

**문제**: 매 프레임 이동 블록이 `m_bMoving != wasMoving` 전환 시 `run`/`idle` 애니를 덮어써서 평타/스펠 애니가 "잠깐만" 재생 후 사라짐.

**수정** (`Scene_InGame.cpp` L238-304):

- L238-240: 액션 락 전/후 스냅
  ```cpp
  const bool bActionLockedBefore = (m_fLastActionTimer > 0.f);
  if (m_fLastActionTimer > 0.f) m_fLastActionTimer -= dt;
  const bool bActionLocked = (m_fLastActionTimer > 0.f);
  ```
- L252-253: 우클릭 이동 트리거에 **호버/락 가드** (LoL 락온 스타일)
  ```cpp
  if (!bImGuiMouse && !bSkipGroundMove && !bActionLocked
      && !m_bSylasHovered && input.IsRButtonDown())
  ```
- L294-304: 자동 애니 교체에 락 가드 + 락 해제 복구
  ```cpp
  if (!bActionLocked && m_bMoving != wasMoving) { /* run/idle 교체 */ }
  else if (bActionLockedBefore && !bActionLocked) { /* 락 풀림 시점 1회 복구 */ }
  ```

`m_fLastActionTimer = 1.2f` 기본 — Phase 4에서 스킬별 캔슬 윈도우로 이관 예정.

### 6. 맵 회전 ImGui 튜너 (`OnImGui_MapTuner`)

- `m_vMapRotation` (라디안 저장, ImGui는 도 단위)
- X/Y/Z SliderFloat (-180 ~ 180)
- "Apply same rotation to 44 map objects" 체크박스 — 체크 시 매 프레임 `m_MapObjects[i].transform.SetRotation(m_vMapRotation)` 적용
- 프리셋 버튼: Reset / Y +90 / Y -90 / Y 180
- **"Copy as C++"** 버튼 → `OutputDebugString`으로 확정값 한 줄 출력 → `OnEnter()`에 하드코딩 교체

### 7. OnImGui 창 배치 재조정

- `OnImGui_CombatDebug()` / `OnImGui_MapTuner()` 호출을 `!m_bShowEditor` return **앞**으로 이동 → F1로 에디터 접어도 전투·맵 튜너는 계속 보임
- 카메라 디버그 창은 기존대로 에디터 내부 유지

---

## 디버깅 히스토리 (1→4차)

### 1차: 기능 추가 + 중첩 이동 블록 버그
- 플레이어 이동 블록이 **중첩·중복 실행**되는 패치 → 속도 2배, ImGui 창 위 우클릭 유출
- 해결: 단일 블록으로 합치고 `input` shadowing 제거, `bImGuiMouse`·`bSkipGroundMove` 가드 1회만

### 2차: ImGui 호출 순서
- F1 에디터 접힘 시 전투/맵 튜너까지 사라지는 문제 → `return` 앞으로 호출 이동

### 3차: 애니 이름 불일치
- `PlayAnimationByName("irelia_attack1")` 가 매칭 실패 → 실제 파일은 `irelia_attack_01`
- 야스오는 `yasuo_attack1~4`로 깔끔 → `FirePlayerAction`에 챔프별 분기 추가

### 4차: 이동 블록의 애니 덮어쓰기 (최종)
- 증상: 우클릭 연타 시 평타 "잠깐", 홀드 시 즉시 사라짐, QWER 전부 무반응
- 원인: `m_bMoving` 전환 프레임의 `PlayAnimationByName(run/idle)` 호출이 방금 시작한 평타/스펠을 1프레임 만에 덮어씀
- 해결: `m_fLastActionTimer` 기반 액션 락 3중 가드 (L238-240, L252-253, L294-304)

---

## 재사용된 기존 인프라

| 대상 | 경로 |
|---|---|
| `CInput::GetMouseWorldRay` | `Engine/Public/Core/CInput.h` |
| `CInput::GetMouseGroundPos` | 동상 |
| `ModelRenderer::PlayAnimationByName` (substring 매칭) | `Engine/Public/Renderer/ModelRenderer.h` |
| `CTransform::SetRotation(Vec3)` (Euler rad) | `Engine/Public/Core/CTransform.h` |
| `CGameInstance::Get_GameContext().SelectedChampion` | `Engine/Include/GameContext.h` |
| `CImGuiLayer::WantsCaptureMouse/Keyboard` + WndProc 선처리 | B-5에서 통합 |

**신규 파일 0개.** 전부 기존 파일 확장.

---

## 런타임 검증 체크리스트

- [x] CInput 에지 API 구현, `fc /b` 동기화 확인
- [x] 이동 블록 단일화
- [x] 사일러스 호버 YES 확인
- [x] 이렐리아 `irelia_attack_01` 재생 확인
- [x] 액션 락으로 QWER/우클릭 전부 정상 재생 (1.2초 유지 후 idle 복귀)
- [ ] 야스오 재선택 후 `yasuo_attack1` / `yasuo_spell1~4` 검증
- [ ] Map Rotation Tuner로 최적 Y축 회전 확정 (-45° 시도 중)
- [ ] 확정값을 `Scene_InGame.h` `m_vMapRotation` 기본값에 하드코딩

---

## 향후 과제 (Phase B-6 밖)

| 항목 | 이관 Phase |
|---|---|
| 런타임 플레이어 챔프 핫스왑 (ChampionDef 테이블화) | B-7 또는 C-1 |
| `m_fLastActionTimer` → `AbilityComponent` + 스킬별 캔슬 윈도우 | Phase 4 |
| 사일러스 HP / `hit` 애니 / 피격 반응 | Phase 4 |
| 타겟 락온 자동 평타 (attack-move) | Phase 4 |
| 카메라 Update 순서 (OnUpdate 선두로) → 타겟팅 1프레임 지연 제거 | Phase 2 이후 |
| 스펠 사거리·AOE 링 Decal | Phase 2 (Deferred) 이후 |
| ImGui 튜너 `WINTERS_EDITOR` 매크로 감싸기 (Release 제거) | 맵 회전 확정 직후 |

---

## gotcha — 신규 기록

1. **LoL 추출 FBX 애니 네이밍이 챔프마다 다름** — `irelia_attack_01` vs `yasuo_attack1`. `PlayAnimationByName` substring 매칭이라 정확한 서브스트링이 없으면 무음 실패. 새 챔프 추가 시 `OnEnter` 말미 애니 이름 덤프 루틴으로 실제 이름 먼저 확인할 것.
2. **매 프레임 이동 블록의 애니 자동 교체는 "위험한 전역 효과"** — 액션 중 보호 장치 없이 두면 공격/스펠 애니가 즉시 덮임. 향후 `AbilityComponent` 도입 전까지는 `m_fLastActionTimer` 기반 락으로 유지.
3. **`bSkipGroundMove`는 `IsRButtonPressed` 1프레임만** true이므로 홀드 상태는 보호 못함 → `m_bSylasHovered` 가드와 `bActionLocked`를 조합해야 락온 의도 달성.
