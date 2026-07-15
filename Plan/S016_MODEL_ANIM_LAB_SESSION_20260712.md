Session - 디자이너용 인게임 Model & Animation Lab(F5)을 반영한다: 챔피언 선택 -> 전체 클립 목록에서 선택 재생(루프/역재생/배속) -> Pause 틱 스크럽 -> 서브메시 분해(Solo/All/None), S015 SpawnChampion과 결합해 "모델 검사 -> 배치 -> 인게임 테스트" 루프를 닫는다.

작성: Claude 레인 / 2026-07-12 / 선행: S014(Pause/Step), S015(SpawnChampion). 조사: 구현급 에이전트 2종(클라 배선/Engine 표면), 전 앵커 rg 검증.
서버/GameSim 무변경 — 클라 표현 툴 전용 슬라이스 (Bot AI GameCommand 원칙 비접점).

---

## 0. 설계 결정 (조사 근거)

- **"인게임 vs 에디터" 판정: P0 = 인게임(연습 모드) 패널.** 근거: ① S014 Pause로 월드를 얼려놓고 관찰, ② S015 SpawnChampion으로 임의 챔피언 배치 후 그 자리에서 검사 → 실전 테스트까지 한 화면(사용자가 요구한 "실제 캐릭터 선택·배치" 연결), ③ F7/F9/F10과 동일 워크플로. **Unity 포폴식 격리 궤도 뷰어는 P1**: 자체 CWorld+궤도 카메라의 `Scene_AssetPreview` — 라이브 월드 안 클라 전용 격리는 2026-05-14 gotcha(서버 봇/이벤트 잔존) 위반 위험이라 별도 씬이 정도이며, 미사용 Cinematic 모듈(CSequencePlayer, Seek 보유)이 타임라인 백본 후보.
- **인덱스 기반 재생 필수**: `CModel::FindAnimationIndex`는 **부분 문자열 매칭**(첫 매치 승) — 이름 기반 재생은 다른 클립에 오염될 수 있어 툴은 인덱스로 다룬다(`PlayAnimationByIndexAdvanced` 신설).
- **DLL 경계**: Engine은 DLL이고 CModel/CAnimation/CAnimator는 비수출 — Client는 헤더 인라인 메서드만 직접 호출 가능(Phase T-2 관례). 클립 열거/인덱스 재생/서브메시 정보는 수출 클래스 `ModelRenderer`의 패스스루로 추가, 스크럽은 `CAnimator` 헤더 인라인 툴 세터 2개로 해결(dllexport 불요, UpdateLib 자동 동기화).
- **스크럽 원리**: `SetPlaySpeedRawForTool(0)` = 일시정지 — Update가 시간을 0만큼 진행하되 포즈는 매 프레임 재평가하므로 `SetCurrentTimeTicksForTool` 슬라이더가 즉시 반영된다. 기존 `SetPlaySpeed`는 |0.01| 클램프라 진짜 0 불가였음.
- **클립 선택 경합**: 로코모션(`UpdateNetworkChampionLocomotion`)은 상태 변화 시에만 클립을 재발행(정지 챔피언에선 패널 클립 생존), 액션 리플리케이션은 이벤트 시에만. 패널은 **이름 가드 매 프레임 재주장**(`PlayLoopNetworkActionIfNeeded` 패턴 — 현재 클립명과 다를 때만 재생)으로 승리하며, OnImGui가 프레임 마지막에 실행되어 패널의 쓰기가 최종. 상태 변화 프레임 1회 스틸은 툴로서 수용(하드 오버라이드 태그는 P1 옵션).
- **공유 모델 안전**: 애니/서브메시 데이터는 ResourceCache 공유 CModel, 렌더 포즈는 인스턴스 애니메이터 전용 — 패널은 `ModelRenderer::GetAnimator()`(인스턴스)와 엔티티의 `MeshGroupVisibilityComponent`만 만지고 CModel은 불변(`CModel::GetAnimator()`는 전 인스턴스 공유 레거시라 금지).
- **서브메시**: 렌더 4패스가 이미 모든 챔피언의 `MeshGroupVisibilityComponent`를 반영 — 패널은 컴포넌트만 편집(렌더 무변경). 비-Yone은 스폰 시 컴포넌트가 제거되므로 ensure/add, Restore Default는 AttachVisual 규칙 준수(Yone=MaskBaseDefault, 그 외=컴포넌트 제거).

## 1. 반영해야 하는 코드 (전부 반영 완료 — as-built)

Engine (헤더 인라인+수출 패스스루, UpdateLib 자동 동기화):
- `Resource/Animator.h` — `SetPlaySpeedRawForTool(f32_t)` / `SetCurrentTimeTicksForTool(f64_t)` (GetPlaySpeed 아래).
- `Renderer/ModelRenderer.h` — `GetAnimationCount()` 아래에 `GetAnimationNameByIndex` / `PlayAnimationByIndexAdvanced` / `GetSubmeshInfoCount` / `GetSubmeshNameByIndex` / `GetSubmeshMaterialIndexByIndex` 선언.
- `Renderer/ModelRenderer.cpp` — 위 5종 구현(`PlayAnimationByNameAdvanced` 856-886 스타일 미러: reverse=속도 부호 반전+끝에서 시작).

Client 신규 2파일 (전문은 파일 참조):
- `Client/Public/UI/ModelAnimPanel.h` — WfxEffectToolPanel 형태의 static Render 패널.
- `Client/Private/UI/ModelAnimPanel.cpp` — ①대상 표(ForEach<ChampionComponent, RenderComponent>, bSceneManaged/ViegoSoul 스킵, 이름=GetChampionDisplayName, (local) 태그, netId=EntityIdMap::ToNet) ②Animation 섹션(클립 리스트박스 인덱스 재생, Preview Active/Loop/Reverse/Pause 체크, Speed 슬라이더, 이름 가드 매 프레임 재주장 + RawForTool 속도 적용, Pause 시 틱 스크럽 슬라이더+시간 읽기) ③Submesh Decomposition 섹션(EnsureVisibility, 체크박스/Solo/All/None/Restore Default[Yone 분기], 이름+materialIndex 표기).

Client 등록 4파일:
- `Scene_InGame.h` — `bool_t m_bShowModelAnimPanel = false;` (m_bShowWfxEffectTool 아래).
- `Scene_InGameImGui.cpp` — include + `VK_F5` 토글(F5/F12만 비어 있었음) + WfxEffectTool 블록 아래 렌더 블록.
- `Client.vcxproj` — ClCompile/ClInclude 각 1줄. `Client.vcxproj.filters` — `05. UI\12. ModelAnim` 필터 + 엔트리 2개.

## 2. 검증

검증 결과 (2026-07-12 실행):
- Engine(+UpdateLib) → Client Debug x64 빌드 **PASS** (반복 0회 — 첫 빌드 통과).
- SimLab 전체 회귀 exit 0 — **키프레임 골든 프로브 포함 전 프로브 PASS** (Engine 헤더 변경이 결정론 비영향 확인).
- `git diff --check` 이상 없음.

인게임 검증 (사용자 게이트) — Debug 서버+클라:
1. **F5** → Model & Animation Lab 열림, 대상 표에 로컬 챔피언 표시. F10 Spawn Champion으로 적 배치 → 표에 즉시 추가.
2. 이렐리아 선택 → 클립 목록(전체 원시 클립, 개수 표시) → 아무 클립 클릭 → **키보드 없이** 재생. Loop/Reverse/Speed 0.5x 동작.
3. Pause 체크 → 포즈 정지 → 스크럽 슬라이더로 틱 단위 전후 탐색(포즈 즉시 반영).
4. 정지 상태에서 클립이 유지되고, 우클릭 이동 시 로코모션이 1프레임 뺏어도 즉시 재주장되는지. Preview Active 해제 → 게임 제어 복귀.
5. Yone 선택 → Submesh에서 GhostKatana Solo → 고스트 검만 렌더 → Restore Default → 기본 마스크 복귀. 타 챔피언은 Restore 시 컴포넌트 제거 확인.
6. F7 WFX 툴과 병용: 모델 띄우고 이펙트 스폰 → "이펙트 달았을 때 어떻게 보이는지" 확인(사용자 시나리오).
7. 회귀: F5 미사용 시 기존 동작 불변, S014 Pause/S015 Rewind와 병용 정상.

확인 필요:
- 미니언/정글 렌더러는 매니저 소유 컨테이너라 대상 표에서 제외(챔피언 전용) — 필요 시 후속.
- Viego 빙의 등 비주얼 스왑 시 패널 편집(마스크/클립)이 리셋됨 — 툴 특성상 수용, 자동 재적용은 P1.

다음 슬라이스 (P1):
- `Scene_AssetPreview` 격리 뷰어(자체 CWorld+궤도 카메라) + Cinematic `CSequencePlayer` 타임라인(Anim+Fx 트랙 스크럽) + 본 라인 오버레이(FX Bone 앵커 피커 겸용) + castFrame/recoveryFrame 마커의 스크럽 바 표시(SkillTimingPanel 통일).
- 발견 파트 인덱스의 네임드 마스크 헤더 박제 워크플로(Yone_MeshGroups.h 패턴 일반화).
