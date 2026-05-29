# Phase 6E - Hybrid Filters 통합 최종 계획서

작성일: 2026-05-06

출처:
- 골격: `.md/TODO/05-06/PHASE_6A_6E_CODE_REVIEW.md` §Phase 6E (L956-L1204)
- 검증: 본 세션 4 vcxproj / 4 vcxproj.filters 전수 read (2026-05-06)
- 보강: codex 식 검토 7 건

목적:
- Visual Studio Solution Explorer 가상 필터와 디스크 폴더가 따로 놀지 않게 정리한다.
- `.vcxproj` 는 변경하지 않는다. `.filters` 만 갱신한다.
- msbuild 영향 0, F12 점프 결과와 솔루션 익스플로러 위치를 일치시킨다.

합격 기준:
- 4 vcxproj.filters 모두 XML 유효, VS 재오픈 silent rewrite 0
- vcxproj cpp/h 등재 개수 == filters 매핑 개수
- 평면 노출 (Filter 자식 없는 ClCompile/ClInclude) 0
- Engine Debug + Engine Debug-DX12 + Client Debug + Server Debug + AssetConverter Debug 5 컨피그 빌드 통과

---

## §0. 6E 원본 검토 결과

원본의 좋은 점:
- Hybrid 정책 명확 (최상위 = 모듈 번호, 내부 = 디스크 미러)
- 4 프로젝트 목표 트리 박제
- 모듈명 갱신 매트릭스 (Manager → RHI 등)
- PowerShell 검증 스크립트 골격
- 완료 기준 6 항목

본 계획서가 추가한 보강 7 건:

```txt
B-1. AssetConverter .filters 의 ClCompile 12 매핑 본체 박제 (원본 = Filter 정의만)
B-2. Server main.cpp <Filter>00. Server</Filter> 매핑 누락 명시
B-3. Server Shared flat 18 항목 (13 cpp + 5 h) 전수 매핑
B-4. Engine Manager/ 디스크 폴더 분해 결정 (옵션 B 권장 = filters 만)
B-5. Champion 01. Sylas 결번 + 05. UI\04 결번 처리 (옵션 a 권장 = 그대로)
B-6. M0 사전 검증 (devenv 종료 + 4 .filters 백업 + branch + xml validation)
B-7. CLAUDE.md §3 + AGENTS.md 모듈명 동기화 본체 박제
```

---

## §1. 잔여 사고

```txt
F-1. Engine cpp 19 미매핑 (WAnim 2 + DX12 17)
F-2. Engine h 21 미매핑 (WMesh/WAnim 5 + IRHI* 3 + DX12 13)
F-3. Client cpp 10 미매핑 (Shared 8 + VisualHookRegistry + SchemaSmoke)
F-4. Server cpp 1 미매핑 (main.cpp) + Shared flat 13 cpp + 5 h
F-5. AssetConverter .filters 자체 부재 → 12 cpp 평면
F-6. Engine 00. Manager 가 RHI/매니저 도메인 혼재 (5 sub-filter 위반)
F-7. CLAUDE.md §3 모듈명 stale (Manager/Structure/Audio/JobSystem)
```

우선순위:
- 즉시 (M1~M4): F-1 ~ F-5
- 별도 PR (M5~M6): F-6, F-7

---

## §2. M0 사전 검증

### M0-1. 환경 정리

```bash
taskkill //F //IM devenv.exe 2>/dev/null

cd /c/Users/user/Desktop/Winters
git checkout -b feature/phase-6e-hybrid-filters

cp Engine/Include/Engine.vcxproj.filters Engine/Include/Engine.vcxproj.filters.bak.2026-05-06
cp Client/Include/Client.vcxproj.filters Client/Include/Client.vcxproj.filters.bak.2026-05-06
cp Server/Include/Server.vcxproj.filters Server/Include/Server.vcxproj.filters.bak.2026-05-06
```

AssetConverter `.filters` 는 부재. 백업 X.

### M0-2. GUID 일괄 발급

PowerShell:

```powershell
1..30 | ForEach-Object { [guid]::NewGuid().ToString("B").ToUpper() }
```

출력된 30 개 GUID 를 본 계획서의 `{NEW-GUID-...}` 자리에 차례로 치환.

### M0-3. 합격 기준

- devenv.exe 종료 확인
- branch 생성
- 3 백업 파일 존재
- 30 GUID 표 작성

---

## §3. M1 - Engine.filters 매핑 보정

본 단계는 매핑 누락만 보정한다. 모듈 재정렬은 §7 별도.

### M1-1. WAnim 2 cpp 매핑

파일: `Engine/Include/Engine.vcxproj.filters`

수정 전 (L879-880):

```xml
<ClCompile Include="..\Private\AssetFormat\Anim\WAnimLoader.cpp" />
<ClCompile Include="..\Private\AssetFormat\Anim\WAnimWriter.cpp" />
```

수정 후:

```xml
<ClCompile Include="..\Private\AssetFormat\Anim\WAnimLoader.cpp">
  <Filter>06. Resource\08. AssetFormat\Anim</Filter>
</ClCompile>
<ClCompile Include="..\Private\AssetFormat\Anim\WAnimWriter.cpp">
  <Filter>06. Resource\08. AssetFormat\Anim</Filter>
</ClCompile>
```

### M1-2. WMesh / WAnim 5 h 매핑

수정 전 (L553-555):

```xml
<ClInclude Include="..\Public\AssetFormat\Mesh\WMeshFormat.h" />
<ClInclude Include="..\Public\AssetFormat\Mesh\WMeshLoader.h" />
<ClInclude Include="..\Public\AssetFormat\Mesh\WMeshWriter.h" />
```

수정 후:

```xml
<ClInclude Include="..\Public\AssetFormat\Mesh\WMeshFormat.h">
  <Filter>06. Resource\08. AssetFormat\Mesh</Filter>
</ClInclude>
<ClInclude Include="..\Public\AssetFormat\Mesh\WMeshLoader.h">
  <Filter>06. Resource\08. AssetFormat\Mesh</Filter>
</ClInclude>
<ClInclude Include="..\Public\AssetFormat\Mesh\WMeshWriter.h">
  <Filter>06. Resource\08. AssetFormat\Mesh</Filter>
</ClInclude>
```

수정 전 (L601-602):

```xml
<ClInclude Include="..\Public\AssetFormat\Anim\WAnimLoader.h" />
<ClInclude Include="..\Public\AssetFormat\Anim\WAnimWriter.h" />
```

수정 후:

```xml
<ClInclude Include="..\Public\AssetFormat\Anim\WAnimLoader.h">
  <Filter>06. Resource\08. AssetFormat\Anim</Filter>
</ClInclude>
<ClInclude Include="..\Public\AssetFormat\Anim\WAnimWriter.h">
  <Filter>06. Resource\08. AssetFormat\Anim</Filter>
</ClInclude>
```

### M1-3. IRHI 3 h 매핑

`IRHIBindGroup/Layout/Device/PipelineState/RenderPass` 5 개는 이미 매핑. Queue/SwapChain/CommandList 3 개만 누락.

추가 매핑:

```xml
<ClInclude Include="..\Public\RHI\IRHIQueue.h">
  <Filter>00. Manager\00. GraphicDev</Filter>
</ClInclude>
<ClInclude Include="..\Public\RHI\IRHISwapChain.h">
  <Filter>00. Manager\00. GraphicDev</Filter>
</ClInclude>
<ClInclude Include="..\Public\RHI\IRHICommandList.h">
  <Filter>00. Manager\00. GraphicDev</Filter>
</ClInclude>
```

### M1-4. DX12 17 cpp + 13 h 매핑

신규 sub-filter 정의 추가 (L131-137 의 `00. Manager\03. Buffer\DX11` 옆):

```xml
<Filter Include="00. Manager\03. Buffer\DX12">
  <UniqueIdentifier>{NEW-GUID-DX12}</UniqueIdentifier>
</Filter>
```

ClCompile 17 매핑:

```xml
<ClCompile Include="..\Private\RHI\DX12\DX12BindGroup.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12Buffer.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12CommandList.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12DescriptorHeap.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12Device.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12MemoryAllocator.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12PipelineState.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12Queue.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12RenderPass.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12ResourceBarrier.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12RootSignature.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12Sampler.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12Shader.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12SwapChain.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
<ClCompile Include="..\Private\RHI\DX12\DX12Texture.cpp">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClCompile>
```

ClInclude 13 매핑 (DX12MemoryAllocator/ResourceBarrier 는 .h 부재):

```xml
<ClInclude Include="..\Private\RHI\DX12\DX12BindGroup.h">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClInclude>
<ClInclude Include="..\Private\RHI\DX12\DX12Buffer.h">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClInclude>
<ClInclude Include="..\Private\RHI\DX12\DX12CommandList.h">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClInclude>
<ClInclude Include="..\Private\RHI\DX12\DX12DescriptorHeap.h">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClInclude>
<ClInclude Include="..\Private\RHI\DX12\DX12Device.h">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClInclude>
<ClInclude Include="..\Private\RHI\DX12\DX12PipelineState.h">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClInclude>
<ClInclude Include="..\Private\RHI\DX12\DX12Queue.h">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClInclude>
<ClInclude Include="..\Private\RHI\DX12\DX12RenderPass.h">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClInclude>
<ClInclude Include="..\Private\RHI\DX12\DX12RootSignature.h">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClInclude>
<ClInclude Include="..\Private\RHI\DX12\DX12Sampler.h">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClInclude>
<ClInclude Include="..\Private\RHI\DX12\DX12Shader.h">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClInclude>
<ClInclude Include="..\Private\RHI\DX12\DX12SwapChain.h">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClInclude>
<ClInclude Include="..\Private\RHI\DX12\DX12Texture.h">
  <Filter>00. Manager\03. Buffer\DX12</Filter>
</ClInclude>
```

### M1 검증 명령

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine\Include\Engine.vcxproj /p:Configuration=Debug-DX12 /p:Platform=x64 /v:minimal
```

### M1 완료 기준

- DX12 17 cpp + 13 h 가 `00. Manager\03. Buffer\DX12` 표시
- WAnim 2 cpp / 5 h 가 `06. Resource\08. AssetFormat` 하위 표시
- IRHI 3 h 가 `00. Manager\00. GraphicDev` 표시
- Engine Debug 와 Debug-DX12 빌드 모두 0 errors
- §9 검증 스크립트 출력 없음

---

## §4. M2 - Client.filters Shared 신설 + 매핑 보정

### M2-1. 07. Shared 신설

파일: `Client/Include/Client.vcxproj.filters`

신규 Filter 정의 추가:

```xml
<Filter Include="07. Shared">
  <UniqueIdentifier>{NEW-GUID-CLIENT-SHARED}</UniqueIdentifier>
</Filter>
<Filter Include="07. Shared\Systems">
  <UniqueIdentifier>{NEW-GUID-CLIENT-SHARED-SYSTEMS}</UniqueIdentifier>
</Filter>
<Filter Include="07. Shared\Registries">
  <UniqueIdentifier>{NEW-GUID-CLIENT-SHARED-REGISTRIES}</UniqueIdentifier>
</Filter>
```

### M2-2. Shared 8 cpp + Client 자체 2 cpp 매핑

수정 전 (L605-613, L632):

```xml
<ClCompile Include="..\Private\GamePlay\VisualHookRegistry.cpp" />
<ClCompile Include="..\..\Shared\GameSim\Systems\GameplayHookRegistry.cpp" />
<ClCompile Include="..\..\Shared\GameSim\Registries\ChampionStatsRegistry.cpp" />
<ClCompile Include="..\..\Shared\GameSim\Registries\SkillScalingRegistry.cpp" />
<ClCompile Include="..\..\Shared\GameSim\Registries\SkinRegistry.cpp" />
<ClCompile Include="..\..\Shared\GameSim\Systems\BuffSystem.cpp" />
<ClCompile Include="..\..\Shared\GameSim\Systems\DamagePipeline.cpp" />
<ClCompile Include="..\..\Shared\GameSim\Systems\SkillRankSystem.cpp" />
<ClCompile Include="..\..\Shared\GameSim\Systems\StatSystem.cpp" />
<ClCompile Include="..\Private\GamePlay\SchemaSmoke.cpp" />
```

수정 후:

```xml
<ClCompile Include="..\Private\GamePlay\VisualHookRegistry.cpp">
  <Filter>03. GamePlay\01. Skill</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\GameplayHookRegistry.cpp">
  <Filter>07. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Registries\ChampionStatsRegistry.cpp">
  <Filter>07. Shared\Registries</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Registries\SkillScalingRegistry.cpp">
  <Filter>07. Shared\Registries</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Registries\SkinRegistry.cpp">
  <Filter>07. Shared\Registries</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\BuffSystem.cpp">
  <Filter>07. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\DamagePipeline.cpp">
  <Filter>07. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\SkillRankSystem.cpp">
  <Filter>07. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\StatSystem.cpp">
  <Filter>07. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\Private\GamePlay\SchemaSmoke.cpp">
  <Filter>03. GamePlay</Filter>
</ClCompile>
```

### M2-3. Champion 결번 슬롯

현재 챔프 번호:

```txt
00 Irelia / 02 Yasuo / 03 Kalista / 04 Garen / 05 Zed / 06 Riven / 07 Ezreal / 08 Fiora / 09 Jax / 10 Annie / 11 Ashe / 12 Yone
```

`01 Sylas` 결번. `05. UI\04` 결번도 있음.

권장: 그대로 유지. Sylas 챔프 박제 시점에 슬롯 추가.

### M2 검증 명령

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

### M2 완료 기준

- `07. Shared\Systems` 5 cpp 표시
- `07. Shared\Registries` 3 cpp 표시
- `VisualHookRegistry.cpp` 가 `03. GamePlay\01. Skill` 표시
- `SchemaSmoke.cpp` 가 `03. GamePlay` 표시
- Client Debug 빌드 0 errors

---

## §5. M3 - Server.filters main.cpp + Shared 세분 + sub 신설

### M3-1. main.cpp 매핑

파일: `Server/Include/Server.vcxproj.filters`

수정 전 (L51):

```xml
<ClCompile Include="..\Private\main.cpp" />
```

수정 후:

```xml
<ClCompile Include="..\Private\main.cpp">
  <Filter>00. Server</Filter>
</ClCompile>
```

### M3-2. Network / Game sub-filter 신설

신규 Filter 정의:

```xml
<Filter Include="01. Network\03. FrameParser">
  <UniqueIdentifier>{NEW-GUID-FRAMEPARSER}</UniqueIdentifier>
</Filter>
<Filter Include="02. Game\04. CommandDispatcher">
  <UniqueIdentifier>{NEW-GUID-COMMANDDISPATCHER}</UniqueIdentifier>
</Filter>
<Filter Include="02. Game\05. SnapshotBuilder">
  <UniqueIdentifier>{NEW-GUID-SNAPSHOTBUILDER}</UniqueIdentifier>
</Filter>
```

매핑 갱신 (L79-87 의 `<Filter>02. Game</Filter>` flat 을 sub 로):

```xml
<ClCompile Include="..\Private\Game\CommandDispatcher.cpp">
  <Filter>02. Game\04. CommandDispatcher</Filter>
</ClCompile>
<ClCompile Include="..\Private\Game\SnapshotBuilder.cpp">
  <Filter>02. Game\05. SnapshotBuilder</Filter>
</ClCompile>
<ClCompile Include="..\Private\Network\FrameParser.cpp">
  <Filter>01. Network\03. FrameParser</Filter>
</ClCompile>
```

ClInclude 동일 패턴:

```xml
<ClInclude Include="..\Public\Network\FrameParser.h">
  <Filter>01. Network\03. FrameParser</Filter>
</ClInclude>
<ClInclude Include="..\Public\Game\SnapshotBuilder.h">
  <Filter>02. Game\05. SnapshotBuilder</Filter>
</ClInclude>
```

### M3-3. Shared flat → 04. Shared 3-tree 재구성

기존 Filter 정의 갱신 (L13-15):

```xml
<Filter Include="04. Shared">
  <UniqueIdentifier>{F3000004-0000-0000-0000-000000000004}</UniqueIdentifier>
</Filter>
<Filter Include="04. Shared\Components">
  <UniqueIdentifier>{NEW-GUID-SERVER-SHARED-COMPONENTS}</UniqueIdentifier>
</Filter>
<Filter Include="04. Shared\Systems">
  <UniqueIdentifier>{NEW-GUID-SERVER-SHARED-SYSTEMS}</UniqueIdentifier>
</Filter>
<Filter Include="04. Shared\Registries">
  <UniqueIdentifier>{NEW-GUID-SERVER-SHARED-REGISTRIES}</UniqueIdentifier>
</Filter>
```

ClCompile 13 매핑 갱신 (L91-129 의 모든 `<Filter>Shared</Filter>` 를 도메인 sub 로):

```xml
<ClCompile Include="..\..\Shared\GameSim\Registries\ChampionStatsRegistry.cpp">
  <Filter>04. Shared\Registries</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Registries\SkillScalingRegistry.cpp">
  <Filter>04. Shared\Registries</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Registries\SkinRegistry.cpp">
  <Filter>04. Shared\Registries</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\BuffSystem.cpp">
  <Filter>04. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\CommandExecutor.cpp">
  <Filter>04. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\DamagePipeline.cpp">
  <Filter>04. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\DamageQueueSystem.cpp">
  <Filter>04. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\DeathSystem.cpp">
  <Filter>04. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\GameplayHookRegistry.cpp">
  <Filter>04. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\MoveSystem.cpp">
  <Filter>04. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\SkillCooldownSystem.cpp">
  <Filter>04. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\SkillRankSystem.cpp">
  <Filter>04. Shared\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\StatSystem.cpp">
  <Filter>04. Shared\Systems</Filter>
</ClCompile>
```

ClInclude 5 매핑 갱신 (L168-182):

```xml
<ClInclude Include="..\..\Shared\GameSim\Components\MoveTargetComponent.h">
  <Filter>04. Shared\Components</Filter>
</ClInclude>
<ClInclude Include="..\..\Shared\GameSim\Systems\DamageQueueSystem.h">
  <Filter>04. Shared\Systems</Filter>
</ClInclude>
<ClInclude Include="..\..\Shared\GameSim\Systems\DeathSystem.h">
  <Filter>04. Shared\Systems</Filter>
</ClInclude>
<ClInclude Include="..\..\Shared\GameSim\Systems\MoveSystem.h">
  <Filter>04. Shared\Systems</Filter>
</ClInclude>
<ClInclude Include="..\..\Shared\GameSim\Systems\SkillCooldownSystem.h">
  <Filter>04. Shared\Systems</Filter>
</ClInclude>
```

### M3 검증 명령

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

### M3 완료 기준

- `00. Server` 산하에 `main.cpp` 표시
- `04. Shared\Systems` 10 cpp 표시
- `04. Shared\Registries` 3 cpp 표시
- `04. Shared\Components` 1 h 표시
- `01. Network\03. FrameParser` / `02. Game\04. CommandDispatcher` / `02. Game\05. SnapshotBuilder` 모두 정상
- Server Debug 빌드 0 errors

---

## §6. M4 - AssetConverter.filters 신규 박제

파일 신규 생성: `Tools/WintersAssetConverter/WintersAssetConverter.vcxproj.filters`

전문:

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup>
    <Filter Include="00. Engine Shared">
      <UniqueIdentifier>{NEW-GUID-AC-ENGINE-SHARED}</UniqueIdentifier>
    </Filter>
    <Filter Include="00. Engine Shared\AssetFormat">
      <UniqueIdentifier>{NEW-GUID-AC-ASSETFORMAT}</UniqueIdentifier>
    </Filter>
    <Filter Include="00. Engine Shared\AssetFormat\Common">
      <UniqueIdentifier>{NEW-GUID-AC-COMMON}</UniqueIdentifier>
    </Filter>
    <Filter Include="00. Engine Shared\AssetFormat\Mesh">
      <UniqueIdentifier>{NEW-GUID-AC-MESH}</UniqueIdentifier>
    </Filter>
    <Filter Include="00. Engine Shared\AssetFormat\Anim">
      <UniqueIdentifier>{NEW-GUID-AC-ANIM}</UniqueIdentifier>
    </Filter>
    <Filter Include="00. Engine Shared\Resource">
      <UniqueIdentifier>{NEW-GUID-AC-RESOURCE}</UniqueIdentifier>
    </Filter>
    <Filter Include="01. Tools">
      <UniqueIdentifier>{NEW-GUID-AC-TOOLS}</UniqueIdentifier>
    </Filter>
  </ItemGroup>
  <ItemGroup>
    <ClCompile Include="..\..\Engine\Private\AssetFormat\Anim\WAnimLoader.cpp">
      <Filter>00. Engine Shared\AssetFormat\Anim</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Engine\Private\AssetFormat\Anim\WAnimWriter.cpp">
      <Filter>00. Engine Shared\AssetFormat\Anim</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Engine\Private\AssetFormat\Anim\WSkelLoader.cpp">
      <Filter>00. Engine Shared\AssetFormat\Anim</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Engine\Private\AssetFormat\Anim\WSkelWriter.cpp">
      <Filter>00. Engine Shared\AssetFormat\Anim</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Engine\Private\AssetFormat\Common\BinaryReader.cpp">
      <Filter>00. Engine Shared\AssetFormat\Common</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Engine\Private\AssetFormat\Common\BinaryWriter.cpp">
      <Filter>00. Engine Shared\AssetFormat\Common</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Engine\Private\AssetFormat\Mesh\WMeshLoader.cpp">
      <Filter>00. Engine Shared\AssetFormat\Mesh</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Engine\Private\AssetFormat\Mesh\WMeshWriter.cpp">
      <Filter>00. Engine Shared\AssetFormat\Mesh</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Engine\Private\Resource\Animation.cpp">
      <Filter>00. Engine Shared\Resource</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Engine\Private\Resource\Bone.cpp">
      <Filter>00. Engine Shared\Resource</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Engine\Private\Resource\Skeleton.cpp">
      <Filter>00. Engine Shared\Resource</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Engine\Private\Tools\AssetConverter\main.cpp">
      <Filter>01. Tools</Filter>
    </ClCompile>
  </ItemGroup>
</Project>
```

GUID 7 개는 M0-2 의 PowerShell 출력에서 차례로 치환.

### M4 검증 명령

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Tools\WintersAssetConverter\WintersAssetConverter.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

### M4 완료 기준

- `.filters` 파일 존재 + XML 유효
- VS 재오픈 시 12 cpp 가 `00. Engine Shared\{AssetFormat\Common, Mesh, Anim} + Resource` + `01. Tools` 안에 정렬
- 평면 표시 0
- AssetConverter Debug 빌드 0 errors

---

## §7. M5 - Engine 모듈 재정렬 (별도 PR)

위험도 높음. 본 PR (M1~M4) 합격 후 별도 PR 권장. 본 절은 박제만, 진입은 사용자 결정 후.

### M5-1. 모듈명 갱신

```txt
00. Manager (RHI 도메인)        → 00. RHI + sub 재구성
00. Manager\06. Sound           → 08. Sound
00. Manager\07. Navigation      → 11. Scene\Navigation
00. Manager\08. UI              → 04. Editor\UI_Manager
00. Manager\09. Scene           → 11. Scene\Manager
00. Manager\10. Profiler        → 01. Core\04. Profiler
02. Structure                   → 02. Framework
08. Audio (현재 빈 필터)         → 제거 (08. Sound 흡수)
10. JobSystem\00. Core          → 01. Core\05. JobSystem
14. Tools                       → 신설
15. Shaders                     → 신설
```

### M5-2. RHI 재구성

```txt
00. RHI
  Interface           IRHI 8 + RHITypes/Handles/Descriptors/CRHIResourceTable + IBuffer + ShaderCompiler
  DX11                CDX11Device + DX11Pipeline/Shader/Buffer/Vertex/Index/Structured/ConstantBuffer
    State             DX11Sampler + BlendStateCache (현 03. Buffer\DX11 의 잡음 분리)
  DX12                M1-4 의 17 cpp + 13 h 모두 이동
```

ClCompile/Include 의 `<Filter>` 텍스트 일괄 sed:

```txt
00. Manager\00. GraphicDev    → 00. RHI\Interface (CDX11Device 만 00. RHI\DX11)
00. Manager\01. Pipeline      → 00. RHI\DX11
00. Manager\02. Shader        → 00. RHI\DX11 (ShaderCompiler 만 00. RHI\Interface)
00. Manager\03. Buffer        → 00. RHI\Interface (IBuffer)
00. Manager\03. Buffer\DX11   → 00. RHI\DX11
00. Manager\03. Buffer\DX12   → 00. RHI\DX12
00. Manager\04. ConstantBuffer → 00. RHI\DX11
00. Manager\05. Geometry      → 03. Renderer\08. Geometry
```

### M5-3. Manager 디스크 폴더 분해 결정

옵션 B 권장 (디스크 폴더 미변경, filters 만 분해):

```txt
디스크 그대로:
  Engine/Public/Manager/Navigation/NavGrid.h
  Engine/Public/Manager/Navigation/Pathfinder.h
  Engine/Public/Manager/UI/UI_Manager.h
  Engine/Public/Manager/Profiler/ProfilerOverlay.h

filters 만 분해:
  NavGrid.h           → 11. Scene\Navigation
  Pathfinder.h        → 11. Scene\Navigation
  UI_Manager.h        → 04. Editor\UI_Manager
  ProfilerOverlay.h   → 01. Core\04. Profiler
```

vcxproj 의 `..\Public\Manager\Navigation\NavGrid.h` path 는 불변. filters 라벨만 갱신.

### M5 완료 기준

- `00. Manager` Filter 정의 제거
- `00. RHI` Filter 정의 추가
- sub-filter 모두 의존성 정합
- Engine/Client/Server 빌드 통과
- §9 검증 스크립트 출력 없음

---

## §8. M6 - CLAUDE.md / AGENTS.md 동기화

M5 동반 필수.

### M6-1. CLAUDE.md §3 모듈명 갱신

파일: `CLAUDE.md`

수정 위치: `## 3. Repo Quick Map` 의 모듈 의존 방향 한 줄.

수정 전:

```txt
- 00.Manager (RHI Device/Pipeline/Shader/Buffer) → 01.Core (Timer/Input/Transform/JobSystem/Profiler) → 02.Structure (Framework/Entry) → 03.Renderer → 04.Editor (ImGui) → 05.ECS → 06.Resource → 07.Physics → 08.Audio → 09.Network → 10.JobSystem → 11.Scene → 12.Collision → 13.AI
```

수정 후:

```txt
- 00.RHI (DX11/DX12/Vulkan backend + IRHI 추상화) → 01.Core (Timer/Input/Transform/Profiler/JobSystem/Fiber) → 02.Framework (CEngineApp/Entry/GameInstance) → 03.Renderer → 04.Editor (ImGui + UI_Manager) → 05.ECS → 06.Resource (+ AssetFormat) → 07.Physics → 08.Sound → 09.Network → 10.Platform → 11.Scene (+ Navigation) → 12.Collision → 13.AI → 14.Tools (AssetConverter) → 15.Shaders (HLSL/HLSLI)
```

### M6-2. AGENTS.md 모듈 번호 갱신

파일: `AGENTS.md` L334-348 (Engine 필터 14 개 정의).

M6-1 과 동일 모듈명 갱신.

### M6-3. CLAUDE.md §1 Stabilization-0 동기화

`## 1. Current Focus` 의 Stabilization-0 옵션 끝에 한 줄 추가:

```txt
6. CLAUDE.md / AGENTS.md 모듈명 갱신 (00. Manager → 00. RHI 등 11 건)
```

### M6 완료 기준

- CLAUDE.md §3 + §1 모듈명 일관
- AGENTS.md 동일 모듈명 사용
- grep 으로 stale 표기 검출 0 (`"00. Manager"` / `"02. Structure"` / `"08. Audio"` / `"10. JobSystem"`)

---

## §9. PowerShell 검증 스크립트

원본 6E-6 의 스크립트는 `$_.Filter` 검사가 ClCompile 의 inner Filter 노드만 검증한다. 보강한 스크립트는 실제 `<Filter>...</Filter>` 자식 노드 vs `<ClCompile Include="..." />` 자기 닫음 구분.

파일 신설 권장: `Tools/Scripts/Validate-VcxprojFilters.ps1`

```powershell
$projects = @(
  "Engine\Include\Engine.vcxproj.filters",
  "Client\Include\Client.vcxproj.filters",
  "Server\Include\Server.vcxproj.filters",
  "Tools\WintersAssetConverter\WintersAssetConverter.vcxproj.filters"
)

$totalMissing = 0
foreach ($filtersPath in $projects) {
  if (-not (Test-Path $filtersPath)) {
    Write-Host "[SKIP] $filtersPath (file not found)" -ForegroundColor Yellow
    continue
  }

  [xml]$xml = Get-Content $filtersPath -Encoding UTF8
  $items = @()
  $xml.Project.ItemGroup | ForEach-Object {
    if ($_.ClCompile) { $items += $_.ClCompile }
    if ($_.ClInclude) { $items += $_.ClInclude }
  }

  $missing = $items | Where-Object {
    $_ -and -not $_.SelectSingleNode("Filter")
  }

  if ($missing.Count -gt 0) {
    Write-Host "[FAIL] $filtersPath : $($missing.Count) items missing" -ForegroundColor Red
    $missing | ForEach-Object {
      Write-Host "  -> $($_.Include)" -ForegroundColor Red
    }
    $totalMissing += $missing.Count
  } else {
    Write-Host "[PASS] $filtersPath : all items mapped" -ForegroundColor Green
  }
}

Write-Host ""
Write-Host "Total missing: $totalMissing"
exit ($totalMissing -gt 0 ? 1 : 0)
```

기대 결과:

```txt
[PASS] Engine\Include\Engine.vcxproj.filters : all items mapped
[PASS] Client\Include\Client.vcxproj.filters : all items mapped
[PASS] Server\Include\Server.vcxproj.filters : all items mapped
[PASS] Tools\WintersAssetConverter\WintersAssetConverter.vcxproj.filters : all items mapped
Total missing: 0
```

---

## §10. 진입 순서

```txt
0. Stabilization-0 통과 (CLAUDE.md §1.B)
1. devenv 종료 + 4 .filters 백업 + branch 생성
2. M1 Engine 매핑 보정 (저위험)
3. M2 Client Shared 신설 + 매핑 보정
4. M3 Server main.cpp + Shared 세분
5. M4 AssetConverter .filters 신규
6. (별도 PR) M5+M6 모듈 재정렬 + 문서 동기화
```

---

## §11. 박제 함정 자체 점검

`.md/process/PLAN_AUTHORING_PITFALLS.md` 의 8 단계 관문:

```txt
A. §1 사전 결정 미박제 0      OK (사고 ID F-1~F-7 모두 명시)
B. PIMPL 본체 read           N/A (filters 만 변경)
C. Render path 전수          N/A
D. 인용 의미 검증             OK (모든 줄번호 본체 인용)
E. ECS Scheduler 동시성       N/A
F. Owner Scope 매트릭스       OK (filters owner = vcxproj 단위)
G. API 실재 검증              OK (12 cpp / 17 cpp / 13 cpp 모두 grep)
H. 도메인 상수 분리           OK (모듈 14, 15 신설로 다른 게임 재사용 가능)
```

---

## §12. 결정 의문점

```txt
1. PR 분할 방식
   M1~M4 한 PR + M5~M6 별도 PR  vs  한 번에
2. GUID 발급
   New-Guid 일괄 30 개 OK ?
3. Champion 결번 슬롯
   01. Sylas + 05. UI\04 결번 그대로 유지 OK ?
4. Engine Manager/ 디스크 폴더
   그대로 유지 (옵션 B) OK ?
5. M4 AssetConverter 라벨
   00. Engine Shared  vs  00. Engine
6. M5+M6 진입 시점
   Stabilization-0 후  vs  Track A/B 후
7. Validate 스크립트 위치
   Tools/Scripts/Validate-VcxprojFilters.ps1 OK ?
```

---

## §13. 영향 범위

```txt
변경:
  Engine/Include/Engine.vcxproj.filters
  Client/Include/Client.vcxproj.filters
  Server/Include/Server.vcxproj.filters
  Tools/WintersAssetConverter/WintersAssetConverter.vcxproj.filters (신규)

미변경:
  4 .vcxproj
  소스 코드
  msbuild 출력
```

합격 시 효과:
- Solution Explorer 4 프로젝트 모두 평면 노출 0
- F12 점프 결과와 filters 위치 일치
- 미래 Vulkan 추가 시 `00. RHI\Vulkan` 즉시 사용
- 미래 게임 분리 (WintersLOL/Elden) 시 Client.filters 의 Champion 트리 그대로 복사

---

## §14. 진입 직전 체크리스트

```txt
- PHASE_6A_6E_CODE_REVIEW.md §6E 1 회 더 read
- CLAUDE.md §1.B Stabilization-0 5 항목 완료
- PLAN_AUTHORING_PITFALLS 8 관문 통과 확인
- devenv.exe 종료 + branch + 3 .filters 백업
- New-Guid 30 회 실행 + GUID 표 작성
- M1 진입 직전 Engine.vcxproj.filters 1 회 더 read (5/6 이후 변경 있는지)
```

박제 진입 OK 면 §10 0 번부터 시작.
