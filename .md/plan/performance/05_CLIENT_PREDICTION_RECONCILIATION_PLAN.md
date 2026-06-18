Session - 클라이언트 예측과 서버 스냅샷 보정

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:
```cpp
#include <string>
#include <unordered_map>
```

아래에 추가:
```cpp
#include <deque>
```

기존 코드:
```cpp
    void BeginNetworkActorInterpolationForSnapshot(u64_t serverTick);
    void ApplyNetworkActorInterpolation(f32_t dt);
    void UpdateNetworkChampionLocomotion(f32_t dt);
```

아래에 추가:
```cpp
    void OnAuthoritativeSnapshot(u64_t serverTick,
        u64_t serverTimeMs,
        u32_t lastAckedCommandSeq,
        u32_t localNetId);
    void RecordNetworkMovePrediction(u32_t commandSeq,
        const Vec3& vPredictedTarget,
        const Vec3& vFacingDirection);
    void PruneAckedNetworkMovePredictions(u32_t lastAckedCommandSeq);
```

기존 코드:
```cpp
    std::unordered_map<EntityID, NetworkSnapshotInterpState> m_NetworkActorInterpStates{};
    u64_t  m_uNetworkActorInterpSnapshotTick = 0;
    bool_t m_bNetworkActorInterpolationEnabled = true;
```

아래에 추가:
```cpp
    struct NetworkMovePrediction
    {
        u32_t commandSeq = 0;
        Vec3 vPredictedTarget{};
        Vec3 vFacingDirection{};
        f32_t fAgeSec = 0.f;
    };
    std::deque<NetworkMovePrediction> m_NetworkMovePredictions{};
    u32_t m_uLastAckedMovePredictionSeq = 0;
    f32_t m_fLocalCorrectionBlendSec = 0.08f;
```

1-2. C:/Users/tnest/Desktop/Winters/Client/Public/Scene/InGameNetworkBridge.h

기존 코드:
```cpp
    std::function<void(EntityID)> bindLocalEntity;

    bool_t bDisableLiveNetwork = false;
```

아래에 추가:
```cpp
    std::function<void(u64_t serverTick,
        u64_t serverTimeMs,
        u32_t lastAckedCommandSeq,
        u32_t localNetId)> onAuthoritativeSnapshot;
```

1-3. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/InGameNetworkBridge.cpp

기존 코드:
```cpp
        desc.snapshotApplier->SetOnAuthoritativeSnapshot(
            [](u64_t, u64_t iServerTimeMs, u32_t, u32_t)
            {
                CGameInstance::Get()->UI_SetGameContextServerTimeMs(iServerTimeMs);
            });
```

아래로 교체:
```cpp
        auto onAuthoritativeSnapshot = desc.onAuthoritativeSnapshot;
        desc.snapshotApplier->SetOnAuthoritativeSnapshot(
            [onAuthoritativeSnapshot](
                u64_t serverTick,
                u64_t iServerTimeMs,
                u32_t lastAckedCommandSeq,
                u32_t localNetId)
            {
                CGameInstance::Get()->UI_SetGameContextServerTimeMs(iServerTimeMs);
                if (onAuthoritativeSnapshot)
                {
                    onAuthoritativeSnapshot(
                        serverTick,
                        iServerTimeMs,
                        lastAckedCommandSeq,
                        localNetId);
                }
            });
```

1-4. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp

기존 코드:
```cpp
        [&scene](EntityID entity)
        {
            scene.m_PlayerEntity = entity;
            scene.BindPlayerToECSChampion(scene.m_PlayerEntity);
        },
        scene.m_bReplayPlaybackMode
```

아래로 교체:
```cpp
        [&scene](EntityID entity)
        {
            scene.m_PlayerEntity = entity;
            scene.BindPlayerToECSChampion(scene.m_PlayerEntity);
        },
        [&scene](
            u64_t serverTick,
            u64_t serverTimeMs,
            u32_t lastAckedCommandSeq,
            u32_t localNetId)
        {
            scene.OnAuthoritativeSnapshot(
                serverTick,
                serverTimeMs,
                lastAckedCommandSeq,
                localNetId);
        },
        scene.m_bReplayPlaybackMode
```

1-5. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:
```cpp
void CScene_InGame::ApplyNetworkActorInterpolation(f32_t dt)
```

아래에 추가:
```text
CONFIRM_NEEDED
구현 직전 정확한 위치를 다시 확인한다.
추가할 함수의 의도는 다음과 같다.
- OnAuthoritativeSnapshot: lastAckedCommandSeq를 받아 m_NetworkMovePredictions를 정리한다.
- RecordNetworkMovePrediction: SendMove가 성공한 seq, target, facing을 저장한다.
- PruneAckedNetworkMovePredictions: ack된 seq 이전 예측을 제거한다.
- 보정은 로컬 플레이어만 별도 정책을 사용한다.
  작은 오차: interpolation duration 안에서 blend
  큰 오차: 순간 이동 대신 0.08초 이내 보정
  죽음/하드 CC/대시 확정: 서버 스냅 우선
```

1-6. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/InGamePlayerControlBridge.cpp

기존 코드:
```cpp
                    const u32_t moveSeq =
                        scene.m_pCommandSerializer->SendMove(
                            *scene.m_pNetworkView,
                            moveIntent,
                            moveFacingDirection);
```

아래에 추가:
```cpp
                    if (moveSeq != 0u)
                    {
                        scene.RecordNetworkMovePrediction(
                            moveSeq,
                            resolvedGround,
                            moveFacingDirection);
                    }
```

2. 검증

```text
빌드:
- git diff --check
- Client Debug x64 빌드

프로파일러:
- 예측 버퍼 추가 후 Scene_InGame::OnUpdate 증가가 0.05ms 이내인지 확인
- deque 크기가 lastAckedCommandSeq로 지속 정리되는지 확인

플레이 QA:
- 우클릭 이동 직후 서버 snapshot이 늦어도 로컬 이동 감각이 즉시 유지된다.
- 서버 위치와 차이가 날 때 순간이동 대신 짧은 보정으로 수렴한다.
- 반 바퀴 yaw 보호 로직과 새 보정 로직이 충돌하지 않는다.

네트워크 QA:
- localhost 정상 RTT
- 인위적 지연/패킷 흔들림
- lastAckedCommandSeq가 멈춘 경우 예측 버퍼가 무한 증가하지 않는지 확인
```
