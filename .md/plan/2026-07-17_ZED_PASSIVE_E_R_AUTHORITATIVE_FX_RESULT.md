Session - 제드 패시브 50% 조건·패시브 BA·잃은 체력 추가 피해와 E/R WMesh 서버 cue 반영 결과를 계획의 예측과 대조한다.
좌표: 신규 좌표 후보 · 축: C7 · C8
관련: 2026-07-17_ZED_PASSIVE_E_R_AUTHORITATIVE_FX_PLAN.md

## 1. 예측 vs 실측

| 예측 | 정적 실측 | 판정 |
|---|---|---|
| 대상 HP가 50%를 초과하면 일반 BA, 50% 이하면 passive stage 2 | ServerPrivate JSON에 `targetHealthThresholdRatio=0.5`, `CommandExecutor`에 `CombatActionFlags::ZedPassive`, ClientPublic Zed BA stage 2와 `skinned_mesh_zed_attack_passive`가 결속됐다. | 코드 반영 PASS, 런타임 미실행 |
| 패시브 추가분은 잃은 체력 비례이며 crit/lifesteal/on-hit을 다시 발동하지 않음 | 별도 `DamageRequest`가 `targetMissingHpRatioOverride=0.1`, `eSourceKind=BasicAttack`, `flags=DamageFlag_None`으로 생성된다. | 코드 반영 PASS |
| E gameplay는 중복하지 않고 검은 WMesh 원만 교체 | 기존 `ZedGameSim::OnE`의 반경 2.75·본체/그림자 대상 de-dup·물리 피해를 유지했고 `e_slash.wfx`만 `zed_atkswipe.wmesh`, scale 2.75, AlphaBlend 검은 tint로 교체했다. | 회귀 방어 PASS, 눈검증 대기 |
| R은 red cross mark와 shield-aware lethal marker show/hide/pop을 가짐 | stage 1은 `zed_crossswipe.wmesh`, stage 4/5는 서버 lethal 전이, stage 2는 client tracked handle 제거 후 pop이다. event `durationMs`가 `fEffectLifetimeSec`로 전달된다. | 코드 반영 PASS, 눈검증 대기 |
| 생성 데이터가 canonical과 일치 | write/check 모두 pack hash `0x062F6277`, 17 champions·85 skills·1 summoner spell이다. JSON 6개 파싱과 지정 asset 6개 존재 확인도 통과했다. | PASS |

실제 패시브 추가 피해 요청:

```cpp
DamageRequest request{};
request.source = caster;
request.target = target;
request.sourceTeam = ResolveTeam(world, caster);
request.type = eDamageType::Physical;
request.iSourceSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
request.eSourceKind = eDamageSourceKind::BasicAttack;
request.targetMissingHpRatioOverride = ratio;
EnqueueDamageRequest(world, request);
```

실제 R marker 권위 전이:

```cpp
const bool_t bLethal = WouldNonCriticalDamageRequestKill(
    world,
    tc,
    previewRequest);
if (bLethal != mark.bLethalMarkerVisible)
{
    mark.bLethalMarkerVisible = bLethal;
    EmitZedEffect(
        world,
        mark.entitySource,
        entity,
        static_cast<u8_t>(eSkillSlot::R),
        mark.rank,
        bLethal ? kZedRLethalMarkerShowStage : kZedRLethalMarkerHideStage,
        targetPos,
        dir,
        bLethal ? static_cast<u16_t>(durationMs) : 0u,
        tc.tickIndex);
}
```

`WouldNonCriticalDamageRequestKill`은 실제 DamagePipeline 순서에 맞춰 raw formula → armor/MR 및 source penetration → Yasuo ready shield 또는 현재 `ShieldComponent` → Annie E shield → Kindred health floor를 읽으며 어떤 상태도 소비하지 않는다.

## 2. 판결

계획 채택. 단, 구현 전 정적 검토에서 두 가지를 수정 반영했다.

- 새 `TargetHealthThresholdRatio`를 enum 중간이 아니라 tail에 append했다. 기존 param numeric ID가 밀리는 practice override·checkpoint·replay 회귀를 차단했다.
- WFX의 attach position은 호출 위치보다 target anchor offset이 최종 좌표를 소유한다. cooked bounds를 읽어 passive 1.10m, red cross 1.20m, lethal marker 2.50m로 계획과 구현을 함께 교정했다.

반영된 흐름은 다음 하나다.

```text
ServerPrivate JSON
  -> CommandExecutor BA threshold/stage flag
  -> CombatAction impact의 일반 BA + 별도 passive DamageRequest
  -> Replicated Action/EffectTrigger
  -> Client stage2 passive animation + Zed.PassiveBA.Hit WFX

ZedDeathMarkComponent
  -> non-mutating server damage preview
  -> R stage4 show / stage5 hide / stage2 terminal pop
  -> tracked WFX entity destroy
```

신규 cue는 `Zed.PassiveBA.Hit`와 `Zed.R.LethalMarker`다. 기존 cue `Zed.E.Slash`와 `Zed.R.Mark`는 이름을 유지해 등록 경로를 늘리지 않고 recursive WFX preload가 새 본문을 읽게 했다. E의 서버 피해 로직은 변경하지 않았다. Bot AI도 계속 `GameCommand` 생산자이며 피해·lethal truth를 직접 쓰지 않는다.

빌드와 exe 실행은 사용자가 Client/Server를 실행 중이고 직접 빌드한다고 명시했으므로 수행하지 않았다. 대신 `RunZedPassiveDeathMarkProbe`와 `--zed-passive-r-only` 진입점을 추가해 다음 빌드에서 50.1/50.0% 경계, 추가 요청 1회, shield-aware show/hide/pop을 한 번에 검증할 수 있게 했다.

## 3. 최신

현재 튜닝값은 다음과 같다.

- 패시브 발동: 현재 HP / 최대 HP `<= 0.50`
- 패시브 추가 피해: 타격 시점 잃은 체력 `× 0.10`, 물리 피해
- E: 기존 반경 `2.75`, `zed_atkswipe.wmesh` scale `2.75`, 검은 AlphaBlend
- R pop: 기존 잃은 체력 `× 0.30`, 물리 피해, mark `3초`
- R red cross: `zed_crossswipe.wmesh` scale `2.1`
- R lethal marker: `zed_base_r_hit_marker.wmesh` scale `0.045`, yaw spin `4.2 rad/s`, 서버가 보낸 남은 mark 시간만 유지

사용자 빌드 후 확인 순서:

```powershell
Client/Bin/Debug/SimLab.exe --zed-passive-r-only
```

1. 50.1% HP 대상 BA는 `zed_attack1`, 추가 이펙트 없음.
2. 정확히 50.0% HP 대상 BA는 passive animation과 `common_circletimer.wmesh + zed_e_hitslash.png`가 대상에 한 번 보임.
3. E는 제드와 활성 그림자 위치에 검은 mesh 원이 보이되 한 대상 피해는 한 번만 들어감.
4. R cast 즉시 대상 몸에 붉은 cross, 현재 pop lethal이면 머리 위 marker 회전.
5. marker가 보이는 동안 heal/shield로 생존 수치가 되면 marker가 사라지고, 다시 lethal이면 재등장하며 pop 순간에는 무조건 사라짐.

다시 열 조건은 `10% 계수 변경`, WMesh scale/높이 미세 조정, 또는 marker가 shield 전이에 남는 실제 캡처다. 빌드/SimLab/인게임 결과가 아직 없으므로 실행 완료로 과장하지 않으며, 현재 상태는 구현 및 비빌드 정합성 검증 완료다.
