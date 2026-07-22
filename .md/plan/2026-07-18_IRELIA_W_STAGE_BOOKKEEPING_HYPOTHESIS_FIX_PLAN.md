# 2026-07-18 이렐리아 W 스테이지 북키핑 가설·검증·수정 계획 (V1~V6)

```text
Session - W 릴리스 분기 미진입의 원인 전수 가설화 + BP 판정별 수정 버전 확정
좌표: 축 C7 권위와 정합성 · C8 검증이 병목
관련: 2026-07-18_ITEM_SHOP.../targetMode 수정 세션, 전수감사(중단분)
```

## 1. 결정 기록

```text
① 문제·제약: W 릴리스 시 Scene_InGameInput.cpp:500 분기 미진입(실측·BP). 조건은
   IsKeyReleased && (s_bWReleasePending || HasPendingSkillStage) — OR 양쪽 false는
   "프레스 시점에 북키핑(currentStage=1)이 안 섰다"는 뜻이 논리적 필연. 그런데 서버는
   W1을 수락해 5.0s StationaryChannel 락을 걸었음(홀드는 보임) = 커맨드는 나감.
   정적 분석은 소진(등록 stageCount=2·병합 max·아톰 경로·중복 등록 0건 확인) —
   남은 후보 6개는 전부 런타임 값 1세트로 판별 가능.
⑤ 대가: 가설별 수정은 상호 배타적이지 않음(복수 성립 가능). BP 판정 전 선반영 금지 —
   잘못 고르면 이 파일 자체가 다음 오진의 사료가 된다. Codex 세션이 같은 파일들을
   활성 편집 중이므로 각 수정은 적용 직전 앵커 재확인 필수.
```

## 2. 가설 트리 + BP 판정 + 버전별 수정

실행 순서대로 검증한다. 각 가설의 "판정"이 참이면 해당 버전을 적용하고 나머지는 버린다.

---

### H1 [V1] 프레스 시 `gameData.stage.stageCount != 2` (아톰/병합 오염)

**BP**: `Client/Private/Scene/Scene_InGameLocalSkills.cpp:2179`
```cpp
        if (gameData.stage.stageCount == 2)
```
**관측**: `gameData.stage.stageCount`, `gameData.stage.stageWindowSec`, `champ`, `lookupSlot`
**판정**: stageCount==1 또는 stageWindowSec==0 → H1 확정. 이때 `champ != IRELIA` 또는 `lookupSlot != 2`면 H1c(Spellbook/FormOverride 리맵)로 분기.

**H1a 확정 절차**: `Client/Private/GamePlay/SkillRegistry.cpp:97` `CSkillRegistry::Add`에 조건부 BP `champ==eChampion::IRELIA && slot==2` — **첫 히트**의 `def.stageCount`와 콜스택 확인. 등록 코드(Irelia_Registration.cpp:85 `s.stageCount = 2;`)보다 먼저 다른 곳이 선점 등록 중이면 그 호출자가 범인(try_emplace는 첫 등록만 유효).

**수정 V1**: `확인 필요` — 선점 호출자의 콜스택 확보 후 확정. 방향만 고정:
- 선점 등록이 실수면: 그 호출 제거.
- 재등록이 정당한 흐름이면: `Add`에서 기존 엔트리 갱신 시 `m_GameAtoms`/`m_VisualAtoms`도 재빌드(현재 try_emplace 3종은 첫 등록에 얼어붙음 — 이 동결 자체가 결함).

---

### H2 [V2] 프레스에서 디스패치가 쿨다운 게이트에 죽음 (커맨드 미전송 — 반복 테스트 혼동)

**BP**: `Scene_InGameLocalSkills.cpp:2157`
```cpp
    if (slot != static_cast<uint8_t>(eSkillSlot::BasicAttack)
        && slotState.cooldownRemaining > 0.f)
```
**관측**: 히트 여부 + `cooldownRemaining` 값. **반드시 리스폰 직후 최초 W로 테스트**(직전 시전의 서버 CD 3.0s가 스냅샷으로 복제됨 — SnapshotApplier.cpp:1524).
**판정**: 최초 W에서도 CD>0이면 스냅샷 CD 오염(서버가 시전 전 CD를 보냄) = 별도 서버 버그. 반복 W에서만 걸리면 정상 쿨다운 동작(HEAD 클라 CD 0.6 → 현 3.0이라 체감 급변한 것 — 버그 아님, 튜닝 영역).
**수정 V2**: 오염 판정 시에만 — 서버 CD 발행 경로 추적 후 확정(`확인 필요`). 정상 판정 시 수정 없음, 값 튜닝은 SkillGameplayDefs.json cooldown.

---

### H3 [V3] 북키핑이 세팅된 뒤 릴리스 전에 소거됨

**BP**: H1의 BP 통과(스테이지 세팅 확인) 후, `slotState.currentStage` 주소에 **데이터 중단점**(값 변경 시 중단).
**알려진 정당 writer 전수** (이 외 콜스택 = 범인):
- `Scene_InGameLocalSkills.cpp:2142,2149` (스테이지 전이 — 2타 발송 시)
- `Scene_InGame.cpp:1258-1260` (감쇠·만료: `stageWindow -= dt; <=0이면 currentStage=0`)
- SnapshotApplier는 쿨다운 필드만 씀(:1524-1532) — stage 필드 무접촉 확인됨

**유력 세부가설 H3a — dt 스파이크 즉사**: 클라 3개 포커스 전환/스톨 직후 큰 `dt`가 들어오면 4.0s 창이 한 프레임에 만료된다. 감쇠 BP에서 `dt` 값 관측.
**수정 V3a**: `Client/Private/Scene/Scene_InGame.cpp` — 기존 코드:
```cpp
            if (ss.slots[i].currentStage == 1 && ss.slots[i].stageWindow > 0.f)
            {
                ss.slots[i].stageWindow -= dt;
```
아래로 교체:
```cpp
            if (ss.slots[i].currentStage == 1 && ss.slots[i].stageWindow > 0.f)
            {
                // 포커스 전환/스톨 직후 dt 스파이크가 스테이지 창을 한 프레임에
                // 만료시키지 않도록 감쇠분을 클램프한다 (서버 권위 창은 별도 유지).
                ss.slots[i].stageWindow -= (std::min)(dt, 0.1f);
```
(§3: `<algorithm>` include 확인 — Scene_InGame.cpp 상단. `확인 필요`)

---

### H4 [V4] 릴리스 에지 유실 (입력층)

**BP/관측**: 릴리스 프레임 `Scene_InGameInput.cpp:500`에서 `in.IsKeyReleased('W')` false + ImGui `io.WantCaptureKeyboard`, 창 포커스 상태.
**성립 시나리오**: 클라 3개 사이 포커스 전환 중 WM_KEYUP이 다른 창으로 감 / 홀드 중 UI가 키보드 캡처.
**수정 V4**: 에지 의존을 상태 폴링으로 보강 — 기존 코드(`Scene_InGameInput.cpp:500`):
```cpp
            else if (in.IsKeyReleased('W') &&
                (s_bWReleasePending || HasPendingSkillStage(*this, wSlot)))
```
아래로 교체:
```cpp
            else if ((in.IsKeyReleased('W') || !in.IsKeyDown('W')) &&
                (s_bWReleasePending || HasPendingSkillStage(*this, wSlot)))
```
(에지가 유실돼도 "키가 안 눌려 있음+pending"이면 2타 발송. `IsKeyDown` API 명칭 `확인 필요` — CInput 헤더 대조.)

---

### H5 [V5] 스테일 pending 역전 (이전 시전 잔재로 프레스 게이트가 디스패치를 스킵)

**BP**: 프레스 프레임 `Scene_InGameInput.cpp:493` `if (!HasPendingSkillStage(*this, wSlot))` — **미진입**(=pending 잔존)이면 H5. 당시 `currentStage/stageWindow` 관측.
**수정 V5**: 프레스 시 pending이면 스킵하는 대신 2타로 처리 — 기존 코드(:493-497):
```cpp
                if (!HasPendingSkillStage(*this, wSlot))
                {
                    ClearNetworkAttackIntent();
                    const bool_t bDispatched = DispatchSkillInput(wSlot);
                    s_bWReleasePending = bDispatched && HasPendingSkillStage(*this, wSlot);
                }
```
아래로 교체:
```cpp
                if (!HasPendingSkillStage(*this, wSlot))
                {
                    ClearNetworkAttackIntent();
                    const bool_t bDispatched = DispatchSkillInput(wSlot);
                    s_bWReleasePending = bDispatched && HasPendingSkillStage(*this, wSlot);
                }
                else
                {
                    // 이전 홀드의 pending이 살아있으면 재프레스를 2타로 처리해
                    // 락에 갇힌 상태를 입력으로 탈출 가능하게 한다.
                    ClearNetworkAttackIntent();
                    DispatchSkillInput(wSlot, 2u);
                    s_bWReleasePending = false;
                }
```

---

### H6 [V6] 클라는 정상 발송, 서버가 거절 (교차 검증용 — 현 관측과 상충하나 병행 확인)

**관측 (BP 불필요)**: 서버 콘솔/출력의 `[Command] cast-skill reject reason=...` — `stage2-window`(CommandExecutor.cpp:2426)면 서버 창 만료(클라-서버 감쇠 불일치), `cooldown`(:2596)이면 bStage2 판정 붕괴(:2343 BP로 3항 관측), `[Data] required definition missing` 동반이면 팩 미스.
**수정 V6**: reason별 — `확인 필요` (판정 후 이 문서에 추가).

---

## 3. 검증 — 예측을 먼저 쓴다

```text
예측:
- H1~H5 중 정확히 하나(또는 H2+H5 복합)가 참으로 판정된다. 전부 거짓이면 가설 트리가
  불완전한 것 — 그 자체가 발견이며 프레스 프레임 전체 스텝 스루로 전환한다.
- 어느 버전이든 적용 후: W 프레스→릴리스 시 서버 콘솔에 "[IreliaSim] W hold" →
  "W release" 쌍이 찍히고, 릴리스 후 0.4s 내 이동 가능해진다.
- SimLab 골든 불변(클라 입력/북키핑 수정은 서버 sim 비접촉). V3a/V4/V5는 서버 권위
  불변 — Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다.
- 게이트: 사용자 BP 세션(이 문서의 판정 절차) + 수정 후 동일 시나리오 재현.

검증 명령:
- 수정 적용 후: msbuild Engine+Client (Debug 우선, /m:1) → 클라·서버 전부 재기동 → W 재현.
- 비에고 W 데이터 수정(별도 확정분: SkillGameplayDefs.json+champions.json lockSeconds
  0.7→4.0 + 생성기 2종 재실행)을 같은 빌드 사이클에 묶는다.

미검증:
- 가설 전부 — BP 판정 대기. 판정 결과와 적용 버전을 이 문서 하단에 RESULT로 기록할 것.

확인 필요:
- V1 선점 호출자 콜스택 / V3a min include / V4 CInput IsKeyDown API 명칭 / V6 reason.
- Codex 교차 검증: Scene_InGameInput.cpp·Scene_InGameLocalSkills.cpp·SkillRegistry.cpp는
  Codex 활성 편집 범위 — 적용 직전 각 앵커 재확인, 동일 취지 수정 존재 시 해당 버전 스킵.
```
