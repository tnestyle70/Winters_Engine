# S17 RHI Validation Harness Report

- Date: 2026-06-24 10:33:33 +09:00
- Repo: `C:\Users\tnest\Desktop\Winters`
- Configuration: `Debug`
- Platform: `x64`
- Overall: `PASS`

## Steps

- `PASS` `git diff --check` exit=`0` seconds=`0.1`
- `PASS` `Client/Public and Shared concrete graphics audit` exit=`1` seconds=`1.3`
  - Notes: No matches.
- `PASS` `Focused common RHI public header audit` exit=`1` seconds=`0.06`
  - Notes: No matches.
- `PASS` `CMake/Ninja S17 targets` exit=`0` seconds=`14.11`
- `PASS` `MSBuild Winters.sln` exit=`0` seconds=`77`
- `PASS` `Runtime smoke` exit=`0` seconds=`50.43`

## Output Tail

### CMake/Ninja S17 targets

```text
  +C:/Users/tnest/Desktop/Winters/.md/build/2026-06-24_VIEGO_SOUL_EATING_AND_KALISTA_SENTINEL_REPORT.md
  +C:/Users/tnest/Desktop/Winters/.md/build/2026-06-24_VIEGO_SOUL_R_E_STEALTH_FIX_REPORT.md
  +C:/Users/tnest/Desktop/Winters/.md/build/2026-06-24_VIEGO_W_CHARGE_GLOW_DASH_TUNING_REPORT.md
  +C:/Users/tnest/Desktop/Winters/.md/build/2026-06-24_YONE_BA_Q_ANIMATION_CONTINUITY_REPORT.md
  +C:/Users/tnest/Desktop/Winters/.md/collab/work-packets/2026-06-24_iocp_rhi_thread_ownership_fix.md
  +C:/Users/tnest/Desktop/Winters/.md/collab/work-packets/2026-06-24_yone_bot_ai_collab_pipeline.md
  +C:/Users/tnest/Desktop/Winters/.md/guide/LOL_SOUND_EXTRACT_GUIDE.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_CHARACTER_BOT_AI_BRAINTYPE_DIFFICULTY_WIRING_PLAN.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_CHARACTER_BOT_AI_PHASED_IMPLEMENTATION_PIPELINE_PLAN.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_CHARACTER_BOT_COMMON_DECISION_AND_CHAMPION_TACTICS_ML_READY_PLAN.
md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_CLIENT_STALE_ENTITY_AND_KILL_SCORE_DEDUP_PLAN.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_EFFECT_TOOL_UE_STYLE_EDITOR_GUIDE.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_IOCP_RHI_THREAD_OWNERSHIP_FIX_HANDOFF_PLAN.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_KALISTA_W_SENTINEL_CONE_VISION_PLAN.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_LOL_CAMERA_F2_FREECAM_WASD_GUARD.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_MAP_BUSH_PLACEMENT_MAPDATA_PLAN.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_VIEGO_SOUL_EATING_AND_KALISTA_SENTINEL_IMPLEMENTATION.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_VIEGO_SOUL_R_E_STEALTH_FIX_PLAN.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_VIEGO_W_CHARGE_GLOW_DASH_TUNING.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_YONE_BA_Q_ANIMATION_CONTINUITY_PLAN.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_YONE_BOT_AI_E_STAGE2_IMPLEMENTATION_PLAN.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_YONE_BOT_AI_RETURN_SCENARIO_TRACE_PLAN.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_YONE_BOT_AI_TACTIC_SCORING_PLAN.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_YONE_CHARACTER_BOT_AI_COMMON_TACTICS_DESIGN.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/Champion/2026-06-24_YONE_BOT_AI_CODEBASE_AUDIT_AND_CONTINUATION_PLAN.md
-- GLOB mismatch!
The following files were added:
  +C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Bush_Manager.cpp
-- GLOB mismatch!
The following files were added:
  +C:/Users/tnest/Desktop/Winters/Client/Public/GameObject/Champion/Kalista/KalistaSentinelVisualComponent.h
  +C:/Users/tnest/Desktop/Winters/Client/Public/Manager/Bush_Manager.h
-- GLOB mismatch!
The following files were added:
  +C:/Users/tnest/Desktop/Winters/Data/LoL/FX/Champions/Viego/w_charge_glow.wfx
-- GLOB mismatch!
The following files were added:
  +C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/KalistaSentinelComponent.h
  +C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/EffectAnchorSubtype.h
[1/2] Re-running CMake...
-- Configuring done (1.2s)
-- Generating done (0.6s)
-- Build files have been written to: C:/Users/tnest/Desktop/Winters/out/build/msvc-ninja
[0/4] Re-checking globbed directories...
[1/10] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\ECS\BushVolumeIndex.cpp.obj
[2/10] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\ECS\Systems\VisionSystem.cpp.obj
C:\Users\tnest\Desktop\Winters\Engine\Public\ECS/Systems/VisionSystem.h(24): warning C4275: DLL 인터페이스가 아닌 class 'ISystem'이(가) DLL 인터페이스 class 'Engine::CVisionSystem'의 기본으로 사용되었습니다.
C:\Users\tnest\Desktop\Winters\Engine\Public\ECS/ISystem.h(9): note: 'ISystem' 선언을 참조하십시오.
C:\Users\tnest\Desktop\Winters\Engine\Public\ECS/Systems/VisionSystem.h(24): note: 'Engine::CVisionSystem' 선언을 참조하십시오.
[3/10] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\ECS\Systems\TurretAISystem.cpp.obj
C:\Users\tnest\Desktop\Winters\Engine\Public\ECS/Systems/TurretAISystem.h(15): warning C4275: DLL 인터페이스가 아닌 class 'ISystem'이(가) DLL 인터페이스 class 'Engine::CTurretAISystem'의 기본으로 사용되었습니다.
C:\Users\tnest\Desktop\Winters\Engine\Public\ECS/ISystem.h(9): note: 'ISystem' 선언을 참조하십시오.
C:\Users\tnest\Desktop\Winters\Engine\Public\ECS/Systems/TurretAISystem.h(15): note: 'Engine::CTurretAISystem' 선언을 참조하십시오.
[4/10] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Manager\UI\UI_Manager.cpp.obj
[5/10] Linking CXX shared library C:\Users\tnest\Desktop\Winters\Engine\Bin\Debug\WintersEngine.dll; Deploying WintersEngine artifacts through UpdateLib.bat
Deleted file - C:\Users\tnest\Desktop\Winters\EngineSDK\inc\Core\Timer_Manager.h
Deleted file - C:\Users\tnest\Desktop\Winters\EngineSDK\inc\Manager\UI\Font_Manager.h
Deleted file - C:\Users\tnest\Desktop\Winters\EngineSDK\inc\Manager\UI\UI_Manager.h
Deleted file - C:\Users\tnest\Desktop\Winters\EngineSDK\inc\Scene\Scene_Manager.h
Deleted file - C:\Users\tnest\Desktop\Winters\EngineSDK\inc\Sound\Sound_Manager.h
0 File(s) copied
0 File(s) copied
C:\Users\tnest\Desktop\Winters\Engine\Bin\Debug\WintersEngine.lib
1 File(s) copied
0 File(s) copied
C:\Users\tnest\Desktop\Winters\Engine\Bin\Debug\WintersEngine.dll
1 File(s) copied
C:\Users\tnest\Desktop\Winters\Engine\Bin\Debug\WintersEngine.pdb
1 File(s) copied
0 File(s) copied
0 File(s) copied
0 File(s) copied
0 File(s) copied
0 File(s) copied
0 File(s) copied
0 File(s) copied
0 File(s) copied
[6/10] Linking CXX executable C:\Users\tnest\Desktop\Winters\EldenRingClient\Bin\Debug\WintersElden.exe; Copy WintersEngine runtime DLL to EldenRingClient output; Copy third-party runtime DLLs to EldenRingClient output; Copy shaders to EldenRingClient output
[7/10] Linking CXX executable C:\Users\tnest\Desktop\Winters\EldenRingEditor\Bin\Debug\WintersEldenRingEditor.exe; Copy WintersEngine runtime DLL to EldenRingEditor output; Copy third-party runtime DLLs to EldenRingEditor output; Copy shaders to EldenRingEditor output
```

### MSBuild Winters.sln

```text
msbuild 버전 18.7.8+1ac568fee(.NET Framework용)

빌드했습니다.
    경고 183개
    오류 0개

경과 시간: 00:01:13.36
```

### Runtime smoke

```text

Name                       AliveAfterSeconds ExitCode Cleanup
----                       ----------------- -------- -------
WintersElden_probe_dx12                 True          killed
WintersElden_probe_dx11                 True          killed
WintersEldenRingEditor                  True          killed
WintersGame                             True          killed
WintersGame_rhi_scene_only              True          killed
```
