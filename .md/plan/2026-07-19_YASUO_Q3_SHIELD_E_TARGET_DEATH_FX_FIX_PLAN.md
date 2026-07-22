Session - 야스오 Q3·패시브·E와 정글 사망/리스폰, F4 데미지 소유권 결함 수정
좌표: 신규 좌표 후보 · 축: C4, C7
관련: 2026-07-15_YASUO_Q3_JUNGLE_AGGRO_HPBAR_REPORT.md

## 1. 결정 기록

① 문제·제약: Q3 cooked clip은 의미 있는 rest translation을 가진 position channel 67개가 아직 전 구간 0이고, 미니언은 야스오 패시브 공격원 3종 중 누락됐다. E ring은 최대 10초, 미니 돌거북은 죽음 clip 없이 잔존하며 정글 리스폰은 0초(미구현)다.
② 순진한 해법의 실패: 캐릭터 Transform Y 보정은 서버 위치 진실을 오염시키고, idle 첫 키 복사는 WSkel 좌표계에서 약 (+0.095,-0.185,-0.262)m 이동을 만들 수 있다. 존재하지 않는 미니 돌거북 death clip 지정도 재생되지 않는다.
③ 메커니즘: Q3의 0 붕괴 position channel만 WSkel rest로 cook 후 복구하고, 피해원/FX anchor 생존/정글 사망·30초 부활을 각 소유 시스템에서 처리한다. F4는 변형별 flat 소유권을 명시한다.
④ 대조: 정글몹을 파괴·재생성할 수도 있지만 NetEntityId 재바인딩과 snapshot churn이 커서, 기존 entity를 untargetable/hidden 상태로 유지한 뒤 같은 entity를 초기화한다.
⑤ 대가: `kill_when_anchor_invalid=true`인 opt-in FX는 health owner가 죽는 즉시 모두 제거되고, 정글 시체는 종류별 death clip 길이와 무관하게 1.5초 후 숨는다. 다른 시체 체류 시간이 필요해지면 data field로 승격해야 한다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Tools/Anim/patch_wanim_root_track.py

기존 docstring의 “root-family bone” 설명과 고정 `.bak` 사용법을 아래로 교체:

```python
"""Repair degenerate Yasuo Q3 position tracks in a cooked .wanim clip.

The affected source exports meaningful skeleton joints with all-(0,0,0)
position keys. CAnimation::Evaluate applies those keys verbatim over the rest
pose, collapsing the skinned mesh while Q3 plays. This targeted post-cook
repair writes each affected position track from the matching .wskel rest
translation.

Usage:
  python Tools/Anim/patch_wanim_root_track.py <spell1_wind.wanim> <yasuo.wskel>
  python Tools/Anim/patch_wanim_root_track.py <spell1_wind.wanim> <yasuo.wskel> --dry-run
"""
```

기존 `ROOT_BONES` 상수는 삭제한다. `parse_wskel`은 기존 반환값에 writer와 동일한 ordered FNV-1a skeleton hash를 함께 반환하도록 교체하고, `parse_wanim_channels`는 아래 검증을 추가한다.

```python
def parse_wskel(path):
    data = read_file(path)
    magic, version_major, _version_minor, flags, content_size = struct.unpack_from(
        "<4sHHII", data, 0)
    if magic != b"WINT" or version_major != 1 or flags != 0:
        raise ValueError(f"invalid WSkel header: {path}")
    if content_size > len(data) - 16:
        raise ValueError(f"truncated WSkel payload: {path}")
    offset = 16
    skeleton_magic, bone_count, _socket_count = struct.unpack_from(
        "<4sII", data, offset)
    if skeleton_magic != b"WSKL" or bone_count == 0 or bone_count > 1024:
        raise ValueError(f"invalid WSkel metadata: {path}")
    offset += 32
    bones = {}
    skeleton_hash = 0xCBF29CE484222325
    for _ in range(bone_count):
        if offset + 256 > len(data):
            raise ValueError(f"truncated WSkel bone table: {path}")
        name_hash, = struct.unpack_from("<Q", data, offset)
        name = data[offset + 8:offset + 72].split(b"\0")[0].decode(
            "ascii", "replace")
        rest = struct.unpack_from("<16f", data, offset + 76)
        bones[name_hash] = (name, (rest[12], rest[13], rest[14]))
        skeleton_hash ^= name_hash
        skeleton_hash = (skeleton_hash * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
        offset += 256
    return bones, skeleton_hash
```

```python
def parse_wanim_channels(data):
    if len(data) < 16 + 32 + 8:
        raise ValueError("truncated WAnim")
    magic, version_major, _version_minor, flags, content_size = struct.unpack_from(
        "<4sHHII", data, 0)
    if magic != b"WINT" or version_major != 1 or flags != 0:
        raise ValueError("invalid WAnim header")
    if content_size > len(data) - 16:
        raise ValueError("truncated WAnim payload")
    payload_base = 16
    anim_magic, channel_count = struct.unpack_from("<4sI", data, payload_base)
    if anim_magic != b"WANM":
        raise ValueError("invalid WAnim metadata")
    channel_base = payload_base + 32
    channel_bytes = 40 * channel_count
    key_block_base = channel_base + channel_bytes
    payload_end = payload_base + content_size
    trailer_offset = payload_end - 8
    if key_block_base > trailer_offset:
        raise ValueError("invalid WAnim channel table")
    channels = []
    for index in range(channel_count):
        channel = struct.unpack_from("<QIIIIIIII", data, channel_base + 40 * index)
        bone_hash, position_count, position_offset = channel[:3]
        position_end = key_block_base + position_offset + 16 * position_count
        if position_end > trailer_offset:
            raise ValueError("position keys exceed WAnim payload")
        channels.append((bone_hash, position_count, position_offset))
    skeleton_hash, = struct.unpack_from("<Q", data, trailer_offset)
    return channels, key_block_base, skeleton_hash
```

기존 `find_zeroed_root_channels`는 함수 이름을 유지하되 root 이름 필터를 제거하고, 유한한 WSkel rest 길이가 `REST_MIN_LEN` 이상인 모든 all-zero position channel만 반환하도록 아래로 교체:

```python
def find_zeroed_root_channels(data, bones):
    channels, key_block_base, _skeleton_hash = parse_wanim_channels(data)
    hits = []
    for bone_hash, position_count, position_offset in channels:
        bone = bones.get(bone_hash)
        if bone is None or position_count == 0:
            continue
        name, rest = bone
        if not all(math.isfinite(value) for value in rest):
            raise ValueError(f"non-finite rest translation: {name}")
        rest_len = math.sqrt(sum(value * value for value in rest))
        if rest_len < REST_MIN_LEN:
            continue
        positions = [
            struct.unpack_from(
                "<4f", data, key_block_base + position_offset + 16 * key)[1:]
            for key in range(position_count)
        ]
        if not all(math.isfinite(value) for position in positions for value in position):
            raise ValueError(f"non-finite position track: {name}")
        if all(all(abs(value) <= ZERO_EPS for value in position) for position in positions):
            hits.append((name, position_count, position_offset, rest, key_block_base))
    return hits
```

`import os` 아래에 `hashlib`, `math`를 추가한다. 기존 `patch_file`은 basename 제한, WAnim/WSkel hash 일치, hash-suffixed 백업, 원자적 교체를 적용한 아래 코드로 교체:

```python
def patch_file(wanim_path, wskel_path, dry_run):
    if os.path.basename(wanim_path).lower() != "skinned_mesh_yasuo_spell1_wind.wanim":
        raise ValueError("repair is restricted to Yasuo spell1_wind.wanim")
    bones, expected_skeleton_hash = parse_wskel(wskel_path)
    original = read_file(wanim_path)
    data = bytearray(original)
    _channels, _key_block_base, actual_skeleton_hash = parse_wanim_channels(data)
    if actual_skeleton_hash != expected_skeleton_hash:
        raise ValueError(
            f"skeleton hash mismatch: wanim={actual_skeleton_hash:016x} "
            f"wskel={expected_skeleton_hash:016x}")
    hits = find_zeroed_root_channels(data, bones)
    if not hits:
        print(f"[ok] no degenerate position track: {wanim_path}")
        return 0
    for name, position_count, position_offset, rest, key_block_base in hits:
        print(f"[patch] {os.path.basename(wanim_path)} ch={name} "
              f"posKeys={position_count} -> "
              f"rest=({rest[0]:.3f}, {rest[1]:.3f}, {rest[2]:.3f})")
        if dry_run:
            continue
        for key in range(position_count):
            key_offset = key_block_base + position_offset + 16 * key
            struct.pack_into("<3f", data, key_offset + 4, *rest)
    if dry_run:
        print(f"[dry-run] {len(hits)} channel(s) would be patched")
        return len(hits)
    digest = hashlib.sha256(original).hexdigest()[:12]
    backup = f"{wanim_path}.{digest}.bak"
    if not os.path.exists(backup):
        with open(backup, "wb") as stream:
            stream.write(original)
    temporary = f"{wanim_path}.tmp"
    with open(temporary, "wb") as stream:
        stream.write(data)
    os.replace(temporary, wanim_path)
    print(f"[done] patched {len(hits)} channel(s), backup: {backup}")
    return len(hits)
```

위 교체본들의 exact 기존 코드는 다음과 같다.

`parse_wskel` 기존 코드:

```python
def parse_wskel(path):
    d = read_file(path)
    magic, _vmaj, _vmin, _flags, _csize = struct.unpack_from("<4sHHII", d, 0)
    assert magic == b"WINT", (path, magic)
    off = 16
    smagic, bone_count, _socket_count = struct.unpack_from("<4sII", d, off)
    assert smagic == b"WSKL", (path, smagic)
    off += 32
    bones = {}
    for _ in range(bone_count):
        name_hash, = struct.unpack_from("<Q", d, off)
        name = d[off + 8:off + 8 + 64].split(b"\0")[0].decode("ascii", "replace")
        rest = struct.unpack_from("<16f", d, off + 76)
        # row-major local rest matrix: translation = m30 m31 m32
        bones[name_hash] = (name, (rest[12], rest[13], rest[14]))
        off += 256
    return bones
```

`parse_wanim_channels` 기존 코드:

```python
def parse_wanim_channels(data):
    magic, _vmaj, _vmin, _flags, csize = struct.unpack_from("<4sHHII", data, 0)
    assert magic == b"WINT", magic
    payload_base = 16
    amagic, channel_count = struct.unpack_from("<4sI", data, payload_base)
    assert amagic == b"WANM", amagic
    chan_base = payload_base + 32
    chans = []
    for i in range(channel_count):
        bone_hash, pos_n, pos_off = struct.unpack_from(
            "<QII", data, chan_base + 40 * i)
        chans.append((bone_hash, pos_n, pos_off))
    key_block_base = chan_base + 40 * channel_count
    return chans, key_block_base, csize
```

`find_zeroed_root_channels` 기존 코드:

```python
def find_zeroed_root_channels(data, bones):
    chans, key_block_base, _ = parse_wanim_channels(data)
    hits = []
    for bone_hash, pos_n, pos_off in chans:
        name, rest = bones.get(bone_hash, ("?%016x" % bone_hash, (0.0, 0.0, 0.0)))
        if name not in ROOT_BONES or pos_n == 0:
            continue
        all_zero = True
        for k in range(pos_n):
            _t, x, y, z = struct.unpack_from(
                "<4f", data, key_block_base + pos_off + 16 * k)
            if abs(x) > ZERO_EPS or abs(y) > ZERO_EPS or abs(z) > ZERO_EPS:
                all_zero = False
                break
        rest_len = (rest[0] ** 2 + rest[1] ** 2 + rest[2] ** 2) ** 0.5
        if all_zero and rest_len >= REST_MIN_LEN:
            hits.append((name, pos_n, pos_off, rest, key_block_base))
    return hits
```

`patch_file` 기존 코드는 현재 함수 전체(고정 `.bak`을 쓰는 `def patch_file(wanim_path, wskel_path, dry_run):`부터 마지막 `return len(hits)`까지)이며, 위에 제시한 새 `patch_file` 전체로 교체한다.

삭제할 기존 코드:

```python
def audit(character_root):
    total = 0
    for champ in sorted(os.listdir(character_root)):
        champ_dir = os.path.join(character_root, champ)
        anims_dir = os.path.join(champ_dir, "anims")
        if not os.path.isdir(anims_dir):
            continue
        wskels = [f for f in os.listdir(champ_dir) if f.endswith(".wskel")]
        if not wskels:
            continue
        bones = parse_wskel(os.path.join(champ_dir, wskels[0]))
        for clip in sorted(os.listdir(anims_dir)):
            if not clip.endswith(".wanim"):
                continue
            path = os.path.join(anims_dir, clip)
            try:
                hits = find_zeroed_root_channels(read_file(path), bones)
            except (AssertionError, struct.error) as e:
                print(f"[skip] unparsable {path}: {e}")
                continue
            for name, pos_n, _po, rest, _kb in hits:
                total += 1
                print(f"[defect] {champ}/{clip} ch={name} posKeys={pos_n} "
                      f"rest=({rest[0]:.2f}, {rest[1]:.2f}, {rest[2]:.2f})")
    print(f"[audit] zeroed-root channels found: {total}")
    return total
```

`main` 기존 코드:

```python
def main(argv):
    if len(argv) >= 2 and argv[0] == "--audit":
        audit(argv[1])
        return 0
    if len(argv) < 2:
        print(__doc__)
        return 2
    dry = "--dry-run" in argv[2:]
    patch_file(argv[0], argv[1], dry)
    return 0
```

아래로 교체:

```python
def main(argv):
    if len(argv) not in (2, 3) or (len(argv) == 3 and argv[2] != "--dry-run"):
        print(__doc__)
        return 2
    try:
        patch_file(argv[0], argv[1], len(argv) == 3)
    except (OSError, ValueError, struct.error) as error:
        print(f"[error] {error}", file=sys.stderr)
        return 1
    return 0
```

### 2-2. C:/Users/user/Desktop/Winters/Tools/convert_all_assets.ps1

기존 코드:

```powershell
function Convert-Champions([string]$ChampRoot) {
    Convert-Champ $ChampRoot "Irelia" "irelia_fixed.fbx"
    Convert-Champ $ChampRoot "Yasuo" "yasuo_fixed.fbx"
```

아래로 교체:

```powershell
function Repair-YasuoQ3([string]$ChampRoot) {
    $yasuoRoot = Join-Path $ChampRoot "Yasuo"
    $source = Join-Path $yasuoRoot "yasuo_fixed.fbx"
    $clip = Join-Path $yasuoRoot "anims\skinned_mesh_yasuo_spell1_wind.wanim"
    $skeleton = Join-Path $yasuoRoot "yasuo_fixed.wskel"
    $repair = Join-Path $PSScriptRoot "Anim\patch_wanim_root_track.py"
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        return
    }
    if (-not (Test-Path -LiteralPath $clip -PathType Leaf) -or
        -not (Test-Path -LiteralPath $skeleton -PathType Leaf)) {
        throw "Yasuo Q3 repair inputs are missing after cook"
    }
    & python $repair $clip $skeleton
    if ($LASTEXITCODE -ne 0) {
        throw "Yasuo Q3 WAnim repair failed ($LASTEXITCODE)"
    }
}

function Convert-Champions([string]$ChampRoot) {
    Convert-Champ $ChampRoot "Irelia" "irelia_fixed.fbx"
    Convert-Champ $ChampRoot "Yasuo" "yasuo_fixed.fbx"
    Repair-YasuoQ3 $ChampRoot
```

이 호출은 `Convert-Champ`가 asset을 새로 cook했거나 up-to-date로 건너뛴 경우 모두 실행되어 ignored runtime 산출물의 수정이 재cook 뒤에도 유지된다.

### 2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp

기존 코드:

```cpp
    bool_t CanTriggerPassiveShieldFromSource(CWorld& world, EntityID source)
    {
        return source != NULL_ENTITY &&
            (world.HasComponent<ChampionComponent>(source) ||
                world.HasComponent<JungleComponent>(source));
    }
```

아래로 교체:

```cpp
    bool_t CanTriggerPassiveShieldFromSource(CWorld& world, EntityID source)
    {
        return source != NULL_ENTITY &&
            (world.HasComponent<ChampionComponent>(source) ||
                world.HasComponent<JungleComponent>(source) ||
                world.HasComponent<MinionComponent>(source));
    }
```

### 2-4. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxSystem.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Renderer/PlaneRenderer.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Renderer/PlaneRenderer.h"
```

`IsBillboardBackedType` 바로 아래에 추가:

```cpp
    bool_t IsDeadFxAttachment(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY ||
            !world.HasComponent<HealthComponent>(entity))
        {
            return false;
        }

        const HealthComponent& health =
            world.GetComponent<HealthComponent>(entity);
        return health.bIsDead || health.fCurrent <= 0.f;
    }
```

기존 코드:

```cpp
                fx.fElapsed += fEffectiveDelta;

                Vec3 vResolvedAnchor{};
```

아래로 교체:

```cpp
                fx.fElapsed += fEffectiveDelta;

                if (fx.bDestroyWhenAttachInvalid &&
                    IsDeadFxAttachment(world, fx.attachTo))
                {
                    vecDelete.push_back(e);
                    return;
                }

                Vec3 vResolvedAnchor{};
```

### 2-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/JungleCampGameDef.h

기존 코드:

```cpp
    f32_t aggroRange = 0.f;
    f32_t leashRange = 0.f;
```

아래로 교체:

```cpp
    f32_t aggroRange = 0.f;
    f32_t leashRange = 0.f;
    f32_t respawnDelaySec = 30.f;
```

### 2-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/GameplayComponents.h

기존 코드:

```cpp
struct JungleComponent
{
    uint32_t subKind = 0;   // eJungleSub static_cast
    uint32_t campId = 0;
    f32_t    hp = 1000.f;
    f32_t    maxHp = 1000.f;
};
```

아래로 교체:

```cpp
struct JungleComponent
{
    uint32_t subKind = 0;   // eJungleSub static_cast
    uint32_t campId = 0;
    f32_t    hp = 1000.f;
    f32_t    maxHp = 1000.f;
    Vec3     vSpawnPosition{};
    f32_t    fRespawnDelaySec = 30.f;
    f32_t    fRespawnTimerSec = 0.f;
    bool_t   bRespawnPending = false;
    u8_t     reserved[3]{};
};
```

### 2-7. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

기존 코드:

```cpp
    jungle.hp = maxHp;
    jungle.maxHp = maxHp;
    m_world.AddComponent<JungleComponent>(entity, jungle);
```

아래로 교체:

```cpp
    jungle.hp = maxHp;
    jungle.maxHp = maxHp;
    jungle.vSpawnPosition = request.position;
    jungle.fRespawnDelaySec = jungleDef.respawnDelaySec;
    m_world.AddComponent<JungleComponent>(entity, jungle);
```

### 2-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/JungleAI/JungleAISystem.cpp

`GameplayStateQuery.h` include 아래에 추가:

```cpp
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"
```

`ClearJungleAggro` 아래에 `UpdateJungleRespawn`을 추가한다. 이 함수는 dead 첫 tick에 aggro/move/attack/combat/status를 지우고 `TargetableTag`를 제거한 뒤 timer를 시작한다. timer 만료 시 같은 entity의 spawn position, Health/Jungle mirror, SkillState/JungleAI, targetable/Idle pose를 복구한다. practice reset으로 이미 살아난 pending entity는 timer를 취소하고 targetable을 복구한다.

```cpp
    bool_t UpdateJungleRespawn(
        CWorld& world,
        EntityID entity,
        JungleComponent& jungle,
        JungleAIComponent& ai,
        f32_t deltaSeconds,
        u64_t tickIndex)
    {
        if (!world.HasComponent<HealthComponent>(entity))
            return true;

        HealthComponent& health = world.GetComponent<HealthComponent>(entity);
        const bool_t bDead = health.bIsDead || health.fCurrent <= 0.f;
        if (!bDead)
        {
            if (jungle.bRespawnPending)
            {
                jungle.bRespawnPending = false;
                jungle.fRespawnTimerSec = 0.f;
                if (!world.HasComponent<TargetableTag>(entity))
                    world.AddComponent<TargetableTag>(entity);
            }
            return true;
        }

        if (!jungle.bRespawnPending)
        {
            jungle.bRespawnPending = true;
            jungle.fRespawnTimerSec = jungle.fRespawnDelaySec;
            ClearJungleAggro(world, entity, ai);
            ai.bReturning = false;
            if (world.HasComponent<CombatActionComponent>(entity))
                world.RemoveComponent<CombatActionComponent>(entity);
            GameplayStatus::ClearStatusEffects(world, entity);
            if (world.HasComponent<TargetableTag>(entity))
                world.RemoveComponent<TargetableTag>(entity);
        }

        jungle.fRespawnTimerSec = std::max(
            0.f, jungle.fRespawnTimerSec - std::max(0.f, deltaSeconds));
        if (jungle.fRespawnTimerSec > 0.f)
            return false;

        jungle.bRespawnPending = false;
        health.fMaximum = jungle.maxHp;
        health.fCurrent = jungle.maxHp;
        health.bIsDead = false;
        jungle.hp = jungle.maxHp;
        if (world.HasComponent<TransformComponent>(entity))
            world.GetComponent<TransformComponent>(entity).SetPosition(
                jungle.vSpawnPosition);
        if (world.HasComponent<SkillStateComponent>(entity))
            world.GetComponent<SkillStateComponent>(entity) = SkillStateComponent{};
        ClearJungleAggro(world, entity, ai);
        ai.attackSequence = 0u;
        ai.bReturning = false;
        if (!world.HasComponent<TargetableTag>(entity))
            world.AddComponent<TargetableTag>(entity);
        SetPoseState(world, entity, ePoseStateId::Idle, tickIndex, true);
        return true;
    }
```

`#include <limits>` 아래에 `#include <algorithm>`을 추가한다. `Execute`의 기존 dead 분기:

```cpp
        if (!IsAlive(world, entity))
        {
            ClearJungleAggro(world, entity, ai);
            ai.bReturning = false;
            continue;
        }
```

아래로 교체:

```cpp
        auto& jungle = world.GetComponent<JungleComponent>(entity);
        if (!UpdateJungleRespawn(
            world, entity, jungle, ai, tc.fDt, tc.tickIndex))
            continue;
```

### 2-9. C:/Users/user/Desktop/Winters/Client/Public/Manager/Jungle_Manager.h

`JungleVisualState`의 기존 끝:

```cpp
        bool_t bAction = false;
        bool_t bDead = false;
```

아래로 교체:

```cpp
        f32_t fDeathElapsedSec = 0.f;
        bool_t bAction = false;
        bool_t bDead = false;
        bool_t bCorpseHidden = false;
```

### 2-10. C:/Users/user/Desktop/Winters/Client/Private/Manager/Jungle_Manager.cpp

anonymous namespace에 추가:

```cpp
    constexpr f32_t kJungleCorpseVisibleSec = 1.5f;
```

`Apply_NetworkAnimation`의 기존 코드:

```cpp
    if (bDeadByHealth || bDeadBySnapshot)
    {
        if (!visual.bDead)
        {
            if (const char* pDeath = Resolve_PlayableAnimation(renderer, anims.death, nullptr))
                renderer.PlayAnimationByNameAdvanced(pDeath, false, false, 1.f);
            visual.bDead = true;
            visual.bAction = false;
        }
        return;
    }

    visual.bDead = false;
```

아래로 교체:

```cpp
    if (bDeadByHealth || bDeadBySnapshot)
    {
        if (!visual.bDead)
        {
            if (const char* pDeath = Resolve_PlayableAnimation(renderer, anims.death, nullptr))
                renderer.PlayAnimationByNameAdvanced(pDeath, false, false, 1.f);
            visual.bDead = true;
            visual.bAction = false;
            visual.actionTimer = 0.f;
            visual.baseAnimId = 0u;
            visual.fDeathElapsedSec = 0.f;
        }
        visual.fDeathElapsedSec += std::max(0.f, dt);
        if (!visual.bCorpseHidden &&
            visual.fDeathElapsedSec >= kJungleCorpseVisibleSec &&
            m_pWorld->HasComponent<RenderComponent>(entity))
        {
            m_pWorld->GetComponent<RenderComponent>(entity).bVisible = false;
            visual.bCorpseHidden = true;
        }
        return;
    }

    if (visual.bCorpseHidden &&
        m_pWorld->HasComponent<RenderComponent>(entity))
    {
        m_pWorld->GetComponent<RenderComponent>(entity).bVisible = true;
    }
    if (visual.bDead)
    {
        visual.baseAnimId = 0u;
        visual.bAction = false;
        visual.actionTimer = 0.f;
    }
    visual.fDeathElapsedSec = 0.f;
    visual.bCorpseHidden = false;
    visual.bDead = false;
```

death 진입과 부활에서 `baseAnimId`를 무효화하므로 사망 전 Idle ID가 같아도 부활 뒤 idle clip이 다시 재생된다.

### 2-11. C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

기존 코드:

```python
JUNGLE_FIELDS = (
    ("maxHp", 1500.0),
    ("radius", 1.0),
    ("attackRange", 1.7),
    ("attackDamage", 45.0),
    ("attackCooldown", 1.4),
    ("moveSpeed", 4.0),
    ("baseArmor", 20.0),
    ("baseMr", 20.0),
    ("aggroRange", 3.0),
    ("leashRange", 9.0),
)
```

아래로 교체:

```python
JUNGLE_FIELDS = (
    ("maxHp", 1500.0),
    ("radius", 1.0),
    ("attackRange", 1.7),
    ("attackDamage", 45.0),
    ("attackCooldown", 1.4),
    ("moveSpeed", 4.0),
    ("baseArmor", 20.0),
    ("baseMr", 20.0),
    ("aggroRange", 3.0),
    ("leashRange", 9.0),
    ("respawnDelaySec", 30.0),
)
```

`normalize_spawn_object_root`의 `turret_ai` 선언 아래에 추가:

```python
    default_jungle_camp = normalize_float_fields(
        require_object(root.get("defaultJungleCamp", {}), "defaultJungleCamp"),
        JUNGLE_FIELDS,
        "defaultJungleCamp",
    )
    if default_jungle_camp["respawnDelaySec"] <= 0.0:
        fail("defaultJungleCamp.respawnDelaySec must be > 0")
```

기존 코드:

```python
        jungle_camps.append(
            {
                "subKind": sub_kind,
                **normalize_float_fields(item, JUNGLE_FIELDS, f"jungleCamps[{index}]"),
            }
        )
```

아래로 교체:

```python
        camp = {
            "subKind": sub_kind,
            **normalize_float_fields(item, JUNGLE_FIELDS, f"jungleCamps[{index}]"),
        }
        if camp["respawnDelaySec"] <= 0.0:
            fail(f"jungleCamps[{index}].respawnDelaySec must be > 0")
        jungle_camps.append(camp)
```

기존 코드:

```python
        "defaultJungleCamp": normalize_float_fields(
            require_object(root.get("defaultJungleCamp", {}), "defaultJungleCamp"),
            JUNGLE_FIELDS,
            "defaultJungleCamp",
        ),
```

아래로 교체:

```python
        "defaultJungleCamp": default_jungle_camp,
```

`append_jungle_def`의 기존 field loop가 generated C++까지 값을 전달한다.

### 2-12. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json

`defaultJungleCamp`와 `subKind` 0~10의 모든 camp에서 기존 마지막 필드:

```json
    "leashRange": 9.0
```

각 객체의 실제 `leashRange` 값을 유지하면서 아래 형태로 교체:

```json
    "leashRange": 9.0,
    "respawnDelaySec": 30.0
```

Baron/Dragon처럼 현재 `leashRange`가 8.0인 객체는 8.0을 유지한다.

### 2-13. C:/Users/user/Desktop/Winters/Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp

기존 코드:

```cpp
        { "aggroRange", &JungleCampGameDef::aggroRange },
        { "leashRange", &JungleCampGameDef::leashRange },
```

아래로 교체:

```cpp
        { "aggroRange", &JungleCampGameDef::aggroRange },
        { "leashRange", &JungleCampGameDef::leashRange },
        { "respawnDelaySec", &JungleCampGameDef::respawnDelaySec },
```

### 2-14. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

spawn definitions hot-load refresh의 기존 코드:

```cpp
				health.fMaximum = camp.maxHp;
				health.fCurrent = health.bIsDead ? 0.f : camp.maxHp * healthRatio;
				jungle.maxHp = camp.maxHp;
				jungle.hp = health.fCurrent;
```

아래로 교체:

```cpp
				health.fMaximum = camp.maxHp;
				health.fCurrent = health.bIsDead ? 0.f : camp.maxHp * healthRatio;
				jungle.maxHp = camp.maxHp;
				jungle.hp = health.fCurrent;
				jungle.fRespawnDelaySec = camp.respawnDelaySec;
```

`ClearJungleStatOverrides`의 기존 코드:

```cpp
                jungle.maxHp = campDef.maxHp;
                health.fMaximum = campDef.maxHp;
```

아래로 교체:

```cpp
                jungle.maxHp = campDef.maxHp;
                jungle.fRespawnDelaySec = campDef.respawnDelaySec;
                health.fMaximum = campDef.maxHp;
```

`ResetJungleMonster`의 기존 코드:

```cpp
                health.fMaximum = camp.maxHp;
                health.fCurrent = camp.maxHp;
                health.bIsDead = false;
                jungle.maxHp = camp.maxHp;
                jungle.hp = camp.maxHp;
```

아래로 교체:

```cpp
                health.fMaximum = camp.maxHp;
                health.fCurrent = camp.maxHp;
                health.bIsDead = false;
                jungle.maxHp = camp.maxHp;
                jungle.hp = camp.maxHp;
                jungle.fRespawnDelaySec = camp.respawnDelaySec;
                jungle.bRespawnPending = false;
                jungle.fRespawnTimerSec = 0.f;
                if (!m_world.HasComponent<TargetableTag>(entity))
                    m_world.AddComponent<TargetableTag>(entity);
```

### 2-15. C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp

사용자 작업: 밸런스 작업자가 Yasuo/Lee Sin Q와 정글몹 11종을 선택해 실제 소유 데미지/리스폰 값을 편집하고 `Save & Hot Load` 성공 또는 구체 실패 상태를 확인한다.

```text
대상 범위: Yasuo Q, Lee Sin Q, default jungle camp와 subKind 0..10
필수 데이터: Q rank Flat/Total AD/Bonus AD, variant baseDamage/tornadoDamage/dashAreaDamage, respawnDelaySec
핵심 행동: Save & Hot Load (Primary), Reload from Disk (기존 Secondary)
제외: 피해 공식을 새 경로로 계산, session-only override, 다른 챔피언 수치 변경
권위/저장: ChampionTuner Draft -> canonical JSON atomic persist -> Server runtime overlay/hot-load -> 기존 revision/status ack
완료 증거: F4 Skills Q와 Objectives 화면에서 저장 성공, invalid/offline 실패 문구, 실제 Q 피해/30초 부활
```

권위 모드: B(정식 gameplay authoring).

| 대상 | 한 편집 경로 | Persist owner | Apply/Ack owner |
|---|---|---|---|
| Yasuo Q1/Q2/Q3/EQ Flat | Skills/Q Runtime Damage Params | SkillEffectGameplayDefs.json | Server runtime overlay / tuner status |
| Yasuo Q AD ratio | Skills/Q rank table | SkillEffectGameplayDefs.json | Server runtime overlay / tuner status |
| Lee Sin Q1 Flat/AD ratio | Skills/Q rank table | SkillEffectGameplayDefs.json | Server runtime overlay / tuner status |
| Lee Sin Q2 Flat | Skills/Q Q2 Recast Base Damage | SkillEffectGameplayDefs.json | Server runtime overlay / tuner status |
| 정글 11종 respawn | Objectives/Jungle Monster | SpawnObjectGameplayDefs.json | Server runtime overlay / tuner status |

기존 코드:

```cpp
					static const std::unordered_set<std::string> kCustomFlatSkills = {
						"skill.yasuo.q", "skill.kalista.e", "skill.leesin.q",
						"skill.ezreal.r", "skill.jax.w", "skill.jax.r"
					};
					const bool_t bCustomFlat = kCustomFlatSkills.contains(effectKey);
```

아래로 교체:

```cpp
					static const std::unordered_set<std::string> kRuntimeFlatOnlySkills = {
						"skill.yasuo.q", "skill.kalista.e", "skill.ezreal.r",
						"skill.jax.w", "skill.jax.r"
					};
					const bool_t bRuntimeFlatOnly =
						kRuntimeFlatOnlySkills.contains(effectKey);
```

`DrawRankRow`의 기존 코드:

```cpp
							if (bDisabled)
							{
								ImGui::TextDisabled("Runtime param");
								continue;
							}
```

아래로 교체:

```cpp
							if (bDisabled)
							{
								ImGui::TextDisabled("Variant param below");
								continue;
							}
```

기존 코드:

```cpp
						DrawRankRow(
							"Flat Damage", damage, "flatByRank",
							1.f, 0.f, 2000.f, "%.0f", bCustomFlat,
							draft.bSkillEffectDirty);
```

아래로 교체:

```cpp
						DrawRankRow(
							"Flat Damage", damage, "flatByRank",
							1.f, 0.f, 2000.f, "%.0f", bRuntimeFlatOnly,
							draft.bSkillEffectDirty);
```

기존 코드:

```cpp
					ImGui::TextDisabled(
						"Drag horizontally to tune. Double-click to type an exact value. Ratio 1.0 = 100%%.");
```

아래로 교체:

```cpp
					ImGui::TextDisabled(
						"Raw = Flat + Total AD Ratio x final Total AD + Bonus AD Ratio x Bonus AD.");
					ImGui::TextDisabled(
						"Armor / Magic Resist is applied after raw damage. Ratio 1.0 = 100%%.");
```

Runtime Parameter label을 다음처럼 skill별로 구체화한다.

```text
Yasuo baseDamage     -> Q1/Q2 Base Damage
Yasuo tornadoDamage  -> Q3 Tornado Base Damage
Yasuo dashAreaDamage -> EQ Base Damage
Lee Sin baseDamage   -> Q2 Recast Base Damage
```

기존 runtime param `EditDragFloat` 호출:

```cpp
								EditDragFloat(
									params,
									pParam,
									pParam,
```

아래로 교체:

```cpp
								const char* pLabel = pParam;
								if (effectKey == "skill.yasuo.q")
								{
									if (std::strcmp(pParam, "baseDamage") == 0)
										pLabel = "Q1/Q2 Base Damage";
									else if (std::strcmp(pParam, "tornadoDamage") == 0)
										pLabel = "Q3 Tornado Base Damage";
									else if (std::strcmp(pParam, "dashAreaDamage") == 0)
										pLabel = "EQ Base Damage";
								}
								else if (effectKey == "skill.leesin.q" &&
									std::strcmp(pParam, "baseDamage") == 0)
								{
									pLabel = "Q2 Recast Base Damage";
								}
								EditDragFloat(
									params,
									pParam,
									pLabel,
```

기존 코드:

```cpp
					if (bCustomFlat)
						ImGui::TextDisabled("Flat Damage uses the Runtime Damage Params for this skill.");
```

아래로 교체:

```cpp
					if (bRuntimeFlatOnly)
						ImGui::TextDisabled("Flat Damage uses the variant parameters above for this skill.");
```

Objectives의 기존 코드:

```cpp
					EditFloat(*pCamp, "baseMr", "Magic Resist", draft.bSpawnObjectDirty, "%.1f");
```

아래에 추가:

```cpp
EditDragFloat(*pCamp, "respawnDelaySec", "Respawn Time (sec)",
    0.5f, 1.f, 600.f, "%.1f s", draft.bSpawnObjectDirty);
```

위 편집기는 신규 중복 경로가 아니라 기존 Objectives의 같은 camp JSON 편집 행에 통합하며, rank table/runtime variant의 기존 두 owner를 역할별로 명명할 뿐 합치거나 복제하지 않는다.

UI 계약:

```text
[Balance Tuner]
 Champion: Yasuo / Lee Sin
 Skills > Q
   Rank Formula: Flat Damage | Total AD Ratio | Bonus AD Ratio
   Runtime Parameters: variant-specific base damage
   [Save & Hot Load]

 Objectives > Jungle Monster
   Respawn Time (sec): [30.0]
   [Save & Hot Load]
```

Primary는 `Save & Hot Load`, Secondary는 기존 `Reload from Disk` 1개다. 수식 설명은 action이 아닌 도움말이다. 지원 최소 해상도·DPI는 코드 근거가 없어 `CONFIRM_NEEDED`다.

수동 화면 경로: `Tools/Bin/Debug/Client.exe -> F5 InGame -> F4 -> Balance Tuner -> Skills/Objectives`. 예상 캡처는 `.md/build/2026-07-19_YASUO_LEESIN_Q_F4_SUCCESS.png`, `.md/build/2026-07-19_JUNGLE_RESPAWN_F4_SUCCESS.png`, `.md/build/2026-07-19_F4_INVALID_OR_OFFLINE.png`이며 실제 캡처 불가 시 RESULT에 미검증으로 남긴다.

### 2-16. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`RunServerAuthoritativeShieldProbe`의 기존 minion negative assertion은 아래 exact block으로 교체한다.

`minionAttacker` 생성 블록 바로 아래에 추가:

```cpp
        const EntityID jungleAttacker = world.CreateEntity();
        world.AddComponent<JungleComponent>(jungleAttacker, JungleComponent{});
```

`yasuoMinionHit` 선언부터 기존 minion negative assertion 끝까지 아래로 교체:

```cpp
        const f32_t expectedYasuoShield =
            YasuoGameSim::ResolvePassiveShieldAmount(
                world.GetComponent<ChampionComponent>(yasuo).level);
        DamageRequest yasuoMinionHit{};
        yasuoMinionHit.source = minionAttacker;
        yasuoMinionHit.target = yasuo;
        yasuoMinionHit.sourceTeam = eTeam::Red;
        yasuoMinionHit.type = eDamageType::True;
        yasuoMinionHit.flatAmount = 10.f;
        EnqueueDamageRequest(world, yasuoMinionHit);
        TickContext yasuoMinionTick = MakeProbeTickContext(
            298ull, rng, entityMap, walkable);
        CDamageQueueSystem::Execute(world, yasuoMinionTick);
        if (!world.HasComponent<ShieldComponent>(yasuo) ||
            std::abs(
                world.GetComponent<ShieldComponent>(yasuo).fCurrent -
                    (expectedYasuoShield - 10.f)) > 0.001f ||
            readyYasuoState.fPassiveFlow != 0.f)
        {
            std::printf("[SimLab][Shield] FAIL: minion did not trigger Yasuo passive\n");
            return false;
        }

        CShieldSystem::Clear(world, yasuo);
        readyYasuoState.fPassiveFlow = readyYasuoState.fPassiveFlowMax;
        DamageRequest yasuoJungleHit = yasuoMinionHit;
        yasuoJungleHit.source = jungleAttacker;
        EnqueueDamageRequest(world, yasuoJungleHit);
        TickContext yasuoJungleTick = MakeProbeTickContext(
            299ull, rng, entityMap, walkable);
        CDamageQueueSystem::Execute(world, yasuoJungleTick);
        if (!world.HasComponent<ShieldComponent>(yasuo) ||
            std::abs(
                world.GetComponent<ShieldComponent>(yasuo).fCurrent -
                    (expectedYasuoShield - 10.f)) > 0.001f ||
            readyYasuoState.fPassiveFlow != 0.f)
        {
            std::printf("[SimLab][Shield] FAIL: jungle did not trigger Yasuo passive\n");
            return false;
        }

        CShieldSystem::Clear(world, yasuo);
        readyYasuoState.fPassiveFlow = readyYasuoState.fPassiveFlowMax;
```

뒤쪽에서 삭제할 중복 코드:

```cpp
        const f32_t expectedYasuoShield =
            YasuoGameSim::ResolvePassiveShieldAmount(
                world.GetComponent<ChampionComponent>(yasuo).level);
```

기존 코드:

```cpp
        if (passiveEventCount != 1u || !bPassiveDurationMatched ||
```

아래로 교체:

```cpp
        if (passiveEventCount != 3u || !bPassiveDurationMatched ||
```

이로써 minion/jungle/champion이 모두 queue path와 EffectTrigger 1회씩을 통과한다.

`RunServerAuthoritativeShieldProbe` 바로 아래에 추가:

```cpp
    bool_t RunJungleRespawnLifecycleProbe()
    {
        const SpawnObjectDefinitionPack& pack =
            ServerData::GetLoLSpawnObjectDefinitionPack();
        if (std::abs(pack.defaultJungleCamp.respawnDelaySec - 30.f) > 0.001f)
            return false;
        for (u8_t subKind = 0u; subKind <= 10u; ++subKind)
        {
            if (std::abs(pack.ResolveJungleCamp(subKind).respawnDelaySec - 30.f) > 0.001f)
                return false;
        }

        CWorld world;
        DeterministicRng rng(20260719ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        const EntityID entity = world.CreateEntity();
        JungleComponent jungle{};
        jungle.subKind = 10u;
        jungle.hp = 0.f;
        jungle.maxHp = 450.f;
        jungle.vSpawnPosition = Vec3{ 4.f, 0.f, 6.f };
        jungle.fRespawnDelaySec = 30.f;
        world.AddComponent<JungleComponent>(entity, jungle);
        HealthComponent health{};
        health.fCurrent = 0.f;
        health.fMaximum = 450.f;
        health.bIsDead = true;
        world.AddComponent<HealthComponent>(entity, health);
        TransformComponent transform{};
        transform.SetPosition(Vec3{ 20.f, 0.f, -5.f });
        world.AddComponent<TransformComponent>(entity, transform);
        world.AddComponent<SkillStateComponent>(entity, SkillStateComponent{});
        world.AddComponent<TargetableTag>(entity, TargetableTag{});
        JungleAIComponent ai{};
        ai.aggroRange = 3.f;
        ai.leashRange = 9.f;
        ai.anchorX = 4.f;
        ai.anchorZ = 6.f;
        ai.bHasAnchor = true;
        world.AddComponent<JungleAIComponent>(entity, ai);

        std::vector<GameCommand> commands;
        for (u64_t tick = 1ull; tick <= 29ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            tc.fDt = 1.f;
            commands.clear();
            CJungleAISystem::Execute(world, tc, commands);
        }
        const JungleComponent& pending = world.GetComponent<JungleComponent>(entity);
        if (!pending.bRespawnPending ||
            std::abs(pending.fRespawnTimerSec - 1.f) > 0.001f ||
            !world.GetComponent<HealthComponent>(entity).bIsDead ||
            world.HasComponent<TargetableTag>(entity))
        {
            return false;
        }

        TickContext respawnTick = MakeProbeTickContext(30ull, rng, entityMap, walkable);
        respawnTick.fDt = 1.f;
        CJungleAISystem::Execute(world, respawnTick, commands);
        const JungleComponent& respawned = world.GetComponent<JungleComponent>(entity);
        const HealthComponent& respawnedHealth = world.GetComponent<HealthComponent>(entity);
        const Vec3 position = world.GetComponent<TransformComponent>(entity).GetPosition();
        if (respawned.bRespawnPending || respawnedHealth.bIsDead ||
            std::abs(respawnedHealth.fCurrent - 450.f) > 0.001f ||
            std::abs(position.x - 4.f) > 0.001f ||
            std::abs(position.z - 6.f) > 0.001f ||
            !world.HasComponent<TargetableTag>(entity))
        {
            return false;
        }
        std::printf("[SimLab][JungleRespawn] PASS: 11 camps, delay=30 sec\n");
        return true;
    }
```

기존 `--shield-only` 분기 바로 위에 추가:

```cpp
    if (argc > 1 && std::strcmp(argv[1], "--jungle-respawn-only") == 0)
    {
        const bool_t bPass = RunJungleRespawnLifecycleProbe();
        std::printf("[SimLab] %s\n", bPass ? "PASS" : "FAIL");
        return bPass ? 0 : 1;
    }
```

Yasuo/Lee Sin Q 수식은 gameplay source를 바꾸지 않고 기존 `--f4-balance-only`와 JSON/코드 경로 대조로 검증한다.

## 3. 검증

예측:
- 현재 Q3 dry-run은 67개 channel, 실제 repair 뒤 재실행은 `[ok] no degenerate position track`이다. CPU skin bounds는 기존 약 `-0.010..1.668m`에서 rest 복구 기준 약 `-0.010..2.50m`로 회복하고 Q3 중 world Transform은 바뀌지 않는다.
- shield probe는 minion/jungle/champion 모두 첫 피해를 shield로 흡수하고 EffectTrigger를 1회씩 낸다. E ring과 같은 `kill_when_anchor_invalid=true` FX는 health owner/target 사망 다음 client FX update에 제거된다.
- KrugMini 포함 모든 정글 시체는 1.5초 뒤 숨고 30초 authoritative timer 뒤 같은 NetEntityId로 full HP·spawn 위치에 보인다. Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다.
- F4에서 Yasuo Q Flat 행은 variant param 안내, Lee Sin Q1 Flat 행은 편집 가능, Q2는 별도 runtime scalar로 보인다. 100 Total AD, ratio 2.0이면 Yasuo Q1 raw는 `60+200=260`, Q3 raw는 `100+200=300`이고 방어력은 이후 적용된다.
- dirty 파일은 작업 전 diff를 보존하며 대상 anchor 주변의 요청 hunk 외 기존 변경은 변하지 않는다.

검증 명령:
- `python Tools/Anim/patch_wanim_root_track.py Client/Bin/Resource/Texture/Character/Yasuo/anims/skinned_mesh_yasuo_spell1_wind.wanim Client/Bin/Resource/Texture/Character/Yasuo/yasuo_fixed.wskel --dry-run`
- 같은 명령에서 `--dry-run` 제거 후 실행, 다시 `--dry-run` 실행.
- `powershell -ExecutionPolicy Bypass -File Tools/convert_all_assets.ps1 champions`
- `python Tools/LoLData/Build-LoLDefinitionPack.py`
- `msbuild Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1`
- `msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1`
- `msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1`
- `msbuild Tools/SimLab/SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1`
- `Tools/Bin/Debug/SimLab.exe --shield-only`
- `Tools/Bin/Debug/SimLab.exe --jungle-respawn-only`
- `Tools/Bin/Debug/SimLab.exe --f4-balance-only`
- `git diff --check`

미검증:
- 자동 harness에는 실제 GPU skinning/F4 ImGui 화면/E target ring capture가 없어 마지막 F5에서 Q3 발바닥, E 대상 사망, 미니 돌거북 1.5초/30초 시각 동작을 확인해야 한다.

확인 필요:
- F4 최소 해상도·DPI 기준은 현재 문서/코드에 명시가 없어 기본 개발 해상도에서 clipping 여부만 수동 확인한다.

## 서브 에이전트 비평

초안 비평 주체: `/root/yasuo_plan_critic` (read-only).

- P1 idle reference 방향이 좌표계상 오히려 world offset을 만들 수 있음: 수용. idle 복사를 폐기하고 CPU skin bounds로 확인한 all-zero meaningful position channel의 WSkel rest 복구로 변경.
- P1 script에 hash/bounds/backup/option 안전장치 부족: 수용. WAnim key bounds, WSkel/WAnim hash, basename 제한, hash backup, strict args를 추가.
- P1 ignored runtime resource 수정이 recook에 남지 않음: 수용. `convert_all_assets.ps1`의 Yasuo cook/up-to-date 공통 후처리에 연결.
- P1 SimLab 산출물 경로 오류: 수용. `Client/Bin/Debug`가 아니라 `Tools/Bin/Debug/SimLab.exe`로 수정.
- P1 dirty SimLab 보존 계약 부족: 수용. 작업 전후 target diff 비교와 수술적 hunk 검증을 예측에 추가.
- P1 generic FX 의미 확장 검증 부족: 수용. Jax R/Riven E/Yasuo passive·E 등 opt-in FX만 죽음으로 무효화되는 계약과 manual client gate를 명시.
- P2 shield direct-call test가 실제 queue를 우회: 수용. 세 source 모두 enqueue/execute 경로와 cue 횟수를 검증.
- 재비평 P1 부활 뒤 `baseAnimId`가 death pose를 유지할 수 있음: 수용. death 진입/부활 모두 base ID와 action timer를 초기화.
- 재비평 P1 Q3 post-cook 입력 누락이 조용히 skip됨: 수용. Yasuo source가 있는데 clip/skel이 없으면 asset script를 실패 처리.
- 재비평 P1 변경 지시/ImGui 계약이 prose-only임: 수용. dirty 기존 파일을 exact add/replace block으로 전환하고 사용자 작업·범위표·owner chain·중복 경로·capture 경로를 추가.
- 재비평 P1 Objectives dirty owner 오기: 수용. 실제 scope의 `draft.bSpawnObjectDirty`로 수정.

수정본 델타 재비평: 대기. P0/P1 잔존 0 확인 전 소스 수정 금지.
