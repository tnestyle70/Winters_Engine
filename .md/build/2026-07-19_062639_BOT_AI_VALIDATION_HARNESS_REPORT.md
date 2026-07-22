# Bot AI Validation Harness Report

- Date: 2026-07-19 06:29:42 +09:00
- Repo: C:\Users\user\Desktop\Winters
- Configuration: Release
- Overall: FAIL

## Steps

- PASS git diff --check exit=0 seconds=0.35
- PASS ChampionAI dependency boundary audit exit=1 seconds=0.02
  - Notes: No matches.
- PASS Yone E stage-2 contract audit exit=0 seconds=0.11
- FAIL LoL data-driven pipeline and SimLab regression exit=1 seconds=182.25

## Output Tail

### git diff --check

```text
warning: in the working copy of 'Shared/GameSim/Definitions/SkillAtomData.h', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/GameSim/Definitions/SnapshotStateFlags.h', LF will be replaced by CRLF the next
 time Git touches it
warning: in the working copy of 'Shared/GameSim/Definitions/SpawnLoadoutPolicyDef.h', LF will be replaced by CRLF the n
ext time Git touches it
warning: in the working copy of 'Shared/GameSim/Generated/ChampionAIPolicyData.generated.inl', LF will be replaced by C
RLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Generated/ChampionGameData.generated.cpp', LF will be replaced by CRLF
the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Include/GameSim.vcxproj', LF will be replaced by CRLF the next time Git
 touches it
warning: in the working copy of 'Shared/GameSim/Include/GameSim.vcxproj.filters', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/GameSim/Spawn/ChampionGameplayAssembly.cpp', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.cpp', LF will be replaced by CRLF th
e next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIPerception.h', LF will be replaced by CRLF
 the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIResearchTypes.h', LF will be replaced by C
RLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.cpp', LF will be replaced by CRL
F the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h', LF will be replaced by CRLF
the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp', LF will be replaced by CR
LF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h', LF will be replaced by CRL
F the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Damage/DamagePipeline.cpp', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Damage/DamagePipeline.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp', LF wi
ll be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.cpp', LF will be replaced by
CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.h', LF will be replaced by CR
LF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Stat/StatSystem.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/Schemas/Command.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Event.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Command_generated.h', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Event_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Hello_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/LobbyCommand_generated.h', LF will be replaced by CRLF th
e next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/LobbyState_generated.h', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/LobbyTypes_generated.h', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Snapshot_generated.h', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Shared/Schemas/Snapshot.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/run_codegen.bat', LF will be replaced by CRLF the next time Git touches
 it
warning: in the working copy of 'Tools/AIResearch/AI_EPISODE_V1.md', LF will be replaced by CRLF the next time Git touc
hes it
warning: in the working copy of 'Tools/Harness/GameRoomBotMatchSoak.cpp', LF will be replaced by CRLF the next time Git
 touches it
warning: in the working copy of 'Tools/Harness/RunGameRoomBotMatchSoak.ps1', LF will be replaced by CRLF the next time
Git touches it
warning: in the working copy of 'Tools/LoLData/Build-LoLDefinitionPack.py', LF will be replaced by CRLF the next time G
it touches it
warning: in the working copy of 'Tools/SimLab/main.cpp', LF will be replaced by CRLF the next time Git touches it
```

### Yone E stage-2 contract audit

```text
Yone E stage-2 contract is consistent.
```

### LoL data-driven pipeline and SimLab regression

```text
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\NavigationSystem.h(18,22):
      'Engine::CNavigationSystem' 선언을 참조하십시오.

C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\VisionSystem.h(24,51): warning C4275: DLL 인터페이스가 아닌 class 'ISystem'이(가) DLL 인터페이스 class 'Engine::CVisionSystem'의 기본으로 사용되었습니다. [C:\Users\user\Desktop\Winters\Client\Include\Client.vcxproj]
  (소스 파일 '../Private/UI/EffectTuner.cpp'을(를) 컴파일하는 중)
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\ISystem.h(9,7):
      'ISystem' 선언을 참조하십시오.
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\VisionSystem.h(24,22):
      'Engine::CVisionSystem' 선언을 참조하십시오.

C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\NavigationSystem.h(18,55): warning C4275: DLL 인터페이스가 아닌 class 'ISystem'이(가) DLL 인터페이스 class 'Engine::CNavigationSystem'의 기본으로 사용되었습니다. [C:\Users\user\Desktop\Winters\Client\Include\Client.vcxproj]
  (소스 파일 '../Private/UI/WfxEffectToolPanel.cpp'을(를) 컴파일하는 중)
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\ISystem.h(9,7):
      'ISystem' 선언을 참조하십시오.
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\NavigationSystem.h(18,22):
      'Engine::CNavigationSystem' 선언을 참조하십시오.

C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\VisionSystem.h(24,51): warning C4275: DLL 인터페이스가 아닌 class 'ISystem'이(가) DLL 인터페이스 class 'Engine::CVisionSystem'의 기본으로 사용되었습니다. [C:\Users\user\Desktop\Winters\Client\Include\Client.vcxproj]
  (소스 파일 '../Private/UI/WfxEffectToolPanel.cpp'을(를) 컴파일하는 중)
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\ISystem.h(9,7):
      'ISystem' 선언을 참조하십시오.
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\VisionSystem.h(24,22):
      'Engine::CVisionSystem' 선언을 참조하십시오.

C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\NavigationSystem.h(18,55): warning C4275: DLL 인터페이스가 아닌 class 'ISystem'이(가) DLL 인터페이스 class 'Engine::CNavigationSystem'의 기본으로 사용되었습니다. [C:\Users\user\Desktop\Winters\Client\Include\Client.vcxproj]
  (소스 파일 '../Private/UI/MapTunerPanel.cpp'을(를) 컴파일하는 중)
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\ISystem.h(9,7):
      'ISystem' 선언을 참조하십시오.
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\NavigationSystem.h(18,22):
      'Engine::CNavigationSystem' 선언을 참조하십시오.

C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\VisionSystem.h(24,51): warning C4275: DLL 인터페이스가 아닌 class 'ISystem'이(가) DLL 인터페이스 class 'Engine::CVisionSystem'의 기본으로 사용되었습니다. [C:\Users\user\Desktop\Winters\Client\Include\Client.vcxproj]
  (소스 파일 '../Private/UI/MapTunerPanel.cpp'을(를) 컴파일하는 중)
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\ISystem.h(9,7):
      'ISystem' 선언을 참조하십시오.
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\VisionSystem.h(24,22):
      'Engine::CVisionSystem' 선언을 참조하십시오.

C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\NavigationSystem.h(18,55): warning C4275: DLL 인터페이스가 아닌 class 'ISystem'이(가) DLL 인터페이스 class 'Engine::CNavigationSystem'의 기본으로 사용되었습니다. [C:\Users\user\Desktop\Winters\Client\Include\Client.vcxproj]
  (소스 파일 '../Private/UI/RenderDebug.cpp'을(를) 컴파일하는 중)
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\ISystem.h(9,7):
      'ISystem' 선언을 참조하십시오.
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\NavigationSystem.h(18,22):
      'Engine::CNavigationSystem' 선언을 참조하십시오.

C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\VisionSystem.h(24,51): warning C4275: DLL 인터페이스가 아닌 class 'ISystem'이(가) DLL 인터페이스 class 'Engine::CVisionSystem'의 기본으로 사용되었습니다. [C:\Users\user\Desktop\Winters\Client\Include\Client.vcxproj]
  (소스 파일 '../Private/UI/RenderDebug.cpp'을(를) 컴파일하는 중)
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\ISystem.h(9,7):
      'ISystem' 선언을 참조하십시오.
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\VisionSystem.h(24,22):
      'Engine::CVisionSystem' 선언을 참조하십시오.

C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\NavigationSystem.h(18,55): warning C4275: DLL 인터페이스가 아닌 class 'ISystem'이(가) DLL 인터페이스 class 'Engine::CNavigationSystem'의 기본으로 사용되었습니다. [C:\Users\user\Desktop\Winters\Client\Include\Client.vcxproj]
  (소스 파일 '../Private/UI/SkillTimingPanel.cpp'을(를) 컴파일하는 중)
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\ISystem.h(9,7):
      'ISystem' 선언을 참조하십시오.
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\NavigationSystem.h(18,22):
      'Engine::CNavigationSystem' 선언을 참조하십시오.

C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\VisionSystem.h(24,51): warning C4275: DLL 인터페이스가 아닌 class 'ISystem'이(가) DLL 인터페이스 class 'Engine::CVisionSystem'의 기본으로 사용되었습니다. [C:\Users\user\Desktop\Winters\Client\Include\Client.vcxproj]
  (소스 파일 '../Private/UI/SkillTimingPanel.cpp'을(를) 컴파일하는 중)
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\ISystem.h(9,7):
      'ISystem' 선언을 참조하십시오.
      C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\Systems\VisionSystem.h(24,22):
      'Engine::CVisionSystem' 선언을 참조하십시오.

     C:\Users\user\Desktop\Winters\Client\Include\..\Bin\Release\WintersGame.lib 라이브러리 및 C:\Users\user\Desktop\Winters\Client\Include\..\Bin\Release\WintersGame.exp 개체를 생성하고 있습니다.
LINK : fatal error LNK1114: 원본 파일 'C:\Users\user\Desktop\Winters\Client\Include\..\Bin\Release\WintersGame.lib'을(를) 덮어쓸 수 없습니다. 오류 코드 5 [C:\Users\user\Desktop\Winters\Client\Include\Client.vcxproj]
cmd.exe : Build Client/Include/Client.vcxproj failed with exit code 1
At C:\Users\user\Desktop\Winters\Tools\Harness\Run-BotAiValidation.ps1:77 char:20
+             $raw = & cmd.exe /c $CommandLine 2>&1
+                    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (Build Client/In...ith exit code 1:String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError

At C:\Users\user\Desktop\Winters\Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1:19 char:9
+         throw "$Name failed with exit code $LASTEXITCODE"
+         ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : OperationStopped: (Build Client/In...ith exit code 1:String) [], RuntimeException
    + FullyQualifiedErrorId : Build Client/Include/Client.vcxproj failed with exit code 1
```
