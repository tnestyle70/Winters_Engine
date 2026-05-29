#include "GameObject/FX/FxLegacyManifest.h"

namespace LegacyFx
{
    const std::vector<FxLegacyManifestEntry>& GetSeedManifestEntries()
    {
        static const std::vector<FxLegacyManifestEntry> s_Entries = {
            {
                "Irelia.Q.Trail", "Irelia", "IreliaFx::SpawnQTrail",
                "Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp",
                "Billboard",
                L"Client/Bin/Resource/FX/LoL/Irelia/Irelia_Q_Trail.wfx",
                L"Client/Bin/Resource/FX/LoL/Irelia/MI_Irelia_Q_Trail.wmi",
                "LegacyOnly"
            },
            {
                "Irelia.Q.Mark", "Irelia", "IreliaFx::SpawnQMark",
                "Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp",
                "Billboard",
                L"Client/Bin/Resource/FX/LoL/Irelia/Irelia_Q_Mark.wfx",
                L"Client/Bin/Resource/FX/LoL/Irelia/MI_Irelia_Q_Mark.wmi",
                "LegacyOnly"
            },
            {
                "Irelia.W.Spin", "Irelia", "CFxCuePlayer::PlayAll",
                "Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp",
                "Billboard,ShockwaveRing",
                L"Data/LoL/FX/Champions/Irelia/w_hold.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Irelia.W.Stage2Slash", "Irelia", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp",
                "Beam,Billboard",
                L"Data/LoL/FX/Champions/Irelia/w_release.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Irelia.E.Place", "Irelia", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp",
                "GroundDecal,Billboard",
                L"Data/LoL/FX/Champions/Irelia/e_place.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Irelia.E.Connect", "Irelia", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp",
                "MeshParticle,Beam",
                L"Data/LoL/FX/Champions/Irelia/e_connect.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Annie.Q.Fireball", "Annie", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Annie/Annie_Skills.cpp",
                "MeshParticle,Billboard",
                L"Data/LoL/FX/Champions/Annie/q_fireball.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Annie.BA.Hit", "Annie", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Annie/Annie_Skills.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Annie/ba_hit.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Annie.BA.Projectile", "Annie", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Annie/Annie_Skills.cpp",
                "MeshParticle,Billboard",
                L"Data/LoL/FX/Champions/Annie/ba_projectile.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Annie.W.Cone", "Annie", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Annie/Annie_Skills.cpp",
                "GroundDecal,Billboard",
                L"Data/LoL/FX/Champions/Annie/w_cone.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Annie.E.Shield", "Annie", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Annie/Annie_Skills.cpp",
                "GroundDecal,Billboard",
                L"Data/LoL/FX/Champions/Annie/e_shield.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Annie.R.Summon", "Annie", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Annie/Annie_Skills.cpp",
                "MeshParticle,GroundDecal,Billboard",
                L"Data/LoL/FX/Champions/Annie/r_summon.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Annie.Stun.Ready", "Annie", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Annie/Annie_FxPresets.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Annie/stun_ready.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Irelia.Target.Mark", "Irelia", "CFxCuePlayer::PlayAll",
                "Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Irelia/target_mark.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Irelia.R.Pulse", "Irelia", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Irelia/r_pulse.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Irelia.R.Hit", "Irelia", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Irelia/r_hit.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Irelia.R.Wall", "Irelia", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Irelia/r_wall.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Irelia.R.BladeFan", "Irelia", "IreliaFx::SpawnRBladeFan",
                "Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp",
                "MeshParticle",
                L"",
                L"",
                "CodeDriven"
            },
            {
                "Yasuo.Q.Slash", "Yasuo", "YasuoFx::SpawnQStraight",
                "Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp",
                "Billboard,GroundDecal",
                L"Data/LoL/FX/Champions/Yasuo/q_slash.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Yasuo.Q.BuildUp", "Yasuo", "YasuoFx::SpawnQBuildUp",
                "Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Yasuo/q_build_up.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Yasuo.Q.Tornado", "Yasuo", "YasuoFx::SpawnQTornado",
                "Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp",
                "Billboard,MeshParticle",
                L"Data/LoL/FX/Champions/Yasuo/q_tornado.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Yasuo.W.WindWall", "Yasuo", "YasuoFx::SpawnWWindWall",
                "Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp",
                "MeshParticle,Billboard",
                L"Data/LoL/FX/Champions/Yasuo/w_windwall.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Yasuo.E.DashTrail", "Yasuo", "YasuoFx::SpawnEDashTrail",
                "Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Yasuo/e_dash_trail.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Yasuo.EQ.Ring", "Yasuo", "YasuoFx::SpawnEQRing",
                "Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp",
                "GroundDecal",
                L"Data/LoL/FX/Champions/Yasuo/eq_ring.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Yasuo.EQ.InnerWind", "Yasuo", "YasuoFx::SpawnEQRing",
                "Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Yasuo/eq_inner_wind.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Yasuo.R.SwordGlow", "Yasuo", "YasuoFx::SpawnRLastBreath",
                "Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp",
                "MeshParticle,Billboard",
                L"Data/LoL/FX/Champions/Yasuo/r_sword_glow.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Yasuo.R.LandImpact", "Yasuo", "YasuoFx::SpawnRLastBreath",
                "Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp",
                "GroundDecal,ShockwaveRing,Billboard",
                L"Data/LoL/FX/Champions/Yasuo/r_land_impact.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Sylas.Q.Cast", "Sylas", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Sylas/SylasSkills.cpp",
                "Billboard,GroundDecal,MeshParticle",
                L"Data/LoL/FX/Champions/Sylas/q_cast.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Sylas.Q.Explosion", "Sylas", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Sylas/SylasSkills.cpp",
                "GroundDecal,ShockwaveRing,Billboard,MeshParticle",
                L"Data/LoL/FX/Champions/Sylas/q_explosion.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Sylas.W.Cast", "Sylas", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Sylas/SylasSkills.cpp",
                "MeshParticle,Billboard,ShockwaveRing",
                L"Data/LoL/FX/Champions/Sylas/w_cast.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Sylas.E1.Dash", "Sylas", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Sylas/SylasSkills.cpp",
                "MeshParticle,Billboard,GroundDecal",
                L"Data/LoL/FX/Champions/Sylas/e1_dash.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Sylas.E2.Chain", "Sylas", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Sylas/SylasSkills.cpp",
                "Beam,Ribbon,Billboard,MeshParticle",
                L"Data/LoL/FX/Champions/Sylas/e2_chain.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Sylas.R.Cast", "Sylas", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Sylas/SylasSkills.cpp",
                "Beam,Ribbon,Billboard,MeshParticle",
                L"Data/LoL/FX/Champions/Sylas/r_cast.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ezreal.BA.Cast", "Ezreal", "Ezreal::Fx::SpawnBAProjectile",
                "Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Ezreal/ba_cast.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ezreal.BA.Projectile", "Ezreal", "Ezreal::Fx::SpawnBAProjectile",
                "Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Ezreal/ba_projectile.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ezreal.Q.Cast", "Ezreal", "Ezreal::Fx::SpawnQProjectile",
                "Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp",
                "MeshParticle,Billboard",
                L"Data/LoL/FX/Champions/Ezreal/q_cast.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ezreal.Q.Projectile", "Ezreal", "Ezreal::Fx::SpawnQProjectile",
                "Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp",
                "MeshParticle,Billboard",
                L"Data/LoL/FX/Champions/Ezreal/q_projectile.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ezreal.Q.Hit", "Ezreal", "ProjectileVisualCatalog::Resolve",
                "Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Ezreal/q_hit.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ezreal.W.Cast", "Ezreal", "Ezreal::Fx::SpawnWProjectile",
                "Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp",
                "MeshParticle,Billboard",
                L"Data/LoL/FX/Champions/Ezreal/w_cast.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ezreal.W.Projectile", "Ezreal", "Ezreal::Fx::SpawnWProjectile",
                "Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp",
                "MeshParticle,Billboard",
                L"Data/LoL/FX/Champions/Ezreal/w_projectile.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ezreal.E.BlinkFlash", "Ezreal", "Ezreal::Fx::SpawnEFlash",
                "Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp",
                "Ribbon,GroundDecal,Billboard",
                L"Data/LoL/FX/Champions/Ezreal/e_blink_flash.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ezreal.R.Cast", "Ezreal", "Ezreal::Fx::SpawnRBow",
                "Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp",
                "MeshParticle,Billboard",
                L"Data/LoL/FX/Champions/Ezreal/r_cast.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ezreal.R.Missile", "Ezreal", "Ezreal::Fx::SpawnRProjectile",
                "Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp",
                "MeshParticle,Billboard",
                L"Data/LoL/FX/Champions/Ezreal/r_missile.wfx",
                L"",
                "WfxPilot"
            }
        };

        return s_Entries;
    }
}
