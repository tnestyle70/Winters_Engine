#include "GameObject/SkillDef.h"

// SkillDef field order (see SkillDef.h):
//   champ, slot, targetMode, cooldownSec, rangeMax, manaCost,
//   animKey, vfxKey, sfxKey,
//   lockDurationSec, bOneShot, rotate,
//   stageCount, stage2TargetMode, stage2AnimKey, stage2LockSec, stage2Rotate, stageWindowSec,
//   castFrame, recoveryFrame, stage2CastFrame, stage2RecoveryFrame,
//   animPlaySpeed, stage2PlaySpeed,
//   endTransitionIdleAnim, endTransitionRunAnim, endTransitionDuration
//
// Keep lockDurationSec long enough for frame hooks to fire before action unlock
// can transition the player back to idle/run:
//   lockDurationSec * animPlaySpeed >= recoveryFrame / FBX_FPS.
// Example: Kalista BA 1.5 * 0.6 = 0.9s >= 14 / 24 = 0.58s.


// ─────────────────────────────────────────────────────────────
//  g_SkillTable — 데이터 드리븐 스킬 정의
//
//  cooldown / range / mana 는 Phase 4 서버 시뮬레이션 때 사용.
//  지금은 eTargetMode 와 animKey 가 실제로 쓰임.
//
//  야스오 Q 는 Conditional: 기본은 Direction 로 해석, E 시전 중이면 AOE.
//  3타 회오리는 slot 과 독립적으로 Execute 시점에서 스택 카운팅
//  (YasuoStateComponent::qStackCount) → Phase 4 에서 처리.
// ─────────────────────────────────────────────────────────────

static SkillDef s_SkillTable[] =
{
    // ── Irelia ─────────────────────────────────────────────
    // 필드 순서: champ, slot, targetMode, cooldownSec, rangeMax, manaCost,
    //           animKey, vfxKey, sfxKey,
    //           lockDurationSec, bOneShot, rotate,
    //           stageCount, stage2TargetMode, stage2AnimKey, stage2LockSec, stage2Rotate, stageWindowSec,
    //           castFrame, recoveryFrame, stage2CastFrame, stage2RecoveryFrame
    { eChampion::IRELIA, 0, eTargetMode::UnitTarget,
      0.6f, 1.5f, 0.f,
      "attack_01", nullptr, nullptr,
      //이렐리아 기본 공격 애니메이션 지속 시간 1초!!!!!!
      1.f, true, eRotateMode::TowardsTarget,
      1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
      6.f, 14.f, 0.f, 0.f,
      1.f, 1.f,
      nullptr, nullptr, 0.f },

    { eChampion::IRELIA, 1, eTargetMode::UnitTarget,
      0.6f, 6.0f, 25.f,
      "spell1", nullptr, nullptr,
      //Q 0.5초 지속!!!!!!! 
      0.5f, true, eRotateMode::TowardsTarget,
      1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
      8.f, 18.f, 0.f, 0.f,
      //animPlaySpeed, stage2PlaySpeed
      1.f, 1.f,
      //Q 이후 자연 젼환 - 돌진 후 정지 / 이동
      "spell1_to_idle", "spell1_into_runbase", 0.05f},

      // W: 2-stage.  1타 = Direction(방어막 모션 + 마우스 방향 회전).  2타 = Direction(원뿔 공격).
      { eChampion::IRELIA, 2, eTargetMode::Direction,
        0.6f, 0.f, 40.f,
        "spell2", nullptr, nullptr,
        //W 4초 지속!! 
        5.f, true, eRotateMode::TowardsCursor,
        2, eTargetMode::Direction, "spell2_2", 0.4f, eRotateMode::TowardsCursor, 4.f,
      0.f, 7.f, 6.f, 14.f,
      //디버깅용으로 0.1f로 테스트
      1.f, 1.f,
      //W 이후 이동 중이면 방어 자세에서 run으로 자연 전환
      nullptr, "spell2_to_run", 0.10f},

      { eChampion::IRELIA, 3, eTargetMode::GroundTarget,
        0.6f, 9.0f, 80.f,
        "spell3", nullptr, nullptr,
        //E 1초 지속!!
        1.f, true, eRotateMode::TowardsCursor,
        //[Phase T-8] E 2-stage: stage1=Placed (GroundTarget), stage2=Placed (GroundTarget), 2s window
        2, eTargetMode::GroundTarget, "spell3", 0.4f, eRotateMode::TowardsCursor, 2.f,
        10.f, 20.f, 6.f, 14.f,
        //animPlaySpeed, stage2PlaySpeed
        1.f, 1.f,
        //E 이후 전환
        "spell3_to_idle", "spell3_run", 0.05f},

      { eChampion::IRELIA, 4, eTargetMode::Direction,
        0.6f, 12.f, 100.f,
        "spell4", nullptr, nullptr,
        //궁극기 지속 시간 설정
        0.65f, true, eRotateMode::TowardsCursor,
        1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
        7.f, 30.f, 0.f, 0.f,
        //궁극기 애니메이션 재생 배속 속도 0.6배로 줄이기
        0.25f, 1.f,
        //궁 이후 전환 - 정지 시 공중 -> 지면, 이동 시 공중 -> 달리기
        "spell4_to_idle", "spell4_to_run",
        //전환 애니메이션 시간
        0.15f},

        // ── Yasuo ──────────────────────────────────────────────
        { eChampion::YASUO, 0, eTargetMode::UnitTarget,
          0.6f, 2.5f, 0.f,
          "attack1", nullptr, nullptr,
          0.35f, true, eRotateMode::TowardsTarget,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          0.f, 0.f, 0.f, 0.f,
          1.f, 1.f,
          nullptr, nullptr, 0.f },

        { eChampion::YASUO, 1, eTargetMode::Conditional,
          0.6f, 5.f, 0.f,
          "spell1", nullptr, nullptr,
          0.3f, true, eRotateMode::TowardsCursor,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          0.f, 0.f, 0.f, 0.f,
          1.f, 1.f,
          nullptr, nullptr, 0.f },

        { eChampion::YASUO, 2, eTargetMode::Direction,
          0.6f, 4.f, 0.f,
          "spell2", nullptr, nullptr,
          0.25f, true, eRotateMode::TowardsCursor,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          0.f, 0.f, 0.f, 0.f,
          1.f, 1.f,
          nullptr, nullptr, 0.f },

        { eChampion::YASUO, 3, eTargetMode::UnitTarget,
          0.6f, 4.75f, 0.f,
          "spell3", nullptr, nullptr,
          0.4f, true, eRotateMode::TowardsTarget,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          0.f, 0.f, 0.f, 0.f,
          1.f, 1.f,
          nullptr, nullptr, 0.f },
        { eChampion::YASUO, 4, eTargetMode::Self,
          0.6f, 14.f, 0.f,
          "spell4", nullptr, nullptr,
          0.6f, true, eRotateMode::None,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          0.f, 0.f, 0.f, 0.f,
          1.f, 1.f,
          nullptr, nullptr, 0.f },

          // ── Kalista ────────────────────────────────────────────
          { eChampion::KALISTA, 0, eTargetMode::UnitTarget,
            0.5f, 5.5f, 0.f,
            "attack1", nullptr, nullptr,
            // Network/registry path uses the same 0.60s action lock.
            0.6f, true, eRotateMode::TowardsTarget,
            1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
            //castFrame, recoveryFrame, stage2CastFrame, stage2RecoveryFrame
            6.f, 14.f, 0.f, 0.f,
            //전체 애니메이션 재생 속도 제어
            1.0f, 1.f,
            nullptr, nullptr, 0.f
           },

           { eChampion::KALISTA, 1, eTargetMode::Direction,
             0.2f, 11.f, 50.f,
             "spell1", nullptr, nullptr,
             0.3f, true, eRotateMode::TowardsCursor,
             1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
             4.f, 10.f, 0.f, 0.f,
             2.8f, 1.f,
             nullptr, nullptr, 0.f },

           { eChampion::KALISTA, 2, eTargetMode::Self,
             18.f, 0.f, 50.f,
             "spell2", nullptr, nullptr,
             0.5f, true, eRotateMode::None,
             1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
             0.f, 0.f, 0.f, 0.f,
             1.f, 1.f,
             nullptr, nullptr, 0.f },

           { eChampion::KALISTA, 3, eTargetMode::Self,
             3.f, 12.f, 30.f,
             "spell3", nullptr, nullptr,
             0.4f, true, eRotateMode::None,
             1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
             0.f, 0.f, 0.f, 0.f,
             1.f, 1.f,
             nullptr, nullptr, 0.f },

           { eChampion::KALISTA, 4, eTargetMode::Self,
             120.f, 0.f, 100.f,
             "spell4_call", nullptr, nullptr,
             0.5f, true, eRotateMode::None,
             1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
             0.f, 0.f, 0.f, 0.f,
             1.f, 1.f,
             nullptr, nullptr, 0.f },

    //Garen
{eChampion::GAREN, 0, eTargetMode::UnitTarget,
  0.6f, 1.5f, 0.f,
  "garen_2013_attack_01", nullptr, nullptr,
  1.f, true, eRotateMode::TowardsTarget,
  1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
    //castFrame, recoveryFrame, stage2CastFrame, stage2RecoveryFrame
    6.f, 14.f, 0.f, 0.f,
    //animPlaySpeed, stage2PlaySpeed
    1.f, 1.f,
    nullptr, nullptr, 0.f },

    //우선 쿨타임들은 전부 다 고정
    // Q — Decisive Strike (다음 평타 강화 + 이동 부스트)
    { eChampion::GAREN, 1, eTargetMode::Self,
      2.f, 0.f, 0.f,
      "garen_2013_spell1", nullptr, nullptr,
      0.6f, true, eRotateMode::None,
      1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
      4.f, 10.f, 0.f, 0.f,
      1.0f, 1.f,
      nullptr, nullptr, 0.f },

    // W — Courage (방어막 + 데미지 감소)
    //v2 정정: castFrame=1.f (0 이면 cast hook miss — Scene_InGame.cpp:L720)
  { eChampion::GAREN, 2, eTargetMode::Self,
    2.f, 0.f, 0.f,
    "garen_2013_channel", nullptr, nullptr,
    0.5f, true, eRotateMode::None,
    1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
    1.f, 8.f, 0.f, 0.f,   // castFrame=1.f (★ v2)
    1.0f, 1.f,
    nullptr, nullptr, 0.f },

    // E — Judgment (회전 칼날, 영역 데미지)
    // 회전 모션이라 castFrame 다중 발생 가능 — 1차는 단일 hit
  { eChampion::GAREN, 3, eTargetMode::Self,
    2.f, 1.65f, 0.f,
    "garen_base_spell3_0", nullptr, nullptr,
    3.0f, true, eRotateMode::None,
    1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
    12.f, 60.f, 0.f, 0.f,
    1.0f, 1.f,
    nullptr, nullptr, 0.f },

    // R — Demacian Justice (처형)
  { eChampion::GAREN, 4, eTargetMode::UnitTarget,
    2.f, 4.f, 100.f,
    "garen_2013_spell4", nullptr, nullptr,
    1.5f, true, eRotateMode::TowardsTarget,
    1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
    24.f, 36.f, 0.f, 0.f,
    1.0f, 1.f,
    nullptr, nullptr, 0.f },

    //Zed
  // ── Zed ──────────────────────────────────────────────
  // BA
  { eChampion::ZED, 0, eTargetMode::UnitTarget,
    0.5f, 1.5f, 0.f,
    "zed_attack1", nullptr, nullptr,
    1.0f, true, eRotateMode::TowardsTarget,
    1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
    6.f, 14.f, 0.f, 0.f,
    1.0f, 1.f,
    nullptr, nullptr, 0.f },

    // Q — Razor Shuriken
  { eChampion::ZED, 1, eTargetMode::Direction,
    2.f, 9.f, 0.f,    // 에너지 무시 (mana=0)
    "zed_spell1", nullptr, nullptr,
    0.7f, true, eRotateMode::TowardsCursor,
    1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
    4.f, 10.f, 0.f, 0.f,
    1.0f, 1.f,
    nullptr, nullptr, 0.f },

    // W — Living Shadow (그림자 소환)
  { eChampion::ZED, 2, eTargetMode::GroundTarget,
    2.f, 6.5f, 0.f,
    "zed_spell2", nullptr, nullptr,
    0.5f, true, eRotateMode::TowardsCursor,
    2, eTargetMode::Self, "zed_spell2", 0.25f, eRotateMode::None, 5.f,
    1.f, 8.f, 1.f, 5.f,   // ★ castFrame=1.f
    1.0f, 1.f,
    nullptr, nullptr, 0.f },

    // E — Shadow Slash (AOE)
  { eChampion::ZED, 3, eTargetMode::Self,
    2.f, 2.5f, 0.f,
    "zed_spell3", nullptr, nullptr,
    0.6f, true, eRotateMode::None,
    1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
    6.f, 14.f, 0.f, 0.f,
    1.0f, 1.f,
    nullptr, nullptr, 0.f },

    // R — Death Mark
  { eChampion::ZED, 4, eTargetMode::UnitTarget,
    2.f, 6.25f, 0.f,
    "zed_spell4", nullptr, nullptr,
    1.5f, true, eRotateMode::TowardsTarget,
    1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
    18.f, 30.f, 0.f, 0.f,
    1.0f, 1.f,
    nullptr, nullptr, 0.f },

  { eChampion::RIVEN, 0, eTargetMode::UnitTarget,
    0.6f, 1.5f, 0.f,
    "attack1", nullptr, nullptr,
    1.0f, true, eRotateMode::TowardsTarget,
    1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
    6.f, 14.f, 0.f, 0.f,
    1.0f, 1.f,
    nullptr, nullptr, 0.f },

  { eChampion::RIVEN, 1, eTargetMode::Conditional,
    1.5f, 4.5f, 0.f,
    "spell1", nullptr, nullptr,
    0.45f, true, eRotateMode::TowardsCursor,
    1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
    4.f, 10.f, 0.f, 0.f,
    1.0f, 1.f,
    nullptr, nullptr, 0.f },

  { eChampion::RIVEN, 2, eTargetMode::Self,
    2.f, 0.f, 0.f,
    "spell2", nullptr, nullptr,
    0.6f, true, eRotateMode::None,
    1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
    4.f, 10.f, 0.f, 0.f,
    1.0f, 1.f,
    nullptr, nullptr, 0.f },

  { eChampion::RIVEN, 3, eTargetMode::Self,
    2.f, 0.f, 0.f,
    "spell3", nullptr, nullptr,
    0.5f, true, eRotateMode::TowardsCursor,
    1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
    3.f, 8.f, 0.f, 0.f,
    1.0f, 1.f,
    nullptr, nullptr, 0.f },

  { 
      eChampion::RIVEN, 4, eTargetMode::Self,
     2.f, 0.f, 0.f,
     "spell4a", nullptr, nullptr,
      0.8f, true, eRotateMode::None,
      1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
      6.f, 14.f, 0.f, 0.f,
       1.0f, 1.f,
       nullptr, nullptr, 0.f
  },

};

const SkillDef* const g_SkillTable = s_SkillTable;
const uint32_t        g_SkillCount = sizeof(s_SkillTable) / sizeof(s_SkillTable[0]);

namespace
{
    // Keep false for normal network-authoritative gameplay. This debug path
    // intentionally overrides authored cooldown/action-lock values and makes
    // client transition timing diverge from the server runtime table.
    constexpr bool_t kUseClientFastSkillVerificationTiming = false;
    constexpr f32_t kVerificationSkillCooldownSec = 0.20f;
    constexpr f32_t kVerificationActionLockSec = 0.20f;

    const SkillDef* ApplyVerificationTiming(const SkillDef& source)
    {
        if constexpr (!kUseClientFastSkillVerificationTiming)
            return &source;

        thread_local SkillDef s_override{};
        s_override = source;
        s_override.cooldownSec = kVerificationSkillCooldownSec;
        s_override.lockDurationSec = kVerificationActionLockSec;
        if (s_override.stage2LockSec > 0.f)
            s_override.stage2LockSec = kVerificationActionLockSec;
        return &s_override;
    }
}

const SkillDef* FindSkillDef(eChampion champ, uint8_t slot)
{
    for (uint32_t i = 0; i < g_SkillCount; ++i)
    {
        if (s_SkillTable[i].champ == champ && s_SkillTable[i].slot == slot)
            return ApplyVerificationTiming(s_SkillTable[i]);
    }
    return nullptr;
}
