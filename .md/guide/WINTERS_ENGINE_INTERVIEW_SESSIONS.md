# Winters Engine Interview Sessions

> 작성일: 2026-05-21
> 목적: Winters Engine을 면접에서 설명할 수 있도록, C++ 빌드 원리부터 GameSim, 네트워크, 렌더링, 하드웨어 실행까지 세션 단위로 누적한다.
> 현재 질문: `Shared/GameSim` 폴더의 코드가 Visual Studio에서 다른 파일들과 함께 전처리, 컴파일, 링크되고 CPU/GPU 하드웨어 위에서 어떻게 실행되는가.

---

## 0. 세션 운영 방식

이 문서는 한 번에 모든 것을 외우는 문서가 아니다. 매 세션마다 다음 구조로 누적한다.

```text
질문
  -> 기초 원리
  -> Winters 코드 매핑
  -> 면접 답변
  -> 실습/검증
  -> 다음 세션 연결
```

원칙은 세 가지다.

- 폴더/파일 이름을 외우기보다, 소스가 실행 파일이 되는 경로를 먼저 이해한다.
- Winters의 실제 구조와 일반 C++/Windows/MSVC 원리를 분리해서 설명한다.
- 면접 답변은 `30초`, `2분`, `심화 꼬리질문` 버전으로 준비한다.

---

## 1. 전체 세션 로드맵

### Session 01 - GameSim 폴더에서 CPU/GPU 실행까지

핵심 질문:

```text
Shared/GameSim 폴더의 코드가 Visual Studio에서 어떻게 다른 파일들과 같이
전처리 -> 컴파일 -> .obj -> 링크 -> 메모리 섹션 -> CPU/GPU 실행으로 이어지는가?
```

배울 것:

- Visual Studio `.sln` / `.vcxproj` / `ClCompile`의 의미
- `.cpp` 번역 단위, 헤더 include, 전처리기 define
- `.obj`의 `.text`, `.rdata`, `.data`, `.bss`, symbol, relocation
- 링커, `.exe`, `.dll`, import library, PDB
- OS loader, 가상 메모리, stack, heap
- CPU register, ALU/FPU/SIMD, cache, RAM
- GPU는 GameSim을 직접 실행하지 않고, Client visual/RHI 경로의 렌더 명령을 실행한다는 구분

### Session 02 - Visual Studio/MSBuild 빌드 그래프

핵심 질문:

```text
Winters.sln이 Engine, Server, Client, Tools 프로젝트를 어떤 순서와 의존성으로 빌드하는가?
```

배울 것:

- Solution configuration: `Debug`, `Release`, `Debug-DX12`, `Release-DX12`
- Project reference: Client/Server가 Engine 산출물을 어떻게 참조하는가
- `OutDir`, `IntDir`, post-build copy, `EngineSDK`, runtime DLL
- `FlatcCodegen`처럼 컴파일 전 실행되는 codegen target
- 현재 작업 트리의 GameSim 폴더 재배치와 `.vcxproj` wiring 점검 방법

### Session 03 - C++ 컴파일러 내부

핵심 질문:

```text
전처리된 C++ 코드는 어떤 과정을 거쳐 기계어와 데이터 섹션이 되는가?
```

배울 것:

- lexical analysis, parsing, AST, semantic analysis
- template instantiation, inline, ODR
- Debug/Release 최적화 차이
- calling convention, name mangling
- COFF object file과 symbol table

### Session 04 - 링커와 PE 실행 파일

핵심 질문:

```text
여러 .obj와 라이브러리는 어떻게 하나의 .exe/.dll로 합쳐지는가?
```

배울 것:

- symbol resolution
- relocation
- static library vs import library vs DLL
- COMDAT folding, function-level linking
- PDB/debug information
- Windows PE section layout

### Session 05 - 프로세스 메모리와 런타임

핵심 질문:

```text
.data, .rdata, .bss, stack, heap은 실제 실행 중 어디에 있고 누가 관리하는가?
```

배울 것:

- virtual memory와 page table
- code section, read-only data, initialized data, zero-initialized data
- per-thread stack
- heap allocator, `new`, `std::vector`, ECS component storage
- TLS, static initialization
- crash dump를 볼 때 메모리 영역을 읽는 방법

### Session 06 - CPU 하드웨어 실행 모델

핵심 질문:

```text
컴파일된 GameSim 함수는 CPU register, ALU, cache, RAM 위에서 어떻게 돈다고 설명할 수 있는가?
```

배울 것:

- instruction pointer, register file, ALU/FPU/SIMD
- load/store, branch, pipeline, branch prediction
- L1/L2/L3 cache, cache line, RAM latency
- false sharing, data-oriented design
- deterministic fixed tick과 cache-friendly ECS 설계의 관계

### Session 07 - Winters GameSim 서버 권위 흐름

핵심 질문:

```text
Client input이 어떻게 GameCommand가 되고, Server GameSim이 어떻게 snapshot/event를 만든 뒤 Client visual로 돌아오는가?
```

배울 것:

- 고정 원칙: `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`
- `GameRoom` tick phase
- `CommandExecutor`, system execution, champion `Tick`
- replicated event와 snapshot broadcast
- local prediction과 server truth의 경계

### Session 08 - 네트워크와 직렬화

핵심 질문:

```text
GameSim 결과는 네트워크 패킷과 FlatBuffers schema로 어떻게 표현되는가?
```

배울 것:

- schema/codegen/generated header
- packet envelope, command/snapshot/event
- TCP/UDP, IOCP, session, queue
- ack/sequence, snapshot cadence, AOI/delta의 이유

### Session 09 - Client visual, RHI, GPU

핵심 질문:

```text
서버 GameSim 결과가 Client 렌더링과 GPU command로 어떻게 이어지는가?
```

배울 것:

- SnapshotApplier/EventApplier
- Transform/animation/FX/UI update
- renderer/RHI/driver/command buffer
- vertex/index/constant buffer, texture, shader
- PCIe, VRAM, GPU pipeline
- CPU simulation과 GPU rendering의 책임 분리

### Session 10 - 면접 실전 압축

핵심 질문:

```text
Winters Engine 전체를 30초, 2분, 10분 답변으로 어떻게 설명할 것인가?
```

배울 것:

- 프로젝트 소개 pitch
- 빌드/런타임/네트워크/렌더링 각각의 핵심 문장
- 꼬리질문 방어: determinism, server authority, memory, performance, tooling
- 모르는 것을 안전하게 말하는 방식

---

## Session 01 - GameSim 폴더에서 CPU/GPU 실행까지

### 1. 첫 결론

`Shared/GameSim` 폴더가 통째로 실행되는 것은 아니다.

Visual Studio/MSBuild는 `.vcxproj` 안의 `ClCompile Include="...cpp"` 항목을 읽고, 그 `.cpp` 파일들을 각각 따로 컴파일한다. 각 `.cpp`는 전처리 후 하나의 번역 단위가 되고, 컴파일러는 번역 단위마다 `.obj`를 만든다. 링커는 여러 `.obj`와 라이브러리를 합쳐 `WintersServer.exe`, `Client.exe`, `WintersEngine.dll` 같은 산출물을 만든다.

런타임에는 Windows loader가 `.exe`와 `.dll`의 섹션을 프로세스 가상 메모리에 매핑한다. CPU는 `.text`에 있는 기계어를 instruction pointer로 따라가며 실행하고, 데이터는 `.rdata`, `.data`, `.bss`, stack, heap에서 읽고 쓴다. GPU는 GameSim 코드를 직접 실행하지 않는다. Client가 GameSim 결과를 visual state로 바꾼 뒤 렌더링 명령과 리소스를 RHI/driver를 통해 GPU에 제출할 때 GPU가 일한다.

면접에서 이 한 문장으로 시작하면 좋다.

```text
폴더가 실행되는 게 아니라, 프로젝트에 등록된 .cpp들이 번역 단위별로 .obj가 되고,
링커가 그것들을 실행 파일로 묶습니다. GameSim은 서버 권위 시뮬레이션이라 CPU에서 실행되고,
GPU는 그 결과를 받은 클라이언트 렌더링 경로에서 draw/dispatch 명령을 처리합니다.
```

### 2. Winters에서 확인한 현재 구조

솔루션 레벨:

```text
Winters.sln
  -> Engine: Engine/Include/Engine.vcxproj
  -> Server: Server/Include/Server.vcxproj
  -> Client: Client/Include/Client.vcxproj
  -> WintersAssetConverter
  -> DX12SmokeHost
```

GameSim 코드 위치:

```text
Shared/GameSim
  -> Components
  -> Definitions
  -> Registries
  -> Systems
  -> Champions
```

개념상 연결:

```text
Server.vcxproj / Client.vcxproj
  -> Shared/GameSim의 필요한 .cpp를 ClCompile로 등록
  -> Shared/GameSim의 .h를 include
  -> 각 프로젝트의 IntDir 아래 .obj 생성
  -> Server/Client 실행 파일 링크에 참여
```

중요한 현재성 메모:

```text
2026-05-21 현재 작업 트리에는 GameSim 파일을 하위 폴더로 재배치하는 변경이 진행 중이다.
예: Shared/GameSim/Systems/Move/MoveSystem.cpp는 존재하지만,
일부 .vcxproj 항목은 과거 평면 경로인 Shared/GameSim/Systems/MoveSystem.cpp를 가리킬 수 있다.
따라서 Session 02에서는 "원리"와 별개로 실제 .vcxproj wiring을 한 번 빌드 기준으로 점검해야 한다.
```

Winters GameSim의 핵심 정신:

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual
```

`Shared/GameSim/World.h`는 GameSim 코드가 Engine ECS에 직접 깊게 묶이지 않도록 어댑터 경계를 둔다.

```cpp
namespace SharedSim
{
    using World = ::CWorld;
}
```

`Shared/GameSim/DeterministicTime.h`는 서버 시뮬레이션을 30Hz fixed tick 기준으로 생각하게 해준다.

```cpp
static constexpr f32_t kFixedDt = 1.f / 30.f;
static constexpr uint64_t kTicksPerSecond = 30;
```

Server `GameRoom`의 큰 흐름은 다음 형태다.

```text
Phase_DrainCommands
Phase_ServerBotAI
Phase_ExecuteCommands
Phase_SimulationSystems
Phase_BroadcastEvents
Phase_BroadcastSnapshot
```

여기서 `Phase_SimulationSystems`는 status, stat, buff, cooldown, recall, move, attack chase, champion tick, projectile, damage, death 같은 GameSim 시스템을 서버 tick 안에서 실행한다. 이 결과가 snapshot/event로 client에 전송되고, client는 그 결과를 visual state로 적용한다.

### 3. 전처리기: `.cpp`가 번역 단위가 되는 순간

컴파일러가 바로 `.cpp`를 기계어로 바꾸는 것이 아니다. 먼저 전처리기가 다음 일을 한다.

- `#include` 파일을 텍스트처럼 펼친다.
- `#define` 매크로를 치환한다.
- `#if`, `#ifdef`, `#pragma` 등을 처리한다.
- include directory에서 헤더 경로를 찾는다.

예를 들어 Server Debug 구성에는 대략 다음 성격의 define/include가 들어간다.

```text
PreprocessorDefinitions:
WIN32; _DEBUG; _CONSOLE; NOMINMAX; WIN32_LEAN_AND_MEAN

AdditionalIncludeDirectories:
Winters root
Server/Public
Shared
EngineSDK/inc
Engine/Public
FlatBuffers include
```

Client DX12 구성에서는 `WINTERS_RHI_BACKEND_DX12` 같은 define이 추가되어 렌더 백엔드 관련 코드 경로가 달라질 수 있다.

주의할 점:

- `.h` 파일은 혼자 `.obj`가 되지 않는다.
- `.h`는 `.cpp`에 include될 때 그 번역 단위 일부가 된다.
- inline 함수, template, constexpr, header-only 코드는 include된 여러 번역 단위에서 인스턴스화될 수 있다.
- `.cpp` 하나가 include를 모두 펼친 결과가 "하나의 번역 단위"다.

면접 표현:

```text
Visual Studio에서 보이는 폴더 구조는 개발자 편의에 가깝고,
컴파일러 입장에서는 프로젝트가 넘겨준 .cpp 목록과 include path, define 목록이 중요합니다.
각 .cpp는 include와 macro 확장을 거쳐 독립적인 translation unit이 됩니다.
```

### 4. 컴파일러: 번역 단위에서 `.obj`까지

전처리된 C++는 컴파일러 안에서 대략 다음 단계를 지난다.

```text
tokens
  -> parse
  -> AST
  -> type check / overload resolution / template instantiation
  -> optimization
  -> machine instruction selection
  -> register allocation
  -> COFF .obj output
```

MSVC가 만드는 `.obj`는 최종 실행 파일이 아니다. 중간 산출물이다. `.obj` 안에는 다음 성격의 정보가 들어간다.

| 영역 | 의미 |
|---|---|
| `.text` | 함수 기계어 코드 |
| `.rdata` | 읽기 전용 상수, 문자열 literal, vtable 일부 등 |
| `.data` | 초기값이 있는 전역/static 변수 |
| `.bss` | 0으로 초기화되는 전역/static 변수. 파일에는 크기 정보 위주로 들어가고 실행 시 0으로 준비된다 |
| symbol table | 이 `.obj`가 정의하거나 외부에서 필요로 하는 함수/변수 이름 |
| relocation | 아직 주소를 모르는 symbol을 링커가 나중에 메울 위치 |
| debug info | Debug/PDB 생성을 위한 소스/타입/라인 정보 |

예를 들어 `MoveSystem.cpp`가 `CMoveSystem::Execute`를 정의하고, 다른 파일에서 그 함수를 호출한다면:

```text
MoveSystem.obj
  -> "CMoveSystem::Execute가 여기 정의되어 있음"

GameRoom.obj
  -> "CMoveSystem::Execute를 호출해야 하는데 실제 주소는 아직 모름"

linker
  -> 두 symbol을 연결하고 call 대상 주소/relocation을 확정
```

면접 표현:

```text
.obj는 아직 실행 가능한 파일이 아니라, 기계어 조각과 데이터 조각, 그리고
"내가 정의한 심볼"과 "남이 정의해줘야 하는 심볼" 목록을 가진 COFF 목적 파일입니다.
```

### 5. 링커: `.obj` 여러 개가 하나의 실행 파일이 되는 과정

링커는 다음 일을 한다.

- 여러 `.obj`를 읽는다.
- unresolved external symbol을 다른 `.obj`나 `.lib`에서 찾는다.
- 중복 가능한 COMDAT/inline/template 조각을 정리한다.
- 각 섹션을 합친다.
- 최종 주소 배치와 relocation을 처리한다.
- `.exe`, `.dll`, `.lib`, `.pdb` 같은 산출물을 만든다.

Winters 관점:

```text
Engine.vcxproj
  -> WintersEngine.dll 생성

Server.vcxproj
  -> GameSim .obj + Server .obj + import libs
  -> WintersServer.exe 생성
  -> post-build에서 WintersEngine.dll과 ThirdParty DLL copy

Client.vcxproj
  -> Client .obj + 일부 Shared/GameSim .obj + import libs
  -> Client 실행 파일 생성
  -> shaders/resource/data copy
```

여기서 중요한 차이:

- `Shared/GameSim`은 독립 실행 파일이 아니다.
- Server/Client 프로젝트가 필요한 GameSim `.cpp`를 자기 프로젝트의 컴파일 대상으로 끌어오면, 그 프로젝트의 `.obj`와 함께 링크된다.
- `Engine`은 DLL로 빌드되고, Server/Client는 DLL을 로드해서 Engine 기능을 쓴다.

### 6. OS loader와 메모리 영역

링커가 만든 PE 파일을 실행하면 Windows loader가 프로세스를 만든다.

프로세스는 물리 RAM을 직접 보는 것이 아니라 가상 주소 공간을 본다. loader는 `.exe`와 필요한 `.dll`의 섹션을 가상 주소 공간에 매핑한다.

대표 영역:

| 영역 | 누가 만들고 언제 쓰나 | 예 |
|---|---|---|
| `.text` | loader가 실행 가능/읽기 전용으로 매핑 | `CMoveSystem::Execute` 기계어 |
| `.rdata` | loader가 읽기 전용으로 매핑 | 문자열 literal, const table |
| `.data` | loader가 초기값 있는 writable data로 매핑 | 초기값 있는 global/static |
| `.bss` | loader가 0으로 초기화된 writable data로 준비 | `static int count;` |
| stack | thread 생성 시 OS가 예약/커밋 | 함수 호출 frame, local variable 일부, return address |
| heap | runtime allocator가 관리 | `new`, `std::vector`, ECS component storage, network buffers |

자주 헷갈리는 점:

- local variable이 항상 stack에 있는 것은 아니다. 최적화되면 register에만 있을 수 있다.
- `std::vector` 객체 자체는 stack에 있을 수 있지만, 내부 buffer는 보통 heap에 있다.
- `.bss`는 "실행 파일에 0이 잔뜩 저장된다"기보다 "이 크기만큼 0으로 초기화해라"에 가깝다.
- CPU cache는 별도 프로그래밍 영역이 아니다. CPU가 메모리 접근을 빠르게 하기 위해 자동으로 쓰는 하드웨어 계층이다.

### 7. CPU에서 실제로 실행된다는 뜻

GameSim 함수가 실행된다는 것은 CPU가 `.text`에 있는 기계어 instruction을 fetch/decode/execute한다는 뜻이다.

아주 단순화하면:

```text
RIP/instruction pointer
  -> 다음 instruction 주소를 가리킴

instruction fetch
  -> L1 instruction cache에서 instruction 읽기

decode
  -> CPU 내부 micro-op으로 해석

execute
  -> ALU/FPU/SIMD/load-store unit이 실행

register/cache/RAM
  -> 데이터 읽고 쓰기
```

예를 들어 GameSim에서 위치를 갱신하는 코드는 개념적으로 다음 일을 한다.

```text
1. TransformComponent 주소를 register에 로드
2. move speed, direction, dt 값을 register/SIMD register에 로드
3. ALU/FPU가 pos += dir * speed * dt 계산
4. 결과를 cache line에 store
5. 나중에 cache가 RAM과 일관성을 맞춤
```

성능 관점:

- register 접근은 가장 빠르다.
- L1 cache hit는 빠르다.
- L2/L3는 더 느리다.
- RAM miss는 훨씬 느리다.
- 포인터를 여기저기 따라가는 구조보다, 같은 종류의 컴포넌트를 연속적으로 순회하는 ECS/data-oriented 구조가 cache에 유리하다.

GameSim이 서버 tick에서 많은 entity를 매 프레임 처리하므로, cache locality와 deterministic iteration이 중요해진다.

### 8. GPU는 어디서 등장하는가

GPU는 GameSim C++ 함수를 실행하지 않는다.

Server GameSim:

```text
GameCommand
  -> CPU에서 gameplay truth 계산
  -> Snapshot/Event 생성
  -> Network send
```

Client visual:

```text
Snapshot/Event receive
  -> Transform/animation/FX/UI state 갱신
  -> Renderer/RHI가 draw call, buffer, texture, shader state 준비
  -> graphics API/driver를 통해 GPU command 제출
  -> GPU가 vertex/pixel/compute shader 실행
```

PCIe는 주로 CPU와 GPU 사이의 command/resource 전송 경로로 이해하면 된다.

- CPU가 GPU command를 준비한다.
- texture, vertex buffer, constant buffer 같은 resource가 system memory에서 VRAM으로 upload될 수 있다.
- GPU는 VRAM에 있는 resource를 읽고 shader core에서 병렬 계산한다.
- 매 프레임 모든 데이터가 PCIe를 오가는 것은 아니다. 자주 쓰는 resource는 VRAM에 유지하고, 변하는 constant/dynamic buffer만 갱신하는 식으로 최적화한다.

면접에서 중요한 구분:

```text
GameSim은 CPU-side authoritative simulation이고,
GPU는 Client가 그 결과를 시각화하기 위해 제출한 rendering workload를 처리합니다.
둘을 섞어 말하면 server authority와 rendering responsibility가 흐려집니다.
```

### 9. Winters 서버 권위와 연결해서 말하기

Winters에서 GameSim을 설명할 때는 반드시 이 흐름을 붙인다.

```text
Client Input
  -> CommandSerializer
  -> Server Session/PacketDispatcher
  -> GameRoom pending command queue
  -> CommandExecutor
  -> Shared/GameSim Systems
  -> ReplicatedEvent / Snapshot
  -> Client EventApplier / SnapshotApplier
  -> VisualHook / Render / UI
```

서버가 소유하는 것:

- 위치, 이동, HP, mana
- damage/hit validation
- skill/cooldown/projectile
- minion/structure/bot AI 결과
- game end/state truth

클라이언트가 소유하는 것:

- input 전송
- 약한 prediction
- interpolation
- animation/FX playback
- rendering
- UI/debug

면접 표현:

```text
Winters에서는 GameSim을 Shared에 두지만, 권위 있는 실행은 Server GameRoom tick에서 일어납니다.
Client도 일부 shared definition이나 query를 포함할 수 있지만, gameplay truth는 서버 snapshot/event가 결정합니다.
Client는 그 결과를 받아 visual state로 재구성하고 GPU 렌더링으로 넘깁니다.
```

### 10. 30초 답변

```text
GameSim 폴더 자체가 실행되는 것은 아니고, Visual Studio의 .vcxproj에 등록된 GameSim .cpp들이
다른 .cpp와 똑같이 번역 단위로 컴파일됩니다. 전처리기가 include와 define을 펼치고,
컴파일러가 각 번역 단위를 .obj로 만들면 그 안에 .text, .rdata, .data, .bss 같은 섹션과
symbol/relocation 정보가 들어갑니다. 링커가 Server/Client의 다른 .obj와 라이브러리를 묶어
실행 파일을 만들고, Windows loader가 이를 프로세스 가상 메모리에 매핑합니다.
실행 중에는 CPU가 .text의 기계어를 register, ALU, cache, RAM을 통해 실행합니다.
GameSim은 CPU-side 서버 권위 시뮬레이션이고, GPU는 클라이언트가 snapshot/event를 visual로 바꿔
렌더 명령을 제출할 때 사용됩니다.
```

### 11. 2분 답변

```text
Winters 기준으로 보면 Shared/GameSim은 독립 프로그램이 아니라 shared gameplay source set입니다.
Visual Studio/MSBuild는 Winters.sln에서 Server.vcxproj와 Client.vcxproj를 읽고,
그 안의 ClCompile 항목에 등록된 .cpp를 각각 컴파일합니다.

각 .cpp는 include path와 preprocessor definition을 받아 전처리됩니다.
예를 들어 Debug/Release, DX11/DX12, Windows 관련 define에 따라 실제 컴파일되는 코드가 달라질 수 있습니다.
전처리 결과는 translation unit이고, 컴파일러는 이 단위마다 machine code와 data section,
symbol table, relocation을 가진 COFF .obj를 만듭니다.

그 다음 링커가 GameSim .obj, Server/Client .obj, Engine import library, system library를 합쳐
WintersServer.exe나 Client 실행 파일을 만듭니다. 실행 시 Windows loader가 PE 파일의 .text,
.rdata, .data, .bss를 프로세스 가상 주소 공간에 올리고, thread stack과 heap도 준비됩니다.

GameSim 함수가 돈다는 건 CPU가 .text의 instruction을 fetch/decode/execute한다는 뜻입니다.
위치나 체력 같은 component data는 register와 cache를 거쳐 읽고 쓰이고, cache miss가 나면 RAM까지 갑니다.
GPU는 이 시뮬레이션 코드를 직접 돌리지 않습니다. 서버가 만든 snapshot/event를 client가 받아
Transform, animation, FX state로 적용하고, renderer/RHI가 draw call과 resource를 GPU에 제출할 때
GPU shader core가 병렬로 렌더링 작업을 수행합니다.

그래서 Winters의 면접 핵심은 "GameSim은 CPU에서 도는 서버 권위 gameplay truth,
GPU는 client presentation"이라고 경계를 명확히 잡는 것입니다.
```

### 12. 심화 꼬리질문 답변 조각

질문: header도 컴파일되나요?

```text
헤더는 보통 독립적으로 .obj가 되지 않습니다. .cpp가 include할 때 전처리 결과에 포함됩니다.
다만 template/inline/constexpr 코드는 include된 번역 단위마다 인스턴스화될 수 있어서,
ODR과 중복 symbol 처리를 이해해야 합니다.
```

질문: `.bss`와 heap은 뭐가 다른가요?

```text
.bss는 0으로 초기화되는 전역/static 저장 영역입니다. 프로그램 로드 시 준비되고 수명은 프로세스 전체입니다.
heap은 런타임 allocator가 관리하는 동적 메모리입니다. std::vector의 내부 buffer나 new로 만든 객체가 보통 heap에 올라갑니다.
```

질문: stack과 register 중 local variable은 어디에 있나요?

```text
소스상 local variable이라고 반드시 stack에 있는 것은 아닙니다. 최적화에 따라 register에만 있을 수도 있고,
주소를 취하거나 spill되면 stack frame에 놓일 수 있습니다.
```

질문: CPU cache를 코드에서 직접 관리하나요?

```text
일반 C++ 코드는 cache를 직접 allocate하지 않습니다. CPU가 메모리 접근 패턴에 따라 cache line을 자동으로 가져옵니다.
엔진 코드는 data layout, iteration order, alignment, false sharing 회피로 cache hit 확률을 높이는 방식으로 제어합니다.
```

질문: GameSim을 GPU compute shader로 돌릴 수도 있나요?

```text
이론적으로 일부 대량 병렬 계산은 GPU compute로 옮길 수 있지만, 서버 권위 GameSim은 determinism,
분기 많은 gameplay logic, 디버깅, 네트워크 tick 일관성이 중요해서 CPU가 기본입니다.
GPU는 렌더링, particle, culling, postprocess처럼 대량 병렬이고 presentation 중심인 작업에 더 적합합니다.
```

질문: Client도 GameSim 코드를 include하면 client가 truth를 가진 것 아닌가요?

```text
아닙니다. 같은 shared type/function을 include할 수 있다는 것과 권위 있게 실행한다는 것은 다릅니다.
Winters의 원칙은 gameplay truth가 Server GameSim에서 결정되고, Client는 snapshot/event를 받아 presentation에 반영하는 것입니다.
```

### 13. 실습/검증 명령

Session 02에서 실제로 확인할 명령들:

```powershell
# 프로젝트에 등록된 GameSim 컴파일 항목 찾기
rg -n "Shared\\GameSim|Shared/GameSim|GameSim" Client/Include/Client.vcxproj Server/Include/Server.vcxproj

# 현재 디스크의 실제 GameSim cpp 목록 보기
rg --files Shared/GameSim -g "*.cpp"

# Server Debug 빌드
msbuild Winters.sln /t:Server /p:Configuration=Debug /p:Platform=x64

# 생성된 obj 확인
Get-ChildItem Server/Bin/Intermediate/Debug -Filter *.obj

# Visual Studio Developer Prompt에서 obj/section 보기
dumpbin /headers Server/Bin/Intermediate/Debug/MoveSystem.obj

# symbol 보기
dumpbin /symbols Server/Bin/Intermediate/Debug/MoveSystem.obj
```

주의:

```text
현재 작업 트리에는 GameSim 파일 재배치가 진행 중이라, 위 빌드 명령은 먼저 .vcxproj 경로가 실제 파일 위치와 맞는지 확인한 뒤 실행해야 한다.
```

---

## 다음 세션 예고

다음은 Session 02로 간다.

목표:

```text
Winters.sln / Server.vcxproj / Client.vcxproj를 기준으로
현재 GameSim 파일들이 어떤 프로젝트에 어떻게 등록되어 있고,
어떤 .obj 이름으로 어느 Intermediate 폴더에 나오는지 확인한다.
```

면접 결과물:

```text
"저는 Visual Studio 프로젝트 파일을 직접 열어서 Shared/GameSim 소스가
Server/Client 빌드 그래프에 어떻게 들어가는지 확인했고,
빌드 산출물인 .obj와 최종 exe/dll 흐름까지 설명할 수 있습니다."
```
