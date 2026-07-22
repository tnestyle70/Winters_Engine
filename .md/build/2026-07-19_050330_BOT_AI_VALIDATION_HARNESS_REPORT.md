# Bot AI Validation Harness Report

- Date: 2026-07-19 05:03:43 +09:00
- Repo: C:\Users\user\Desktop\Winters
- Configuration: Release
- Overall: FAIL

## Steps

- PASS git diff --check exit=0 seconds=0.33
- PASS ChampionAI dependency boundary audit exit=1 seconds=0.02
  - Notes: No matches.
- PASS Yone E stage-2 contract audit exit=0 seconds=0.12
- FAIL LoL data-driven pipeline and SimLab regression exit=1 seconds=12.2

## Output Tail

### git diff --check

```text
ime Git touches it
warning: in the working copy of 'Shared/GameSim/Champions/Yone/YoneGameSim.h', LF will be replaced by CRLF the next tim
e Git touches it
warning: in the working copy of 'Shared/GameSim/Components/ChampionAIComponent.h', LF will be replaced by CRLF the next
 time Git touches it
warning: in the working copy of 'Shared/GameSim/Components/DamageRequestComponent.h', LF will be replaced by CRLF the n
ext time Git touches it
warning: in the working copy of 'Shared/GameSim/Components/IreliaSimComponent.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/GameSim/Components/YoneSimComponent.h', LF will be replaced by CRLF the next ti
me Git touches it
warning: in the working copy of 'Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/GameSim/Definitions/DamageTypes.h', LF will be replaced by CRLF the next time G
it touches it
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
warning: in the working copy of 'Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.cpp', LF will be replaced by
CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.h', LF will be replaced by CR
LF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Stat/StatSystem.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/Schemas/Command.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Command_generated.h', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Snapshot_generated.h', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Shared/Schemas/Snapshot.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/run_codegen.bat', LF will be replaced by CRLF the next time Git touches
 it
warning: in the working copy of 'Tools/AIResearch/AI_EPISODE_V1.md', LF will be replaced by CRLF the next time Git touc
hes it
warning: in the working copy of 'Tools/Harness/GameRoomBotMatchSoak.cpp', LF will be replaced by CRLF the next time Git
 touches it
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
    "generatedAtUtc":  "2026-07-18T20:03:40.5283098Z",
    "root":  "C:\\Users\\user\\Desktop\\Winters",
    "source":  "C:\\Users\\user\\Desktop\\Winters\\Data\\LoL\\ClientPublic\\Visual\\ChampionVisualDefs.json",
    "output":  "C:\\Users\\user\\Desktop\\Winters\\Data\\LoL\\ClientPublic\\Visual\\Champion\\ChampionVisualTimingSeed.json",
    "championCount":  17,
    "skillStageCount":  101,
    "mismatchCount":  0,
    "mismatches":  [

                   ]
}
[LoLDataDriven] Build Shared/GameSim/Include/GameSim.vcxproj
msbuild 버전 17.14.40+3e7442088(.NET Framework용)

  파일 삭제 - C:\Users\user\Desktop\Winters\EngineSDK\inc\Core\Timer_Manager.h
  파일 삭제 - C:\Users\user\Desktop\Winters\EngineSDK\inc\Manager\UI\Font_Manager.h
  파일 삭제 - C:\Users\user\Desktop\Winters\EngineSDK\inc\Manager\UI\UI_Manager.h
  파일 삭제 - C:\Users\user\Desktop\Winters\EngineSDK\inc\Scene\Scene_Manager.h
  파일 삭제 - C:\Users\user\Desktop\Winters\EngineSDK\inc\Sound\Sound_Manager.h
  0개 파일이 복사되었습니다.
  0개 파일이 복사되었습니다.
  0개 파일이 복사되었습니다.
  0개 파일이 복사되었습니다.
  0개 파일이 복사되었습니다.
  0개 파일이 복사되었습니다.
  0개 파일이 복사되었습니다.
  0개 파일이 복사되었습니다.
  0개 파일이 복사되었습니다.
  0개 파일이 복사되었습니다.
  0개 파일이 복사되었습니다.
  0개 파일이 복사되었습니다.
  0개 파일이 복사되었습니다.
  0개 파일이 복사되었습니다.
  [SharedBoundary] PASS - Shared has no direct Engine/DX/ImGui/product includes (Phase 7F adapters excluded).
  Usage: C:\Users\user\Desktop\Winters\Shared\Schemas\..\..\Tools\Bin\flatc.exe
  [-b|--binary, -c|--cpp, -n|--csharp, -d|--dart, -g|--go, -j|--java, -t|--json,
  --jsonschema, --kotlin, --kotlin-kmp, --lobster, -l|--lua, --nim, --php,
  --proto, -p|--python, -r|--rust, --swift, -T|--ts, -o, -I, -M, --version,
  -h|--help, --strict-json, --allow-non-utf8, --natural-utf8, --defaults-json,
  --unknown-json, --no-prefix, --scoped-enums, --no-emit-min-max-enum-values,
  --swift-implementation-only, --gen-includes, --no-includes, --gen-mutable,
  --gen-onefile, --gen-name-strings, --gen-object-api, --gen-compare,
  --gen-nullable, --java-package-prefix, --java-checkerframework, --gen-generated,
  --gen-jvmstatic, --gen-all, --gen-json-emit, --cpp-include, --cpp-ptr-type,
  --cpp-str-type, --cpp-str-flex-ctor, --cpp-field-case-style, --cpp-std,
  --cpp-static-reflection, --object-prefix, --object-suffix, --go-namespace,
  --go-import, --go-module-name, --raw-binary, --size-prefixed,
  --proto-namespace-suffix, --oneof-union, --keep-proto-id, --proto-id-gap,
  --grpc, --schema, --bfbs-filenames, --bfbs-absolute-paths, --bfbs-comments,
  --bfbs-builtins, --bfbs-gen-embed, --conform, --conform-includes,
  --filename-suffix, --filename-ext, --include-prefix, --keep-prefix,
  --reflect-types, --reflect-names, --rust-serialize, --rust-module-root-file,
  --root-type, --require-explicit-ids, --force-defaults, --force-empty,
  --force-empty-vectors, --flexbuffers, --no-warnings, --warnings-as-errors,
  --cs-global-alias, --cs-gen-json-serializer, --json-nested-bytes,
  --ts-flat-files, --ts-entry-points, --annotate-sparse-vectors, --annotate,
  --no-leak-private-annotation, --python-no-type-prefix-suffix, --python-typing,
  --python-version, --python-decode-obj-api-strings, --python-gen-numpy,
  --ts-omit-entrypoint, --ts-undefined-for-optionals, --file-names-only,
  --grpc-filename-suffix, --grpc-additional-header, --grpc-use-system-headers,
  --grpc-search-path, --grpc-python-typed-handlers, --grpc-callback-api]...
  FILE... [-- BINARY_FILE...]

  C:\Users\user\Desktop\Winters\Shared\Schemas\..\..\Tools\Bin\flatc.exe: [ERROR] flatc cpp codegen failed for Command
EXEC : error :  [C:\Users\user\Desktop\Winters\Shared\GameSim\Include\GameSim.vcxproj]
    Unable to generate C++ for Command

C:\Users\user\Desktop\Winters\Shared\GameSim\Include\GameSim.vcxproj(334,5): error MSB3073: ""C:\Users\user\Desktop\Winters\Shared\GameSim\Include\..\..\Schemas\run_codegen.bat"" 명령이 종료되었습니다(코드: 1).
cmd.exe : Build Shared/GameSim/Include/GameSim.vcxproj failed with exit code 1
At C:\Users\user\Desktop\Winters\Tools\Harness\Run-BotAiValidation.ps1:77 char:20
+             $raw = & cmd.exe /c $CommandLine 2>&1
+                    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (Build Shared/Ga...ith exit code 1:String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError

At C:\Users\user\Desktop\Winters\Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1:19 char:9
+         throw "$Name failed with exit code $LASTEXITCODE"
+         ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : OperationStopped: (Build Shared/Ga...ith exit code 1:String) [], RuntimeException
    + FullyQualifiedErrorId : Build Shared/GameSim/Include/GameSim.vcxproj failed with exit code 1
```
