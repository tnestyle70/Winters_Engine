Session - Winters Engine 면접 예상 질문과 답변 은행을 만든다.

이 문서는 실제 면접에서 바로 말할 수 있는 답변 재료다. 답변은 외운 문장보다 구조를 외우는 것이 좋다.

## 1. 자기소개

### Q. 본인을 한 문장으로 소개해 주세요.

답변:

```text
저는 C++ 기반 게임 엔진 구조, 서버 권위 Gameplay, DX11/RHI 렌더링, ECS, 데이터 기반 FX/툴 파이프라인을 직접 설계하고 구현해 온 게임 개발자입니다.
```

확장:

```text
Winters Engine에서는 Engine, Client, Shared GameSim, Server를 분리하고, LoL 스타일 입력/전투/스킬/FX/AI를 서버 권위 구조로 옮기는 작업을 해왔습니다. 단순 기능 구현보다 어떤 데이터와 책임이 어느 계층에 있어야 장기적으로 유지보수 가능한지를 계속 검증했습니다.
```

### Q. Winters Engine은 어떤 프로젝트인가요?

답변:

```text
Winters Engine은 하나의 게임 클라이언트가 아니라 여러 제품 클라이언트를 얹기 위한 C++ 게임 엔진 프로젝트입니다. 현재는 LoL 스타일 Client/Server GameSim과 EldenRing 계열 showcase/editor slice를 같은 Engine 기반 위에 분리해서 올리는 방향으로 개발하고 있습니다.
```

### Q. 본인이 가장 강하게 어필하고 싶은 부분은?

답변:

```text
저는 기능을 빨리 붙이는 것보다 그 기능의 truth owner가 어디인지 먼저 나눕니다. 예를 들어 전투 판정과 쿨타임은 Server/GameSim, 애니메이션과 FX는 Client, 편집 가능한 값은 Data/WFX, 공용 렌더링과 리소스는 Engine에 두는 식으로 책임을 분리했습니다.
```

## 2. C++ 기본 질문

### Q. RAII가 무엇이고, Winters에서는 어떻게 썼나요?

답변:

```text
RAII는 자원 획득과 해제를 객체 수명에 묶는 방식입니다. Winters에서는 매니저와 시스템 소유권은 unique_ptr로, DirectX COM 자원은 ComPtr로 관리했습니다. IOCP server도 소멸자에서 Shutdown을 호출해 worker thread와 socket handle을 정리합니다.
```

핵심 코드:

- `Engine/Private/GameInstance.cpp`
- `Engine/Private/RHI/DX11/CDX11Device.h`
- `Server/Private/Network/IOCPCore.cpp`

### Q. unique_ptr와 raw pointer를 어떻게 구분하나요?

답변:

```text
unique_ptr는 소유권을 표현하고 raw pointer는 소유하지 않는 관찰 관계에 사용합니다. 예를 들어 GameInstance는 내부 매니저를 unique_ptr로 소유하지만, RHI device나 World처럼 외부에서 수명을 보장하는 대상은 raw pointer로 넘깁니다.
```

### Q. 왜 GameInstance 소멸자를 cpp에 정의했나요?

답변:

```text
GameInstance.h는 매니저들을 전방 선언한 unique_ptr로 들고 있습니다. unique_ptr의 default_delete는 소멸자 instantiation 시점에 완전한 타입을 요구하기 때문에, 헤더에서 default destructor를 inline으로 두면 incomplete type 문제가 생길 수 있습니다. 그래서 cpp에서 실제 매니저 헤더를 include한 뒤 소멸자를 정의했습니다.
```

### Q. virtual destructor는 언제 필요한가요?

답변:

```text
base pointer로 파생 객체를 삭제할 수 있는 인터페이스에는 virtual destructor가 필요합니다. Winters의 ISystem, ICommandExecutor, IWalkableQuery, IRHIDevice 같은 인터페이스는 파생 구현을 base pointer로 다루므로 virtual destructor를 둡니다.
```

### Q. copy를 막고 move만 허용한 이유는?

답변:

```text
World, Scheduler, JobSystem 같은 객체는 내부에 unique_ptr, thread, atomic, GPU handle처럼 복사 의미가 없는 자원을 갖습니다. 이런 객체가 복사되면 소유권이 중복되거나 내부 상태가 깨지기 때문에 copy는 delete하고 필요한 경우 move만 허용했습니다.
```

### Q. static_assert를 어떻게 활용했나요?

답변:

```text
파일 포맷과 네트워크 이벤트처럼 메모리 배치가 중요한 타입에 static_assert를 사용했습니다. WMeshFormat은 header, vertex, bone entry 크기를 고정하고, ReplicatedEventComponent는 trivially copyable이어야 한다고 검증합니다.
```

### Q. 예외를 많이 쓰지 않은 이유는?

답변:

```text
게임 런타임과 서버 tick에서는 실패 지점을 명시적으로 관측하고 격리하는 것이 중요해서 bool, HRESULT, nullptr, invalid handle 반환을 기본으로 했습니다. 다만 BinaryReader처럼 파서 내부의 지역적인 read past EOF는 예외로 표현하고 loader 경계에서 false로 변환합니다.
```

### Q. 템플릿은 어디에 썼나요?

답변:

```text
ECS ComponentStore와 World의 Add/Get/TryGet/ForEach에 템플릿을 사용했습니다. 컴포넌트 타입 안정성을 얻고, 컴포넌트 데이터는 sparse/dense store로 유지합니다. 반대로 시스템 실행이나 RHI 경계처럼 교체 가능한 부분은 virtual interface를 사용했습니다.
```

### Q. DLL/public header에서 주의한 점은?

답변:

```text
public header는 의존성이 퍼지는 경계이기 때문에 using namespace를 피하고, std::를 명시하고, DX11 concrete type이나 product-specific type이 새지 않도록 했습니다. Engine은 Client/Server를 몰라야 하고, Shared/GameSim은 Engine/Renderer/UI/DX를 몰라야 합니다.
```

## 3. Engine 구조 질문

### Q. 왜 CGameInstance가 필요한가요?

답변:

```text
GameInstance는 Client가 Engine 내부 매니저를 직접 알지 않게 하는 gateway입니다. Timer, Scene, Sound, UI, Blueprint, Profiler, JobSystem, FX asset registry 같은 high-level API를 제공하되, 내부 매니저의 구현과 소유권은 Engine 안에 숨깁니다.
```

주의점:

```text
모든 것을 GameInstance wrapper로 만들면 hot path overhead와 비대한 API 문제가 생기므로, 자주 호출되는 ECS/RHI/JobSystem 내부 루프는 직접 인터페이스나 핸들로 접근하는 방향을 병행했습니다.
```

### Q. ECS를 왜 사용했나요?

답변:

```text
챔피언, 미니언, 구조물, 투사체, FX처럼 서로 다른 객체가 공통 데이터 조합으로 동작해야 했습니다. ECS를 사용하면 Transform, Health, SkillState, Render, Vision 같은 데이터를 독립 컴포넌트로 두고 시스템이 필요한 조합만 순회할 수 있습니다.
```

### Q. ComponentStore 구조를 설명해 주세요.

답변:

```text
Winters ComponentStore는 sparse/dense 구조입니다. sparse는 entity id에서 dense index를 찾고, dense/data vector는 실제 순회 데이터를 연속 메모리로 보관합니다. 삭제는 마지막 원소를 swap해 빠르게 처리하고, 결정성이 필요한 서버 시스템은 entity id를 정렬해 순회합니다.
```

### Q. EntityHandle은 왜 필요한가요?

답변:

```text
EntityID는 index라 entity가 destroy된 뒤 같은 index가 재사용될 수 있습니다. 오래된 참조가 새 entity를 가리키는 문제를 막기 위해 index와 generation을 묶은 EntityHandle을 사용하고, TryResolve에서 generation이 일치하는지 확인합니다.
```

### Q. JobSystem을 어떻게 설계했나요?

답변:

```text
Worker thread마다 work-stealing deque를 두고, worker는 자기 deque에서 LIFO로 pop하고 다른 worker는 top에서 steal합니다. main이나 worker가 counter를 기다릴 때도 그냥 sleep하지 않고 가능한 job을 훔쳐 실행하는 help-stealing 구조를 넣었습니다.
```

### Q. SystemScheduler는 어떻게 race를 막나요?

답변:

```text
각 ISystem이 어떤 컴포넌트를 읽고 쓰는지 DescribeAccess로 선언합니다. Scheduler는 같은 컴포넌트에 write 충돌이 있거나 world structure write가 있으면 같은 batch에 넣지 않습니다. 충돌하지 않는 시스템만 병렬 실행합니다.
```

### Q. RHI는 왜 만들었나요?

답변:

```text
DX11이 현재 기본 backend이지만, Client 코드가 ID3D11Device를 직접 알면 DX12나 다른 backend로 가기 어렵습니다. 그래서 IRHIDevice, command list, RHI resource handle을 두고, DX11 concrete type은 Engine 내부로 격리했습니다.
```

### Q. 렌더링에서 pImpl을 쓴 이유는?

답변:

```text
RHISceneRenderer는 public header에서 구현 세부와 리소스 컬렉션을 숨기기 위해 Impl 포인터를 사용합니다. 이렇게 하면 public header 변경을 줄이고 DX/RHI 세부 타입이 상위에 퍼지는 것을 막을 수 있습니다.
```

## 4. Server Authority 질문

### Q. 서버 권위 구조를 설명해 주세요.

답변:

```text
Client는 입력을 GameCommand로 보내고, Server가 GameSim에서 이동, 데미지, 쿨타임, 사망, 투사체를 판정합니다. 결과는 Snapshot과 Event로 Client에 전달되고, Client는 애니메이션, FX, UI로 표현합니다.
```

핵심 문장:

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual
```

### Q. GameCommandWire와 GameCommand를 나눈 이유는?

답변:

```text
Wire command에는 네트워크 ID와 client tick이 들어가고, 서버 내부 command에는 process-local EntityID가 들어갑니다. 외부 입력을 바로 내부 EntityID로 믿지 않고, session binding과 EntityIdMap을 통해 변환하기 위해 둘을 분리했습니다.
```

### Q. IOCP server에서 gameplay world race는 어떻게 막나요?

답변:

```text
IOCP worker는 packet을 받아 CommandIngress에 넣고, gameplay world는 room tick에서 drain한 command로만 변경합니다. 네트워크 스레드가 Transform이나 HP를 직접 수정하지 않게 해서 truth owner를 tick thread로 모았습니다.
```

### Q. 빠른 이동 입력은 어떻게 처리하나요?

답변:

```text
Move command는 같은 session에 pending Move가 있으면 최신 Move로 교체합니다. 오래된 이동 명령이 뒤늦게 실행되어 조작감이 흔들리는 것을 줄이기 위한 coalescing입니다. 단, skill이나 attack 같은 non-move command는 유지합니다.
```

### Q. Bot AI는 어떻게 권위를 지키나요?

답변:

```text
Bot AI는 Transform, HP, cooldown을 직접 수정하지 않고 GameCommand를 생성합니다. 사람이 입력한 command와 Bot command가 같은 CommandExecutor 경로를 타게 만들어 서버 권위와 검증 로직을 공유합니다.
```

## 5. Client/FX 질문

### Q. Snapshot과 Event의 차이는?

답변:

```text
Snapshot은 위치, 체력, 상태, 쿨타임 같은 지속 상태를 동기화하고, Event는 스킬 시전, 데미지, FX, kill feed 같은 1회성 사건을 전달합니다. Client는 Snapshot으로 world state를 맞추고 Event로 animation/FX/UI를 재생합니다.
```

### Q. FX 중복 재생은 어떻게 막나요?

답변:

```text
서버가 보낸 EffectTrigger를 single source로 보고, Client EventApplier에서 effect cue key를 만들어 이미 처리한 cue는 다시 재생하지 않습니다. 이렇게 legacy local hook과 network event가 동시에 같은 FX를 틀어버리는 문제를 줄였습니다.
```

### Q. WFX는 어떤 구조인가요?

답변:

```text
WFX는 effect cue와 emitter들을 JSON 형태로 저장하는 데이터입니다. emitter에는 render type, texture/model path, lifetime, transform, color, blend/depth/material, atlas, ribbon/beam 설정이 들어갑니다. Runtime에서는 FxCuePlayer가 이 데이터를 읽어 ECS FX component로 생성합니다.
```

### Q. 디자이너가 튜닝한 effect가 다른 PC에서도 같게 나오나요?

답변:

```text
WFX JSON과 texture/model resource 경로가 git으로 관리되고 runtime resource root가 동일하면 같은 결과를 받을 수 있습니다. 중요한 것은 editor에서 저장한 값이 code-local 임시값이 아니라 Data/LoL/FX 같은 공유 데이터로 남아야 한다는 점입니다.
```

### Q. animation replication은 어떻게 보나요?

답변:

```text
서버는 action id, stage, sequence, start tick을 event/snapshot으로 보내고, Client는 ChampionDef와 SkillDef/VisualDefinition을 통해 실제 animation key와 playback speed를 결정합니다. gameplay timing은 서버가 갖고, 시각 재생은 클라이언트가 그 timing에 맞춰 표현합니다.
```

## 6. Data/Tool/Collaboration 질문

### Q. 기획자, 디자이너, 개발자의 책임을 어떻게 나눴나요?

답변:

```text
기획자는 gameplay JSON과 skill/stat 값을, 디자이너는 texture/model/animation/WFX emitter를, 개발자는 runtime loader, server authority, renderer, validation pipeline을 책임지는 구조를 목표로 잡았습니다. 서로의 파일을 직접 고치지 않아도 결과가 runtime에 반영되는 것이 협업 구조의 핵심입니다.
```

### Q. JSON과 runtime data의 차이는?

답변:

```text
JSON은 사람이 편집하는 authoring/cook input입니다. runtime tick은 JSON 문자열을 직접 파싱하지 않고 검증된 generated pack이나 immutable data를 읽는 방향입니다. 그래야 프레임 성능과 데이터 안정성을 둘 다 지킬 수 있습니다.
```

### Q. 실패 처리 정책은?

답변:

```text
실패를 조용히 묻지 않는 것을 원칙으로 했습니다. RHI resource 생성 실패, FlatBuffers verify 실패, asset/cue miss 같은 경계는 bounded log를 남깁니다. routine trace는 제거하거나 gate하지만, failure diagnostic은 유지합니다.
```

### Q. 문서화는 어떻게 관리했나요?

답변:

```text
반복 실수는 gotchas, 구조 방향은 codebase compass와 architecture 문서, 특정 작업 계획과 결과는 날짜가 붙은 plan 문서에 남겼습니다. 문서는 코드가 바로 답할 수 있는 inventory가 아니라 의사결정과 협업 규칙을 남기는 용도로 관리했습니다.
```

## 7. EldenRing/확장성 질문

### Q. EldenRingClient는 왜 따로 있나요?

답변:

```text
Winters가 LoL 클라이언트 하나에 묶이지 않고 여러 게임 클라이언트를 얹을 수 있는 Engine인지 검증하기 위해 EldenRingClient를 별도 제품 클라이언트로 뒀습니다. 같은 Engine 위에서 scene, camera, resource, renderer, editor data 방향을 검증하는 slice입니다.
```

### Q. Elden showcase에서 중요한 점은?

답변:

```text
source placement가 runtime asset으로 닫히는지, 즉 placement JSON의 wmesh/model/transform이 실제 spawn 가능한 상태인지 검증하는 점입니다. map closure audit로 source, closed, open, missing asset, spawn failed를 기록해 리소스 파이프라인의 빈 구멍을 보이게 합니다.
```

## 8. 어려웠던 점 질문

### Q. 가장 어려웠던 기술 문제는?

답변:

```text
클라이언트 조작감과 서버 권위를 동시에 만족시키는 것이 어려웠습니다. Client는 즉각적인 animation/FX/prediction이 필요하지만, 실제 이동/데미지/쿨타임은 Server GameSim이 결정해야 합니다. 그래서 입력, command, authoritative simulation, snapshot/event, client visual을 분리했습니다.
```

### Q. 버그를 어떻게 구조적으로 접근하나요?

답변:

```text
먼저 현상이 어느 계층의 책임인지 나눕니다. gameplay truth면 Server/GameSim, presentation이면 Client, resource면 Data/Resource, renderer면 Engine/RHI로 들어갑니다. 그 다음 authoritative path에 bounded log나 debug UI를 놓고, 최소 수정 후 build와 runtime smoke로 검증합니다.
```

### Q. 리팩터링에서 가장 중요하게 보는 기준은?

답변:

```text
같은 책임이 두 군데 있으면 먼저 소유자를 하나로 정합니다. 예를 들어 FX는 server cue single-source, gameplay result는 Server/GameSim, visual playback은 Client로 모읍니다. 중복 경로를 지우지 않고 새 경로만 더하면 버그가 더 숨기 때문에 owner를 먼저 정합니다.
```

## 9. 약점과 향후 개선 질문

### Q. 아직 부족한 점은?

답변:

```text
일부 챔피언 특수 로직과 visual fallback이 아직 code path에 남아 있고, data-driven pack으로 완전히 이동하지 못한 부분이 있습니다. 또한 Elden showcase 쪽에는 ad hoc JSON parsing이 남아 있어 장기적으로 공용 parser/validator와 asset catalog로 통합해야 합니다.
```

### Q. 다음 단계는?

답변:

```text
첫째, Shared/GameSim의 Engine 의존 adapter를 완성해 경계를 더 깨끗하게 만들고, 둘째, champion gameplay/visual data를 JSON/cook pack으로 더 이동하고, 셋째, WFX와 editor pipeline을 안정화해 디자이너가 저장한 데이터가 빌드와 git을 통해 그대로 재현되도록 만들 계획입니다.
```

### Q. 왜 회사에서 가치가 있나요?

답변:

```text
저는 기능 하나를 구현할 때도 그 기능의 owner, data path, runtime path, failure path, verification path를 같이 봅니다. 그래서 단기 구현뿐 아니라 팀 협업과 장기 유지보수를 고려한 구조를 만들 수 있습니다.
```

## 10. 말하기 연습용 압축 문장

30초:

```text
Winters Engine은 C++ 기반으로 Engine, Client, Shared GameSim, Server를 분리해 만든 게임 엔진 프로젝트입니다. 저는 서버 권위 Gameplay, DX11/RHI 렌더링, ECS, JobSystem, 데이터 기반 FX/WFX 툴, Bot AI command pipeline을 구현하며 기능의 책임 경계를 나누는 데 집중했습니다.
```

90초:

```text
처음에는 클라이언트에서 빠르게 전투 감각을 만들 수 있었지만, 멀티클라이언트와 Bot AI가 들어오면서 서버 권위 구조가 필요했습니다. 그래서 Client Input을 GameCommand로 보내고 Server GameSim이 이동, 데미지, 쿨타임, 사망을 판정한 뒤 Snapshot/Event로 Client가 시각화하는 구조로 옮겼습니다. Engine은 RHI, ECS, Resource, UI, JobSystem 같은 공용 runtime을 맡고, Client는 animation/FX/UI/prediction, Shared는 deterministic gameplay contract, Server는 IOCP와 tick authority를 담당합니다. 최근에는 WFX effect data와 EldenRingClient/Editor까지 확장해 기획자, 디자이너, 개발자의 협업 경계를 나누는 방향으로 정리하고 있습니다.
```

3분:

```text
Winters에서 가장 중요하게 본 것은 "본질적인 소유자"를 찾는 것이었습니다. 예를 들어 데미지와 사망은 Client가 아니라 Server/GameSim이 소유해야 하고, FX와 animation은 Client presentation이지만 서버 event가 single source여야 합니다. 데이터도 마찬가지로, 사람이 편집하는 JSON/WFX와 runtime이 읽는 validated pack/resource를 나눴습니다.

C++ 관점에서는 이 구조를 소유권과 수명으로 표현했습니다. Engine의 매니저는 unique_ptr, DX11 COM 자원은 ComPtr, ECS entity는 index/generation 기반 EntityHandle, 파일 포맷과 replicated event는 static_assert와 trivially-copyable 제약으로 관리했습니다. 병렬화도 단순히 thread를 늘리는 방식이 아니라, system access를 선언하고 충돌하지 않는 batch만 JobSystem에 태우는 방식으로 접근했습니다.

이 프로젝트를 통해 게임 개발에서 중요한 것은 단일 기능보다 입력, 시뮬레이션, 네트워크, 렌더링, 툴, 데이터가 서로 어디까지 책임지는지를 분리하는 것이라고 배웠습니다. 앞으로는 data-driven 전환, RHI 경계 정리, WFX/editor pipeline 안정화, Bot AI 완성도를 더 높이는 방향으로 발전시키고 싶습니다.
```

## 11. 검증과 핸드오프

이 Q/A 문서는 다음 두 문서를 말하기용으로 압축한 것이다.

- `.md/plan/2026-07-10_CPP_INTERVIEW_PREP_WINTERS_ENGINE.md`
- `.md/plan/2026-07-10_WINTERS_ENGINE_DOMAIN_INTERVIEW_PREP.md`

면접 전에는 각 답변을 30초 안에 말하는 연습을 먼저 하고, 꼬리 질문이 들어오면 코드 근거 파일명과 설계 이유를 붙여 확장한다.
