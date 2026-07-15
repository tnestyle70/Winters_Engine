Session - C++의 본질을 Winters Engine 코드로 설명한다.

이 문서는 면접 대비용이다. 목표는 C++ 문법을 나열하는 것이 아니라, Winters Engine에서 실제로 사용한 C++ 선택을 근거로 "왜 그렇게 설계했는가"를 말할 수 있게 만드는 것이다.

## 1. 코드 근거와 학습 구조

주요 코드 근거:

- `Engine/Include/GameInstance.h`, `Engine/Private/GameInstance.cpp`
- `Engine/Public/Engine_Macro.h`, `Engine/Include/WintersTypes.h`
- `Engine/Public/ECS/Entity.h`, `Engine/Public/ECS/World.h`, `Engine/Public/ECS/ComponentStore.h`
- `Engine/Public/ECS/ISystem.h`, `Engine/Public/ECS/SystemAccess.h`, `Engine/Private/ECS/SystemScheduler.cpp`
- `Engine/Public/Core/JobSystem.h`, `Engine/Private/Core/JobSystem.cpp`, `Engine/Public/Core/JobSystem/WorkStealingDeque.h`
- `Engine/Private/RHI/DX11/CDX11Device.h`, `Engine/Private/RHI/DX11/CDX11Device.cpp`
- `Engine/Public/Renderer/RHISceneRenderer.h`, `Engine/Private/Renderer/RHISceneRenderer.cpp`
- `Engine/Public/AssetFormat/Common/BinaryReader.h`, `Engine/Public/AssetFormat/Mesh/WMeshFormat.h`, `Engine/Private/AssetFormat/Mesh/WMeshLoader.cpp`
- `Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h`
- `Shared/GameSim/Components/ReplicatedEventComponent.h`
- `Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h`
- `Server/Private/Network/IOCPCore.cpp`, `Server/Private/Game/CommandIngress.cpp`
- `Client/Private/Network/Client/SnapshotApplier.cpp`, `Client/Private/Network/Client/EventApplier.cpp`
- `Client/Private/GameObject/FX/FxCuePlayer.cpp`, `Client/Private/GameObject/FX/WfxDocument.cpp`

면접에서 보여줘야 할 정체성:

```text
C++을 문법이 아니라 소유권, 수명, 메모리 배치, 컴파일 경계, 스레드 경계, 데이터 흐름을 통제하는 언어로 이해하고 사용했다.
```

## 2. C++의 가장 기본 본질

C++의 본질은 "무엇이 언제 생성되고, 누가 소유하며, 언제 파괴되고, 어떤 메모리 모양으로 존재하며, 어느 경계까지 노출되는가"를 개발자가 직접 설계하는 것이다.

Winters에서 이 본질은 다음 다섯 질문으로 압축된다.

- 이 객체의 소유자는 누구인가?
- 이 포인터는 소유인가, 관찰인가?
- 이 타입은 값으로 복사해도 되는가, 이동만 가능한가, 절대 복사하면 안 되는가?
- 이 데이터는 런타임 내부 ID인가, 네트워크/저장소를 건너도 되는 ID인가?
- 이 헤더를 공개했을 때 어떤 의존성이 다른 프로젝트로 새어 나가는가?

면접 답변 예시:

```text
C++은 객체 수명과 메모리 배치를 숨기지 않는 언어라고 이해합니다. 그래서 Winters에서는 unique_ptr, ComPtr, EntityHandle, POD 이벤트, RHI 핸들처럼 소유권과 경계를 타입으로 드러내려고 했습니다.
```

## 3. 기본 C++ 개념과 Winters 적용

### 3.1 헤더와 cpp, 컴파일 경계

C++은 헤더가 include되는 모든 번역 단위에 영향을 준다. 그래서 공개 헤더는 단순한 선언 파일이 아니라 의존성 전파 경계다.

Winters 사례:

- `CGameInstance`는 헤더에서 여러 매니저를 `unique_ptr<class CTimer_Manager>`처럼 전방 선언으로 보유한다.
- 생성자/소멸자는 `GameInstance.cpp`에 out-of-line으로 둔다.
- 이유는 `unique_ptr<T>`의 기본 삭제자가 소멸자 instantiation 시점에 완전한 `T` 타입을 요구하기 때문이다.

면접 답변 예시:

```text
GameInstance.h는 클라이언트가 많이 include하는 공개 경계라서 매니저 구현 헤더를 직접 노출하지 않도록 전방 선언을 사용했습니다. 대신 unique_ptr 멤버의 소멸이 incomplete type 문제를 만들 수 있기 때문에 소멸자는 cpp에서 정의했습니다.
```

### 3.2 RAII와 소유권

RAII는 자원의 수명을 객체 수명에 묶는 C++의 핵심이다.

Winters 사례:

- `CGameInstance`는 `CTimer_Manager`, `CScene_Manager`, `CSound_Manager`, `CUI_Manager`, `CJobSystem`, `CFxAssetRegistry`를 `std::unique_ptr`로 소유한다.
- `CDX11Device`는 DirectX COM 객체를 `Microsoft::WRL::ComPtr`로 소유한다.
- `CIOCPCore`는 소멸자에서 `Shutdown()`을 호출해 IOCP worker thread와 socket handle을 정리한다.

구분:

- `unique_ptr`: 단일 소유권. 복사 금지, 이동 가능.
- `ComPtr`: COM reference count를 RAII로 감싼 소유권.
- raw pointer: 소유하지 않는 관찰 포인터 또는 외부에서 수명을 보장하는 포인터.
- handle: 내부 포인터를 직접 노출하지 않는 안정적인 식별자.

면접 답변 예시:

```text
Winters에서는 소유권이 있는 자원은 unique_ptr이나 ComPtr로 묶고, 수명을 소유하지 않는 연결에는 raw pointer를 사용했습니다. raw pointer를 완전히 금지하기보다, 소유인지 관찰인지가 코드에서 드러나도록 역할을 나누었습니다.
```

### 3.3 복사 금지와 이동 허용

엔진 자원 객체는 복사하면 안 되는 경우가 많다. thread, GPU resource, ECS world, scheduler처럼 복사되면 소유권이 중복되거나 내부 포인터가 깨질 수 있다.

Winters 사례:

- `CWorld`는 `unique_ptr<IComponentStoreBase>`를 보유하므로 복사 금지, 이동 허용이다.
- `CSystemSchedular`는 `unique_ptr<ISystem>` 컬렉션을 보유하므로 복사 금지를 명시한다.
- `CJobSystem`, `CWorkStealingDeque`는 thread/atomic 상태 때문에 복사 금지다.

면접 답변 예시:

```text
엔진에서는 복사가 의미 없는 타입이 많습니다. World나 Scheduler는 내부 소유권을 가진 컨테이너라 복사를 막고, 필요하면 이동만 허용했습니다. C++에서 복사 가능성은 성능 문제가 아니라 소유권의 의미 문제라고 봅니다.
```

### 3.4 값, 참조, 포인터

값은 독립적인 복사본, 참조는 반드시 존재하는 대상을 의미하고, 포인터는 없을 수도 있는 대상을 표현한다.

Winters 적용:

- `Render(const RenderWorldSnapshot& snapshot)`: 렌더러가 스냅샷을 소유하지 않고 읽기만 한다.
- `Get_RHIDevice()`는 외부가 소유하지 않는 엔진 내부 장치 포인터를 준다.
- `TryGetComponent<T>()`는 컴포넌트가 없을 수 있으므로 포인터를 반환한다.
- `GetComponent<T>()`는 존재가 전제된 코드 경로라 참조를 반환하고 assert를 둔다.

면접 답변 예시:

```text
값은 독립 데이터, 참조는 반드시 존재하는 대상, 포인터는 optional 관계로 구분했습니다. ECS에서는 TryGetComponent는 포인터, GetComponent는 참조로 나눠 호출부의 전제를 드러냈습니다.
```

### 3.5 타입 별칭과 정수 크기

게임 엔진에서는 네트워크, 파일 포맷, GPU 버퍼, 바이너리 cook 데이터 때문에 타입 크기가 중요하다.

Winters 사례:

- `WintersTypes.h`는 `f32_t`, `u32_t`, `i32_t`, `u64_t` 같은 명시적 타입 별칭을 제공한다.
- `WMeshFormat.h`는 바이너리 파일 포맷 구조체 크기를 `static_assert`로 고정한다.
- `DefinitionKey`, `ChampionDefId`, `SkillDefId`, `EntityHandle`은 서로 다른 ID 생명주기를 타입으로 분리한다.

면접 답변 예시:

```text
게임 엔진에서 int 하나도 경계에 따라 의미가 달라집니다. 파일 포맷, 네트워크, 런타임 내부 ID가 섞이지 않도록 고정 크기 타입과 별도 ID 타입을 사용했습니다.
```

### 3.6 POD와 trivially copyable

네트워크 이벤트, binary file header, ECS snapshot에 들어가는 데이터는 복사와 직렬화가 예측 가능해야 한다.

Winters 사례:

- `ReplicatedEventComponent`는 `std::is_trivially_copyable_v` static_assert를 가진다.
- `CBinaryReader::Read<T>()`는 `std::is_trivially_copyable_v<T>`만 허용한다.
- `WMeshFormat.h`는 `#pragma pack(push, 1)`과 `static_assert(sizeof(...))`로 binary layout을 고정한다.

면접 답변 예시:

```text
네트워크와 파일 경계의 타입은 생성자, 가상 함수, 포인터 소유권이 섞이면 위험합니다. 그래서 이벤트와 파일 헤더는 trivially copyable/POD 성격을 유지하고 static_assert로 깨지는 순간 빌드에서 알 수 있게 했습니다.
```

### 3.7 템플릿과 타입 기반 ECS

템플릿은 런타임 상속보다 컴파일 타임 타입 정보를 활용할 수 있게 해준다. 하지만 모든 것을 템플릿으로 만들면 빌드 의존성과 코드 가시성이 나빠진다.

Winters 사례:

- `CComponentStore<T>`는 sparse/dense storage를 템플릿으로 구현한다.
- `CWorld::AddComponent<T>`, `TryGetComponent<T>`, `ForEach<T...>`는 타입 기반 API를 제공한다.
- `std::type_index(typeid(T))`로 컴포넌트 타입별 store를 런타임 map에 보관한다.

면접 답변 예시:

```text
ECS 컴포넌트 접근은 타입 안정성이 중요해서 템플릿 API로 만들었습니다. 대신 시스템 실행, 스케줄링, DLL 경계는 virtual interface와 non-template 함수로 분리해서 빌드 경계를 제어했습니다.
```

### 3.8 가상 함수와 인터페이스

C++에서 virtual은 런타임 다형성을 제공하지만, 비용과 수명 경계를 분명히 해야 한다.

Winters 사례:

- `ISystem`은 `Execute`, `GetPhase`, `DescribeAccess`를 가진 시스템 인터페이스다.
- `ICommandExecutor`, `IWalkableQuery`, `ILagCompensationQuery`는 서버 GameSim이 구체 구현을 몰라도 되게 하는 경계다.
- `IRHIDevice`, `IRHICommandList`는 DX11/DX12 방향을 위한 렌더링 추상화다.

면접 답변 예시:

```text
자주 호출되는 내부 루프는 값과 템플릿으로 단순하게 두고, 교체 가능성이 있는 경계에는 인터페이스를 사용했습니다. 예를 들어 GameSim은 IWalkableQuery만 알고, 실제 서버 nav 구현은 Server가 제공합니다.
```

### 3.9 예외 처리

Winters의 기본 에러 모델은 예외가 아니라 `bool`, `HRESULT`, `nullptr`, invalid handle 반환이다.

예외를 제한하는 이유:

- 프레임 루프와 서버 tick에서 예외 전파는 실패 위치를 흐릴 수 있다.
- C/DLL/Win32/COM 경계와 섞이면 복구 정책이 불명확해진다.
- 게임 런타임은 실패를 관측 가능하게 남기고 부분 격리해야 한다.

예외 사용 사례:

- `CBinaryReader`는 read past EOF를 `std::runtime_error`로 던진다.
- 상위 loader가 catch해서 `false`로 바꾼다.
- 즉, throw는 파일 파서 내부 구현 디테일이고 시스템 경계 밖으로 새지 않는다.

면접 답변 예시:

```text
Winters에서는 런타임 경계에서 예외를 기본으로 쓰지 않았습니다. 대신 실패를 bool/HRESULT/invalid handle로 반환하고 bounded log를 남깁니다. 단, BinaryReader처럼 지역적인 파서 내부에서는 예외를 쓰고 loader가 false로 변환해 경계를 정리했습니다.
```

## 4. 심화 C++ 개념과 Winters 적용

### 4.1 EntityHandle과 세대 기반 수명 안전성

단순 `EntityID`는 index 재사용 문제가 있다. 죽은 entity의 ID가 새 entity에 재사용되면 stale reference가 살아있는 것처럼 보일 수 있다.

Winters 구조:

- `EntityID`: 기존 sparse store 호환용 index.
- `EntityGeneration`: entity slot 재사용 세대.
- `EntityHandle`: 64bit 안에 generation과 index를 pack.
- `TryResolve(handle, outEntity)`가 generation을 비교해 stale handle을 차단한다.

면접 답변 예시:

```text
EntityID만 쓰면 destroy 후 같은 index가 재사용될 때 오래된 참조가 새 객체를 가리킬 수 있습니다. 그래서 index와 generation을 묶은 EntityHandle을 도입해 수명 안전성이 필요한 경로에서 stale handle을 걸러냈습니다.
```

### 4.2 Sparse/Dense ComponentStore

`CComponentStore<T>`는 `sparse[entity] -> dense index`, `dense[index] -> entity`, `data[index] -> T` 구조다.

장점:

- 컴포넌트 데이터가 dense vector에 모여 있어 순회가 캐시 친화적이다.
- `Has(entity)`가 빠르다.
- 삭제 시 마지막 원소를 swap해서 O(1)에 가깝게 제거한다.

주의:

- 삭제는 순서를 보존하지 않는다.
- 결정성이 필요한 시스템은 `DeterministicEntityIterator::CollectSorted`처럼 entity ID를 정렬해 순회한다.

면접 답변 예시:

```text
ComponentStore는 sparse/dense 구조로 만들었습니다. 순회는 dense vector를 타기 때문에 캐시에 유리하고, 삭제는 마지막 원소 swap으로 빠르게 처리합니다. 순서가 필요한 서버 시뮬레이션은 별도 정렬 iterator로 결정성을 보장합니다.
```

### 4.3 SystemScheduler와 데이터 의존성 기반 병렬화

무작정 시스템을 병렬로 돌리면 같은 컴포넌트를 동시에 쓰는 순간 data race가 난다.

Winters 구조:

- `ISystem::DescribeAccess`가 읽기/쓰기 컴포넌트를 선언한다.
- `SystemAccessConflicts`가 같은 타입에 write가 겹치거나 world structure write가 있으면 충돌로 본다.
- `CSystemSchedular::RebuildExecutionPlan`이 충돌하지 않는 시스템만 batch로 묶는다.
- batch 크기가 충분하면 `CJobSystem`에 submit한다.

면접 답변 예시:

```text
병렬화는 thread를 많이 만드는 문제가 아니라 데이터 충돌을 증명하는 문제라고 봤습니다. Winters Scheduler는 각 시스템이 읽고 쓰는 컴포넌트를 선언하게 하고, 충돌하지 않는 시스템만 같은 batch에서 병렬 실행합니다.
```

### 4.4 JobSystem, atomic, work stealing

Winters `CJobSystem`은 worker thread, per-worker deque, global queue, help-stealing wait를 가진다.

핵심 C++ 포인트:

- `std::thread` 수명은 `Shutdown()`과 소멸자에서 join으로 정리한다.
- `std::atomic<bool>`로 shutdown 상태를 공유한다.
- `thread_local`로 현재 worker index를 추적한다.
- `CWorkStealingDeque`는 `alignas(64)`로 false sharing을 줄인다.
- owner는 bottom에서 push/pop, thief는 top에서 steal한다.
- `WaitForCounter`는 단순 busy wait가 아니라 가능한 일을 훔쳐 실행한다.

면접 답변 예시:

```text
JobSystem은 작업을 제출하고 기다리는 동안 메인 스레드가 놀지 않도록 help-stealing을 넣었습니다. 또 work stealing deque의 top/bottom atomic은 false sharing이 생기기 쉬워 alignas(64)로 분리했습니다.
```

### 4.5 RHI, COM, opaque handle

렌더링은 플랫폼/API 구체 타입이 쉽게 새어 나온다. C++에서는 헤더 include 하나로 DX 타입이 전역으로 퍼질 수 있으므로 경계가 중요하다.

Winters 구조:

- `CDX11Device`는 DX11 구현체이며 `ID3D11Device`, `IDXGISwapChain`, `ID3D11DeviceContext`를 `ComPtr`로 보유한다.
- `IRHIDevice`는 backend-neutral 인터페이스다.
- `Client/Public`에는 `ID3D11*`를 노출하지 않는 방향이다.
- `RHIBufferHandle`, `RHITextureHandle`, `RHIPipelineHandle`은 native pointer 대신 핸들을 전달한다.
- `CRHISceneRenderer`는 pImpl로 구현 세부를 숨긴다.

면접 답변 예시:

```text
DX11은 현재 기본 backend지만, 클라이언트 코드가 ID3D11Device를 직접 알게 만들면 DX12나 다른 backend로 갈 수 없습니다. 그래서 Engine 내부에서는 ComPtr로 DX 자원을 RAII 관리하고, 상위에는 IRHIDevice와 RHI handle을 노출했습니다.
```

### 4.6 Binary asset format과 메모리 배치

Winters의 `.wmesh`, `.wanim`, `.wtex`, `.wmat`, `.wfx` 방향은 runtime이 빠르게 읽는 cooked format을 목표로 한다.

`WMeshFormat` 사례:

- magic, count, stride, flags를 header에 둔다.
- `VertexStatic`, `VertexSkinned`, `SubMeshDesc`, `BoneEntry` 크기를 `static_assert`로 고정한다.
- loader는 vertex/index block을 zero-copy pointer로 참조한다.
- 상한값 `MAX_VERTICES`, `MAX_SUBMESHES`, `MAX_BONES`로 잘못된 파일을 방어한다.

면접 답변 예시:

```text
원본 FBX를 런타임마다 해석하는 대신 cooked binary를 읽게 했습니다. 파일 포맷은 static_assert로 구조체 크기를 고정하고, loader는 magic/stride/count를 검증한 뒤 vertex/index blob을 zero-copy로 참조합니다.
```

### 4.7 서버 권위와 C++ 데이터 경계

서버 권위 구조는 C++ 타입 경계와 직결된다.

Winters 구조:

- Client packet은 `GameCommandWire`다.
- Server는 session의 controlled entity와 `EntityIdMap`을 통해 `GameCommand`로 변환한다.
- `GameCommandWire`는 network ID를 가진다.
- `GameCommand`는 process-local `EntityID`를 가진다.
- `EntityHandle`은 프로세스 내부 lifetime identity이며 network/save 경계를 넘지 않는다.

면접 답변 예시:

```text
네트워크에서 받은 값과 서버 내부 EntityID를 바로 섞지 않았습니다. wire command는 net id를 갖고, 서버가 controlled entity와 EntityIdMap을 통해 GameCommand로 변환합니다. 이 구조가 authority와 보안을 동시에 지켜줍니다.
```

### 4.8 FlatBuffers verify와 실패 관측

네트워크 데이터는 신뢰할 수 없다.

Winters 사례:

- Client `SnapshotApplier`와 `EventApplier`는 FlatBuffers verifier로 buffer를 검증한다.
- 실패하면 조용히 무시하지 않고 bounded `OutputDebugStringA`를 남긴다.
- Server `ReplicatedEventSerializer`는 event kind별로 NetEntityId 유효성을 확인한다.

면접 답변 예시:

```text
스키마 drift나 손상된 packet을 그냥 return하면 클라이언트 입장에서는 월드가 멈춘 것처럼만 보입니다. 그래서 verify 실패를 bounded log로 남겨 네트워크 문제인지 schema 문제인지 볼 수 있게 했습니다.
```

## 5. Winters C++에서 중요한 설계 습관

### 5.1 public header는 제품 경계다

헤더 하나가 잘못 노출되면 Client, Server, Shared, Engine의 의존성 방향이 무너진다.

면접 답변:

```text
Winters에서는 public header를 단순 include 파일이 아니라 제품 경계로 봅니다. Engine public header에 Client나 Server 타입이 들어오면 아키텍처 규칙을 컴파일로 지키기 어려워지기 때문입니다.
```

### 5.2 데이터와 코드를 분리하되 runtime은 validated pack을 읽는다

JSON은 authoring/cook input이다. runtime tick은 JSON 문자열을 계속 파싱하지 않는다.

면접 답변:

```text
기획자가 고치는 값은 JSON에 두되, 런타임은 검증된 pack이나 generated data를 읽게 했습니다. JSON은 편집 포맷이고, tick 경로는 안정적인 immutable data를 보는 방향입니다.
```

### 5.3 실패는 즉시 관측 가능해야 한다

`return false`만 있으면 면접에서 "디버깅은 어떻게 했나"를 설명하기 어렵다.

Winters 원칙:

- 실패 위치를 남긴다.
- 실패 원인을 구분한다.
- 로그는 bounded로 남긴다.
- routine trace와 failure diagnostic을 구분한다.

면접 답변:

```text
실패 처리는 단순히 크래시를 막는 게 아니라 원인을 보이게 만드는 것이라고 봅니다. 그래서 verify 실패, asset miss, RHI resource 생성 실패 같은 경계에는 bounded diagnostic을 남겼습니다.
```

## 6. 면접에서 C++을 설명하는 공식

답변은 이 순서로 말하면 안정적이다.

```text
1. 문제: 어떤 위험이 있었는가?
2. C++ 원리: 수명, 소유권, 메모리, 타입, 경계 중 무엇의 문제였는가?
3. Winters 적용: 어떤 파일/구조에서 어떻게 해결했는가?
4. 검증: 빌드, 로그, smoke, runtime 확인을 어떻게 했는가?
5. 한계/다음 단계: 아직 남은 위험은 무엇인가?
```

예시:

```text
ECS EntityID는 index라 재사용 위험이 있습니다. C++에서 오래된 값이 여전히 유효한 것처럼 보이는 lifetime 문제가 생길 수 있어, EntityHandle에 generation을 같이 pack했습니다. TryResolve에서 generation이 맞는지 확인해 stale handle을 걸렀고, 네트워크 경계에는 process-local handle을 넘기지 않는 규칙을 세웠습니다.
```

## 7. 검증과 핸드오프

문서 작성 검증:

- 코드 근거는 현재 Winters 코드와 architecture 문서에서 확인했다.
- 이 문서는 구현 변경을 하지 않는다.
- 면접 준비 시 이 문서를 "개념 암기"가 아니라 "내 코드 사례로 답변하기"의 기준으로 사용한다.

다음 단계:

- 실제 이력서에는 이 문서의 문장을 짧게 압축한다.
- 면접 연습에는 별도 Q/A 문서의 답변을 30초, 90초, 3분 버전으로 나눠 말한다.
