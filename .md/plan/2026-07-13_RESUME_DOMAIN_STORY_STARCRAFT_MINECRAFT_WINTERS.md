Session - Starcraft, SR_MinecraftDungeons, Winters Engine의 실제 코드와 설계 경험을 이력서 문장, 포트폴리오 설명, 면접 답변으로 환전한다.

# 0. 결론

이 문서의 본체는 세 프로젝트를 따로 자랑하는 것이 아니라, 같은 사람이 같은 문제를 더 높은 추상화로 계속 해결해온 진화 서사를 정리하는 것이다.

```text
Starcraft
-> OOP 기반 2D RTS 도메인 구현
-> CObj 상속, Object Manager, Factory, deque 생산 큐, Command UI, Selection, Portrait, Minimap

SR_MinecraftDungeons
-> DirectX9 3D 엔진/게임/툴/서버 분리
-> CBase 수동 ref-count, GameObject/Component/Prototype, Client/Server/Shared/UI_EDITOR

Winters Engine
-> C++ 자체 게임 엔진과 서버 권위 MOBA 런타임
-> ECS, shared_ptr/unique_ptr 기반 lifetime, GameSim, Snapshot/Event, RHI, Tools, AIResearch, Services/Docker
```

이력서에서 가장 강한 문장은 다음이다.

```text
WinAPI/GDI 기반 Starcraft 모작에서 CObj 상속 구조, Object Manager, Factory, deque 생산 큐, Command Card UI를 직접 구현했고, 이후 DirectX9 3D 프로젝트에서 수동 Reference Count 기반 Engine/Client/Server/Tool 구조로 확장한 뒤, Winters Engine에서는 ECS, 스마트 포인터, 서버 권위 GameSim, Snapshot/Event 복제, RHI 추상화, 데이터/툴/백엔드 파이프라인까지 포함한 자체 C++ 게임 엔진 아키텍처로 발전시켰습니다.
```

짧게 말하면:

```text
처음에는 객체를 잘 만드는 법을 배웠고,
그다음에는 객체를 안전하게 소유하고 복제하고 툴과 서버로 나누는 법을 배웠고,
지금은 게임의 진실, 렌더링, 데이터, 툴, 백엔드, AI를 서로 다른 도메인으로 분리해서 엔진으로 구성하는 법을 설계하고 있습니다.
```

# 1. 현재 코드 근거

## 1.1 Starcraft

근거 폴더:

```text
C:\Users\user\Desktop\스타크래프트\Starcraft
```

확인한 핵심 파일:

```text
C:\Users\user\Desktop\스타크래프트\Starcraft\Starcraft\CObj.h
C:\Users\user\Desktop\스타크래프트\Starcraft\Starcraft\CObjMgr.h
C:\Users\user\Desktop\스타크래프트\Starcraft\Starcraft\CObjMgr.cpp
C:\Users\user\Desktop\스타크래프트\Starcraft\Starcraft\CAbstractFactory.h
C:\Users\user\Desktop\스타크래프트\Starcraft\Starcraft\CUIMgr.h
C:\Users\user\Desktop\스타크래프트\Starcraft\Starcraft\CUIMgr.cpp
C:\Users\user\Desktop\스타크래프트\Starcraft\Starcraft\CPortraitMgr.h
C:\Users\user\Desktop\스타크래프트\Starcraft\Starcraft\CSelectionMgr.h
C:\Users\user\Desktop\스타크래프트\Starcraft\Starcraft\Commandable.h
C:\Users\user\Desktop\스타크래프트\Starcraft\Starcraft\CommandSlot.h
```

핵심 증거:

- `CObj`는 `Initialize`, `Update`, `Late_Update`, `Render`, `Release`를 강제하는 최상위 객체 추상화다.
- `CObjMgr`는 `OBJID`별 객체 리스트, 유닛/건물/적 보조 벡터, 렌더 리스트, pending delete, 선택/오더 정리까지 담당한다.
- `CAbstractFactory<T>`는 템플릿 기반 객체 생성과 초기화를 통합한다.
- `CBuildingFactory::Create(eBuildingType)`는 건물 타입 enum을 실제 건물 클래스 생성으로 매핑한다.
- 생산/업그레이드 건물은 `std::deque` 큐를 사용해 `push_back`, `front`, `pop_front`, `UpdateProduction`, `ProductionComplete` 흐름을 가진다.
- `Commandable`, `CommandSlot`, `CUIMgr`가 command card UI와 실제 유닛/건물 행동 실행을 분리한다.
- `CSelectionMgr`는 단일/다중 선택, 컨트롤 그룹, 우클릭 smart command를 담당한다.
- `CPortraitMgr`는 UI portrait animation을 별도 도메인으로 분리한다.

## 1.2 SR_MinecraftDungeons

근거 폴더:

```text
C:\Users\user\Desktop\SR_MinecraftDungeons\SR_Minecraft_Dungeons
```

확인한 핵심 파일:

```text
C:\Users\user\Desktop\SR_MinecraftDungeons\SR_Minecraft_Dungeons\Base\CBase.h
C:\Users\user\Desktop\SR_MinecraftDungeons\SR_Minecraft_Dungeons\Base\CBase.inl
C:\Users\user\Desktop\SR_MinecraftDungeons\SR_Minecraft_Dungeons\Reference\Header\CGameObject.h
C:\Users\user\Desktop\SR_MinecraftDungeons\SR_Minecraft_Dungeons\Reference\Header\CComponent.h
C:\Users\user\Desktop\SR_MinecraftDungeons\SR_Minecraft_Dungeons\Reference\Header\CProtoMgr.h
C:\Users\user\Desktop\SR_MinecraftDungeons\SR_Minecraft_Dungeons\Reference\Header\CManagement.h
C:\Users\user\Desktop\SR_MinecraftDungeons\SR_Minecraft_Dungeons\Server\CServer.h
C:\Users\user\Desktop\SR_MinecraftDungeons\SR_Minecraft_Dungeons\Shared\PacketDef.h
```

핵심 증거:

- `CBase`는 `AddRef`, `Release`, `Free`로 수동 reference count와 객체 생명주기를 관리한다.
- `CGameObject`는 `CBase`를 상속하고 `CComponent` multimap을 가진다.
- `CComponent`는 `Clone()`을 통해 Prototype 패턴 기반 복제를 지원한다.
- `CProtoMgr`는 prototype component를 등록하고 `Clone_Prototype`으로 복제한다.
- `CManagement`는 Scene 교체, update, late update, render를 관리한다.
- 프로젝트 구조가 `Base`, `Engine`, `Client`, `Server`, `Shared`, `UI_EDITOR`로 나뉘어 있다.
- `PacketDef.h`는 `PKT_HEADER`, `C2S_INPUT`, `S2C_STATE_SNAPSHOT`, `C2S_ARROW`, `S2C_DAMAGE` 등 명시적 네트워크 패킷 스키마를 가진다.
- `CServer`는 accept/recv thread, session, login/input/projectile/damage/dragon sync 처리 흐름을 가진다.

## 1.3 Winters Engine

근거 폴더:

```text
C:\Users\user\Desktop\Winters
```

확인한 핵심 파일/영역:

```text
C:\Users\user\Desktop\Winters\EngineSDK\inc\ECS\World.h
C:\Users\user\Desktop\Winters\Shared\Schemas\Snapshot.fbs
C:\Users\user\Desktop\Winters\Engine\Public\RHI\IRHIDevice.h
C:\Users\user\Desktop\Winters\Engine\Private\RHI\DX11\CDX11Device.cpp
C:\Users\user\Desktop\Winters\Engine\Private\RHI\DX12\DX12Device.cpp
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\CommandExecutor\CommandExecutor.cpp
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\ChampionAI\ChampionAISystem.cpp
C:\Users\user\Desktop\Winters\Server\Private\Game\GameRoomChampionAI.cpp
C:\Users\user\Desktop\Winters\Client\Public\UI\MinimapPanel.h
C:\Users\user\Desktop\Winters\Client\Public\UI\AIDebugPanel.h
C:\Users\user\Desktop\Winters\Client\Public\UI\WfxEffectToolPanel.h
C:\Users\user\Desktop\Winters\Tools\AIResearch
C:\Users\user\Desktop\Winters\Tools\WintersAssetConverter
C:\Users\user\Desktop\Winters\Services\docker-compose.yml
```

핵심 증거:

- `CWorld`는 Entity 생성/파괴, `EntityHandle`, typed component store, `ForEach<T>`, `ForEach<T1,T2>`, spatial index를 제공한다.
- `Snapshot.fbs`는 서버 tick, entity snapshot, stat, inventory, action, AI debug trace, Chrono metadata까지 복제 계약을 정의한다.
- `Engine/Public/RHI`와 `Engine/Private/RHI/DX11`, `DX12`가 backend-neutral RHI 방향을 만든다.
- `Shared/GameSim`은 `GameCommand`, CommandExecutor, ChampionAI, deterministic simulation의 핵심 도메인이다.
- `Server`는 bot AI도 직접 truth를 바꾸지 않고 `GameCommand` 생산자로 다룬다.
- `Client`는 minimap, AI debug, WFX effect tool 등 presentation/tool UI를 가진다.
- `Services/docker-compose.yml`은 Postgres, Redis, Kafka를 local backend state 실험 환경으로 구성한다.

# 2. 이력서 상단 요약 문장

## 2.1 긴 버전

```text
C++ 기반 게임 시스템 개발을 중심으로 WinAPI/GDI 2D RTS 모작, DirectX9 3D 액션 프로젝트, 자체 게임 엔진 Winters Engine까지 단계적으로 확장해왔습니다. Starcraft 모작에서는 CObj 기반 객체 계층, Object Manager, Factory 패턴, deque 생산 큐, Command Card UI, Selection/Minimap/Portrait/Hotkey 분리를 구현했고, Minecraft Dungeons 모작에서는 DirectX9 엔진 구조, GameObject/Component/Prototype, 수동 Reference Count, Client/Server/Shared/UI Editor 분리를 경험했습니다. Winters Engine에서는 스마트 포인터와 ECS 기반 lifetime 구조, 서버 권위 GameSim, Snapshot/Event 복제, RHI(DX11/DX12/Vulkan 확장 방향), 데이터/툴/백엔드/Docker/AIResearch 파이프라인까지 포함한 엔진 아키텍처를 설계하고 있습니다.
```

## 2.2 짧은 버전

```text
WinAPI/GDI 2D RTS에서 Object Manager/Factory/Command UI를 구현한 경험을 바탕으로, DirectX9 3D 엔진의 수동 ref-count/Component/Prototype 구조를 거쳐, Winters Engine에서 ECS, 서버 권위 GameSim, Snapshot 복제, RHI, 데이터/툴/백엔드 파이프라인을 설계했습니다.
```

## 2.3 더 공격적인 버전

```text
게임 객체를 상속 구조로 관리하던 초기 OOP 엔진에서 출발해, 수동 Reference Count 기반 DirectX9 엔진을 거쳐, 현재는 ECS, 서버 권위 시뮬레이션, RHI, 데이터 주도 설계, 툴체인, 백엔드 인프라를 분리한 자체 C++ 게임 엔진을 구축하고 있습니다.
```

# 3. Starcraft 프로젝트

## 3.1 이력서 bullet

```text
- WinAPI/GDI 기반 Starcraft 모작에서 CObj 최상위 추상 클래스, Object Manager, Factory 패턴, 생산/업그레이드 deque 큐, Command Card UI, Selection/Control Group/Portrait/Minimap/Hotkey 시스템을 구현했습니다.
- CAbstractFactory<T>와 CBuildingFactory를 통해 유닛/건물 생성 경로를 통합하고, CObjMgr에서 객체 update, late update, render ordering, dead object cleanup, selection/order invalidation을 관리했습니다.
- 생산 건물과 업그레이드 건물의 command slot, hotkey, resource check, production queue, completion callback을 분리해 RTS의 명령 입력과 결과 생성을 구조화했습니다.
```

## 3.2 핵심 도메인

Starcraft의 본질은 실시간 전략 게임에서 수많은 객체가 동시에 존재하고, 사용자의 선택과 명령이 객체들의 상태 변화로 이어지는 구조를 구현하는 것이다.

```text
Input
-> Selection
-> Command Card
-> Commandable::ExecuteCommand
-> Unit/Building Order or Production Queue
-> Object Manager Update
-> Render/UI/Portrait/Minimap
```

여기서 가장 중요한 도메인은 다음이다.

```text
Object Domain
-> 모든 유닛, 건물, 투사체, 자원, UI 객체의 공통 생명주기

Command Domain
-> move, attack, hold, gather, build, train, upgrade 같은 명령 표현

Production Domain
-> 건물이 생산/업그레이드를 큐에 쌓고 시간이 지나면 결과를 생성

UI Domain
-> command card, portrait, minimap, selection feedback, hotkey 표시

Resource/Tech Domain
-> mineral, gas, building dependency, upgrade unlock, race-specific command
```

## 3.3 CObj 상속 구조

`CObj`는 모든 게임 객체의 공통 인터페이스다.

```text
CObj
-> CUnit
-> CBuilding
-> CProjectile
-> CResource
-> CButton/UI object
```

`CObj`가 강제한 함수:

```text
Initialize()
Update()
Late_Update()
Render(HDC)
Release()
```

이 구조의 장점:

- 모든 객체를 같은 update/render 루프에 올릴 수 있다.
- `CObjMgr`가 `CObj*`만 들고도 다양한 객체를 관리할 수 있다.
- `OBJID`별 리스트로 유닛, 건물, 적, 투사체, UI를 나눠 처리할 수 있다.
- 초기 게임 구조를 빠르게 구축하기 쉽다.

이 구조의 한계:

- 모든 객체가 같은 base class에 묶이므로 시간이 지날수록 base class가 비대해지기 쉽다.
- 유닛만 필요한 필드, 건물만 필요한 필드가 상위 클래스로 올라갈 위험이 있다.
- `dynamic_cast`와 manager singleton이 늘어나면 의존성이 복잡해진다.
- 데이터가 객체별로 흩어져 cache locality가 떨어질 수 있다.

면접에서 말할 문장:

```text
초기 RTS 모작에서는 모든 게임 객체를 CObj로 추상화해 update/render/lifetime을 통합했습니다. 이 방식은 구현 속도와 구조 이해에는 좋았지만, 객체 종류가 많아질수록 상속 계층과 manager 의존성이 커졌고, 이 한계를 Winters Engine의 ECS 설계로 이어서 해결하려고 했습니다.
```

## 3.4 Object Manager

`CObjMgr`의 역할은 단순한 vector/list 보관이 아니다.

```text
Add_Object
-> OBJID별 list에 등록
-> 유닛/건물/적 보조 vector에 캐싱

Update
-> 모든 객체 Update 호출
-> DEAD 반환 객체를 list에서 제거
-> 바로 delete하지 않고 pending delete로 이동

Late_Update
-> Late_Update 호출
-> RenderID별 render list 등록

Render
-> render list 정렬
-> 객체별 Render 호출

CleanUpDeadObject
-> selection/control group/order/projectile target에서 죽은 객체 제거
-> Safe_Delete
```

중요한 설계 포인트:

- 죽은 객체를 update 도중 바로 delete하지 않고 pending delete로 넘긴다.
- 죽은 객체가 selection, control group, projectile target, unit order에 남아 있으면 dangling pointer가 된다.
- 그래서 cleanup 단계에서 참조 제거를 한 번에 처리한다.

이력서 문장:

```text
Object Manager에서 객체 등록, update/render ordering, pending delete, selection/control group/order invalidation을 통합해 RTS 객체 생명주기를 관리했습니다.
```

면접 설명:

```text
RTS에서는 죽은 유닛이 단순히 화면에서 사라지는 것으로 끝나지 않습니다. 선택 목록, 컨트롤 그룹, 공격 명령, 투사체 타겟, UI 표시에서 모두 제거되어야 합니다. 그래서 CObjMgr에서 dead object를 바로 delete하지 않고 pending delete로 모은 뒤, 보조 리스트와 명령 참조를 정리하고 마지막에 해제하는 구조를 만들었습니다.
```

## 3.5 Factory 패턴

Starcraft에는 두 종류의 Factory 흐름이 있다.

```text
CAbstractFactory<T>
-> new T
-> Initialize
-> Set_Pos / Set_Size
-> CObj* 반환

CBuildingFactory::Create(eBuildingType)
-> enum 기반 switch
-> 실제 건물 클래스 생성
-> Initialize
-> SetBuildingData
-> CBuilding* 반환
```

이 구조의 목적:

- 객체 생성 코드를 한 곳으로 모은다.
- 유닛/건물 생산 로직이 구체 클래스의 생성자를 직접 알지 않아도 된다.
- 생산 완료 시점에 `CAbstractFactory<CMarine>::Create(...)` 같은 방식으로 결과물을 만들 수 있다.
- 건물 타입 enum과 실제 클래스의 대응을 명시적으로 관리할 수 있다.

이력서 문장:

```text
템플릿 기반 CAbstractFactory와 enum 기반 CBuildingFactory를 구성해 유닛/건물 생성 경로를 통합하고, 생산 완료 로직에서 일관된 객체 생성 API를 사용했습니다.
```

면접 설명:

```text
Factory 패턴을 쓴 이유는 생산 건물마다 new Marine, new Tank 같은 생성 코드가 흩어지는 것을 막기 위해서였습니다. 생성 직후 Initialize, 위치 설정, 건물 데이터 초기화가 반복되기 때문에, 생성과 초기화 규칙을 factory에 모아 생산 시스템과 객체 생성 시스템을 분리했습니다.
```

## 3.6 deque 생산 큐

생산 건물과 업그레이드 건물은 `std::deque`를 사용한다.

개념 흐름:

```text
CommandSlot 선택 또는 hotkey 입력
-> 자원/조건 확인
-> m_queue.push_back(ProductionItem)
-> 매 tick UpdateProduction
-> m_queue.front().remainTime -= dt
-> remainTime <= 0
-> done = m_queue.front().command
-> m_queue.pop_front()
-> ProductionComplete(done)
```

왜 `deque`인가:

- 뒤에 생산 명령을 추가해야 한다.
- 앞에서 완료된 명령을 제거해야 한다.
- RTS 생산 큐는 FIFO 구조다.
- vector로도 가능하지만 앞 제거가 잦다면 deque/list가 더 자연스럽다.

이력서 문장:

```text
생산 건물과 업그레이드 건물에 deque 기반 FIFO 생산 큐를 적용해 command 입력, 생산 시간 진행, 완료 callback, 유닛/업그레이드 결과 생성을 분리했습니다.
```

면접 설명:

```text
생산 명령은 즉시 유닛을 만드는 것이 아니라 시간과 자원을 소비하는 예약 작업입니다. 그래서 command 입력 시점에는 queue에 작업을 넣고, update loop에서 front 작업의 남은 시간을 감소시키며, 완료 시점에 factory로 실제 유닛을 생성하거나 upgrade flag를 켜는 구조로 만들었습니다.
```

## 3.7 UI Manager와 Commandable 분리

`CUIMgr`는 선택된 객체를 직접 모두 알지 않고, `Commandable` 인터페이스를 통해 command slot을 가져온다.

```text
SelectionMgr
-> selected CObj
-> dynamic_cast<Commandable*>
-> CommandCardSlot(outSlot)
-> CUIMgr::RenderCommandSlots
-> race-specific icon rendering
```

이 구조의 의미:

- UI는 "어떤 버튼을 보여줄지"를 객체에게 묻는다.
- 유닛/건물은 자기 command slot을 스스로 정의한다.
- UI는 Terran/Zerg/Protoss 아이콘 렌더링만 담당한다.
- 실제 명령 실행은 `ExecuteCommand`로 분리된다.

이력서 문장:

```text
Commandable 인터페이스와 CommandSlot 구조체를 통해 유닛/건물별 command card 데이터를 UI Manager와 분리하고, 종족별 아이콘 렌더링과 hotkey feedback을 별도 처리했습니다.
```

면접 설명:

```text
처음에는 UI가 모든 유닛 타입을 switch로 알 수 있지만, 그렇게 하면 유닛이 늘어날수록 UI가 게임 로직을 침범합니다. 그래서 객체는 자신의 command slot을 제공하고, UI는 그 slot을 렌더링하는 방식으로 나눴습니다. 이 경험이 Winters에서 Client UI가 GameSim truth를 직접 소유하지 않는 방향으로 이어졌습니다.
```

## 3.8 Selection, Hotkey, Portrait, Minimap

RTS UI는 단순 HUD가 아니라 입력과 상태 확인의 중심이다.

구현 도메인:

```text
SelectionMgr
-> drag selection
-> single selection
-> same type selection
-> control group save/load
-> right click smart command

CUIMgr
-> selected object UI info
-> command slot rendering
-> icon state
-> button feedback

CPortraitMgr
-> selected unit portrait animation

Minimap
-> world overview
-> camera/object relationship visualization
```

이력서 문장:

```text
RTS 조작감을 위해 drag selection, control group, smart command, command card hotkey feedback, portrait animation, minimap 표시를 각각 별도 manager/domain으로 분리했습니다.
```

면접 설명:

```text
RTS에서는 객체 로직만큼 UI 도메인이 중요합니다. 유닛을 선택하고, 명령을 보고, 단축키로 실행하고, 미니맵과 portrait로 상태를 확인하는 전체 흐름이 게임의 조작감입니다. 그래서 selection, command card, portrait, minimap을 별도 manager로 나누어 구현했습니다.
```

# 4. SR_MinecraftDungeons 프로젝트

## 4.1 이력서 bullet

```text
- DirectX9 기반 Minecraft Dungeons 모작에서 Base/Engine/Client/Server/Shared/UI_EDITOR 구조를 구성하고, CBase 수동 reference count, GameObject/Component, Prototype/Clone, Scene Management, 패킷 기반 멀티플레이 서버 구조를 구현했습니다.
- CBase의 AddRef/Release/Free 패턴을 통해 COM 스타일 lifetime을 직접 관리했고, 이후 Winters Engine에서는 shared_ptr/unique_ptr 기반 RAII와 명확한 ownership 구조로 발전시켰습니다.
- Shared PacketDef 기반으로 login, input, snapshot, despawn, arrow, flame, damage, boss sync 패킷을 명시적으로 정의하고, server thread/session/game loop 흐름을 구성했습니다.
```

## 4.2 핵심 도메인

SR_MinecraftDungeons의 본질은 2D RTS 객체 관리에서 3D 엔진형 구조로 넘어간 것이다.

```text
Base
-> 모든 엔진 객체의 lifetime 규칙

Engine
-> D3D device, component, prototype, scene, renderer, collider, camera

Client
-> 실제 게임 객체, player, monster, boss, UI, inventory, world/chunk

Server
-> session, game loop, packet handling, state broadcast

Shared
-> packet schema, protocol contract

UI_EDITOR
-> ImGui 기반 UI/editor 실험
```

## 4.3 CBase와 수동 Reference Count

`CBase`는 수동 lifetime 학습의 핵심이다.

```text
CBase
-> m_dwRefCnt
-> AddRef()
-> Release()
-> Free()
-> delete this
```

이 구조의 의미:

- DirectX/COM 스타일의 생명주기 관리 방식이다.
- 객체를 여러 시스템이 참조할 때 언제 해제되는지 직접 제어한다.
- `Safe_Release` 패턴과 잘 맞는다.
- 하지만 ref count 증감 누락, 순환 참조, dangling pointer, release 순서 문제가 생기기 쉽다.

이력서 문장:

```text
DirectX9 프로젝트에서 CBase 기반 수동 Reference Count와 Safe_Release 패턴을 사용해 엔진 객체 생명주기를 직접 관리했습니다.
```

면접 설명:

```text
CBase의 AddRef/Release 구조를 통해 객체 소유권과 해제 타이밍을 직접 다뤄봤습니다. 이 경험 덕분에 shared_ptr이 단순히 편한 도구가 아니라, 소유권을 타입으로 표현하고 release 누락을 줄이기 위한 RAII 도구라는 점을 체감했습니다.
```

## 4.4 GameObject/Component/Prototype

DirectX9 프로젝트의 엔진 구조:

```text
CGameObject : CBase
-> component map
-> Ready_GameObject
-> Update_GameObject
-> LateUpdate_GameObject
-> Render_GameObject

CComponent : CBase
-> Clone()
-> Update_Component
-> LateUpdate_Component

CProtoMgr
-> Ready_Prototype(tag, component)
-> Clone_Prototype(tag)
```

이 구조의 의미:

- `GameObject`는 행위의 껍데기다.
- `Component`는 transform, texture, collider, buffer 같은 기능 단위다.
- `Prototype`은 매번 새로 로드하지 않고 원본을 등록한 뒤 clone으로 복제하는 구조다.
- Unity의 Prefab/Component 구조와도 연결되는 사고다.

이력서 문장:

```text
GameObject/Component/Prototype 구조를 통해 Transform, Texture, Collider, Mesh Buffer 같은 기능을 조합하고, Clone_Prototype 기반으로 반복 생성 비용과 초기화 중복을 줄였습니다.
```

면접 설명:

```text
Starcraft에서는 CObj 상속으로 모든 객체를 표현했지만, Minecraft Dungeons에서는 기능을 Component로 쪼개는 방식을 경험했습니다. Prototype Manager에 원형 component를 등록하고 clone해서 쓰는 구조는 Unity prefab, Unreal CDO/asset archetype과도 연결되는 개념이라고 이해하고 있습니다.
```

## 4.5 Client/Server/Shared 분리

`PacketDef.h`는 shared contract다.

```text
PKT_HEADER
-> wSize
-> wType

C2S
-> Login
-> Input
-> Arrow
-> Flame
-> Damage
-> DragonSync

S2C
-> LoginAck
-> Spawn
-> StateSnapshot
-> Despawn
-> Arrow
-> Damage
-> Boss/Dragon sync
```

이 구조의 의미:

- Client와 Server가 같은 패킷 정의를 공유한다.
- network boundary에서 구조체 크기를 `static_assert`로 검증한다.
- snapshot은 서버가 가진 상태를 클라이언트에게 전달하는 핵심 구조다.
- 이 경험이 Winters의 FlatBuffers Snapshot/Event 구조로 발전한다.

이력서 문장:

```text
Shared 패킷 정의를 통해 Client/Server 통신 계약을 명시하고, 입력, 투사체, 피해, 스폰, 상태 스냅샷, despawn 패킷을 기반으로 멀티플레이 동기화 구조를 구현했습니다.
```

면접 설명:

```text
이 프로젝트에서 client와 server를 나누면서, 같은 게임 객체라도 네트워크 경계에서는 구조체 계약으로 바뀐다는 것을 배웠습니다. 이후 Winters에서는 이 아이디어가 FlatBuffers Snapshot/Event, GameCommand, 서버 권위 GameSim으로 발전했습니다.
```

## 4.6 UI_EDITOR와 Tool 분리

SR_MinecraftDungeons에는 `UI_EDITOR` 폴더가 따로 있다.

의미:

- 런타임 게임과 툴을 분리하려는 초기 시도다.
- ImGui, D3DDevice, Window/Application/Editor 구조가 있다.
- 툴은 게임을 직접 망가뜨리는 코드가 아니라 asset/layout/data를 편집하는 별도 프로그램이어야 한다.

이력서 문장:

```text
UI_EDITOR를 별도 프로젝트로 구성해 runtime client와 editor/tool domain을 분리하고, ImGui 기반 편집 UI와 texture/resource 관리 흐름을 실험했습니다.
```

면접 설명:

```text
처음에는 게임 안에 debug UI를 넣는 정도였지만, UI_EDITOR를 따로 만들면서 툴은 런타임과 다른 생명주기와 데이터 소유권을 가진다는 걸 배웠습니다. Winters에서는 이 경험이 WFX Effect Tool, Model/Anim Panel, AI Debug Panel, Asset Converter 같은 툴 구조로 이어졌습니다.
```

# 5. Winters Engine

## 5.1 이력서 bullet

```text
- C++ 자체 게임 엔진 Winters Engine에서 Engine/Client/Server/Shared(GameSim)/Tools/Services를 분리하고, 서버 권위 MOBA 시뮬레이션, GameCommand, Snapshot/Event 복제, ECS, RHI, 데이터 주도 gameplay/visual pack, AIResearch, Docker 기반 backend state 실험 환경을 설계했습니다.
- 기존 OOP 기반 객체 관리의 한계를 해결하기 위해 Entity/Component/Typed Store/ForEach 기반 ECS를 구성하고, cache locality와 system-oriented update를 고려한 구조로 발전시켰습니다.
- DX11 기반 렌더링 경로를 유지하면서 IRHIDevice/CommandList/Pipeline/Resource Handle 등 RHI 계층을 도입해 DX12/Vulkan/console/mobile 확장 가능한 renderer boundary를 설계했습니다.
- 클라이언트를 신뢰하지 않는 dedicated server authoritative 구조를 바탕으로 Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual 흐름을 유지했습니다.
```

## 5.2 핵심 도메인

Winters Engine의 본질은 "게임 하나"가 아니라 여러 도메인의 경계를 명확히 나눈 엔진이다.

```text
Engine
-> frame loop, renderer, RHI, resource, ECS primitive, UI primitive, asset runtime

Shared/GameSim
-> deterministic gameplay truth, GameCommand, component, system, snapshot schema

Server
-> client input 검증, authoritative GameSim 실행, bot command 생산, Snapshot/Event broadcast

Client
-> input, camera, presentation, interpolation, weak prediction, animation, FX, HUD, debug UI

Tools
-> asset converter, effect editor, model/animation/debug panels, AI research probes

Services
-> account, matchmaking, profile, store, entitlement, telemetry/live ops backend state

Data
-> ClientPublic visual data, ServerPrivate gameplay data, SharedContract identity/manifest

Docker
-> Postgres, Redis, Kafka local backend infrastructure
```

## 5.3 서버 권위 GameSim

가장 중요한 문장:

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual
```

이 구조의 의미:

- 클라이언트는 원하는 행동을 요청한다.
- 서버는 그 요청이 가능한지 검증한다.
- 서버 GameSim이 위치, 체력, 쿨타임, 피해, 상태이상, 골드, 경험치의 진실을 바꾼다.
- 클라이언트는 서버 snapshot/event를 받아 화면에 보여준다.
- bot AI도 truth를 직접 바꾸지 않고 `GameCommand`를 생산한다.

이력서 문장:

```text
클라이언트를 신뢰하지 않는 dedicated server authoritative 구조를 설계해, 입력은 GameCommand로 검증하고 위치/전투/쿨타임/상태이상/경제 정보의 source of truth를 Server GameSim에 집중시켰습니다.
```

면접 설명:

```text
멀티플레이 게임에서 클라이언트가 체력이나 위치를 직접 결정하면 치트와 동기화 문제가 생깁니다. 그래서 Winters에서는 클라이언트 입력을 GameCommand로 보내고, 서버 GameSim이 검증과 실행을 담당합니다. 클라이언트는 snapshot/event를 받아 visual state, animation, FX, UI로 보여주는 구조입니다.
```

## 5.4 ECS로의 진화

Starcraft의 구조:

```text
CObj
-> CUnit
-> CMarine
-> CGhost
-> CBuilding
-> CBarracks
```

Winters의 구조:

```text
EntityID
-> TransformComponent
-> StatComponent
-> SkillComponent
-> TeamComponent
-> VisibilityComponent
-> NavAgentComponent

System
-> MoveSystem
-> CombatSystem
-> SkillSystem
-> VisionSystem
-> ChampionAISystem
```

왜 ECS인가:

- 객체 상속 대신 데이터 조합으로 entity를 표현한다.
- system이 필요한 component만 순회할 수 있다.
- 같은 타입 component를 store에 모으면 cache locality가 좋아진다.
- entity lifetime과 component lifetime을 분리할 수 있다.
- 서버 시뮬레이션, replay, snapshot, rollback, debug trace에 유리하다.

이력서 문장:

```text
OOP 상속 기반 객체 구조의 확장성과 cache locality 한계를 해결하기 위해 EntityID, typed ComponentStore, ForEach 기반 ECS 구조를 설계하고, gameplay system이 필요한 component 조합만 순회하도록 구성했습니다.
```

면접 설명:

```text
Starcraft에서는 CObj 상속 구조가 직관적이었지만, 챔피언, 미니언, 투사체, 구조물, 이펙트 앵커가 늘어나면 상속 계층이 복잡해집니다. Winters에서는 Entity는 identity만 갖고, 데이터는 component에 두고, logic은 system이 순회하는 구조로 바꾸었습니다. 이 구조가 서버 권위 GameSim, snapshot, replay, AI debug에도 더 잘 맞습니다.
```

## 5.5 shared_ptr/unique_ptr와 lifetime

SR_MinecraftDungeons의 수동 ref-count:

```text
CBase::AddRef()
CBase::Release()
Free()
delete this
```

Winters의 방향:

```text
unique_ptr
-> 단일 소유권

shared_ptr
-> 공유 소유권

weak_ptr
-> 생명주기 연장 없는 관찰자

EntityHandle
-> process-local entity lifetime identity
```

이력서 문장:

```text
DirectX9 프로젝트에서 수동 Reference Count를 직접 구현한 경험을 바탕으로, Winters Engine에서는 unique_ptr/shared_ptr/weak_ptr와 EntityHandle을 사용해 소유권, 관찰, 엔티티 생존 여부를 더 명확히 표현했습니다.
```

면접 설명:

```text
수동 ref-count를 써보면 AddRef/Release 누락이 얼마나 위험한지 알게 됩니다. 그래서 Winters에서는 C++ RAII와 스마트 포인터를 기본으로 두고, entity는 pointer 자체보다 EntityHandle과 generation을 통해 생존 여부를 확인하는 방식으로 관리합니다.
```

## 5.6 RHI

Winters의 RHI 목표:

```text
Game/Renderer code
-> IRHIDevice
-> IRHICommandList
-> IRHIPipelineState
-> IRHIBindGroup
-> RHIResourceHandle
-> DX11 backend / DX12 backend / future Vulkan backend
```

왜 RHI가 필요한가:

- 렌더러가 `ID3D11Device`에 직접 묶이면 DX12/Vulkan/console/mobile로 갈 수 없다.
- backend concrete type을 private adapter에 가둬야 한다.
- 렌더러는 "무엇을 그릴지"를 알고, backend는 "해당 API로 어떻게 제출할지"를 안다.
- Unreal의 RHI/RenderCore/Renderer 분리와 연결되는 구조다.

이력서 문장:

```text
DX11 렌더링 경로를 유지하면서 IRHIDevice, command list, pipeline state, bind group, resource handle 기반 RHI 계층을 설계해 DX12/Vulkan/console/mobile 확장 가능한 renderer boundary를 구축했습니다.
```

면접 설명:

```text
RHI는 단순히 그래픽 API를 한 번 감싸는 wrapper가 아니라, 렌더러가 API 세부사항을 몰라도 되게 만드는 경계입니다. Winters에서는 DX11을 현재 기본 경로로 유지하되, public API에는 backend-neutral handle과 interface를 노출하고, DX11/DX12 concrete type은 Engine private backend에 가두는 방향으로 설계했습니다.
```

## 5.7 Snapshot/Event 복제

`Snapshot.fbs`의 의미:

```text
serverTick
serverTimeMs
rngState
entities
lastAckedCommandSeq
yourNetId
game score/objective state
AI debug trace
Chrono timeline metadata
```

이 구조의 의미:

- 서버가 가진 시뮬레이션 상태를 클라이언트에 복제한다.
- 클라이언트는 snapshot을 visual state로 적용한다.
- AI debug도 snapshot에 실어 같은 타임라인에서 확인할 수 있다.
- replay/chrono/debug와도 연결된다.

이력서 문장:

```text
FlatBuffers 기반 Snapshot/Event schema를 설계해 서버 tick, entity state, combat stat, action timeline, AI debug trace, Chrono metadata를 클라이언트와 replay/debug 도구에 일관되게 전달했습니다.
```

면접 설명:

```text
Snapshot은 단순 위치 동기화가 아니라 서버가 인정한 세계 상태의 계약입니다. Winters에서는 위치, 체력, 스킬 쿨타임, action lock, projectile, inventory, AI debug trace까지 schema에 포함해 클라이언트 visual, replay, debug panel이 같은 데이터를 보도록 구성했습니다.
```

## 5.8 Data Backend와 Docker

Winters의 `Services`는 in-match truth가 아니다.

```text
Services
-> account
-> matchmaking
-> profile
-> store
-> entitlement
-> telemetry/live ops

Not Services
-> in-match gameplay truth
-> Client visual state
-> Engine runtime primitive
-> cooked asset source
```

Docker 구성:

```text
Postgres
-> account/profile/store persistent state

Redis
-> cache/session/queue style state

Kafka
-> telemetry/live ops/event stream 실험
```

이력서 문장:

```text
Services 도메인을 in-match GameSim과 분리하고, Docker Compose 기반 Postgres/Redis/Kafka local backend 환경을 구성해 account, matchmaking, profile, store, telemetry/live ops 상태를 별도 service boundary로 설계했습니다.
```

면접 설명:

```text
게임 서버라고 해서 모든 상태가 같은 서버에 있으면 안 됩니다. 전투 중 체력과 위치는 GameSim truth이고, 계정/상점/매치메이킹/텔레메트리는 backend state입니다. Winters에서는 이 경계를 Services로 분리하고, local docker-compose로 Postgres, Redis, Kafka를 구성해 backend 실험 환경을 만들었습니다.
```

## 5.9 Tools와 AIResearch

Winters의 tool 방향:

```text
WfxEffectToolPanel
-> effect authoring/debug

ModelAnimPanel
-> model/animation inspection

MinimapPanel
-> gameplay presentation/debug

AIDebugPanel
-> bot perception/utility/action trace

WintersAssetConverter
-> raw asset -> runtime asset conversion

Tools/AIResearch
-> influence map, policy, research probe, Python/PyTorch bridge 방향
```

이력서 문장:

```text
런타임 기능뿐 아니라 WFX Effect Tool, Model/Animation Panel, Minimap Panel, AI Debug Panel, Asset Converter, AIResearch probe를 구성해 디자이너/개발자 검증 가능한 툴 중심 엔진 워크플로우를 설계했습니다.
```

면접 설명:

```text
엔진의 본질은 런타임만이 아니라 반복 제작 속도입니다. 그래서 Winters에서는 effect, animation, minimap, AI debug, asset conversion, AI research probe 같은 도구를 런타임과 연결하되, gameplay truth는 서버 GameSim에 남기는 방향으로 설계했습니다.
```

# 6. 세 프로젝트를 하나의 성장 서사로 연결

## 6.1 객체 생성의 진화

```text
Starcraft
-> CAbstractFactory<T>, CBuildingFactory
-> 객체 생성과 Initialize 통합

SR_MinecraftDungeons
-> Prototype/Clone
-> component 원형 등록 후 복제

Winters Engine
-> EntityBlueprint, data-driven definition, asset converter
-> 데이터/툴 기반 생성과 runtime identity 분리
```

면접 문장:

```text
처음에는 Factory로 C++ 객체 생성을 정리했고, 이후 Prototype/Clone 구조로 반복 생성과 component 복제를 경험했습니다. Winters에서는 이 흐름을 더 확장해 entity blueprint와 data-driven definition으로 runtime 생성과 authoring data를 분리하는 방향으로 발전시켰습니다.
```

## 6.2 생명주기의 진화

```text
Starcraft
-> raw pointer, Safe_Delete, pending delete

SR_MinecraftDungeons
-> CBase AddRef/Release, Safe_Release

Winters Engine
-> unique_ptr/shared_ptr/weak_ptr, EntityHandle, generation, ECS store
```

면접 문장:

```text
raw pointer와 pending delete, 수동 reference count를 직접 구현하면서 lifetime bug가 어디서 생기는지 경험했습니다. Winters에서는 그 경험을 바탕으로 RAII, 스마트 포인터, EntityHandle/generation, ECS store를 사용해 소유권과 생존 여부를 더 명확히 표현하려고 했습니다.
```

## 6.3 객체 모델의 진화

```text
Starcraft
-> OOP inheritance

SR_MinecraftDungeons
-> GameObject + Component

Winters Engine
-> ECS + System-oriented GameSim
```

면접 문장:

```text
상속 구조는 작은 프로젝트에서는 빠르고 직관적이지만, 객체 수와 조합이 늘면 base class가 비대해지고 예외가 많아집니다. Component 구조를 거쳐 Winters에서는 entity는 identity, component는 data, system은 behavior를 담당하는 ECS로 발전시켰습니다.
```

## 6.4 UI 구조의 진화

```text
Starcraft
-> CUIMgr, CommandSlot, Portrait, Minimap, Selection

SR_MinecraftDungeons
-> Client UI, Inventory, HUD, UI_EDITOR

Winters Engine
-> Client UI panels, debug panels, tool panels, server snapshot-driven presentation
```

면접 문장:

```text
Starcraft에서 command card와 selection UI를 분리하면서 UI가 단순 표시가 아니라 게임 조작의 핵심 도메인이라는 것을 배웠습니다. 이후 툴 UI와 런타임 UI를 분리했고, Winters에서는 서버 snapshot을 바탕으로 client presentation과 debug panel을 구성하는 방향으로 확장했습니다.
```

## 6.5 네트워크 구조의 진화

```text
SR_MinecraftDungeons
-> C2S/S2C packet, input, snapshot, projectile, damage sync

Winters Engine
-> GameCommand, server authoritative GameSim, Snapshot/Event, replay, AI debug trace
```

면접 문장:

```text
Minecraft Dungeons 프로젝트에서 패킷 구조와 snapshot sync를 구현하면서 client/server contract의 중요성을 배웠습니다. Winters에서는 이를 더 엄격하게 확장해, 클라이언트 입력은 GameCommand로 검증하고 서버 GameSim만 gameplay truth를 바꾸며 Snapshot/Event로 클라이언트와 replay/debug 도구에 전달하는 구조를 만들었습니다.
```

## 6.6 렌더링 구조의 진화

```text
Starcraft
-> WinAPI/GDI, HDC, bitmap/png, sprite animation

SR_MinecraftDungeons
-> DirectX9 device, mesh buffer, texture, camera, collider

Winters Engine
-> DX11 runtime, DX12 backend 실험, RHI abstraction, renderer/resource separation
```

면접 문장:

```text
2D GDI 렌더링에서 sprite와 UI의 기본을 익혔고, DirectX9에서 device/resource/component 구조를 경험했습니다. Winters에서는 DX11을 현재 경로로 유지하면서도 RHI를 통해 renderer가 backend concrete API에 묶이지 않도록 설계하고 있습니다.
```

# 7. 도메인별 이력서 문장 묶음

## 7.1 Game Architecture

```text
- Starcraft 모작, DirectX9 3D 프로젝트, Winters Engine을 거치며 OOP 상속, GameObject/Component, ECS 기반 GameSim까지 객체 모델을 단계적으로 설계했습니다.
- 게임 객체 생성, 생명주기, update/render ordering, command 처리, snapshot 복제, tool/debug workflow를 도메인별로 분리해 엔진 구조로 확장했습니다.
```

## 7.2 C++/Memory

```text
- raw pointer 기반 pending delete, CBase 수동 Reference Count, Safe_Release, 스마트 포인터 기반 RAII, EntityHandle/generation을 모두 경험하며 객체 소유권과 lifetime 안정성을 개선했습니다.
- DirectX/COM 스타일 lifetime 관리 경험을 바탕으로, Winters Engine에서는 unique_ptr/shared_ptr/weak_ptr와 ECS store를 사용해 ownership을 코드 구조로 명확히 표현했습니다.
```

## 7.3 Gameplay/Simulation

```text
- RTS 생산 큐, command card, smart command, unit/building update 구조를 구현했고, Winters에서는 이를 서버 권위 GameCommand/GameSim/Snapshot 구조로 확장했습니다.
- bot AI, influence map, utility decision, command executor, action lock, cooldown/target validation을 서버 GameSim 경계 안에서 처리하는 구조를 설계했습니다.
```

## 7.4 UI/Tools

```text
- Starcraft의 command card, portrait, minimap, hotkey feedback, selection UI를 구현했고, 이후 UI_EDITOR와 Winters debug/tool panels로 runtime UI와 authoring/debug tool을 분리했습니다.
- WFX Effect Tool, Minimap Panel, AI Debug Panel, Model/Animation Panel, Asset Converter 등 툴 중심 워크플로우를 통해 반복 제작과 검증 가능한 엔진 구조를 설계했습니다.
```

## 7.5 Rendering/RHI

```text
- WinAPI/GDI sprite 렌더링과 DirectX9 device/resource 구조를 거쳐, Winters Engine에서는 DX11 runtime과 DX12/Vulkan 확장 방향을 고려한 RHI abstraction을 설계했습니다.
- renderer가 backend concrete type에 직접 묶이지 않도록 IRHIDevice, command list, pipeline state, bind group, resource handle 기반 API 경계를 구성했습니다.
```

## 7.6 Network/Server

```text
- Shared packet schema 기반으로 login, input, snapshot, projectile, damage, boss sync 패킷을 구현했고, Winters에서는 GameCommand와 FlatBuffers Snapshot/Event 기반 서버 권위 동기화 구조로 발전시켰습니다.
- dedicated server authoritative 구조를 통해 클라이언트 입력을 검증하고, gameplay source of truth를 서버 GameSim에 집중시키는 설계를 적용했습니다.
```

## 7.7 Data/Backend/Docker

```text
- Winters Engine에서 ClientPublic visual data, ServerPrivate gameplay data, SharedContract identity를 분리하고, backend state는 Services 도메인으로 별도 관리하는 구조를 설계했습니다.
- Docker Compose 기반 Postgres/Redis/Kafka local backend 환경을 구성해 account, matchmaking, profile, store, telemetry/live ops와 in-match GameSim truth를 분리했습니다.
```

# 8. 면접 답변 템플릿

## 8.1 "가장 자신 있는 프로젝트는 무엇인가요?"

```text
가장 자신 있는 프로젝트는 Winters Engine입니다. 다만 Winters가 갑자기 나온 것은 아니고, Starcraft 모작에서 CObj 기반 객체 관리, Object Manager, Factory, 생산 큐, RTS UI를 직접 구현했고, Minecraft Dungeons 모작에서 DirectX9 엔진 구조, Component/Prototype, 수동 Reference Count, Client/Server/Shared 분리를 경험한 뒤 발전한 결과입니다. Winters에서는 이 경험을 바탕으로 ECS, 서버 권위 GameSim, Snapshot/Event, RHI, Tools, Services를 분리한 자체 C++ 게임 엔진 구조를 설계하고 있습니다.
```

## 8.2 "Object Manager를 왜 만들었나요?"

```text
RTS에서는 유닛, 건물, 투사체, 자원, UI 객체가 동시에 update/render되어야 하고, 객체가 죽었을 때 선택 목록, 컨트롤 그룹, 공격 명령, 투사체 타겟에서도 제거되어야 합니다. 그래서 CObjMgr에서 OBJID별 list, 유닛/건물 보조 vector, render list, pending delete, order invalidation을 통합 관리했습니다.
```

## 8.3 "Factory 패턴을 왜 사용했나요?"

```text
유닛과 건물 생성은 new만 하는 것이 아니라 Initialize, 위치 설정, 건물 데이터 설정, manager 등록까지 반복됩니다. 이 생성 규칙이 각 생산 건물에 흩어지면 유지보수가 어려워져서 CAbstractFactory<T>와 CBuildingFactory로 생성 경로를 통합했습니다.
```

## 8.4 "수동 Reference Count를 써본 경험이 어떤 의미가 있나요?"

```text
DirectX9 프로젝트에서 CBase의 AddRef/Release/Free 구조를 사용해 COM 스타일 lifetime을 직접 구현했습니다. 이를 통해 release 누락, 순환 참조, dangling pointer 문제가 어디서 생기는지 체감했고, Winters에서는 RAII, unique_ptr/shared_ptr/weak_ptr, EntityHandle/generation으로 소유권과 생존 여부를 더 명확하게 표현하려고 했습니다.
```

## 8.5 "ECS를 왜 도입했나요?"

```text
초기 OOP 상속 구조는 직관적이지만 객체 종류가 늘수록 base class가 비대해지고, dynamic_cast와 manager 의존성이 늘며, 데이터가 흩어져 cache locality도 나빠질 수 있습니다. Winters에서는 entity를 identity로 두고, 데이터는 component store에 모으고, behavior는 system이 필요한 component 조합을 순회하는 ECS 구조로 바꾸었습니다. 이 구조는 서버 GameSim, snapshot, replay, AI debug에도 더 적합합니다.
```

## 8.6 "서버 권위 구조를 어떻게 이해하고 있나요?"

```text
클라이언트가 체력, 위치, 쿨타임 같은 gameplay truth를 직접 결정하면 치트와 동기화 문제가 생깁니다. 그래서 Winters에서는 클라이언트 입력을 GameCommand로 보내고, 서버 GameSim이 검증과 실행을 담당하며, 결과를 Snapshot/Event로 클라이언트에 전달합니다. 클라이언트는 visual, animation, FX, UI, weak prediction만 담당합니다.
```

## 8.7 "RHI는 왜 필요한가요?"

```text
렌더러가 ID3D11Device 같은 concrete API에 직접 묶이면 DX12, Vulkan, console, mobile로 확장하기 어렵습니다. RHI는 renderer가 backend API 세부사항이 아니라 resource handle, command list, pipeline state 같은 추상 계약만 보게 만드는 경계입니다. Winters에서는 DX11을 현재 경로로 유지하면서 public API에는 backend-neutral interface를 두고, DX11/DX12 구현은 Engine private backend에 가두는 방향으로 설계하고 있습니다.
```

# 9. 프로젝트별 압축 버전

## 9.1 Starcraft 한 문단

```text
WinAPI/GDI 기반 Starcraft 모작에서 모든 게임 객체를 CObj로 추상화하고, CObjMgr에서 update/render/lifetime/pending delete/selection invalidation을 관리했습니다. CAbstractFactory<T>와 CBuildingFactory로 유닛/건물 생성 경로를 통합했으며, 생산 및 업그레이드 건물에는 deque 기반 FIFO 큐를 적용해 command 입력, 생산 시간 진행, 완료 callback을 분리했습니다. 또한 Commandable/CommandSlot/CUIMgr를 통해 command card UI, hotkey feedback, 종족별 아이콘 렌더링을 분리하고, SelectionMgr/Portrait/Minimap 등 RTS 조작 도메인을 별도 시스템으로 구현했습니다.
```

## 9.2 SR_MinecraftDungeons 한 문단

```text
DirectX9 기반 Minecraft Dungeons 모작에서 Base/Engine/Client/Server/Shared/UI_EDITOR 구조를 구성하고, CBase의 AddRef/Release/Free를 통한 수동 Reference Count, CGameObject/CComponent 기반 component architecture, CProtoMgr의 Prototype/Clone 구조, Scene Management를 구현했습니다. Shared PacketDef로 login/input/snapshot/projectile/damage/boss sync 패킷을 정의하고, Server에서 accept/recv thread, session, game loop, state broadcast를 구성했습니다. 이 경험을 통해 engine runtime, client gameplay, shared network contract, editor/tool domain을 분리하는 관점을 얻었습니다.
```

## 9.3 Winters Engine 한 문단

```text
Winters Engine에서는 이전 프로젝트의 OOP/Component 경험을 바탕으로 Engine/Client/Server/Shared(GameSim)/Tools/Services를 분리한 자체 C++ 게임 엔진을 설계하고 있습니다. ECS 기반 CWorld와 typed component store, 서버 권위 GameCommand/GameSim/Snapshot/Event 구조, AI perception/utility/command producer, DX11/DX12 확장 가능한 RHI, data-driven gameplay/visual pack, WFX/Animation/AI debug tool, AssetConverter, Docker 기반 Postgres/Redis/Kafka backend state 실험 환경을 구성해 게임 런타임과 제작/검증 파이프라인을 함께 설계하고 있습니다.
```

# 10. 포트폴리오 방향

## 10.1 지금 가장 강한 포트폴리오 제목

```text
OOP RTS Object Manager에서 Server-Authoritative ECS Game Engine까지
```

또는:

```text
C++ Game Engine Architecture: Object Manager, Component/Prototype, ECS, RHI, Server Authority
```

## 10.2 보여줘야 할 산출물

30% 천장 작업으로 실제 외부에 보여줄 산출물을 먼저 만든다.

```text
1. 3분 데모 영상
   - Starcraft: selection, production queue, command card
   - SR_MinecraftDungeons: DirectX9 gameplay, boss/network/editor
   - Winters: server-authoritative LoL bot/snapshot/RHI/tool panel

2. 기술 글 1편
   - "CObj 상속 구조에서 ECS GameSim으로 넘어간 이유"

3. 아키텍처 그림 1장
   - Starcraft -> SR_MinecraftDungeons -> Winters 진화 다이어그램

4. 이력서 bullet 6개
   - 위 문서의 도메인별 bullet에서 압축

5. 면접 답변 스크립트
   - Object Manager, Factory, RefCount, ECS, Server Authority, RHI
```

## 10.3 이력서에 너무 많이 쓰면 안 되는 것

다음은 이력서 본문에서는 짧게 쓰고, 면접에서 설명한다.

```text
- 모든 클래스 이름 나열
- 모든 구현 파일 나열
- 사용한 라이브러리만 긴 목록으로 나열
- "언리얼의 장점을 결합" 같은 큰 문장만 쓰고 코드 근거를 빼는 것
- 아직 완성되지 않은 기능을 완료된 제품처럼 쓰는 것
```

대신 이렇게 쓴다.

```text
문제
-> 설계 선택
-> 구현 도메인
-> 배운 한계
-> 다음 프로젝트에서 개선한 점
```

# 11. 최종 이력서 bullet 후보

아래 8개 중 지원 공고에 맞춰 4~6개만 선택한다.

```text
- WinAPI/GDI 기반 Starcraft 모작에서 CObj 추상 클래스, CObjMgr, Factory 패턴, deque 생산 큐, Command Card UI, Selection/Control Group/Portrait/Minimap/Hotkey 시스템을 구현했습니다.
- Object Manager에서 update/render ordering, pending delete, selection/control group/order invalidation을 처리해 RTS 객체 생명주기를 관리했습니다.
- DirectX9 Minecraft Dungeons 모작에서 CBase 수동 Reference Count, CGameObject/CComponent, Prototype/Clone, Scene Management 기반 3D 엔진 구조를 구현했습니다.
- Client/Server/Shared 패킷 계약을 분리하고 login, input, snapshot, projectile, damage, boss sync 패킷 기반 멀티플레이 동기화 구조를 설계했습니다.
- Winters Engine에서 Engine/Client/Server/Shared(GameSim)/Tools/Services를 분리하고, 서버 권위 GameCommand/GameSim/Snapshot/Event 구조를 구축했습니다.
- OOP 상속 기반 객체 구조의 한계를 해결하기 위해 EntityID, typed ComponentStore, ForEach 기반 ECS를 설계하고 gameplay system update 구조로 확장했습니다.
- DX11 경로를 유지하면서 IRHIDevice, command list, pipeline state, bind group, resource handle 기반 RHI 계층을 도입해 DX12/Vulkan 확장 가능한 렌더링 경계를 설계했습니다.
- Asset Converter, WFX Effect Tool, Minimap Panel, AI Debug Panel, AIResearch probe, Docker 기반 Postgres/Redis/Kafka Services 환경을 구성해 런타임, 툴, 백엔드, AI 검증 파이프라인을 연결했습니다.
```

# 12. 한계와 보완 방향

## 12.1 Starcraft의 한계

```text
한계:
- manager singleton 의존이 강하다.
- CObj base class가 비대해질 수 있다.
- dynamic_cast가 많아질수록 타입 분기 비용과 구조 복잡도가 증가한다.
- raw pointer lifetime이 불안정하다.

보완:
- 이 경험을 ECS, explicit ownership, system boundary 설계로 환전한다.
```

## 12.2 SR_MinecraftDungeons의 한계

```text
한계:
- 수동 AddRef/Release는 누락과 순서 문제가 생기기 쉽다.
- DirectX9 concrete dependency가 강하다.
- client/server sync는 있으나 완전한 dedicated server authoritative GameSim까지는 더 발전이 필요하다.

보완:
- Winters에서 RAII, RHI, GameCommand, Snapshot/Event, server authority로 환전한다.
```

## 12.3 Winters의 현재 과제

```text
과제:
- ECS와 Shared/GameSim의 Engine 의존을 더 줄여야 한다.
- RHI 전환은 DX11 생존 경로를 유지하면서 점진적으로 완성해야 한다.
- 툴은 런타임 truth를 우회하지 않고 server path와 연결되어야 한다.
- AI/RL 파이프라인은 Python 학습과 C++ runtime truth를 분리해야 한다.

보완:
- 공개 가능한 데모, 아키텍처 그림, README, 면접 스크립트로 이해를 산출물로 환전한다.
```

# 13. Verification / Handoff

작성 기준:

- 실제 바탕화면 폴더를 확인했다.
- Starcraft의 `CObj`, `CObjMgr`, `CAbstractFactory`, `CUIMgr`, `Commandable`, `CommandSlot`, `CSelectionMgr`, `CPortraitMgr`를 확인했다.
- SR_MinecraftDungeons의 `CBase`, `CGameObject`, `CComponent`, `CProtoMgr`, `CManagement`, `CServer`, `PacketDef`를 확인했다.
- Winters의 `CWorld`, `Snapshot.fbs`, RHI 파일, AI/GameSim/Tool/Services/Docker 구성을 확인했다.

다음 작업:

```text
1. 이 문서에서 이력서용 bullet 4~6개만 골라 실제 이력서에 붙인다.
2. 각 bullet마다 30초 면접 답변을 만든다.
3. 3분 데모 영상의 순서를 만든다.
4. "OOP -> Component -> ECS" 진화 다이어그램을 README/포트폴리오 첫 화면에 배치한다.
```
