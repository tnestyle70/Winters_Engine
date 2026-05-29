# 이렐리아 이후 챔프 파이프라인 마스터 로드맵

> **생성**: 2026-04-25 (이렐리아 Phase D Step 7 완료 직후)
> **목표**: 150 챔프 + 엘든링 보스 전체를 단일 파이프라인으로 확장
> **우선순위**: 이렐리아 FBX → Yasuo → Kalista → IChampionController → CMesh submesh API → Yone+6챔프 → ChampionCatalog → 전수 파싱

---

## Phase 순서

### ⭐ Phase 1 (최우선) — 이렐리아 FBX/PNG 파이프라인 도입

**목표**: `Client/Bin/Resource/Texture/FX/Irelia/fbx/` (8 FBX) + `render/` (8 PNG) 를 실제 3D 메쉬로 렌더.

**현재 상태**: `render/*.png` 를 FxBillboard 로 빌보드 사용 중 (Q/W/E/R). 시각이 "눕혀서 가로로" 보이는 한계.

**작업 범위**:
1. `CFxMeshComponent` 신규 — ECS 엔티티가 FBX 메쉬 렌더 (CModel + CTexture 조합)
2. `CIreliaBladeSystem::SpawnPlaced` 에서 FxBillboard 대신 FxMesh 스폰 (`e_blade.fbx` + `e_blade.png`)
3. E beam 도 `e_beam.fbx` 로 교체 (두 검 방향 회전 = 메쉬 yaw)
4. Q 트레일 (`q_dark_trail`), W stage2 (`w_blade_erode`), R 투사체 (`pulse_mesh_tex`) 는 당장 유지 — FBX 없음
5. `CModel::LoadModel` 에 static-mesh fast-path (Assimp 없이 GLB/FBX 직접) 검토. 또는 기존 Assimp 재사용 + FbxCache 도입

**성공 기준**: E 1/2타 시 실제 검 메쉬 지면 배치, 회전 반영. 그 뒤 동일 파이프라인으로 Q/W/R 의 `glow_lines` / `pulse_mesh_tex` 도 FBX 교체.

**세부 계획서**: `.md/plan/Champion/03_IRELIA_FBX_PIPELINE.md` (Phase 1 진입 시 작성)

---

### Phase 2 — 야스오 특화

**목표**: Phase D FxBillboard / StatusEffect / FxSystem 재사용. 야스오만의 로직:
- **Q Conditional**: `eTargetMode::Conditional` 이미 SkillTable 에 존재. 3타 회오리 분기 (`YasuoStateComponent.qStackCount` 기반)
- **Q stack 3타**: qStackCount=3 일 때 Direction AOE 원뿔 투사체 (회오리) + 넉업
- **E 이중점프**: 대상 엔티티 쪽으로 돌진. IreliaBladeComponent 패턴 응용 (단 2단 점프 조합)

**FX 에셋**: LoL 추출 → `Client/Bin/Resource/Texture/FX/Yasuo/` 정리 (현재 비어있음)

**Scene_InGame 변경**: 야스오 Q/W/E/R 에 castFrame 훅 (추가 필요 시) + ApplyLocalPrediction 야스오 분기 추가

---

### Phase 3 — 칼리스타 특화

**목표**: 원작 특수 메커니즘 구현
- **평타 이동 취소**: 평타 애니 중 우클릭 시 이동 interrupt
- **Q Pierce 투사체**: 일직선 관통. UltWaveSystem 확장 (pierce=true 옵션)
- **E Rend 박힘**: 적에게 E 창이 박혀 스택 누적. 새 ECS 컴포넌트 `KalistaRendComponent`

**자산**: `kalista_spear.glb` 별도 로드 — Model 파이프라인에 GLB submesh 통합 (기존 LOL_Resource 처리 방식)

**재사용**: IreliaBlade 의 쌍 관리 패턴 → Kalista Rend 스택 엔티티 관리

---

### Phase 4 — IChampionController 분리

**목표**: 3챔프 (Irelia/Yasuo/Kalista) 완성 후 공통 패턴 추출.

**인터페이스**:
```cpp
class IChampionController {
public:
    virtual void OnSkillCast(uint8_t slot, const CastSkillCommand& cmd) = 0;
    virtual void OnCastFrame(uint8_t slot) = 0;        // castFrame 도달 시 1회
    virtual void OnEndTransition(uint8_t slot) = 0;    // recoveryFrame 후 전환 시
    virtual ~IChampionController() = default;
};
```

**구현**:
- `CIreliaController`, `CYasuoController`, `CKalistaController`
- Scene_InGame 의 `ApplyLocalPrediction` / castFrame 훅이 챔프별 controller 에 위임
- `ChampionComponent` 에 `std::unique_ptr<IChampionController> controller`

**효과**: Scene_InGame 이 챔프 특수 로직으로 부풀어오르는 문제 해결. 새 챔프 추가 = Controller 1개 + SkillTable 엔트리만.

---

### Phase 5 — CMesh::name + submesh API (Yone 선결)

**목표**: 단일 FBX 안 여러 submesh 에 이름으로 접근. Yone 등 멀티 머티리얼 챔프 전제.

**계획서**: `.md/plan/Champion/01_MULTI_MATERIAL_CHAMPION_YONE.md` §4 이미 설계됨.

**변경**:
- `CMesh::GetName() const`, `CModel::FindSubmeshByName(const char*)` API 추가
- `LoadMeshTexture(const char* name, path)` 오버로드

---

### Phase 6 — Yone + 나머지 6챔프

**대상**: Yone / Fiora / Jax / Riven / Master Yi / Zed / Kindred

**작업량**: 챔프당 IChampionController + FX 에셋 + SkillTable 엔트리. 파이프라인 정착 후 챔프당 **~2시간** 목표.

**절차**:
1. LoL 추출 → FX PNG / FBX
2. ModelRenderer 로딩 + 텍스처 바인딩
3. SkillTable Irelia 블록 복사 → 챔프 치환
4. `CXxxController` 구현 (가장 시간 드는 부분)
5. 인게임 테스트

---

### Phase 7 — Part C: ChampionCatalog + ImGui 선택 + 봇 4

**목표**: 플레이어가 10 챔프 중 선택해서 인게임 진입.

**새 데이터**:
- `ChampionDef` 확장: `iconPath`, `splashArtPath`, `unlockable` 등
- ChampionCatalog 데이터 (JSON 또는 코드 테이블)
- Scene_ChampionSelect UI (ImGui)

**봇**: Annie/Ashe/Garen/Sylas 4체 — 단순 AI (근접 돌진 / 원거리 평타 / 탱커 / 암살)

**현재 Scene_InGame::CreateECSEntities** 의 하드코딩된 5챔프 배치 → ChampionCatalog 기반 동적 생성으로 교체.

---

### Phase 99 — 모든 캐릭터/이펙트 싹 다 파싱

**목표**: LoL 150+ 챔프 전원 포용. 엘든링 보스 60+.

**전제**:
- FBX/GLB 추출 자동화 (Tools/WintersAssetConverter.exe 확장 — 배치 모드)
- FX PNG 추출 자동화 (`scb` → `fbx` + `render/*.png` 추출기)
- Skill 데이터 수집 (JSON 또는 DB)

**스코프 가드**: Phase 99 는 "하루 수작업" 이 아니라 "자동화 도구 + 데이터 수집 인프라" 가 먼저. 파이프라인 먼저 완성.

---

## 의존 그래프

```
Phase 1 (Irelia FBX 파이프라인) ──┬─→ Phase 2 (Yasuo) ──┐
                                 └─→ Phase 3 (Kalista) ─┴─→ Phase 4 (IChampionController)
                                                              │
                                                              ├─→ Phase 5 (CMesh submesh API)
                                                              │    │
                                                              │    └─→ Phase 6 (Yone + 6챔프)
                                                              │
                                                              └─→ Phase 7 (ChampionCatalog + 봇)
                                                                     │
                                                                     └─→ Phase 99 (전수 파싱)
```

**크리티컬 패스**: Phase 1 → 2 → 3 → 4 → (5|7) → 6 → 99

---

## 다음 세션 진입 — 슬래시 명령어

`/phase-d-next` — 이 문서 + 최근 메모리 `project_session_2026_04_25.md` + Step 7 플랜 파일 전부 로드 + 이렐리아 FBX 파이프라인 Phase 1 착수.

상세는 `.claude/commands/phase-d-next.md` 참조.
