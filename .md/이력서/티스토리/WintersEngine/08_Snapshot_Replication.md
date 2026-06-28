# Winters Engine 해부기 8 - Snapshot과 Replication

Snapshot/Replication의 본질은 서버 상태를 "그냥 보내는 것"이 아니다.

> 서버 내부 entity state를 Client가 안정적으로 해석할 수 있는 network identity와 deterministic order로 변환하는 것

## 문제 정의

서버 내부의 `EntityID`는 process-local identity다.

즉 서버 프로세스 안에서만 의미가 있다. 이것을 그대로 네트워크로 보내면 안 된다. Client와 Server의 entity allocation 순서가 다를 수 있고, 재접속이나 저장, replay 경계에서도 의미가 흔들린다.

따라서 network-facing identity가 필요하다.

또한 snapshot 생성 순서가 매번 달라지면 debugging과 regression이 어려워진다. 같은 상황을 재현해도 snapshot payload 순서가 달라지면 비교와 추적이 복잡해진다.

## Winters의 접근

Winters는 Server 내부 entity identity와 network identity를 분리한다.

관련 개념:

- `EntityID`: process-local runtime entity
- `NetEntityId`: network-facing replicated identity
- `EntityIdMap`: runtime entity와 network id 사이의 mapping

관련 파일:

- `Server/Private/Game/SnapshotBuilder.cpp`
- `Shared/GameSim/Replication/EntityIdMap.h`
- `Shared/Schemas/Snapshot.fbs`
- `Client/Private/Network/Client/SnapshotApplier.cpp`

## SnapshotBuilder의 역할

SnapshotBuilder는 CWorld에서 replication 대상 상태를 수집하고, network id 기준으로 정렬한 뒤, FlatBuffers snapshot으로 만든다.

코드 흐름은 다음과 같은 형태다.

```cpp
struct SnapshotEntity
{
    NetEntityId netId = NULL_NET_ENTITY;
    EntityID entity = NULL_ENTITY;
};

std::sort(sorted.begin(), sorted.end(),
    [](const SnapshotEntity& lhs, const SnapshotEntity& rhs)
    {
        return lhs.netId < rhs.netId;
    });
```

이 정렬은 작은 디테일처럼 보이지만 중요하다.

네트워크 복제에서 deterministic order는 debugging, diff, replay, regression을 쉽게 만든다.

## 무엇을 복제하는가

Snapshot에는 visual만 들어가지 않는다.

MOBA에서 Client가 화면을 정확히 복원하려면 다음 정보가 필요하다.

- position / rotation
- HP / mana / shield
- stat
- pose
- replicated action
- skill state
- projectile state
- gold / score
- AI debug trace
- visibility / vision 관련 정보

하지만 이것은 gameplay truth를 Client가 만든다는 뜻이 아니다. Client는 Server가 보낸 상태를 visual state로 적용한다.

## Snapshot과 Event의 차이

Snapshot은 현재 상태를 복제한다.

Event는 특정 순간의 사건을 전달한다.

예를 들어 위치, HP, pose는 snapshot에 적합하다. 반면 특정 스킬 FX cue, hit event, death event처럼 "한 번 발생한 사건"은 event/cue 형태가 더 적합하다.

Winters에서는 Snapshot/Event/FX cue를 함께 고려해야 Client visual이 Server result를 따라갈 수 있다.

## 면접에서 말할 포인트

Replication을 설명할 때 "서버 상태를 클라이언트에 보냈습니다"로 말하면 약하다.

더 좋은 설명은 이것이다.

> 서버 내부 lifetime identity와 네트워크 계약 identity를 분리하고, deterministic ordering으로 snapshot을 생성해 Client가 안정적으로 visual state를 복원하도록 했다.

이 설명은 네트워크 동기화의 핵심을 짚는다.

## 이 글을 이력서 문장으로 압축하면

> Server 내부 `EntityID`와 network-facing `NetEntityId`를 분리하고, deterministic ordering 기반 snapshot 생성/적용 파이프라인을 구현했습니다.

## 다음 글

다음 글에서는 챔피언/스킬 수치와 visual timing을 하드코딩에서 Definition Pack으로 옮기는 DataDriven 구조를 설명한다.

