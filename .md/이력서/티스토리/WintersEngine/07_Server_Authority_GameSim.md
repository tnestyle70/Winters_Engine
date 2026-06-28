# Winters Engine 해부기 7 - Server Authority와 GameSim

Server Authority의 본질은 "서버도 있다"가 아니다.

핵심은 이것이다.

> gameplay 결과를 누가 만들 권한이 있는가를 명확히 정하는 것

MOBA에서 이 질문은 매우 중요하다. 스킬이 맞았는지, 쿨다운이 돌았는지, 피해가 들어갔는지, 이동이 막혔는지, 죽었는지는 Client가 결정하면 안 된다.

## 문제 정의

로컬 게임에서 시작하면 Client가 모든 것을 처리하기 쉽다.

입력을 받고, 스킬을 쏘고, 충돌을 보고, 이펙트를 틀고, HP를 깎는다. 화면도 바로 반응하니 개발 속도는 빠르다.

하지만 네트워크 게임이 되면 이 구조는 바로 문제가 된다.

- Client마다 판정 결과가 달라질 수 있다.
- cheating에 취약하다.
- replay와 regression 검증이 어렵다.
- AI와 human input이 다른 경로로 실행된다.
- visual FX가 server result보다 먼저 성공한 것처럼 보인다.

그래서 Winters는 gameplay truth의 소유자를 Server/Shared GameSim으로 둔다.

## Winters의 권위 흐름

Winters의 기본 흐름은 다음이다.

```text
Client Input
-> GameCommandWire
-> Server BuildServerCommand
-> CDefaultCommandExecutor::ExecuteCommand
-> Shared/GameSim component mutation
-> Snapshot/Event
-> Client Visual
```

Client는 input을 command로 보낸다. Server는 command를 검증하고 GameSim에 실행한다. Client는 그 결과를 snapshot/event로 받아 visual에 적용한다.

## 코드 근거

관련 파일:

- `Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h`
- `Server/Private/Game`
- `Client/Private/Network/Client/CommandSerializer.cpp`
- `Client/Private/Network/Client/SnapshotApplier.cpp`

Command wire 구조:

```cpp
struct GameCommandWire
{
    eCommandKind kind = eCommandKind::None;
    uint64_t clientTick = 0;
    uint32_t sequenceNum = 0;
    uint8_t slot = 0;
    NetEntityId targetNet = NULL_NET_ENTITY;
    Vec3 groundPos{};
    Vec3 direction{};
    uint16_t itemId = 0;
};
```

Command executor 계약:

```cpp
class ICommandExecutor
{
public:
    virtual ~ICommandExecutor() = default;

    virtual void ExecuteCommand(CWorld& world,
        const TickContext& tc,
        const GameCommand& cmd) = 0;
};
```

## AI도 같은 경로를 탄다

여기서 중요한 점은 Bot AI도 같은 원칙을 따른다는 것이다.

AI가 직접 적 HP를 깎거나, 위치를 순간이동시키거나, cooldown을 조작하면 안 된다. AI는 world state를 관찰하고 command를 생산한다. 그 command가 Server GameSim에서 실행된다.

이렇게 하면 human input과 bot decision이 같은 authority path를 통과한다.

## 왜 FX도 서버 결과를 따라야 하는가

스킬 FX는 보기에는 Client presentation이지만, 언제 재생되어야 하는지는 gameplay result와 연결된다.

예를 들어 스킬이 빗나갔는데 Client가 먼저 hit FX를 재생하면 visual은 성공했지만 Server truth는 실패한 상태가 된다. 그래서 Winters의 방향은 FX도 server cue/event를 통해 한 번 재생되도록 하는 것이다.

## 면접에서 말할 포인트

서버 권위 구조를 설명할 때는 "소켓 통신을 했습니다"보다 이렇게 말하는 편이 좋다.

> Client 입력과 Bot AI 행동을 모두 command로 수렴시키고, Server GameSim만 gameplay result를 만들도록 경계를 잡았다.

이 말은 네트워크, AI, gameplay architecture를 함께 이해하고 있다는 신호다.

## 이 글을 이력서 문장으로 압축하면

> Client 입력과 Bot AI 행동을 모두 `GameCommand`로 수렴시키고, Server GameSim이 gameplay truth를 생성하는 서버 권위 파이프라인을 설계했습니다.

## 다음 글

다음 글에서는 Server의 권위 상태를 Client가 해석할 수 있도록 바꾸는 Snapshot / Replication 구조를 설명한다.

