#include "UI/ChampionTuner.h"
#include "Scene/Scene_InGame.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace UI
{
	void UI::CChampionTuner::Render(CScene_InGame* pScene)
	{
		if (!pScene)
			return;

        ImGui::SetNextWindowPos(ImVec2(620.f, 260.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(380.f, 560.f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Champion Tuner")) { ImGui::End(); return; }

        ImGui::TextDisabled("이렐리아 실시간 튜너 (재빌드 불필요)");
        ImGui::Separator();

        // ── 애니 재생속도 (공격/스킬 전용) ──
        ImGui::Text("Attack Speed");
        {
            f32_t v = pScene->GetAttackSpeedMul();
            if (ImGui::SliderFloat("AS Multiplier", &v, 0.2f, 3.0f, "%.2fx"))
                pScene->SetAttackSpeedMul(v);
            ImGui::TextDisabled("공격 애니 중에만 적용 — Idle/run 은 영향 없음");
        }

        ImGui::Spacing(); ImGui::Separator();

        // ── 애니 전체 배속 (디버그용) ──
        ImGui::Text("Global Anim Speed (Debug)");
        {
            f32_t v = pScene->GetGlobalAnimSpeed();
            if (ImGui::SliderFloat("Global Speed", &v, 0.2f, 3.0f, "%.2fx"))
                pScene->SetGlobalAnimSpeed(v);
            ImGui::TextDisabled("Idle/run 포함 전체 애니 계수");
        }

        ImGui::Spacing(); ImGui::Separator();

        // ── 평타 사거리 ──
        ImGui::Text("Basic Attack Range Preview");
        {
            f32_t v = pScene->GetBasicAttackRange();
            if (ImGui::SliderFloat("Range (m)", &v, 1.0f, 12.0f, "%.1f m"))
                pScene->SetBasicAttackRange(v);
            ImGui::TextDisabled("A 키 링 반경 + (차후) 평타 자동 발동 거리");
        }

        ImGui::Spacing(); ImGui::Separator();

        // ── 스킬 파라미터 실시간 튜닝 ──
        ImGui::Text("SSAO");
        {
            bool bEnabled = pScene->GetSSAOEnabled();
            if (ImGui::Checkbox("Enable SSAO", &bEnabled))
                pScene->SetSSAOEnabled(bEnabled);

            f32_t v = pScene->GetSSAORadius();
            if (ImGui::SliderFloat("SSAO Radius", &v, 0.1f, 4.0f, "%.2f"))
                pScene->SetSSAORadius(v);

            v = pScene->GetSSAOIntensity();
            if (ImGui::SliderFloat("SSAO Intensity", &v, 0.1f, 4.0f, "%.2f"))
                pScene->SetSSAOIntensity(v);
        }

        ImGui::Spacing(); ImGui::Separator();

        if (ImGui::CollapsingHeader("Skill Parameters"))
        {
            ImGui::Text("Q (Bladesurge / Dash)");
            {
                f32_t v;
                v = pScene->GetDashDuration();
                if (ImGui::SliderFloat("Dash Duration", &v, 0.05f, 1.0f, "%.2f s"))
                    pScene->SetDashDuration(v);
                v = pScene->GetDashMeleeRange();
                if (ImGui::SliderFloat("Dash Melee Gap", &v, 0.3f, 3.0f, "%.2f m"))
                    pScene->SetDashMeleeRange(v);
            }

            ImGui::Spacing();
            ImGui::Text("E (Blade / Bind)");
            {
                f32_t v;
                v = pScene->GetBladeTravelSpeed();
                if (ImGui::SliderFloat("E Travel Speed", &v, 5.f, 40.f, "%.1f"))
                    pScene->SetBladeTravelSpeed(v);
                v = pScene->GetBladeStunSec();
                if (ImGui::SliderFloat("E Stun Duration", &v, 0.5f, 3.f, "%.2f s"))
                    pScene->SetBladeStunSec(v);

                ImGui::Text("E Sword (FBX Mesh)");
                v = pScene->GetBladeScale();
                if (ImGui::SliderFloat("E Blade Scale", &v, 0.001f, 0.1f, "%.4f"))
                    pScene->SetBladeScale(v);
                v = pScene->GetBladePitch();
                if (ImGui::SliderFloat("E Blade Pitch (X)", &v, -3.14f, 3.14f, "%.3f"))
                    pScene->SetBladePitch(v);
                v = pScene->GetBladeYaw();
                if (ImGui::SliderFloat("E Blade Yaw (Y)", &v, -3.14f, 3.14f, "%.3f"))
                    pScene->SetBladeYaw(v);
                v = pScene->GetBladeRoll();
                if (ImGui::SliderFloat("E Blade Roll (Z)", &v, -3.14f, 3.14f, "%.3f"))
                    pScene->SetBladeRoll(v);
                v = pScene->GetBladeSpinSpeed();
                if (ImGui::SliderFloat("E Blade Spin Speed (rad/s)", &v, -12.57f, 12.57f, "%.2f"))
                    pScene->SetBladeSpinSpeed(v);

                Vec4 c = pScene->GetEBladeColor();
                if (ImGui::ColorEdit4("E Blade RGBA", &c.x))
                    pScene->SetEBladeColor(c);

                ImGui::Text("E Ground / Close Layers");
                c = pScene->GetEGroundGlowColor();
                if (ImGui::ColorEdit4("E Ground Glow RGBA", &c.x))
                    pScene->SetEGroundGlowColor(c);
                c = pScene->GetEGroundCoreColor();
                if (ImGui::ColorEdit4("E Ground Core RGBA", &c.x))
                    pScene->SetEGroundCoreColor(c);
                c = pScene->GetECloseSparkColor();
                if (ImGui::ColorEdit4("E Close Spark RGBA", &c.x))
                    pScene->SetECloseSparkColor(c);
                c = pScene->GetECloseBeamColor();
                if (ImGui::ColorEdit4("E Close Beam RGBA", &c.x))
                    pScene->SetECloseBeamColor(c);
                v = pScene->GetEGroundYOffset();
                if (ImGui::SliderFloat("E Ground Y Offset", &v, -1.f, 1.f, "%.2f"))
                    pScene->SetEGroundYOffset(v);
                v = pScene->GetEGroundGlowSize();
                if (ImGui::SliderFloat("E Ground Glow Size", &v, 0.1f, 8.f, "%.2f"))
                    pScene->SetEGroundGlowSize(v);
                v = pScene->GetEGroundCoreSize();
                if (ImGui::SliderFloat("E Ground Core Size", &v, 0.1f, 8.f, "%.2f"))
                    pScene->SetEGroundCoreSize(v);
                v = pScene->GetEGroundSpinSpeed();
                if (ImGui::SliderFloat("E Ground Spin Speed", &v, -6.28f, 6.28f, "%.2f"))
                    pScene->SetEGroundSpinSpeed(v);
                v = pScene->GetECloseSparkSize();
                if (ImGui::SliderFloat("E Close Spark Size", &v, 0.1f, 8.f, "%.2f"))
                    pScene->SetECloseSparkSize(v);
                v = pScene->GetECloseBeamWidth();
                if (ImGui::SliderFloat("E Close Beam Width", &v, 0.05f, 4.f, "%.2f"))
                    pScene->SetECloseBeamWidth(v);

                ImGui::Text("E Beam (FBX Mesh)");
                v = pScene->GetBeamScaleAxis();
                if (ImGui::SliderFloat("E Beam Length Scale", &v, 0.5f, 3.f, "%.2f"))
                    pScene->SetBeamScaleAxis(v);
                v = pScene->GetBeamGirth();
                if (ImGui::SliderFloat("E Beam Girth Scale", &v, 0.1f, 2.f, "%.2f"))
                    pScene->SetBeamGirth(v);
                v = pScene->GetBeamMeshBaseScale();
                if (ImGui::SliderFloat("E Beam Base Scale", &v, 0.001f, 0.1f, "%.4f"))
                    pScene->SetBeamMeshBaseScale(v);
                v = pScene->GetBeamYawOffset();
                if (ImGui::SliderFloat("E Beam Yaw Offset", &v, -3.14f, 3.14f, "%.3f"))
                    pScene->SetBeamYawOffset(v);
            }

            ImGui::Spacing();
            ImGui::Text("R (Wave)");
            {
                f32_t v;
                v = pScene->GetWaveLength();
                if (ImGui::SliderFloat("R Length",  &v, 6.f, 20.f, "%.1f"))
                    pScene->SetWaveLength(v);
                v = pScene->GetWaveWidth();
                if (ImGui::SliderFloat("R Width",   &v, 1.f,  8.f, "%.1f"))
                    pScene->SetWaveWidth(v);
                v = pScene->GetWaveSpeed();
                if (ImGui::SliderFloat("R Speed",   &v,10.f, 50.f, "%.1f"))
                    pScene->SetWaveSpeed(v);
                v = pScene->GetWaveMaxDist();
                if (ImGui::SliderFloat("R MaxDist", &v, 6.f, 30.f, "%.1f"))
                    pScene->SetWaveMaxDist(v);
                v = pScene->GetWaveDamage();
                if (ImGui::SliderFloat("R Damage",  &v,50.f,500.f, "%.0f"))
                    pScene->SetWaveDamage(v);

                ImGui::Text("R Pulse FX (Visual)");
                v = pScene->GetRFxWidth();
                if (ImGui::SliderFloat("R FX Width", &v, 0.5f, 16.f, "%.1f"))
                    pScene->SetRFxWidth(v);
                v = pScene->GetRFxHeight();
                if (ImGui::SliderFloat("R FX Height", &v, 0.5f, 16.f, "%.1f"))
                    pScene->SetRFxHeight(v);
                v = pScene->GetRFxYOffset();
                if (ImGui::SliderFloat("R FX Y Off", &v, 0.f, 5.f, "%.2f"))
                    pScene->SetRFxYOffset(v);
                v = pScene->GetRFxFwdOffset();
                if (ImGui::SliderFloat("R FX Fwd Off", &v, 0.f, 5.f, "%.2f"))
                    pScene->SetRFxFwdOffset(v);
                v = pScene->GetRFxYawOffset();
                if (ImGui::SliderFloat("R FX Yaw Offset", &v, -3.14f, 3.14f, "%.3f"))
                    pScene->SetRFxYawOffset(v);

                // ── ★ R Blade Fan Triangle (▲ — forward 끝이 뾰족) ──
                ImGui::Spacing();
                ImGui::Text("R Blade Fan Triangle");
                {
                    bool bTri = pScene->GetRTriangleMode();
                    if (ImGui::Checkbox("R Triangle Mode (pointed tip)", &bTri))
                        pScene->SetRTriangleMode(bTri);
                    v = pScene->GetRTipBoost();
                    if (ImGui::SliderFloat("R Tip Boost (center fwd m)", &v, 0.f, 5.f, "%.2f"))
                        pScene->SetRTipBoost(v);
                    v = pScene->GetRSideShrink();
                    if (ImGui::SliderFloat("R Side Shrink (0~0.9)", &v, 0.f, 0.9f, "%.2f"))
                        pScene->SetRSideShrink(v);
                }
            }

            // ── ★ W2 Release Layers (swipe_blades + mis_glow) ──
            //     blade_erode 는 SpawnWStage2Slash 가 처리 — 별도 슬라이더 없음
            ImGui::Spacing();
            ImGui::Text("W Release Layers (W2 시점 swipe + glow)");
            {
                f32_t v = pScene->GetWLayerLifetime();
                if (ImGui::SliderFloat("W Layer Lifetime (s)", &v, 0.05f, 2.0f, "%.2f"))
                    pScene->SetWLayerLifetime(v);
                v = pScene->GetWLayerSize();
                if (ImGui::SliderFloat("W Layer Size", &v, 0.5f, 6.f, "%.2f"))
                    pScene->SetWLayerSize(v);

                Vec4 c = pScene->GetWLayerBladesColor();
                if (ImGui::ColorEdit4("W Blades RGBA (AlphaBlend)", &c.x))
                    pScene->SetWLayerBladesColor(c);
                c = pScene->GetWLayerGlowColor();
                if (ImGui::ColorEdit4("W Glow RGBA (Additive)", &c.x))
                    pScene->SetWLayerGlowColor(c);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Yasuo (Phase 2)");
        {
            f32_t v;
            v = pScene->GetYasuoQSpeed();
            if (ImGui::SliderFloat("Y Q Speed", &v, 5.f, 50.f, "%.1f"))
                pScene->SetYasuoQSpeed(v);
            v = pScene->GetYasuoQLifetime();
            if (ImGui::SliderFloat("Y Q Lifetime", &v, 0.1f, 2.f, "%.2f"))
                pScene->SetYasuoQLifetime(v);
            v = pScene->GetYasuoQTornadoSpeed();
            if (ImGui::SliderFloat("Y Q3 Tornado Speed", &v, 1.f, 30.f, "%.1f"))
                pScene->SetYasuoQTornadoSpeed(v);
            v = pScene->GetYasuoQTornadoLifetime();
            if (ImGui::SliderFloat("Y Q3 Tornado Life", &v, 0.1f, 3.f, "%.2f"))
                pScene->SetYasuoQTornadoLifetime(v);
            v = pScene->GetYasuoQTornadoScale();
            if (ImGui::SliderFloat("Y Q3 Tornado Scale", &v, 0.001f, 0.1f, "%.4f"))
                pScene->SetYasuoQTornadoScale(v);

            ImGui::Spacing();
            v = pScene->GetYasuoWLifetime();
            if (ImGui::SliderFloat("Y W Wall Lifetime", &v, 0.5f, 10.f, "%.1f"))
                pScene->SetYasuoWLifetime(v);
            v = pScene->GetYasuoWWidth();
            if (ImGui::SliderFloat("Y W Wall Width", &v, 1.f, 12.f, "%.1f"))
                pScene->SetYasuoWWidth(v);
            v = pScene->GetYasuoWHeight();
            if (ImGui::SliderFloat("Y W Wall Thickness", &v, 0.1f, 2.f, "%.2f"))
                pScene->SetYasuoWHeight(v);

            ImGui::Spacing();
            v = pScene->GetYasuoEDashDuration();
            if (ImGui::SliderFloat("Y E Dash Duration", &v, 0.05f, 0.5f, "%.2f"))
                pScene->SetYasuoEDashDuration(v);

            ImGui::Spacing();
            v = pScene->GetYasuoRSearchRadius();
            if (ImGui::SliderFloat("Y R Airborne Radius", &v, 1.f, 15.f, "%.1f"))
                pScene->SetYasuoRSearchRadius(v);
            v = pScene->GetYasuoRSequenceDuration();
            if (ImGui::SliderFloat("Y R Sequence Duration", &v, 0.1f, 3.f, "%.2f"))
                pScene->SetYasuoRSequenceDuration(v);

            ImGui::Spacing();
            ImGui::Text("Yasuo Damage (Phase 2-2)");
            v = pScene->GetYasuoQDamage();
            if (ImGui::SliderFloat("Y Q Damage", &v, 0.f, 300.f, "%.0f"))
                pScene->SetYasuoQDamage(v);
            v = pScene->GetYasuoQTornadoDamage();
            if (ImGui::SliderFloat("Y Q3 Tornado Damage", &v, 0.f, 500.f, "%.0f"))
                pScene->SetYasuoQTornadoDamage(v);
            v = pScene->GetYasuoQTornadoStunSec();
            if (ImGui::SliderFloat("Y Q3 Stun Sec", &v, 0.f, 3.f, "%.2f"))
                pScene->SetYasuoQTornadoStunSec(v);
            v = pScene->GetYasuoEDamage();
            if (ImGui::SliderFloat("Y E Damage", &v, 0.f, 300.f, "%.0f"))
                pScene->SetYasuoEDamage(v);
            v = pScene->GetYasuoRPerHitDamage();
            if (ImGui::SliderFloat("Y R Per-Hit Damage", &v, 0.f, 200.f, "%.0f"))
                pScene->SetYasuoRPerHitDamage(v);
            v = pScene->GetYasuoRHitInterval();
            if (ImGui::SliderFloat("Y R Hit Interval (s)", &v, 0.05f, 1.f, "%.2f"))
                pScene->SetYasuoRHitInterval(v);

            ImGui::Spacing();
            ImGui::Text("Yasuo Visual Fix (Phase 2-3a)");
            Vec4 c = pScene->GetYasuoQTornadoColor();
            if (ImGui::ColorEdit4("Q3 Tornado RGBA", &c.x))
                pScene->SetYasuoQTornadoColor(c);
            v = pScene->GetYasuoWMeshScale();
            if (ImGui::SliderFloat("W Wall MeshScale (cm->m)", &v, 0.001f, 0.1f, "%.4f"))
                pScene->SetYasuoWMeshScale(v);
            ImGui::TextDisabled("Default 0.01 = cm->m. 1.0 = original mesh size");

            ImGui::Spacing();
            ImGui::Text("Yasuo Combo (Phase 2-3)");
            v = pScene->GetFlashRange();
            if (ImGui::SliderFloat("Flash Range (m)", &v, 1.f, 10.f, "%.2f"))
                pScene->SetFlashRange(v);
            v = pScene->GetFlashCooldown();
            if (ImGui::SliderFloat("Flash Cooldown (s)", &v, 1.f, 300.f, "%.0f"))
                pScene->SetFlashCooldown(v);
            ImGui::Text("Flash CD Left: %.1f s", pScene->GetFlashCooldownLeft());
            v = pScene->GetYasuoQHitDelay();
            if (ImGui::SliderFloat("Q Hit Delay (s)", &v, 0.f, 1.f, "%.2f"))
                pScene->SetYasuoQHitDelay(v);
            v = pScene->GetYasuoEQDelay();
            if (ImGui::SliderFloat("EQ Hit Delay (s)", &v, 0.f, 1.f, "%.2f"))
                pScene->SetYasuoEQDelay(v);
            v = pScene->GetYasuoEQRadius();
            if (ImGui::SliderFloat("EQ Ring Radius (m)", &v, 0.5f, 6.f, "%.2f"))
                pScene->SetYasuoEQRadius(v);
            v = pScene->GetYasuoEQDamage();
            if (ImGui::SliderFloat("EQ Damage", &v, 0.f, 300.f, "%.0f"))
                pScene->SetYasuoEQDamage(v);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Kalista (Phase 3)");
        {
            f32_t v;
            v = pScene->GetKalistaQSpeed();
            if (ImGui::SliderFloat("K Q Speed", &v, 5.f, 60.f, "%.1f"))
                pScene->SetKalistaQSpeed(v);
            v = pScene->GetKalistaQMaxDist();
            if (ImGui::SliderFloat("K Q MaxDist", &v, 3.f, 25.f, "%.1f"))
                pScene->SetKalistaQMaxDist(v);
            v = pScene->GetKalistaQRadius();
            if (ImGui::SliderFloat("K Q Radius", &v, 0.1f, 2.f, "%.2f"))
                pScene->SetKalistaQRadius(v);
            v = pScene->GetKalistaQDamage();
            if (ImGui::SliderFloat("K Q Damage", &v, 0.f, 300.f, "%.0f"))
                pScene->SetKalistaQDamage(v);
            ImGui::Spacing();
            ImGui::Text("Kalista Spear Scales");
            v = pScene->GetKalistaBAFlySpearScale();
            if (ImGui::SliderFloat("BA Fly Scale", &v, 0.001f, 0.03f, "%.4f"))
                pScene->SetKalistaBAFlySpearScale(v);
            v = pScene->GetKalistaBAStuckSpearScale();
            if (ImGui::SliderFloat("BA Stuck Scale", &v, 0.001f, 0.03f, "%.4f"))
                pScene->SetKalistaBAStuckSpearScale(v);
            v = pScene->GetKalistaQFlySpearScale();
            if (ImGui::SliderFloat("Q Fly Scale", &v, 0.001f, 0.05f, "%.4f"))
                pScene->SetKalistaQFlySpearScale(v);
            v = pScene->GetKalistaQStuckSpearScale();
            if (ImGui::SliderFloat("Q Stuck Scale", &v, 0.001f, 0.03f, "%.4f"))
                pScene->SetKalistaQStuckSpearScale(v);

            ImGui::Spacing();
            ImGui::Text("Kalista Passive Dash");
            v = pScene->GetKalistaPassiveDashDist();
            if (ImGui::SliderFloat("K Passive Dash Dist", &v, 0.2f, 5.f, "%.2f"))
                pScene->SetKalistaPassiveDashDist(v);
            v = pScene->GetKalistaPassiveDashDuration();
            if (ImGui::SliderFloat("K Passive Dash Duration", &v, 0.03f, 1.f, "%.2f"))
                pScene->SetKalistaPassiveDashDuration(v);
            v = pScene->GetKalistaPassiveDashAnimSpeed();
            if (ImGui::SliderFloat("K Passive Dash Anim Speed", &v, 0.2f, 3.f, "%.2f"))
                pScene->SetKalistaPassiveDashAnimSpeed(v);
            v = pScene->GetKalistaPassiveDashInputGrace();
            if (ImGui::SliderFloat("K Passive Dash Input Grace", &v, 0.f, 0.5f, "%.2f"))
                pScene->SetKalistaPassiveDashInputGrace(v);

            ImGui::Spacing();
            v = pScene->GetKalistaERendBaseDmg();
            if (ImGui::SliderFloat("K E Base Dmg", &v, 0.f, 200.f, "%.0f"))
                pScene->SetKalistaERendBaseDmg(v);
            v = pScene->GetKalistaRendStackDmg();
            if (ImGui::SliderFloat("K E Stack Dmg", &v, 0.f, 100.f, "%.0f"))
                pScene->SetKalistaRendStackDmg(v);
            v = pScene->GetKalistaERendWispSize();
            if (ImGui::SliderFloat("K E Wisp Size", &v, 0.2f, 6.f, "%.2f"))
                pScene->SetKalistaERendWispSize(v);
            v = pScene->GetKalistaERendWispLifetime();
            if (ImGui::SliderFloat("K E Wisp Life", &v, 0.05f, 1.5f, "%.2f"))
                pScene->SetKalistaERendWispLifetime(v);
            v = pScene->GetKalistaERendWispFps();
            if (ImGui::SliderFloat("K E Wisp FPS", &v, 1.f, 60.f, "%.0f"))
                pScene->SetKalistaERendWispFps(v);
        }

        ImGui::End();
	}
}
