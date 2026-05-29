# Phase B-8 — 가렌 추가 파이프라인 (계단식 검증 계획서, v2)

**작성일**: 2026-04-27
**v2 갱신**: 2026-04-27 — Codex 코드 검증 비판 반영 (P0 2건 / P1 3건 / 추가 2건)
**전제**: Phase B-7 칼리스타 풀 사이클 완료. 가렌 1체 추가로 **OOP vs Winters ECS 6 레이어** 명시 분리 체득.
**참조**:
- `C:\Users\user\.claude\plans\nested-honking-stream.md` (Phase B-8 섹션, L674~910)
- `C:\Users\user\.claude\projects\C--Users-user-Desktop-Winters\memory\project_phase_b7_kalista_full.md`
- `CLAUDE.md` L20-36 (다음 세션 진입 가이드)

---

## 0. 경로 컨벤션 (★ 신규)

기존 `C:/Users/user/Desktop/LOL_Resource/...` 절대 경로 → **`Client/Bin/Resource/Texture/...` 프로젝트 루트 상대 경로** 로 이관.

| 종류 | 신규 경로 |
|------|----------|
| 챔피언 FBX | `Client/Bin/Resource/Texture/Character/Garen/garen.fbx` |
| 챔피언 텍스처 | `Client/Bin/Resource/Texture/Character/Garen/garen_base_tx_cm.png` |
| FX FBX | `Client/Bin/Resource/Texture/FX/Garen/particles/fbx/<name>.fbx` ★ |
| FX 텍스처 | `Client/Bin/Resource/Texture/FX/Garen/particles/<name>.png` ★ |

★ **v2 정정**: 실제 추출 구조는 `FX/Garen/particles/fbx/` 와 `FX/Garen/particles/` (PNG). v1 의 `FX/Garen/fbx/` 는 잘못. 실재 파일:
- `garen_base_e_spin.fbx` (E 회전)
- `garen_base_w_shield.fbx` (W 방어막)
- `garen_base_r_sword_plane.fbx`, `garen_base_r_halfdome.fbx`, `garen_base_r_praxis.fbx` (R)
- `garen_base_q_impact.fbx`, `garen_base_q_jump.fbx`, `garen_base_q_sidewall.fbx`, `garen_base_q_sword_scroll.fbx` (Q)

**적용 범위**: 가렌 신규 코드 전부. 기존 챔프 절대 경로는 별도 sweep.
**근거**: cwd 와 무관하게 결정적. KalistaFxPresets.cpp L11-20 동일 패턴.

---

## 0.5 v2 변경 요약 — Codex 검증 반영

| # | 항목 | 우선 | 수정 내용 |
|---|------|------|----------|
| 1 | **ChampionTable.cpp GAREN row 추가** | **P0** | `FindChampionDef` 실패 시 ApplyLocalPrediction (L1585/L1984) 이 애니/castFrame 감시 전부 skip → 스킬 무반응. 신설 Stage 3.5 |
| 2 | **Sync/Update/Render 3 곳 추가** | **P0** | Scene 수동 렌더 구조라 멤버만으론 안 보임. Stage 4 에 L617/L1156/L1342 3 군데 명시 |
| 3 | **W castFrame=0 → 1.f** | P1 | L720 `d.castFrame > 0.f` 조건. 0 이면 hook miss. SkillTable W 정정 |
| 4 | **FX 경로 `particles/fbx/`** | P1 | 위 0절 정정 + GarenFxPresets.cpp 의 `kPath*` 상수 갱신 |
| 5 | **vcxproj/.filters 수동 등록** | P1 | Stage 7 에 명시. 자동 스캔 없음 |
| 6 | **BA/R hp 감소 — `ApplyGarenHit` 신설** | 추가 | `ApplyYasuoHit` (L2454) 패턴 미러. F5 #2 기대값 정정 |
| 7 | **R 입력 경로 정정** | 추가 | "호버 + R 키" (호버 + 클릭 X). 입력 분기 확인 |

---

## 1. 6 레이어 매핑

| 레이어 | 가렌 작업 | 파일 | 패러다임 |
|--------|----------|------|---------|
| 1. 자원 (RAII) | `ModelRenderer m_Garen` + `CTransform m_GarenTransform` Scene 멤버 + Init/LoadMeshTexture + Update/Render | `Scene_InGame.h/.cpp` | **OOP** |
| 2. 상태 (Component) | `CreateChampionEntity` + `SkillStateComponent` add | `Scene_InGame.cpp::CreateECSEntities` | **ECS** |
| 3. 정의 (Table) | `SkillTable.cpp` 가렌 5 행 + `ChampionTable.cpp` 가렌 1 행 | 2 파일 | **POD 데이터** |
| 4. 로직 (System) | 별도 System 불필요 — Scene_InGame castFrame hook + `ApplyGarenHit` | `Scene_InGame.cpp` | **Scene 통합** |
| 5. 연출 (FxPreset) | `GarenFxPresets.h/.cpp` (Q trail / W shield / E spin / R sword) | 신규 2 파일 | **함수 헬퍼** |
| 6. 통합 (Scene) | OnEnter Init + player 분기 + BanPick 버튼 + castFrame hook + vcxproj 등록 | `Scene_InGame.cpp` + `Scene_BanPick.cpp` + `Client.vcxproj` | **Scene** |

> **OOP vs ECS 경계 자가 질문**:
> - Q. `ModelRenderer` 는 왜 Scene 멤버 (OOP) 인가? → A. 자원 수명 = Scene 수명. 인스턴스별 스키닝 메시 GPU 자원이라 Component 로 분산하면 zero-copy 못 함. (= `Layer 1`)
> - Q. `ChampionComponent` 는 왜 ECS 인가? → A. team/hp/mana 같은 **상태**는 시스템 (전투/AI/네트워크 동기화) 에서 일괄 순회되므로 Component. (= `Layer 2`)
> - Q. `SkillTable` 은 왜 POD 인가? → A. 정의 (정적 데이터) 는 RAM 한 번만 → 챔프 X 슬롯 매트릭스. 직렬화 가능. Phase 7 에서 Lua 이관. (= `Layer 3`)
> - Q. `ChampionTable` 은 왜 따로 있나? → A. **챔프별 메타데이터** (animPrefix/idle/run/평타) 는 SkillDef 와 별도 차원. SkillDef 가 슬롯 단위, ChampionDef 가 챔프 단위. (= `Layer 3`)
> - Q. `GarenFxPresets` 는 왜 함수 네임스페이스 인가? → A. **함수 = 1회성 연출 트리거**. 클래스화하면 인스턴스 수명 관리 비용 ↑, OOP 끼워맞추기. (= `Layer 5`)

---

## 2. 계단식 검증 마일스톤

```
Stage 3.5 (ChampionTable 1행)  ─┐  ★ v2 신설 — 누락 시 스킬 전체 무반응
Stage 4-1 (멤버 + Init)        │
Stage 4-2 (Update/Render/Sync) │  ★ v2 추가 — 누락 시 모델 안 보임
Stage 4-3 (CreateECSEntities)  ├─ F5 #1: 가렌 모델 + idle/run 애니 표시
Stage 4-4 (player 분기)        │  (스킬 없이, BanPick 진입 후 카메라 follow 확인)
Stage 5  (BanPick 버튼)        ─┘

Stage 6  (SkillTable 5행, W=1.f) ─┐
Stage 7  (GarenFxPresets + vcxproj) ├─ F5 #2: BA + Q/W/E/R 동작 + FX 출력
Stage 8  (castFrame hook + Hit)  ─┘
```

각 마일스톤 통과 후 다음 단계 진입. 빌드 실패 / 런타임 크래시 시 직전 단계로 롤백.

---

## 3. Stage 3.5 — ChampionTable.cpp 가렌 1행 (★ v2 신설, P0)

**근거**: `Scene_InGame.cpp` L1585/L1984 `FindChampionDef(champ)` 가 nullptr 반환 시 `ApplyLocalPrediction` 의 애니 재생 / `m_pActiveSkillDef` 세팅 / castFrame 감시 전부 **건너뜀**. SkillTable 5 행을 넣어도 스킬 무반응.

### `ChampionDef` 구조 (`Client/Public/GameObject/ChampionDef.h:4-12`)
```cpp
struct ChampionDef
{
    eChampion   id = eChampion::END;
    const char* animPrefix = "";          // "irelia_" / "yasuo_"
    const char* idleAnimKey = "idle1";
    const char* runAnimKey = "run";
    const char* basicAttackKey = "attack_01";
    f32_t basicAttackRange = 6.f;
};
```

**핵심**: `animPrefix=""` 로 두면 `string(cd->animPrefix) + key` (Scene_InGame.cpp:L1593) 가 SkillTable 풀 키 그대로 흘림. 가렌은 `garen_2013_*` / `garen_base_*` 두 prefix 혼재라 빈 prefix + SkillTable 풀키가 정답.

### before (`Client/Private/GameObject/ChampionTable.cpp:3-8`)
```cpp
static const ChampionDef s_ChampionTable[] =
{
    { eChampion::IRELIA,  "irelia_",  "idle1", "run", "attack_01" },
    { eChampion::YASUO,   "yasuo_",   "idle1", "run", "attack1"   },
    { eChampion::KALISTA, "kalista_", "idle1", "run", "attack1"   },
};
```

### after
```cpp
static const ChampionDef s_ChampionTable[] =
{
    { eChampion::IRELIA,  "irelia_",  "idle1", "run", "attack_01" },
    { eChampion::YASUO,   "yasuo_",   "idle1", "run", "attack1"   },
    { eChampion::KALISTA, "kalista_", "idle1", "run", "attack1"   },
    // ── Phase B-8 가렌 — animPrefix="" + SkillTable 풀키 (garen_2013_* / garen_base_* 혼재) ──
    { eChampion::GAREN,   "",         "garen_2013_idle1", "garen_2013_run", "garen_2013_attack1", 1.5f },
};
```

**검증 포인트**: `FindChampionDef(eChampion::GAREN)` 가 nullptr 아님 — Scene_InGame.cpp L1585 디버거 확인.

---

## 4. Stage 4 — Scene_InGame 가렌 인스턴스 (Layer 1·2·6)

### 4.1 Scene_InGame.h — 멤버 추가 2 곳

**before** (`Client/Public/Scene/Scene_InGame.h:147-152`):
```cpp
    // Champions (5) — 기존 유지
    ModelRenderer   m_Irelia;   CTransform m_IreliaTransform;
    ModelRenderer   m_Yasuo;    CTransform m_YasuoTransform;
    ModelRenderer   m_Sylas;    CTransform m_SylasTransform;
    ModelRenderer   m_Viego;    CTransform m_ViegoTransform;
    ModelRenderer   m_Kalista;  CTransform m_KalistaTransform;
```

**after**:
```cpp
    // Champions (6) — Phase B-8 가렌 추가
    ModelRenderer   m_Irelia;   CTransform m_IreliaTransform;
    ModelRenderer   m_Yasuo;    CTransform m_YasuoTransform;
    ModelRenderer   m_Sylas;    CTransform m_SylasTransform;
    ModelRenderer   m_Viego;    CTransform m_ViegoTransform;
    ModelRenderer   m_Kalista;  CTransform m_KalistaTransform;
    ModelRenderer   m_Garen;    CTransform m_GarenTransform;
```

**before** (`Scene_InGame.h:272-278`):
```cpp
    EntityID m_IreliaEntity = NULL_ENTITY;
    EntityID m_YasuoEntity = NULL_ENTITY;
    EntityID m_SylasEntity = NULL_ENTITY;
    EntityID m_ViegoEntity = NULL_ENTITY;
    EntityID m_KalistaEntity = NULL_ENTITY;
    EntityID m_PlayerEntity = NULL_ENTITY;
    EntityID m_MapEntity = NULL_ENTITY;
```

**after**:
```cpp
    EntityID m_IreliaEntity = NULL_ENTITY;
    EntityID m_YasuoEntity = NULL_ENTITY;
    EntityID m_SylasEntity = NULL_ENTITY;
    EntityID m_ViegoEntity = NULL_ENTITY;
    EntityID m_KalistaEntity = NULL_ENTITY;
    EntityID m_GarenEntity = NULL_ENTITY;
    EntityID m_PlayerEntity = NULL_ENTITY;
    EntityID m_MapEntity = NULL_ENTITY;
```

**(선택) 메서드 선언 추가** — `ApplyGarenHit` 를 cpp 에 둘 거면 헤더 private 섹션에 forward 선언 (Stage 8 참조):
```cpp
    void ApplyGarenHit(EntityID target, f32_t fDamage);
```

### 4.2 Scene_InGame.cpp::OnEnter — Init + LoadMeshTexture (Layer 1)

**삽입 위치**: `Scene_InGame.cpp:188` 직후 (Kalista Init 블록 끝). Cube transform 윗줄 (L190) 직전.

**after**:
```cpp
    m_KalistaTransform.SetPosition(15.f, 1.f, 0.f);
    m_KalistaTransform.SetScale(0.01f);

    // ── Phase B-8 가렌 ─────────────────────────────────────
    // Material: [0] 단일 머티리얼 (가렌 원작 통합 텍스처)
    m_Garen.Init("Client/Bin/Resource/Texture/Character/Garen/garen.fbx",
        L"Shaders/Mesh3D.hlsl");
    m_Garen.LoadMeshTexture(0, L"Client/Bin/Resource/Texture/Character/Garen/garen_base_tx_cm.png");
    m_GarenTransform.SetPosition(18.f, 1.f, 0.f);    // 칼리스타(15) 옆
    m_GarenTransform.SetScale(0.01f);
    // ──────────────────────────────────────────────────────

    m_CubeTransform.SetPosition(21.f, 3.f, 0.f);
```

**서브메시 텍스처**: 가렌 FBX 다중 머티리얼이면 슬롯 N 만큼 LoadMeshTexture(i, ...) 추가. 1차는 슬롯 0 가정.

### 4.3 Scene_InGame.cpp::OnEnter — player 분기 추가 (Layer 6)

**before** (`Scene_InGame.cpp:212-218`):
```cpp
        else if (champ == eChampion::KALISTA)
        {
            m_pPlayerRenderer = &m_Kalista;
            m_pPlayerTransform = &m_KalistaTransform;
            m_pPlayerIdleAnim = "kalista_idle1";
            m_pPlayerRunAnim = "kalista_run";
        }
```

**after**:
```cpp
        else if (champ == eChampion::KALISTA)
        {
            m_pPlayerRenderer = &m_Kalista;
            m_pPlayerTransform = &m_KalistaTransform;
            m_pPlayerIdleAnim = "kalista_idle1";
            m_pPlayerRunAnim = "kalista_run";
        }
        else if (champ == eChampion::GAREN)
        {
            m_pPlayerRenderer = &m_Garen;
            m_pPlayerTransform = &m_GarenTransform;
            m_pPlayerIdleAnim = "garen_2013_idle1";   // FBX 내 anim key
            m_pPlayerRunAnim = "garen_2013_run";
        }
```

### 4.4 Scene_InGame.cpp::OnEnter — idle 명시 기동 (Layer 1)

**after** (`Scene_InGame.cpp:233` 다음 줄):
```cpp
        m_Kalista.PlayAnimationByName("kalista_idle1");
        m_Garen.PlayAnimationByName("garen_2013_idle1");   // bind pose 탈출
```

### 4.5 Scene_InGame.cpp::SyncECSTransformsFromLegacy — push 추가 ★ v2

**근거**: ECS TransformComponent 동기화 누락 시 ECS 시스템 (피킹/AI/타겟팅) 이 가렌 위치 못 봄.

**before** (`Scene_InGame.cpp:617-622`):
```cpp
    push(m_IreliaEntity,  m_IreliaTransform);
    push(m_YasuoEntity,   m_YasuoTransform);
    push(m_SylasEntity,   m_SylasTransform);
    push(m_ViegoEntity,   m_ViegoTransform);
    push(m_KalistaEntity, m_KalistaTransform);
    push(m_MapEntity,     m_MapTransform);
```

**after**:
```cpp
    push(m_IreliaEntity,  m_IreliaTransform);
    push(m_YasuoEntity,   m_YasuoTransform);
    push(m_SylasEntity,   m_SylasTransform);
    push(m_ViegoEntity,   m_ViegoTransform);
    push(m_KalistaEntity, m_KalistaTransform);
    push(m_GarenEntity,   m_GarenTransform);
    push(m_MapEntity,     m_MapTransform);
```

### 4.6 Scene_InGame.cpp::OnUpdate — 애니 Update 추가 ★ v2

**근거**: `m_Garen.Update(dt)` 누락 시 애니 진행 안 됨 → bind pose 정지.

**before** (`Scene_InGame.cpp:1156-1160`):
```cpp
    m_Irelia.Update(dt);
    m_Yasuo.Update(dt);
    m_Sylas.Update(dt);
    m_Viego.Update(dt);
    m_Kalista.Update(dt);
```

**after**:
```cpp
    m_Irelia.Update(dt);
    m_Yasuo.Update(dt);
    m_Sylas.Update(dt);
    m_Viego.Update(dt);
    m_Kalista.Update(dt);
    m_Garen.Update(dt);
```

### 4.7 Scene_InGame.cpp::OnRender — UpdateCamera/Transform/Render 추가 ★ v2

**근거**: 누락 시 화면에 안 보임 (모델 자체가 draw call 안 들어감).

**before** (`Scene_InGame.cpp:1342-1360`):
```cpp
    m_Irelia.UpdateCamera(vp);
    m_Irelia.UpdateTransform(m_IreliaTransform.GetWorldMatrix());
    m_Irelia.Render();

    m_Yasuo.UpdateCamera(vp);
    m_Yasuo.UpdateTransform(m_YasuoTransform.GetWorldMatrix());
    m_Yasuo.Render();

    m_Sylas.UpdateCamera(vp);
    m_Sylas.UpdateTransform(m_SylasTransform.GetWorldMatrix());
    m_Sylas.Render();

    m_Viego.UpdateCamera(vp);
    m_Viego.UpdateTransform(m_ViegoTransform.GetWorldMatrix());
    m_Viego.Render();

    m_Kalista.UpdateCamera(vp);
    m_Kalista.UpdateTransform(m_KalistaTransform.GetWorldMatrix());
    m_Kalista.Render();
```

**after**:
```cpp
    m_Irelia.UpdateCamera(vp);
    m_Irelia.UpdateTransform(m_IreliaTransform.GetWorldMatrix());
    m_Irelia.Render();

    m_Yasuo.UpdateCamera(vp);
    m_Yasuo.UpdateTransform(m_YasuoTransform.GetWorldMatrix());
    m_Yasuo.Render();

    m_Sylas.UpdateCamera(vp);
    m_Sylas.UpdateTransform(m_SylasTransform.GetWorldMatrix());
    m_Sylas.Render();

    m_Viego.UpdateCamera(vp);
    m_Viego.UpdateTransform(m_ViegoTransform.GetWorldMatrix());
    m_Viego.Render();

    m_Kalista.UpdateCamera(vp);
    m_Kalista.UpdateTransform(m_KalistaTransform.GetWorldMatrix());
    m_Kalista.Render();

    m_Garen.UpdateCamera(vp);
    m_Garen.UpdateTransform(m_GarenTransform.GetWorldMatrix());
    m_Garen.Render();
```

### 4.8 Scene_InGame.cpp::CreateECSEntities — Entity 생성 (Layer 2)

**before** (`Scene_InGame.cpp:517-518`):
```cpp
    m_IreliaEntity = CreateChampionEntity(m_Irelia, m_IreliaTransform, eChampion::IRELIA, eTeam::Blue);
    m_KalistaEntity = CreateChampionEntity(m_Kalista, m_KalistaTransform, eChampion::KALISTA, eTeam::Blue);
    m_YasuoEntity = CreateChampionEntity(m_Yasuo, m_YasuoTransform, eChampion::YASUO, eTeam::Blue);
```

**after**:
```cpp
    m_IreliaEntity = CreateChampionEntity(m_Irelia, m_IreliaTransform, eChampion::IRELIA, eTeam::Blue);
    m_KalistaEntity = CreateChampionEntity(m_Kalista, m_KalistaTransform, eChampion::KALISTA, eTeam::Blue);
    m_YasuoEntity = CreateChampionEntity(m_Yasuo, m_YasuoTransform, eChampion::YASUO, eTeam::Blue);
    m_GarenEntity = CreateChampionEntity(m_Garen, m_GarenTransform, eChampion::GAREN, eTeam::Blue);
```

**before** (`Scene_InGame.cpp:543-544`):
```cpp
    m_World.AddComponent<SkillStateComponent>(m_IreliaEntity);
    m_World.AddComponent<SkillStateComponent>(m_KalistaEntity);
```

**after**:
```cpp
    m_World.AddComponent<SkillStateComponent>(m_IreliaEntity);
    m_World.AddComponent<SkillStateComponent>(m_KalistaEntity);
    m_World.AddComponent<SkillStateComponent>(m_GarenEntity);
```

### 4.9 Scene_InGame.cpp::CreateECSEntities — Player 분기 (Layer 6)

**before** (`Scene_InGame.cpp:568-571`):
```cpp
    eChampion champ = CGameInstance::Get()->Get_GameContext().SelectedChampion;
    if      (champ == eChampion::IRELIA) m_PlayerEntity = m_IreliaEntity;
    else if (champ == eChampion::YASUO)  m_PlayerEntity = m_YasuoEntity;
    else if (champ == eChampion::KALISTA) m_PlayerEntity = m_KalistaEntity;
```

**after**:
```cpp
    eChampion champ = CGameInstance::Get()->Get_GameContext().SelectedChampion;
    if      (champ == eChampion::IRELIA) m_PlayerEntity = m_IreliaEntity;
    else if (champ == eChampion::YASUO)  m_PlayerEntity = m_YasuoEntity;
    else if (champ == eChampion::KALISTA) m_PlayerEntity = m_KalistaEntity;
    else if (champ == eChampion::GAREN)  m_PlayerEntity = m_GarenEntity;
```

---

## 5. Stage 5 — BanPick Garen 버튼 (Layer 6)

**before** (`Client/Private/Scene/Scene_BanPick.cpp:77-91`):
```cpp
        ImGui::SameLine();
        //칼리스타 캐릭터 선택!
        if (ImGui::Button("Kalista", ImVec2(150.f, 60.f)))
        {
            CGameInstance::Get()->Get_GameContext().SelectedChampion = eChampion::KALISTA;
            auto pLoadingMatch = CScene_MatchLoading::Create(
                []() -> std::unique_ptr<IScene> {
                    return std::unique_ptr<IScene>(new CScene_InGame());
                }, 3.f);
            CGameInstance::Get()->Change_Scene((uint32_t)eSceneID::MatchLoading, std::move(pLoadingMatch));
            ImGui::End();
            return;
        }
    }
```

**after**:
```cpp
        ImGui::SameLine();
        //칼리스타 캐릭터 선택!
        if (ImGui::Button("Kalista", ImVec2(150.f, 60.f)))
        {
            CGameInstance::Get()->Get_GameContext().SelectedChampion = eChampion::KALISTA;
            auto pLoadingMatch = CScene_MatchLoading::Create(
                []() -> std::unique_ptr<IScene> {
                    return std::unique_ptr<IScene>(new CScene_InGame());
                }, 3.f);
            CGameInstance::Get()->Change_Scene((uint32_t)eSceneID::MatchLoading, std::move(pLoadingMatch));
            ImGui::End();
            return;
        }

        ImGui::SameLine();
        // ── Phase B-8 가렌 ──
        if (ImGui::Button("Garen", ImVec2(150.f, 60.f)))
        {
            CGameInstance::Get()->Get_GameContext().SelectedChampion = eChampion::GAREN;
            auto pLoadingMatch = CScene_MatchLoading::Create(
                []() -> std::unique_ptr<IScene> {
                    return std::unique_ptr<IScene>(new CScene_InGame());
                }, 3.f);
            CGameInstance::Get()->Change_Scene((uint32_t)eSceneID::MatchLoading, std::move(pLoadingMatch));
            ImGui::End();
            return;   // ★ self-destruct 방지 (CLAUDE.md gotcha — Change_Scene 직후)
        }
    }
```

---

## ⏸ F5 #1 검증 (Stage 3.5 + 4 + 5 후)

**기대**:
1. BanPick UI 에 `Garen` 버튼 표시
2. `Garen` 클릭 → MatchLoading → InGame 전환
3. 가렌 모델 표시 (위치 18, 1, 0)
4. idle 애니 재생 (`garen_2013_idle1`)
5. 우클릭 이동 → run 애니 (`garen_2013_run`)
6. 카메라 follow 동작
7. **스킬 입력 (Q/W/E/R) 무반응** — Stage 6 미진행이라 정상

**실패 패턴**:
- 모델 안 보임 → ① OnRender 추가 누락 (Stage 4.7) ② FBX 경로 오타
- 애니 정지 → OnUpdate 추가 누락 (Stage 4.6)
- ECS 피킹 안 됨 → SyncECSTransformsFromLegacy 누락 (Stage 4.5)
- 텍스처 흰색/검정 → PNG 경로 / WIC 로드 실패
- 애니 키 매칭 실패 → `garen_2013_idle1` FBX 내 실재 확인 (filesystem `garen_2013_idle1.anm` ✅ 존재)
- 카메라 안 따라옴 → player 분기 GAREN 누락 (Stage 4.3)

---

## 6. Stage 6 — SkillTable 가렌 5행 (Layer 3)

**LoL 원작 가렌 스킬**:
- BA: 평타 (`garen_2013_attack1`)
- Q (Decisive Strike): 다음 평타 강화 + 무빙스피드 + 침묵. `garen_2013_spell1`
- W (Courage): 방어막 + 데미지 감소. `garen_2013_channel`
- E (Judgment): 회전 칼날 (영역 데미지). `garen_base_spell3_0` (4 방향 변형 존재)
- R (Demacian Justice): 단일 처형. `garen_2013_spell4`

**castFrame/recoveryFrame 24 FPS 기준 1차 추정** — F5 검증 후 정정.

**삽입 위치**: `Client/Private/GameObject/SkillTable.cpp:164` 직후 (Kalista R 끝) `};` 직전.

**after** (Kalista R `},` 다음):
```cpp
        { eChampion::KALISTA, 4, eTargetMode::Self,
          120.f, 0.f, 100.f,
          "spell4_call", nullptr, nullptr,
          0.5f, true, eRotateMode::None,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f },

        // ── Garen ──────────────────────────────────────────────
        // BA — 평타 (단순 근접)
        { eChampion::GAREN, 0, eTargetMode::UnitTarget,
          0.6f, 1.5f, 0.f,
          "garen_2013_attack1", nullptr, nullptr,
          1.0f, true, eRotateMode::TowardsTarget,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          //castFrame, recoveryFrame, stage2CastFrame, stage2RecoveryFrame
          6.f, 14.f, 0.f, 0.f,
          //animPlaySpeed, stage2PlaySpeed
          1.0f, 1.f },

        // Q — Decisive Strike (다음 평타 강화 + 이동 부스트)
        { eChampion::GAREN, 1, eTargetMode::Self,
          8.f, 0.f, 0.f,
          "garen_2013_spell1", nullptr, nullptr,
          0.6f, true, eRotateMode::None,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          4.f, 10.f, 0.f, 0.f,
          1.0f, 1.f },

        // W — Courage (방어막 + 데미지 감소)
        // ★ v2 정정: castFrame=1.f (0 이면 cast hook miss — Scene_InGame.cpp:L720)
        { eChampion::GAREN, 2, eTargetMode::Self,
          24.f, 0.f, 0.f,
          "garen_2013_channel", nullptr, nullptr,
          0.5f, true, eRotateMode::None,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          1.f, 8.f, 0.f, 0.f,   // castFrame=1.f (★ v2)
          1.0f, 1.f },

        // E — Judgment (회전 칼날, 영역 데미지)
        // 회전 모션이라 castFrame 다중 발생 가능 — 1차는 단일 hit
        { eChampion::GAREN, 3, eTargetMode::Self,
          9.f, 1.65f, 0.f,
          "garen_base_spell3_0", nullptr, nullptr,
          3.0f, true, eRotateMode::None,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          12.f, 60.f, 0.f, 0.f,
          1.0f, 1.f },

        // R — Demacian Justice (처형)
        { eChampion::GAREN, 4, eTargetMode::UnitTarget,
          120.f, 4.f, 100.f,
          "garen_2013_spell4", nullptr, nullptr,
          1.5f, true, eRotateMode::TowardsTarget,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          24.f, 36.f, 0.f, 0.f,
          1.0f, 1.f },
};
```

**부등식 검증 (CLAUDE.md gotcha — `lockDuration × animPlaySpeed ≥ recoveryFrame / FBX_FPS`)**:
- BA: `1.0 × 1.0 = 1.0s ≥ 14/24 = 0.583s` ✅
- Q:  `0.6 × 1.0 = 0.6s ≥ 10/24 = 0.417s` ✅
- W:  `0.5 × 1.0 = 0.5s ≥ 8/24 = 0.333s` ✅
- E:  `3.0 × 1.0 = 3.0s ≥ 60/24 = 2.5s` ✅
- R:  `1.5 × 1.0 = 1.5s ≥ 36/24 = 1.5s` ✅ (경계, 마진 추가 권장)

---

## 7. Stage 7 — GarenFxPresets + vcxproj 등록 (Layer 5)

### 7.1 신규 파일 — `Client/Public/GameObject/Champion/Garen/GarenFxPresets.h`
```cpp
#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

// ─────────────────────────────────────────────────────────────
//  GarenFxPresets — Phase B-8 가렌 FX 함수 헬퍼
//
//  Layer 5 (연출). 함수 = 1회성 트리거. 클래스화 X.
//  KalistaFxPresets 동일 패턴.
// ─────────────────────────────────────────────────────────────
namespace GarenFx
{
    // Q — Decisive Strike: 무기 트레일
    void SpawnQTrail(CWorld& world, EntityID owner, f32_t fLifetime);

    // W — Courage: 방어막 (caster attach)
    void SpawnWShield(CWorld& world, EntityID owner, f32_t fDuration);

    // E — Judgment: 회전 칼날 (caster attach, spinning mesh)
    EntityID SpawnESpinBlade(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, f32_t fDuration);

    // R — Demacian Justice: 처형 검 (target 머리 위)
    void SpawnRSword(CWorld& world, EntityID target, f32_t fLifetime);
}
```

### 7.2 신규 파일 — `Client/Private/GameObject/Champion/Garen/GarenFxPresets.cpp`

**v2 정정**: `kPath*` 경로를 실제 추출 위치 (`particles/fbx/`, `particles/`) 로. 가렌 머티리얼 PNG 사용 (칼리스타 텍스처 임시 차용 X).

```cpp
#include "GameObject/Champion/Garen/GarenFxPresets.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"
#include <cmath>

namespace
{
    // ★ v2: 실제 가렌 추출 구조 — particles/fbx/, particles/ 하위
    constexpr const char* kPathESpinFbx =
        "Client/Bin/Resource/Texture/FX/Garen/particles/fbx/garen_base_e_spin.fbx";
    constexpr const char* kPathWShieldFbx =
        "Client/Bin/Resource/Texture/FX/Garen/particles/fbx/garen_base_w_shield.fbx";
    constexpr const char* kPathRSwordFbx =
        "Client/Bin/Resource/Texture/FX/Garen/particles/fbx/garen_base_r_sword_plane.fbx";

    // 1차는 가렌 base aura/ball 텍스처 사용 — 정확한 머티리얼 매핑은 RenderDoc 검증 후 확정
    constexpr const wchar_t* kPathQTrailTex =
        L"Client/Bin/Resource/Texture/FX/Garen/particles/garen_aura_self.png";
    constexpr const wchar_t* kPathWShieldTex =
        L"Client/Bin/Resource/Texture/FX/Garen/particles/garen_aura_self_02.png";
    constexpr const wchar_t* kPathRSwordTex =
        L"Client/Bin/Resource/Texture/FX/Garen/particles/garen_ball01.png";
}

void GarenFx::SpawnQTrail(CWorld& world, EntityID owner, f32_t fLifetime)
{
    if (owner == NULL_ENTITY)
        return;

    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.5f, 0.f };
    fx.texturePath = kPathQTrailTex;
    fx.fWidth = 1.5f;
    fx.fHeight = 1.5f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 1.0f, 0.9f, 0.4f, 1.0f };  // 데마시아 황금
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fLifetime * 0.4f;
    CFxSystem::Spawn(world, fx);
}

void GarenFx::SpawnWShield(CWorld& world, EntityID owner, f32_t fDuration)
{
    if (owner == NULL_ENTITY)
        return;

    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.0f, 0.f };
    fx.texturePath = kPathWShieldTex;
    fx.fWidth = 2.5f;
    fx.fHeight = 2.5f;
    fx.bBillboard = true;
    fx.fLifetime = fDuration;
    fx.vColor = { 0.8f, 0.7f, 0.3f, 0.7f };
    fx.blendMode = eBlendPreset::AlphaBlend;
    fx.fFadeOut = fDuration * 0.3f;
    CFxSystem::Spawn(world, fx);
}

EntityID GarenFx::SpawnESpinBlade(CWorld& /*world*/, Engine::CFxStaticMeshRenderer* /*pRenderer*/,
    EntityID /*owner*/, f32_t /*fDuration*/)
{
    // Phase B-8 1차 stub — FBX 검증 후 활성화.
    //   `kPathESpinFbx` 메시 + spinning rotation + caster attach.
    //   현재는 castFrame hook 호출만 받고 시각 효과 없음 (안전 stub).
    return NULL_ENTITY;
}

void GarenFx::SpawnRSword(CWorld& world, EntityID target, f32_t fLifetime)
{
    if (target == NULL_ENTITY)
        return;

    FxBillboardComponent fx{};
    fx.attachTo = target;
    fx.vAttachOffset = { 0.f, 3.0f, 0.f };
    fx.texturePath = kPathRSwordTex;
    fx.fWidth = 1.5f;
    fx.fHeight = 3.0f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 1.5f, 1.3f, 0.5f, 1.0f };
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fLifetime * 0.5f;
    CFxSystem::Spawn(world, fx);
}
```

### 7.3 vcxproj / .filters 수동 등록 ★ v2

**필요 이유**: Visual Studio 수동 등록 없이 추가하면 cpp 가 빌드에 안 들어가고 `GarenFx::Spawn*` 미해결 외부 심볼 (LNK2019) 발생.

**`Client/Client.vcxproj`** — 다음 두 항목 추가:
```xml
  <ItemGroup>
    <ClInclude Include="Public\GameObject\Champion\Garen\GarenFxPresets.h" />
    <!-- ... -->
  </ItemGroup>
  <ItemGroup>
    <ClCompile Include="Private\GameObject\Champion\Garen\GarenFxPresets.cpp" />
    <!-- ... -->
  </ItemGroup>
```

**`Client/Client.vcxproj.filters`** — 동일 항목에 필터 분류 (KalistaFxPresets 검색 후 옆에 미러):
```xml
    <ClInclude Include="Public\GameObject\Champion\Garen\GarenFxPresets.h">
      <Filter>02. GameObject\Champion\Garen</Filter>
    </ClInclude>
    <ClCompile Include="Private\GameObject\Champion\Garen\GarenFxPresets.cpp">
      <Filter>02. GameObject\Champion\Garen</Filter>
    </ClCompile>
    <Filter Include="02. GameObject\Champion\Garen">
      <UniqueIdentifier>{새 GUID}</UniqueIdentifier>
    </Filter>
```

**검증**: 빌드 시 `[ClCompile] GarenFxPresets.cpp` 가 출력 로그에 보여야 함. 안 보이면 vcxproj 등록 누락.

---

## 8. Stage 8 — Scene_InGame castFrame hook + ApplyGarenHit (Layer 6)

### 8.1 castFrame hook 분기

**삽입 위치**: `Scene_InGame.cpp:808` 직후 (Kalista Q 분기 끝, `}` 다음).

**after**:
```cpp
            if (bCastHit)
            {
                m_bCastFrameFired = true;
                using namespace Engine;
                const eChampion champCur = CGameInstance::Get()->Get_GameContext().SelectedChampion;
                if (champCur == eChampion::KALISTA && /* slot==0 */) { /* BA — 기존 */ }
                else if (champCur == eChampion::KALISTA && /* slot==1 */) { /* Q — 기존 */ }

                // ── Phase B-8 가렌 castFrame hooks ──
                else if (champCur == eChampion::GAREN
                    && m_pActiveSkillDef
                    && m_pPlayerTransform)
                {
                    const i32_t slot = m_pActiveSkillDef->slot;
                    if (slot == 0)        // BA — 평타 hit
                    {
                        const EntityID target = m_ActiveSkillCommandStorage.targetEntityId;
                        if (target != NULL_ENTITY)
                            ApplyGarenHit(target, 60.f);     // 1차 추정 데미지
                    }
                    else if (slot == 1)   // Q — 트레일 + 다음 BA 강화 (1차는 트레일만)
                    {
                        GarenFx::SpawnQTrail(m_World, m_PlayerEntity, 0.5f);
                    }
                    else if (slot == 2)   // W — 방어막
                    {
                        GarenFx::SpawnWShield(m_World, m_PlayerEntity, 1.5f);
                    }
                    else if (slot == 3)   // E — 회전 (1차 stub)
                    {
                        GarenFx::SpawnESpinBlade(m_World, m_pFxMeshRenderer.get(), m_PlayerEntity, 3.0f);
                    }
                    else if (slot == 4)   // R — 처형
                    {
                        const EntityID target = m_ActiveSkillCommandStorage.targetEntityId;
                        if (target != NULL_ENTITY)
                        {
                            GarenFx::SpawnRSword(m_World, target, 1.0f);
                            ApplyGarenHit(target, 250.f);    // 처형 (큰 데미지)
                        }
                    }
                }
            }
```

### 8.2 `ApplyGarenHit` 신설 ★ v2

**근거**: `ApplyYasuoHit` (L2454) 패턴 — 챔프별 hit 함수. 가렌 BA/R 데미지 적용 채널이 없으면 hp 감소 안 일어남.

**Scene_InGame.h** (private 섹션):
```cpp
    void ApplyGarenHit(EntityID target, f32_t fDamage);
```

**Scene_InGame.cpp** — `ApplyYasuoHit` 정의 (L2454) 옆에 추가:
```cpp
void CScene_InGame::ApplyGarenHit(EntityID target, f32_t fDamage)
{
    if (target == NULL_ENTITY) return;
    if (!m_World.HasComponent<HealthComponent>(target)) return;

    auto& hp = m_World.GetComponent<HealthComponent>(target);
    hp.fCurrent = (hp.fCurrent > fDamage) ? (hp.fCurrent - fDamage) : 0.f;

    char buf[128];
    sprintf_s(buf, "[GarenHit] target=%u dmg=%.1f hp=%.1f/%.1f\n",
        static_cast<u32_t>(target), fDamage, hp.fCurrent, hp.fMax);
    OutputDebugStringA(buf);
}
```

**참고**: `HealthComponent` 필드명이 다를 수 있음 — 실제 정의 (`ECS/Components/HealthComponent.h`) 확인 후 맞출 것. `ApplyYasuoHit` 본문 그대로 미러하면 안전.

### 8.3 include 추가

`Scene_InGame.cpp` 상단:
```cpp
#include "GameObject/Champion/Garen/GarenFxPresets.h"
```

---

## ⏸ F5 #2 검증 (Stage 6 + 7 + 8 후) ★ v2 기대값 정정

**기대 (v2 정정 — Apply*Hit 통한 hp 감소까지 명시)**:
1. BA 우클릭 → `garen_2013_attack1` 애니 재생 + `[GarenHit]` 로그 + 사일러스 hp 감소 (60)
2. Q 키 → `garen_2013_spell1` 애니 + 노란 트레일 빌보드
3. W 키 → `garen_2013_channel` 애니 + 황금 방어막 빌보드
4. E 키 → `garen_base_spell3_0` 회전 (3초 lock, FX 시각은 stub — 안전 무동작)
5. **R = 사일러스 호버 + R 키** (호버 + 클릭 X) → `garen_2013_spell4` + 사일러스 머리 위 황금 검 + `[GarenHit]` 로그 + hp 감소 (250)

**1차 통과 시 hp 감소 메커니즘 미동작이라도 OK** (애니 + FX 만 출력되면 통과). `ApplyGarenHit` 가 `HealthComponent` 필드명 미스매치 시 컴파일 에러 → 즉시 정정.

**실패 패턴**:
- 애니 키 매칭 실패 → FBX 내 anim 이름 OutputDebugString 으로 덤프 (Substring 매칭 사용 중)
- castFrame hook 미발동 → ① ChampionTable 누락 (Stage 3.5) ② castFrame=0 (W 정정 확인) ③ `m_pActiveSkillDef->slot` 로그 + `m_bCastFrameFired` 확인
- W 만 hook 미발동 → SkillTable W castFrame 1.f 정정 확인
- LNK2019 `GarenFx::Spawn*` 미해결 → vcxproj 등록 (Stage 7.3)
- `garen_base_spell3_-180/-90/0/180` 4 방향 변형 — F5 후 회전 자연스럽지 않으면 4 분기 추가

---

## 9. Stage 9 — F5 풀 검증 + 후속 다듬기

| 항목 | 합격 기준 |
|------|---------|
| 모델 표시 | 가렌 위치 (18, 1, 0) bind pose 탈출 |
| Idle/Run | 정지 = idle1, 이동 = run |
| BA | 우클릭 → attack1 애니 + `[GarenHit]` 로그 (hp 감소 옵션) |
| Q | 키 → spell1 + 트레일 |
| W | 키 → channel + 방어막 (★ castFrame=1.f 검증) |
| E | 키 → spell3 + 3초 lock |
| R | 호버 + R 키 → spell4 + 황금 검 (hp 감소 옵션) |
| 6 레이어 즉답 | "왜 ModelRenderer 는 OOP / ChampionComponent 는 ECS / ChampionTable 은 SkillTable 과 별도" 사용자 즉답 |

---

## 10. 사이클 종료 후 갱신할 파일

1. **CLAUDE.md** L11-18 — `현재 진행`/`다음 세션` 갱신 (B-8 완료 → B-9 요네)
2. **CLAUDE.md** Phase B-8 Gotchas 신설 — 발견될 가능성 높은 항목:
   - **G-1**: `ChampionTable.cpp` 행 누락 시 ApplyLocalPrediction skip → 신규 챔프 체크리스트 1순위
   - **G-2**: Scene 수동 렌더 구조 — 멤버만으론 안 됨, Sync/Update/Render 3 곳 추가 필수
   - **G-3**: `castFrame > 0.f` 조건 — 0 이면 hook miss. 즉시 발동은 ApplyLocalPrediction 시점 사용
   - **G-4**: vcxproj/.filters 자동 스캔 없음 → 신규 cpp 수동 등록 (LNK2019 예방)
   - **G-5**: `ChampionDef::animPrefix=""` 트릭 — SkillTable 풀키 흘리기 (혼합 prefix 챔프 패턴)
3. **MEMORY.md** + 신규 메모 `project_phase_b8_garen.md`
4. **`.md/plan/Champion/03_GAREN_PHASE_B8_PIPELINE.md`** — 사이클 종료 후 학습 결과 부록 추가

---

## 11. 후속 사이클

- **B-9 요네**: 메쉬 분리 (스킬 사용 시 영혼/본체 분리). 가렌 통과 시 1.5h 목표.
- **B-10 잔여 챔프**: Annie/Ashe/Fiora/Riven/Jax/MasterYi/Kindred/Yone/Zed 일괄. 챔프당 30분 (B-8 gotcha 5건 박제 후 가능).
- **C-1 그래픽스**: GGX/Schlick/Smith/Cook-Torrance/Forward+ (별도 7-Stage 사이클).

---

## 한 줄 요약

**v2: Codex 비판 7건 반영. ChampionTable + Sync/Update/Render + W castFrame + FX 경로 + vcxproj + ApplyGarenHit + R 호버키. Stage 3.5 → 4 → 5 → F5 #1 → 6 → 7 → 8 → F5 #2. 4시간 목표.**
