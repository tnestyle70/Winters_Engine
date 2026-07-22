Session - 억제기 파괴 후 살아 있는 모델이 남는 시각 버그 수정 결과
좌표: 신규 좌표 후보 · 축 C3 공유 원본/산출물 동시 편집 충돌, C8 검증 병목
관련: 2026-07-19_INHIBITOR_DESTROYED_VISUAL_STATE_FIX_PLAN/RESULT

# 1. 예언 vs 실제

- 예언: 서버 파괴 상태 전달이나 공통 렌더 마스크가 아니라 억제기 에셋 서브메시 인덱스와 시각 정의의 불일치가 원인이다.
- 실제: 일치했다. Blue/Red WMesh는 모두 2개 서브메시이고 WMat 재질 순서는 `0=일반 억제기`, `1=Destroyed`인데, JSON과 생성 C++가 반대로 선택하고 있었다.
- 수정 후: canonical JSON과 Client 런타임 생성 정의 모두 `0=Alive`, `1=Destroyed`로 일치한다.
- 빗나감: 전체 Client Debug 링크까지 통과할 것으로 예상했지만, 다른 세션의 스키마 codegen 및 `ChampionAIComponent` 레이아웃 변경이 현재 빌드를 차단했다.

# 2. 반영 내용

- `Data/LoL/ClientPublic/Visual/ObjectVisualDefs.json`
  - `structure.inhibitor.blue/red`의 서브메시 0을 Alive, 1을 Destroyed로 교정했다.
- `Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp`
  - Blue/Red 억제기 두 생성 함수의 alive/destroyed bool만 같은 규칙으로 교정했다.
  - 다른 세션의 기존 global build hash 변경은 보존했다.
- 변경하지 않음:
  - `Structure_Manager.cpp`, `SnapshotApplier.cpp`, `EventApplier.cpp`, Server/GameSim.
  - 전체 definition pack 생성기와 서버/manifest 산출물.

# 3. 검증 결과

## PASS

- WMesh 헤더: Blue/Red 모두 `submesh_count == 2`.
- WMat 의미: material 0 일반 억제기, material 1 Destroyed 텍스처 확인.
- JSON 계약: Blue/Red 모두 `Alive index 0`, `Destroyed index 1` 확인.
- 생성 C++ 계약: Blue/Red 두 함수 모두 alive/destroyed bool 매핑 확인.
- 변경 TU 컴파일: `LoLVisualDefinitions.generated.cpp`가 Client Debug 빌드 중 성공적으로 컴파일되어 `Client/Bin/Intermediate/Debug/LoLVisualDefinitions.generated.obj`가 갱신됨.
- `git diff --check` 대상 파일 통과.
- definition-pack stale 집합: 수정 전후 동일한 6개 파일. 이번 세션이 타 세션의 stale/충돌 범위를 늘리지 않음.

## 외부 dirty 차단

1. 일반 Client 의존 빌드
   - 다른 세션이 수정 중인 `Shared/Schemas/run_codegen.bat`가 `flatc`에 인자를 전달하지 못해 GameSim codegen에서 중단.
2. 프로젝트 참조 제외 Client 빌드
   - 이번 변경 TU는 컴파일 성공.
   - 다른 세션이 수정 중인 `Shared/GameSim/Components/ChampionAIComponent.h`의 정적 레이아웃 assert가 실패해 전체 Client 링크 전 중단.

위 두 파일은 충돌 회피 원칙에 따라 수정하지 않았다. 따라서 “수정 TU 컴파일 PASS”와 “전체 Client 링크 미완료”를 분리한다.

## 미검증

- 새 실행 파일이 링크되지 않았으므로 실제 인게임 Blue/Red 억제기 파괴 전후 육안 확인은 아직 수행하지 않았다.

# 4. 해결 판정

- 원인 수정: 완료.
- canonical/runtime 정의 동기화: 완료.
- 타 세션 변경 보존: 완료.
- 전체 빌드 및 인게임 육안 검증: 타 세션의 현재 빌드 차단 때문에 미완료.

# 5. 다음 재검증

다른 세션이 schema/ChampionAI 변경을 닫은 뒤 아래만 재실행한다.

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\Client\Include\Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

그 뒤 Debug 클라이언트에서 Blue/Red 억제기를 각각 파괴해 살아 있는 본체가 사라지고 파괴 모델만 남는지 확인한다.

# 6. 2026-07-19 실플레이 재개방 판정

- 사용자가 Release 실플레이에서 억제기 본체가 남는 것을 재현했다. 따라서 위 `# 4. 해결 판정`의 "원인 수정 완료"는 최종 판정으로 사용할 수 없고 **재개방**한다.
- 이전 구현은 억제기 WMesh 내부의 일반/Destroyed submesh를 뒤집었을 뿐, 사용자가 요구한 "현재 포탑 파괴 때 보이는 잔해 모델"을 사용하지 않았다. 요구 해석부터 잘못됐다.
- 바이너리 실측:
  - Blue/Red 억제기 WMesh: 각 2 submesh. 0번은 일반 재질, 1번은 Destroyed 재질이지만 두 submesh AABB가 거의 같은 억제기 자산 변형이다.
  - Blue/Red 포탑 WMesh: 각 8 submesh. 현재 canonical 포탑 destroyed state는 3번 `Stage3Stump`를 표시한다.
- 서버/스냅샷 경로는 원인이 아니다. `SnapshotBuilder`가 구조물 `HealthComponent`의 0 HP를 보내고, `SnapshotApplier`가 `StructureComponent.hp`에 적용하며, normal/RHI 두 구조물 렌더 경로 모두 그 값을 읽는다.
- 이전 검증은 Debug 변경 TU 컴파일까지만 통과했고 Release 링크/실플레이가 없었다. 따라서 당시 Release 적용을 보증하지 못했다.
- 보정 구현 및 검증은 PLAN §6을 따른다. 아직 source edit 전이며, 새 renderer delta의 독립 비평 게이트와 Release 실플레이가 남아 있다.
## 7. 2026-07-19 포탑 잔해 교체·5분 재생성 최종 결과

### 반영

- 파괴된 억제기는 기존 억제기 WMesh의 Destroyed 재질 변형이 아니라 같은 팀 포탑 renderer와 canonical turret destroyed mask를 사용한다.
- normal DX11과 RHI snapshot 제출 경로가 같은 renderer 선택 helper를 사용한다.
- 억제기별 destroyed renderer는 spawn/remove/clear 수명에 맞춰 관리한다.
- Server spawn 시 `InhibitorRespawnComponent`를 붙이고 Shared/GameSim `CDeathSystem`에서 300초 권위 카운트다운을 진행한다.
- 파괴 중 HP 0/untargetable을 유지하고, 만료 시 `HealthComponent`와 `StructureComponent`를 최대 HP로 함께 복구한다.
- 다음 snapshot의 `hp > 0`에서 Client는 포탑 잔해 제출을 멈추고 기존 팀별 억제기 renderer를 선택한다. 같은 HP 복구 snapshot을 체력바도 소비한다.
- countdown component를 world keyframe registry에 등록해 rewind/replay 분기에서도 remaining time을 보존한다.

### 검증 PASS

- asset converter: 억제기 WMesh 2 submeshes, 포탑 WMesh 8 submeshes 확인.
- `[SimLab][StructureRemnant] PASS: remnant contract and deterministic 300s inhibitor respawn`.
- 테스트는 최초 파괴 시 300초/pending/untargetable, 축약 countdown 만료 시 full HP/targetable, 중간 keyframe restore 뒤 동일 respawn tick을 검증한다.
- Release SimLab, Server, Client 링크 PASS.
- 추가 범위 독립 비평 최종 accepted/held P0 0 / P1 0.

### 판정

코드·자동 회귀·Release 빌드는 완료했다. 실제 Release 화면에서 파괴 순간 포탑 잔해가 보이고 정확히 5분 뒤 잔해가 사라지며 원래 억제기와 체력바가 함께 돌아오는 육안 수용 검사는 아직 수행하지 않았으므로 `LIVE_VISUAL_CONFIRM_NEEDED`로 남긴다.
