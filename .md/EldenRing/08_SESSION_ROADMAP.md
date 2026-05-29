# EldenRing Session Roadmap

## 진행 철학

바로 EldenRing 클라이언트 콘텐츠를 많이 붙이지 않는다.

먼저 "에셋 파이프라인"과 "클라이언트 분리"를 안정화한다. 이후 액션/월드/네트워크/에디터를 단계적으로 쌓는다.

## Phase ELD-0: 문서/의사결정 고정

완료 기준:

1. `.md/EldenRing` 문서 세트 작성
2. 클라이언트 분리 결정 고정
3. 에셋 파이프라인 순서 고정
4. 첫 캐릭터 후보 `chr3010` 결정

산출:

```
.md/EldenRing/*.md
```

## Phase ELD-1: WintersElden 프로젝트 생성

작업:

1. `WintersElden/` 폴더 생성
2. `WintersElden.vcxproj` 추가
3. `Winters.sln`에 프로젝트 등록
4. `UpdateLib.bat`이 Elden output에도 DLL 복사
5. `CEldenGameApp`, `main.cpp`, `Scene_EldenFieldTest` 최소 구현

완료 기준:

```
WintersElden.exe boots
Scene_EldenFieldTest enters
clear color or debug grid renders
```

## Phase ELD-2: 에셋 인벤토리/스테이징

작업:

1. `C:/Users/tnest/Desktop/EldenRing` 파일 inventory 생성
2. `WintersElden/Bin/Resource/Character/chr3010`로 첫 후보 복사/정리
3. texture folder/mapping.csv 정리
4. Blender export 표준 확인

완료 기준:

```
chr3010.fbx
anim3010.fbx
textures/*.png
mapping.csv
```

## Phase ELD-3: Winters Binary 변환

작업:

1. `WintersAssetConverter.exe skel`
2. `WintersAssetConverter.exe mesh --skel`
3. `WintersAssetConverter.exe anim --skel`
4. `info` 검증

완료 기준:

| 파일 | 기준 |
|---|---|
| `chr3010.wskel` | bones/hash 출력 |
| `chr3010.wmesh` | stride 76, bone count match |
| `anims/*.wanim` | skel hash match |

## Phase ELD-4: 첫 캐릭터 런타임 로드

작업:

1. `Scene_EldenFieldTest`에서 chr3010 load
2. 모델 표시
3. idle animation 재생
4. 카메라 임시 orbit/follow

완료 기준:

```
.wmesh+.wskel fast-path log
model visible
idle animation plays
no skinning explosion
```

## Phase ELD-5: Third-Person Camera

작업:

1. `CThirdPersonCamera`
2. `CSpringArm`
3. mouse yaw/pitch
4. collision solver stub
5. lock-on target selection

완료 기준:

```
WASD movement + camera follow
lock-on rotates view toward dummy target
```

## Phase ELD-6: Action Combat Minimum

작업:

1. `ActionStateMachine`
2. stamina
3. dodge/iframe window
4. light attack
5. hitbox timeline
6. dummy enemy hurtbox

완료 기준:

```
attack during hit window hits dummy
dodge consumes stamina
iframe prevents hit
debug hitbox visible
```

## Phase ELD-7: World Partition Minimum

작업:

1. grid math
2. world/cell JSON
3. streaming source radius
4. cell load/unload
5. debug panel

완료 기준:

```
moving player changes loaded cell set
cell mesh appears/disappears by streaming state
```

## Phase ELD-8: Asset Loader

작업:

1. AssetHandleRegistry
2. request queue
3. async file load
4. GPU upload queue
5. placeholder/fallback

완료 기준:

```
cell requests assets
main frame does not hard stall for full cell load
asset streaming panel shows states
```

## Phase ELD-9: PvP/Co-op Network Prototype

전제:

LoL UDP input/snapshot/reconciliation path가 먼저 안정화되어 있어야 한다.

작업:

1. Elden input packet
2. Elden snapshot packet
3. server room skeleton
4. movement authority
5. action state authority

완료 기준:

```
2 clients connect
server authoritative positions replicate
light attack event replicated
```

## Phase ELD-10: Raid Boss Prototype

작업:

1. boss entity
2. phase graph
3. server action selection
4. telegraph FX event
5. hitbox timeline authority
6. boss HP UI

완료 기준:

```
4 clients see same boss phase
boss attack hit result decided by server
raid wipe/win condition exists
```

## Phase ELD-11: FX Graph / Sequencer / Editor

작업:

1. Editor dockspace
2. HitboxTimelineEditor
3. BossPatternEditor
4. Sequencer camera track
5. FX graph burst billboard
6. save/load JSON

완료 기준:

```
edit -> save -> reload -> runtime preview
```

## Phase ELD-12: Packaging

작업:

1. `.wtex/.wmat` 전환
2. `.wcell/.wmap` 전환
3. `.wfx/.wseq` 전환
4. `.winters` bundle

완료 기준:

```
WintersElden loads from Winters binary bundle first
loose FBX fallback disabled in release-like config
```

## 우선순위 요약

지금 당장 다음 세션:

1. `WintersElden.vcxproj` 생성
2. `CEldenGameApp` 부팅
3. `chr3010` 에셋 staging
4. `.wmesh/.wskel/.wanim` 변환
5. 런타임 표시

그 다음:

1. Third-person camera
2. Action combat
3. World partition
4. Asset streaming
5. Network/Raid
6. FX/Sequencer/Editor

## 리스크와 대응

| 리스크 | 대응 |
|---|---|
| FBX skeleton mismatch | Blender에서 mesh/anim 같은 armature 기준 재export |
| bone count 256 초과 | export bone filter, only deform bones |
| material 경로 깨짐 | `mapping.csv`와 `.wmat`로 분리 |
| 에셋 저작권 | 로컬/비공개 검증, 공개 빌드는 대체 에셋 |
| 클라 분리 중 빌드 깨짐 | 기존 Client는 유지, 새 프로젝트만 추가 |
| World Partition 과설계 | JSON + cell 1개부터 시작 |
| Network 과조기 착수 | LoL UDP 안정화 후 Elden 확장 |
