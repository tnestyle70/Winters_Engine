# Asset Loader And Streaming Runtime

## 목표

EldenRing 클라이언트는 에셋이 무겁다. 캐릭터, 맵, FX, UI, 사운드, 월드 셀을 모두 동기 로드하면 액션 RPG 클라이언트로 설득력이 떨어진다.

목표:

```
Async disk IO
  -> decode/parse
  -> dependency resolve
  -> GPU upload queue
  -> handle publish
```

## 문제 정의

초기 개발에서는 `CModel::LoadModel(path)` 동기 호출로 충분하지만, World Partitioning과 Raid/FX가 붙으면 다음 문제가 생긴다.

1. cell 전환 시 프레임 hitch
2. 보스 FX 대량 spawn 시 텍스처/메시 로딩 지연
3. 캐릭터 preview에서 animation load stall
4. editor에서 hot reload 시 런타임 멈춤

## Loader 계층

```
CAssetStreamingSystem
├── CAssetManifest
├── CAssetRequestQueue
├── CAssetDependencyResolver
├── CAsyncFileLoader
├── CAssetDecodeWorkers
├── CGpuUploadQueue
├── CAssetHandleRegistry
└── CAssetStreamingBudget
```

## Asset Handle

```cpp
using AssetHandle = u32_t;

enum class eAssetKind : u8_t
{
    Mesh,
    Skeleton,
    Animation,
    Texture,
    Material,
    Fx,
    Sequence,
    WorldCell,
    Sound
};

enum class eAssetState : u8_t
{
    Unloaded,
    Queued,
    LoadingDisk,
    Decoding,
    WaitingGpuUpload,
    Ready,
    Failed
};
```

## Request

```cpp
struct AssetLoadRequest
{
    u64_t pathHash = 0;
    eAssetKind kind = eAssetKind::Mesh;
    u32_t priority = 0;
    bool_t bBlockingAllowed = false;
    bool_t bGpuRequired = true;
};
```

Priority 예:

| Priority | 예 |
|---:|---|
| 1000 | player, current boss |
| 800 | current visible cell |
| 500 | nearby prefetch cell |
| 300 | editor preview |
| 100 | far HLOD |

## GPU Upload Queue

DX11에서는 GPU resource 생성이 device/context와 얽혀 있으므로 worker thread에서 모든 일을 끝내기 어렵다.

정책:

1. worker는 파일 읽기/파싱/중간 버퍼 준비
2. render/main thread의 upload phase에서 GPU resource 생성
3. frame budget을 둔다

```cpp
struct GpuUploadJob
{
    AssetHandle handle = 0;
    eAssetKind kind = eAssetKind::Mesh;
    const void* payload = nullptr;
    size_t payloadSize = 0;
    u32_t estimatedBytes = 0;
};
```

예산:

| Budget | 초기값 |
|---|---:|
| max upload jobs per frame | 4 |
| max upload bytes per frame | 16 MB |
| max blocking load ms | 5 ms |

## World Partition 연동

Cell load는 에셋 요청 묶음이다.

```
Cell queued
  -> request cell metadata
  -> request dependencies
  -> wait all required ready
  -> instantiate entities
```

Required/optional 분리:

| 종류 | 예 |
|---|---|
| Required | collision, near mesh, material fallback |
| Optional | high-res texture, far decals, ambient FX |

## Fallback 정책

에셋이 아직 준비되지 않았을 때:

| 에셋 | fallback |
|---|---|
| mesh | placeholder cube 또는 hidden |
| texture | gray/white/normal fallback |
| material | default material |
| animation | bind pose 또는 idle fallback |
| FX | skip visual-only FX |
| sound | skip |

## Hot Reload

Editor에서 필요하다.

```
file changed
  -> mark asset dirty
  -> keep old handle alive
  -> load new asset in background
  -> atomic swap when ready
  -> release old after no references
```

## Winters Binary 우선순위

로드 우선순위:

1. `.winters` bundle
2. loose `.w*`
3. loose FBX/PNG fallback

초기 개발 중에는 fallback을 허용한다. 안정화 후 `WINTERS_FORBID_LOOSE_FILES` 같은 빌드 플래그로 loose source load를 막을 수 있다.

## Manifest

```json
{
  "assets": [
    {
      "name": "Character/chr3010/chr3010",
      "kind": "Character",
      "mesh": "Character/chr3010/chr3010.wmesh",
      "skeleton": "Character/chr3010/chr3010.wskel",
      "animations": "Character/chr3010/anims/*.wanim",
      "materials": [
        "Character/chr3010/body.wmat"
      ]
    }
  ]
}
```

## 구현 순서

| 단계 | 내용 | 기준 |
|---|---|---|
| L0 | AssetHandleRegistry | handle -> state |
| L1 | sync wrapper | 기존 ResourceCache 감싸기 |
| L2 | request queue | priority sort |
| L3 | async file read | worker thread |
| L4 | GPU upload queue | frame budget |
| L5 | WorldCell dependency | cell load에 연결 |
| L6 | hot reload | editor에서 갱신 |
| L7 | bundle loader | `.winters` |

## Debug Panel

`Asset Streaming` 패널:

1. queued requests
2. loading assets
3. failed assets
4. GPU upload bytes this frame
5. cache memory usage
6. reference counts
7. selected asset dependency tree

## 완료 기준

첫 완료 기준:

1. `chr3010.wmesh`를 request로 로드
2. 로딩 중 placeholder 표시
3. ready 후 model swap
4. World cell 1개가 asset dependency를 통해 표시
5. ImGui에서 asset states 확인
