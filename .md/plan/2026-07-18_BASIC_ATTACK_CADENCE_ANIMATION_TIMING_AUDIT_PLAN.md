# 2026-07-18 기본 공격 cadence · 애니메이션 타이밍 전수 감사/수정 계획서

```text
Session - 표시 공격속도 0.8/1.0/1.5/2.0/2.5가 30 Hz 서버의 실제 재공격 가능 틱과 일치하고, 17챔피언 BA impact가 실제 .wanim TPS의 cast marker에 맞도록 전수 검증·수정
좌표: 신규 좌표 후보 · 축 C1 기준계 · C7 권위/정합성 · C8 검증이 병목
관련: 2026-07-14_IRELIA_ATTACK_SPEED_LAB_IMPLEMENTATION_PLAN, 2026-07-18_GAMEFEEL_EZREAL_BASELINE_ANALYSIS_AND_REMEDIATION_PLAN/_RESULT, 2026-07-18_SUSTAINED_BASIC_ATTACK_CHASE_RESULT
```

## 1. 결정 기록

```text
① 문제·제약: 서버 BA cooldown은 정확히 1/attackSpeed지만 float countdown 잔여로 AS 2.0이 15틱이 아닌 16틱 뒤 ready가 될 수 있다(실효 1.875회/초). 기존 SimLab은 대입된 float만 검사해 이 실패를 PASS했다. 17종 BA 중 cast marker가 있는 16종은 실제 .wanim TPS=24인데 basicAttackWindupSec는 castFrame/30/playback으로 저장돼 impact가 33~50ms 빠르다.
② 순진한 해법의 실패: attackSpeed·animationPlaybackSpeed·lockDurationSec를 같은 숫자로 만들면 단위가 다른 계약을 섞는다. 공격 주기는 초당 횟수의 역수, 애니메이션은 최종AS/baseAS 배율, impact는 asset tick/TPS/playback, action lock은 행동 수명이다. lock을 cadence로 줄이거나 client 보정만 더하면 서버 impact/피해와 시각이 갈라진다.
③ 메커니즘: SkillCooldown의 1e-6 이하 양의 잔여를 같은 tick에 0으로 정규화하고, SimLab이 실제 GameRoom 순서(Cooldown→CombatAction→AttackChase→pending execute)를 300틱 진행해 ceil((1/AS)*30)마다 BA가 재시작되는지 검사한다. BA windup은 runtime loader와 같은 validity/skel-hash/substring 선택 및 frame-duration clamp 뒤 castFrame/TPS/playback으로 고정한다.
④ 대조군: 최종 attackSpeed 산식과 snapshot 값, 서버/client attackSpeed/baseAttackSpeed 배율, BA action/windup의 동일 배율은 이미 정합이다. Yasuo는 cast/recovery marker가 0이라 기존 0.175s manual fallback을 유지한다. Irelia action lock은 recovery exact 0.466667s로 정렬한다. AS 3.003에서 cadence는 30Hz 양자화로 10틱=3.0/s이고 animation scale은 server/client 모두 의도된 4x 안전 cap을 유지한다.
⑤ 대가·예산: epsilon은 모든 스킬 cooldown의 무의미한 1e-6 이하 잔여도 한 tick 일찍 0으로 정규화한다. 30Hz에서는 0.8 AS가 38틱=약 0.78947/s라 임의 실수 AS를 모두 정확히 표현할 수 없지만, 사용자 기준 1.5/2.0은 20/15틱으로 정확하다. 70% 바닥 일은 데이터·실제 cadence·asset 계약·Debug/Release 빌드, 30% 천장 일은 Irelia 1.5/2.0의 동일 조건 10초 캡처이나 자동 환경에서는 미검증으로 남긴다. Lethal Tempo/Irelia passive는 별도 gameplay 기능 공백이다.
```

## 서브 에이전트 비평

```text
비평 주체: /root/plan_critic
상태: 완료 — 실제 코드/데이터/asset loader 대조, 파일 수정 없음
P0-1 수용: BuyItem은 HandleBuyItem(void) 뒤 Unknown을 반환하므로 Accepted 검사를 삭제하고 inventory/gold/bDirty의 상태 전이를 검사한다.
P1-1 수용: 직접 두 번째 ExecuteCommand 대신 실제 GameRoom 순서 Cooldown→CombatAction→AttackChase→pending execute를 300틱 반복하고 emission/Accepted/action start/sequence/cooldown reset/총 횟수를 검사한다.
P1-2 수용: Python gate는 WINT version/flags/content-size, WSKL bone hash, WANM meta/span/trailer hash를 검사하고 invalid clip을 건너뛴다. marker frame은 min(frame,durationTicks)로 runtime과 같이 clamp한다.
P1-3 수용: Irelia BA lock 0.460000을 recovery exact 0.466667로 정렬한다. 1 server tick tolerance는 제거한다.
P1-4 수용(정책 유지): AS 3.003을 matrix에 추가한다. server/client 4x animation cap은 동일하고 clip 과속 방지 정책으로 유지하되, cadence 10틱(3.0/s)과 시각 배율 cap이 서로 다른 단위임을 명시한다.
P1-5 수용: codegen은 generate→--check 순서로 바꾼다.
P1-6 수용: 다른 writer가 멈춘 시점에 1회 생성하고, 생성 직전/직후 dirty 목록과 generated diff를 비교한다. 산출물 목록을 실제 generator 전체 출력으로 확장한다.
P2 전부 수용: BA는 배열 index가 아니라 slot/key로 찾고, Yasuo는 manual fallback으로 기록하며, 0.8은 38틱=0.78947/s 양자화로 표현한다. repeat action start/sequence/cooldown reset을 검사하고 Irelia item probe는 level-6 공식 대조군으로 한정한다.
```

## 2. 반영해야 하는 코드

### 2-0. 계획서 서브 에이전트 비평 공통 계약

사용자 추가 요청에 따라 `AGENTS.md`, `CLAUDE.md`, `.md/계획서작성규칙.md`에 다음 계약을 같은 의미로 반영한다.

```text
- dated 구현 계획서는 소스 수정 전 최소 1명의 독립 서브 에이전트 read-only 비평을 받는다.
- 주 에이전트는 P0/P1/P2 지적별 수용/기각/보류와 근거를 계획서에 기록하고 수정한 뒤 구현한다.
- sub-agent 도구가 없으면 CONFIRM_NEEDED로 표시하고 구현/검토 완료를 주장하지 않는다.
```

### 2-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.cpp

익명 namespace 시작부에 아래 상수를 추가한다.

```cpp
    constexpr f32_t kCooldownReadyEpsilon = 0.000001f;
```

`SkillStateComponent` slot cooldown 감소 블록의 기존 코드:

```cpp
            if (slot.cooldownRemaining > 0.f)
            {
                slot.cooldownRemaining -= tc.fDt;
                if (slot.cooldownRemaining <= 0.f)
                {
                    slot.cooldownRemaining = 0.f;
                    slot.cooldownDuration = 0.f;
                }
            }
```

아래로 교체한다.

```cpp
            if (slot.cooldownRemaining > 0.f)
            {
                slot.cooldownRemaining -= tc.fDt;
                if (slot.cooldownRemaining <= kCooldownReadyEpsilon)
                {
                    slot.cooldownRemaining = 0.f;
                    slot.cooldownDuration = 0.f;
                }
            }
```

SummonerSpell cooldown과 stageWindow는 이번 BA cadence 범위에서 변경하지 않는다.

### 2-2. C:/Users/user/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

각 champion의 `stats.basicAttackWindupSec`를 아래 값으로 교체한다. 값은 `ChampionVisualDefs.json` BA `castFrame / 실제 선택 .wanim TPS / animationPlaybackSpeed`다. Yasuo는 marker 0 fallback이라 유지한다.

```text
IRELIA   0.160000 -> 0.200000
YASUO    0.175000 -> 0.175000 (유지: marker 없음)
KALISTA  0.200000 -> 0.250000
GAREN    0.200000 -> 0.250000
ZED      0.200000 -> 0.250000
RIVEN    0.200000 -> 0.250000
EZREAL   0.200000 -> 0.250000
FIORA    0.200000 -> 0.250000
JAX      0.200000 -> 0.250000
LEESIN   0.133333 -> 0.166667
KINDRED  0.133333 -> 0.166667
MASTERYI 0.133333 -> 0.166667
ANNIE    0.200000 -> 0.250000
ASHE     0.166667 -> 0.208333
VIEGO    0.200000 -> 0.250000
YONE     0.196078 -> 0.245098
SYLAS    0.133333 -> 0.166667
```

Irelia BA stage의 action lock도 실제 recovery marker와 exact 정렬한다.

기존 코드:

```json
                            "lockDurationSec": 0.46,
```

아래로 교체:

```json
                            "lockDurationSec": 0.466667,
```

### 2-3. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`RunAttackSpeedLabMatrixProbe()`의 target matrix에 최종 AS cap을 추가한다.

```cpp
        static constexpr f32_t kTargetAttackSpeeds[] =
        {
            0.8f, 1.0f, 1.5f, 2.0f, 2.5f, 3.003f,
        };
```

`RunBasicAttackGameFeelContractProbe()`의 기존 `kExpected` 전체를 아래로 교체한다.

```cpp
        static constexpr BasicAttackWindupExpectation kExpected[] =
        {
            { eChampion::IRELIA, 0.2f },
            { eChampion::YASUO, 0.175f },
            { eChampion::KALISTA, 0.25f },
            { eChampion::GAREN, 0.25f },
            { eChampion::ZED, 0.25f },
            { eChampion::RIVEN, 0.25f },
            { eChampion::EZREAL, 0.25f },
            { eChampion::FIORA, 0.25f },
            { eChampion::JAX, 0.25f },
            { eChampion::LEESIN, 0.166667f },
            { eChampion::KINDRED, 0.166667f },
            { eChampion::MASTERYI, 0.166667f },
            { eChampion::ANNIE, 0.25f },
            { eChampion::ASHE, 0.208333f },
            { eChampion::VIEGO, 0.25f },
            { eChampion::YONE, 0.245098f },
            { eChampion::SYLAS, 0.166667f },
        };
```

`RunAttackSpeedLabMatrixProbe()`의 `attackTiming`/`sequence` 기존 코드:

```cpp
        const ChampionBasicAttackTimingDefaults attackTiming =
            GetDefaultChampionBasicAttackTiming(eChampion::IRELIA);
        u32_t sequence = 1u;
```

바로 아래에 Irelia 레벨 6의 Trinity Force + Blade of the Ruined King 구매/공속 산출 계약을 추가한다.

```cpp
        {
            CWorld itemWorld;
            DeterministicRng itemRng(2026071801ull);
            EntityIdMap itemEntityMap;
            FlatWalkable itemWalkable;
            auto itemExecutor = CDefaultCommandExecutor::Create();
            const EntityID itemIrelia = SpawnChampion(
                itemWorld,
                itemEntityMap,
                eChampion::IRELIA,
                static_cast<u8_t>(eTeam::Blue),
                0u);
            itemWorld.GetComponent<GoldComponent>(itemIrelia).amount = 10000u;

            static constexpr u16_t kItems[] = { 3078u, 3153u };
            for (u32_t itemIndex = 0u; itemIndex < 2u; ++itemIndex)
            {
                GameCommand buy{};
                buy.kind = eCommandKind::BuyItem;
                buy.issuerEntity = itemIrelia;
                buy.itemId = kItems[itemIndex];
                buy.sequenceNum = itemIndex + 1u;
                TickContext itemTick = MakeProbeTickContext(
                    itemIndex + 1u,
                    itemRng,
                    itemEntityMap,
                    itemWalkable);
                itemExecutor->ExecuteCommand(itemWorld, itemTick, buy);
                static constexpr u32_t kExpectedGold[] = { 6667u, 3467u };
                const InventoryComponent& inventory =
                    itemWorld.GetComponent<InventoryComponent>(itemIrelia);
                const GoldComponent& gold =
                    itemWorld.GetComponent<GoldComponent>(itemIrelia);
                const StatComponent& pendingStat =
                    itemWorld.GetComponent<StatComponent>(itemIrelia);
                if (inventory.count != itemIndex + 1u ||
                    inventory.itemIds[itemIndex] != kItems[itemIndex] ||
                    gold.amount != kExpectedGold[itemIndex] ||
                    !pendingStat.bDirty)
                {
                    std::printf(
                        "[SimLab][AttackSpeedLab] FAIL: Irelia core item %u count=%u slot=%u gold=%u dirty=%u\n",
                        static_cast<u32_t>(kItems[itemIndex]),
                        static_cast<u32_t>(inventory.count),
                        static_cast<u32_t>(inventory.itemIds[itemIndex]),
                        gold.amount,
                        pendingStat.bDirty ? 1u : 0u);
                    return false;
                }
            }

            CStatSystem::Execute(
                itemWorld,
                ServerData::GetLoLGameplayDefinitionPack());
            const StatComponent& itemStat =
                itemWorld.GetComponent<StatComponent>(itemIrelia);
            if (!NearlyEqual(itemStat.bonusAttackSpeed, 0.55f) ||
                !NearlyEqual(itemStat.attackSpeed, 1.08158f))
            {
                std::printf(
                    "[SimLab][AttackSpeedLab] FAIL: Irelia Trinity+BORK level6 bonus=%.6f/0.550000 effective=%.6f/1.081580\n",
                    itemStat.bonusAttackSpeed,
                    itemStat.attackSpeed);
                return false;
            }
            std::printf(
                "[SimLab][AttackSpeedLab] Irelia level6 Trinity+BORK attackSpeed=%.6f\n",
                itemStat.attackSpeed);
        }
```

각 target AS의 최초 timing 검증 블록 직후, practice override 제거 전 위치에 아래 실제 GameRoom 지속 공격 순서 300틱 검사를 추가한다.

```cpp
            const u64_t expectedCadenceTicks = static_cast<u64_t>(std::ceil(
                static_cast<f64_t>(expectedCooldown - 0.000001f) *
                static_cast<f64_t>(DeterministicTime::kTicksPerSecond)));
            constexpr u64_t kSustainTicks =
                10u * DeterministicTime::kTicksPerSecond;
            u32_t acceptedAttackCount = 1u;
            for (u64_t elapsedTick = 1u;
                elapsedTick <= kSustainTicks;
                ++elapsedTick)
            {
                TickContext cadenceTick = MakeProbeTickContext(
                    attackTick.tickIndex + elapsedTick,
                    rng,
                    entityMap,
                    walkable);
                CSkillCooldownSystem::Execute(world, cadenceTick);
                CCombatActionSystem::Execute(world, cadenceTick);
                std::vector<GameCommand> pendingCommands;
                CAttackChaseSystem::Execute(
                    world,
                    cadenceTick,
                    pendingCommands);

                const bool_t bExpectedAttack =
                    elapsedTick % expectedCadenceTicks == 0u;
                if (pendingCommands.size() != (bExpectedAttack ? 1u : 0u))
                {
                    std::printf(
                        "[SimLab][AttackSpeedLab] FAIL: chase emission target=%.3f tick=%llu cadence=%llu commands=%zu expected=%u\n",
                        targetAttackSpeed,
                        static_cast<unsigned long long>(elapsedTick),
                        static_cast<unsigned long long>(expectedCadenceTicks),
                        pendingCommands.size(),
                        bExpectedAttack ? 1u : 0u);
                    return false;
                }

                if (bExpectedAttack)
                {
                    const GameCommand& repeatAttack = pendingCommands[0];
                    const CommandExecutionResult repeatResult =
                        executor->ExecuteCommand(world, cadenceTick, repeatAttack);
                    const CombatActionComponent* pRepeatAction =
                        world.TryGetComponent<CombatActionComponent>(attacker);
                    const SkillSlotRuntime& repeatSlot = world
                        .GetComponent<SkillStateComponent>(attacker)
                        .slots[0];
                    if (repeatResult.state != eCommandExecutionState::Accepted ||
                        repeatAttack.sequenceNum != attack.sequenceNum ||
                        !pRepeatAction ||
                        pRepeatAction->uStartTick != cadenceTick.tickIndex ||
                        pRepeatAction->uSequenceNum != attack.sequenceNum ||
                        !NearlyEqual(repeatSlot.cooldownRemaining, expectedCooldown) ||
                        !NearlyEqual(repeatSlot.cooldownDuration, expectedCooldown))
                    {
                        std::printf(
                            "[SimLab][AttackSpeedLab] FAIL: repeat start target=%.3f tick=%llu state=%u seq=%u actionTick=%llu actionSeq=%u cooldown=%.9f/%.9f\n",
                            targetAttackSpeed,
                            static_cast<unsigned long long>(elapsedTick),
                            static_cast<u32_t>(repeatResult.state),
                            repeatAttack.sequenceNum,
                            pRepeatAction
                                ? static_cast<unsigned long long>(pRepeatAction->uStartTick)
                                : 0ull,
                            pRepeatAction ? pRepeatAction->uSequenceNum : 0u,
                            repeatSlot.cooldownRemaining,
                            expectedCooldown);
                        return false;
                    }
                    ++acceptedAttackCount;
                }
            }

            const u32_t expectedAttackCount = 1u + static_cast<u32_t>(
                kSustainTicks / expectedCadenceTicks);
            if (acceptedAttackCount != expectedAttackCount)
            {
                std::printf(
                    "[SimLab][AttackSpeedLab] FAIL: sustained count target=%.3f attacks=%u/%u cadenceTicks=%llu\n",
                    targetAttackSpeed,
                    acceptedAttackCount,
                    expectedAttackCount,
                    static_cast<unsigned long long>(expectedCadenceTicks));
                return false;
            }
```

행별 성공 로그에 cadence tick을 추가한다.

```cpp
            std::printf(
                "[SimLab][AttackSpeedLab] target=%.3f cooldown=%.3f cadenceTicks=%llu attacks10s=%u actionTicks=%llu windupTicks=%llu\n",
                targetAttackSpeed,
                expectedCooldown,
                static_cast<unsigned long long>(expectedCadenceTicks),
                acceptedAttackCount,
                static_cast<unsigned long long>(actualActionTicks),
                static_cast<unsigned long long>(actualWindupTicks));
```

### 2-4. 새 파일: C:/Users/user/Desktop/Winters/Tools/LoLData/Test-BasicAttackTimingContract.py

전체 파일 내용:

```python
from __future__ import annotations

import json
import math
import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
RESOURCE_ROOT = ROOT / "Client" / "Bin" / "Resource"
GAMEPLAY_PATH = ROOT / "Data" / "Gameplay" / "ChampionGameData" / "champions.json"
VISUAL_PATH = ROOT / "Data" / "LoL" / "ClientPublic" / "Visual" / "ChampionVisualDefs.json"
ASSET_PATH = ROOT / "Data" / "LoL" / "ClientPublic" / "Visual" / "ChampionAssetVisualDefs.json"
FLOAT_TOLERANCE = 0.00001
ZERO_MARKER_FALLBACKS = {"YASUO"}
WINT_HEADER_SIZE = 16
WSKEL_META_SIZE = 32
WSKEL_BONE_SIZE = 256
WSKEL_GLOBAL_ROOT_SIZE = 128
WSKEL_SOCKET_SIZE = 128
WANIM_META_SIZE = 32
WANIM_CHANNEL_SIZE = 40
WANIM_EVENT_SIZE = 32
WANIM_TRAILER_SIZE = 8
FNV64_OFFSET = 0xCBF29CE484222325
FNV64_PRIME = 0x100000001B3


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def read_wint_payload(path: Path) -> bytes:
    data = path.read_bytes()
    if len(data) < WINT_HEADER_SIZE:
        raise ValueError(f"truncated WINT header: {path}")
    magic, major, _minor, flags, content_size = struct.unpack_from(
        "<4sHHII", data, 0)
    if magic != b"WINT" or major != 1 or flags != 0:
        raise ValueError(
            f"invalid WINT header magic={magic!r} major={major} flags={flags}: {path}")
    if content_size > len(data) - WINT_HEADER_SIZE:
        raise ValueError(f"WINT content exceeds file: {path}")
    return data[WINT_HEADER_SIZE:WINT_HEADER_SIZE + content_size]


def read_wskel_hash(path: Path) -> int:
    payload = read_wint_payload(path)
    if len(payload) < WSKEL_META_SIZE:
        raise ValueError(f"truncated WSKL meta: {path}")
    magic, bone_count, socket_count = struct.unpack_from("<4sII", payload, 0)
    if magic != b"WSKL" or bone_count == 0 or bone_count > 1024 or socket_count > 256:
        raise ValueError(f"invalid WSKL meta: {path}")
    required = (
        WSKEL_META_SIZE + bone_count * WSKEL_BONE_SIZE +
        WSKEL_GLOBAL_ROOT_SIZE + socket_count * WSKEL_SOCKET_SIZE)
    if required > len(payload):
        raise ValueError(f"truncated WSKL payload: {path}")

    skeleton_hash = FNV64_OFFSET
    for bone_index in range(bone_count):
        bone_offset = WSKEL_META_SIZE + bone_index * WSKEL_BONE_SIZE
        name_hash = struct.unpack_from("<Q", payload, bone_offset)[0]
        parent_index = struct.unpack_from("<i", payload, bone_offset + 72)[0]
        if parent_index >= bone_count:
            raise ValueError(f"invalid WSKL parent index: {path}")
        skeleton_hash ^= name_hash
        skeleton_hash = (skeleton_hash * FNV64_PRIME) & 0xFFFFFFFFFFFFFFFF
    return skeleton_hash


def read_valid_wanim(path: Path, expected_skeleton_hash: int) -> tuple[float, float]:
    payload = read_wint_payload(path)
    if len(payload) < WANIM_META_SIZE:
        raise ValueError(f"truncated WANM meta: {path}")
    (
        magic,
        channel_count,
        duration_ticks,
        ticks_per_second,
        total_key_count,
        event_count,
    ) = struct.unpack_from("<4sIffII", payload, 0)
    if (
        magic != b"WANM" or channel_count > 1024 or
        total_key_count > 1_000_000 or
        not math.isfinite(duration_ticks) or duration_ticks < 0.0 or
        not math.isfinite(ticks_per_second) or ticks_per_second <= 0.0
    ):
        raise ValueError(f"invalid WANM meta: {path}")

    channel_bytes = channel_count * WANIM_CHANNEL_SIZE
    channel_end = WANIM_META_SIZE + channel_bytes
    event_bytes = event_count * WANIM_EVENT_SIZE
    if len(payload) < channel_end + event_bytes + WANIM_TRAILER_SIZE:
        raise ValueError(f"truncated WANM payload: {path}")
    key_block_size = len(payload) - channel_end - event_bytes - WANIM_TRAILER_SIZE
    for channel_index in range(channel_count):
        channel_offset = WANIM_META_SIZE + channel_index * WANIM_CHANNEL_SIZE
        (
            _bone_hash,
            pos_count,
            pos_offset,
            rot_count,
            rot_offset,
            scale_count,
            scale_offset,
            _cached_index,
            _reserved,
        ) = struct.unpack_from("<QIIIIIIiI", payload, channel_offset)
        spans = (
            (pos_offset, pos_count, 16),
            (rot_offset, rot_count, 20),
            (scale_offset, scale_count, 16),
        )
        if any(offset > key_block_size or count * stride > key_block_size - offset
               for offset, count, stride in spans):
            raise ValueError(f"WANM key span exceeds block: {path}")

    trailer_hash = struct.unpack_from(
        "<Q", payload, len(payload) - WANIM_TRAILER_SIZE)[0]
    if trailer_hash != expected_skeleton_hash:
        raise ValueError(f"WANM skeleton hash mismatch: {path}")
    return duration_ticks, ticks_per_second


def resolve_basic_attack_clip(model: dict) -> tuple[Path, str, float, float]:
    prefix = str(model.get("animPrefix", ""))
    animation = str(model["basicAttackAnimation"])
    query = animation if animation.startswith(prefix) else prefix + animation
    mesh_path = RESOURCE_ROOT / Path(str(model["mesh"]))
    mesh_parent = mesh_path.parent
    skeleton_hash = read_wskel_hash(mesh_path.with_suffix(".wskel"))
    anim_dir = mesh_parent / "anims"
    candidates = sorted(anim_dir.glob("*.wanim"), key=lambda path: str(path))
    for candidate in candidates:
        try:
            duration_ticks, ticks_per_second = read_valid_wanim(
                candidate, skeleton_hash)
        except ValueError:
            continue
        if query in candidate.stem:
            return candidate, query, duration_ticks, ticks_per_second
    raise ValueError(f"no runtime-valid .wanim contains '{query}' under {anim_dir}")


def main() -> int:
    gameplay = load_json(GAMEPLAY_PATH)
    visuals = load_json(VISUAL_PATH)
    assets = load_json(ASSET_PATH)

    visual_by_champion = {
        str(entry["key"]).split(".")[-1].upper(): entry
        for entry in visuals["champions"]
    }
    model_by_champion = {
        str(entry["champion"]).upper(): entry
        for entry in assets["models"]
    }

    errors: list[str] = []
    aligned_count = 0
    fallback_count = 0
    observed_fallbacks: set[str] = set()

    for champion in gameplay["champions"]:
        name = str(champion["champion"]).upper()
        try:
            visual = visual_by_champion[name]
            model = model_by_champion[name]
            basic_visual = next(
                skill for skill in visual["skills"]
                if str(skill["key"]).endswith(".basic_attack"))
            stage = basic_visual["stages"][0]
            playback = float(stage["animationPlaybackSpeed"])
            cast_frame = float(stage["castFrame"])
            recovery_frame = float(stage["recoveryFrame"])
            windup = float(champion["stats"]["basicAttackWindupSec"])
            basic_gameplay = next(
                skill for skill in champion["skills"]
                if int(skill["slot"]) == 0)
            lock = float(basic_gameplay["stages"][0]["lockDurationSec"])
            (
                clip_path,
                query,
                duration_ticks,
                ticks_per_second,
            ) = resolve_basic_attack_clip(model)
        except (KeyError, StopIteration, TypeError, ValueError) as exc:
            errors.append(f"{name}: {exc}")
            continue

        if not math.isfinite(playback) or playback <= 0.0:
            errors.append(f"{name}: invalid playback {playback}")
            continue

        if cast_frame > 0.0:
            expected_windup = min(cast_frame, duration_ticks) / ticks_per_second / playback
            if abs(windup - expected_windup) > FLOAT_TOLERANCE:
                errors.append(
                    f"{name}: windup={windup:.6f}, expected={expected_windup:.6f} "
                    f"from cast={cast_frame:.3f}/TPS={ticks_per_second:.3f}/play={playback:.3f}")
            else:
                aligned_count += 1
        else:
            observed_fallbacks.add(name)
            fallback_count += 1
            if name not in ZERO_MARKER_FALLBACKS or windup <= 0.0:
                errors.append(f"{name}: unexpected zero cast marker/fallback windup={windup:.6f}")

        if windup > lock + FLOAT_TOLERANCE:
            errors.append(f"{name}: windup={windup:.6f} exceeds action lock={lock:.6f}")

        if recovery_frame > 0.0:
            recovery = min(recovery_frame, duration_ticks) / ticks_per_second / playback
            if recovery > lock + FLOAT_TOLERANCE:
                errors.append(
                    f"{name}: recovery={recovery:.6f} exceeds action lock={lock:.6f} "
                    "and would be truncated by the client clamp")
        elif name not in ZERO_MARKER_FALLBACKS:
            errors.append(f"{name}: unexpected zero recovery marker")

        print(
            f"[BasicAttackTiming] {name:<9} clip={clip_path.name} query={query} "
            f"TPS={ticks_per_second:.3f} durationTicks={duration_ticks:.3f} "
            f"cast={cast_frame:.3f} "
            f"windup={windup:.6f} lock={lock:.6f}")

    if observed_fallbacks != ZERO_MARKER_FALLBACKS:
        errors.append(
            f"zero-marker fallback set={sorted(observed_fallbacks)}, "
            f"expected={sorted(ZERO_MARKER_FALLBACKS)}")

    if errors:
        for error in errors:
            print(f"[BasicAttackTiming] FAIL: {error}", file=sys.stderr)
        return 1

    print(
        f"[BasicAttackTiming] PASS: champions={len(gameplay['champions'])} "
        f"asset-marker-aligned={aligned_count} explicit-fallbacks={fallback_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

### 2-5. C:/Users/user/Desktop/Winters/.claude/gotchas.md

`Format:` 설명 바로 아래 목록 시작에 추가한다.

```md
- 2026-07-18 - [Basic attack timing] visual `castFrame`/`recoveryFrame` are cooked animation ticks, not 30Hz GameSim frames; dividing them by 30 made every marker-authored BA impact 33-50ms early when the selected `.wanim` TPS was 24 -> resolve the same animation substring the runtime loads, read TPS from its `WANM` header, and gate `basicAttackWindupSec == castFrame / TPS / animationPlaybackSpeed`.
```

### 2-6. 생성 산출물

아래 파일은 source JSON을 직접 복제 편집하지 않고 생성기로 갱신한다.

```text
Shared/GameSim/Generated/ChampionGameData.generated.h
Shared/GameSim/Generated/ChampionGameData.generated.cpp
Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp
Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp
Shared/GameSim/Generated/ChampionAIPolicyData.generated.inl
Data/LoL/ServerPrivate/Gameplay/ChampionGameplayDefs.json
Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json
Data/LoL/SharedContract/DefinitionManifest.json
.md/TODO/06-22/LOL_DEFINITION_PACK_PARITY.json
```

다른 writer가 멈춘 뒤 생성 직전 dirty 목록을 보존하고 한 번만 생성한다. 생성 직후 `git diff --name-only`와 위 파일별 diff를 검사해 병행 AI/skill/visual source 변경이 포함됐는지 기록하며, 임의로 되돌리지 않는다.

## 3. 검증

예상:

- 수정 전 재현 계산: AS 2.0 cooldown 0.5가 float 잔여로 16틱 ready, 실효 1.875회/초. 수정 후 실제 AttackChase에서 15틱마다 재공격한다. AS 1.5는 20틱, AS 2.5는 12틱, AS 0.8은 양자화된 38틱(약 0.78947/s)이다.
- Irelia 레벨 6 + Trinity(0.30) + BORK(0.25)는 rune/passive를 제외한 성장·아이템 공식 대조군에서 최종 AS `1.081580`; 두 아이템만으로 1.5가 되는 것은 아니다.
- asset gate는 17종, 실제 marker 정합 16종, Yasuo explicit fallback 1종을 PASS한다.
- SimLab `--gamefeel-only`는 17 windup, 6개 AS의 300틱 지속 AttackChase cadence, Irelia item 공식 대조군을 함께 PASS한다. AS 3.003은 10틱 cadence/4x 시각 cap 정책을 검증한다.
- generated diff는 windup과 기존 병행 세션 데이터만 반영하고, 병행 작업의 AI/스키마 변경을 덮어쓰지 않는다.

검증 명령:

```powershell
python Tools/ChampionData/build_champion_game_data.py
python Tools/ChampionData/build_champion_game_data.py --check
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --check
python Tools/LoLData/Test-BasicAttackTimingContract.py
msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
Tools/Bin/Debug/SimLab.exe --gamefeel-only
msbuild Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Engine/Include/Engine.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
msbuild Server/Include/Server.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
msbuild Client/Include/Client.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
git diff --check
```

미검증:

- 자동 환경에서는 Irelia AS 1.5/2.0의 동일 대상 10초 직접 플레이 영상과 타격음/피해 프레임 체감 비교를 생성하지 못한다. 빌드 후 사용자가 F5에서 확인할 ceiling gate로 남긴다.
- Lethal Tempo 스택의 실제 AS 효과와 Irelia passive는 현재 정의/reader가 없는 별도 gameplay 기능이며 이번 타이밍 수정에 포함하지 않는다.

확인 필요:

- Yasuo BA cast/recovery marker가 모두 0이므로 0.175s는 asset-derived 정합값이 아니라 명시적 manual fallback이다. 신규 marker가 저작되면 gate의 fallback set에서 제거하고 실제 TPS 값으로 재산출한다.
