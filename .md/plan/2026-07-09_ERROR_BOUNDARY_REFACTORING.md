Session - 감사로 확정된 silent-failure 지점 11개 파일에 "실패 즉시 가시화(bounded trace) + 실패 격리" 정책을 적용하고, 죽은 진단 코드(sprintf 후 미출력)를 정리한다. 근거: 2026-07-09 아키텍처 감사(65건 발견, high 20건 중 18건 적대적 검증 확정). 정책 문서는 `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md` 참조.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Engine/Private/Scene/Scene_Manager.cpp

`CScene_Manager::Change_Scene` 안에서

기존 코드:

```cpp
    if (m_pCurrentScene)
        m_pCurrentScene->OnEnter();

    return S_OK;
```

아래로 교체:

```cpp
    if (m_pCurrentScene && !m_pCurrentScene->OnEnter())
    {
        char msg[128]{};
        sprintf_s(msg, "[Scene_Manager] Change_Scene OnEnter FAILED sceneID=%u\n", iNextSceneID);
        OutputDebugStringA(msg);
        return E_FAIL;
    }

    return S_OK;
```

근거: `Set_StaticScene`(같은 파일)은 `if (!pScene->OnEnter()) return E_FAIL;`로 검사하는데 `Change_Scene`만 bool 반환을 버리고 무조건 `S_OK`를 반환. 실패한 씬이 이전 씬 파괴 후 반쯤 초기화된 채 계속 구동됨. 잔여 정책(실패 씬 대체/폴백)은 다음 슬라이스.

1-2. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX11/CDX11Device.cpp

`CDX11Device::CreateBuffer` 안에서

기존 코드:

```cpp
    if (FAILED(m_pDevice->CreateBuffer(
        &bufferDesc,
        pInitialData ? &initData : nullptr,
        pBuffer->pBuffer.GetAddressOf())))
    {
        delete pBuffer;
        return {};
    }
```

아래로 교체:

```cpp
    const HRESULT hrBuffer = m_pDevice->CreateBuffer(
        &bufferDesc,
        pInitialData ? &initData : nullptr,
        pBuffer->pBuffer.GetAddressOf());
    if (FAILED(hrBuffer))
    {
        char msg[256]{};
        sprintf_s(msg, "[CDX11Device] FAIL: CreateBuffer hr=0x%08X size=%u name=%s\n",
            static_cast<unsigned>(hrBuffer),
            desc.sizeBytes,
            desc.debugName ? desc.debugName : "(unnamed)");
        OutputDebugStringA(msg);
        delete pBuffer;
        return {};
    }
```

`CDX11Device::CreateTexture` 안에서

기존 코드:

```cpp
    if (FAILED(m_pDevice->CreateTexture2D(
        &texDesc,
        pInitialData ? &initData : nullptr,
        pTexture->pTexture.GetAddressOf())))
    {
        delete pTexture;
        return {};
    }

    if (FAILED(m_pDevice->CreateShaderResourceView(
        pTexture->pTexture.Get(),
        nullptr,
        pTexture->pSRV.GetAddressOf())))
    {
        delete pTexture;
        return {};
    }
```

아래로 교체:

```cpp
    const HRESULT hrTexture = m_pDevice->CreateTexture2D(
        &texDesc,
        pInitialData ? &initData : nullptr,
        pTexture->pTexture.GetAddressOf());
    if (FAILED(hrTexture))
    {
        char msg[256]{};
        sprintf_s(msg, "[CDX11Device] FAIL: CreateTexture2D hr=0x%08X %ux%u name=%s\n",
            static_cast<unsigned>(hrTexture),
            desc.width,
            desc.height,
            desc.debugName ? desc.debugName : "(unnamed)");
        OutputDebugStringA(msg);
        delete pTexture;
        return {};
    }

    const HRESULT hrSRV = m_pDevice->CreateShaderResourceView(
        pTexture->pTexture.Get(),
        nullptr,
        pTexture->pSRV.GetAddressOf());
    if (FAILED(hrSRV))
    {
        char msg[256]{};
        sprintf_s(msg, "[CDX11Device] FAIL: CreateShaderResourceView hr=0x%08X name=%s\n",
            static_cast<unsigned>(hrSRV),
            desc.debugName ? desc.debugName : "(unnamed)");
        OutputDebugStringA(msg);
        delete pTexture;
        return {};
    }
```

근거: 엔진 전체 RHI 버퍼/텍스처가 이 초크포인트를 지나는데 desc.debugName까지 갖고 있으면서 HRESULT를 버리고 빈 핸들만 반환 — 하위에서 "메시가 조용히 안 그려짐"(T-pose류) 증상으로만 나타남. 같은 파일 16곳의 기존 `[CDX11Device] FAIL:` 로그 컨벤션과 일치시킴.

1-3. C:/Users/user/Desktop/Winters/Engine/Private/RHI/RHITextureLoader.cpp

익명 namespace 안 `CScopedCOMInit` 클래스 정의 아래에 추가:

기존 코드:

```cpp
    private:
        bool_t m_bNeedsUninit = false;
    };
}
```

아래로 교체:

```cpp
    private:
        bool_t m_bNeedsUninit = false;
    };

    void LogTextureLoadFailure(const char* pStage, const wchar_t* pPath, HRESULT hr)
    {
        char msg[512]{};
        sprintf_s(msg, "[RHITextureLoader] FAIL: %s hr=0x%08X path=%ls\n",
            pStage,
            static_cast<unsigned>(hr),
            pPath ? pPath : L"(null)");
        OutputDebugStringA(msg);
    }
}
```

`RHI_CreateTextureFromFile` 안의 침묵 실패 7곳을 각각 교체:

기존 코드:

```cpp
    if (FAILED(hr))
        return {};
```

(CoCreateInstance 다음) 아래로 교체:

```cpp
    if (FAILED(hr))
    {
        LogTextureLoadFailure("CoCreateInstance(WICImagingFactory2)", loadPath.c_str(), hr);
        return {};
    }
```

(CreateDecoderFromFilename 다음) 아래로 교체:

```cpp
    if (FAILED(hr))
    {
        LogTextureLoadFailure("CreateDecoderFromFilename", loadPath.c_str(), hr);
        return {};
    }
```

(GetFrame 다음) 아래로 교체:

```cpp
    if (FAILED(hr))
    {
        LogTextureLoadFailure("GetFrame", loadPath.c_str(), hr);
        return {};
    }
```

기존 코드:

```cpp
    if (FAILED(hr) || width == 0 || height == 0)
        return {};
```

아래로 교체:

```cpp
    if (FAILED(hr) || width == 0 || height == 0)
    {
        LogTextureLoadFailure("GetSize", loadPath.c_str(), hr);
        return {};
    }
```

(CreateFormatConverter 다음) 아래로 교체:

```cpp
    if (FAILED(hr))
    {
        LogTextureLoadFailure("CreateFormatConverter", loadPath.c_str(), hr);
        return {};
    }
```

(pConverter->Initialize 다음) 아래로 교체:

```cpp
    if (FAILED(hr))
    {
        LogTextureLoadFailure("FormatConverter.Initialize", loadPath.c_str(), hr);
        return {};
    }
```

(CopyPixels 다음) 아래로 교체:

```cpp
    if (FAILED(hr))
    {
        LogTextureLoadFailure("CopyPixels", loadPath.c_str(), hr);
        return {};
    }
```

근거: 8개 실패 경로 전부 무로그 `return {}` — 텍스처 파일 누락(가장 흔한 컨텐츠 에러)이 완전 침묵. 호출자는 폴백 텍스처로 조용히 대체(Scene_InGameLifecycle.cpp)하므로 잘못된 아트가 흔적 없이 화면에 나감. 마지막 `pDevice->CreateTexture(...)` 실패는 1-2의 디바이스 레벨 로그가 커버.

1-4. C:/Users/user/Desktop/Winters/Engine/Public/ECS/Systems/MCTSSystem.h

`class WINTERS_ENGINE CMCTSSystem` 의 public 영역에서

기존 코드:

```cpp
    ~CMCTSSystem() override = default;
```

아래에 추가:

```cpp
    CMCTSSystem(const CMCTSSystem&) = delete;
    CMCTSSystem& operator=(const CMCTSSystem&) = delete;
```

근거: gotcha 2026-04-23 — WINTERS_ENGINE dllexport 클래스의 `std::unique_ptr` 멤버(m_pPlanner)는 copy ctor/assign 명시 delete 필수. 전수 스캔에서 이 클래스가 유일한 잔존 위반.

1-5. C:/Users/user/Desktop/Winters/Server/Private/Network/Session.cpp

파일 상단 include에서

기존 코드:

```cpp
#include "Network/Session.h"

#include "Network/PacketDispatcher.h"
```

아래로 교체:

```cpp
#include "Network/Session.h"

#include "Network/PacketDispatcher.h"

#include <iostream>
```

`CSession::Send` 안에서

기존 코드:

```cpp
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        CompletePendingIo();
        m_sendQueue.pop_back();
        m_bSendPending = false;
        return false;
    }
```

아래로 교체:

```cpp
    if (result == SOCKET_ERROR)
    {
        const int sendError = WSAGetLastError();
        if (sendError != WSA_IO_PENDING)
        {
            CompletePendingIo();
            m_sendQueue.pop_back();
            m_bSendPending = false;
            std::cerr << "[Session] send failed sid=" << m_sessionId
                      << " wsa=" << sendError << '\n';
            return false;
        }
    }
```

`CSession::OnSendComplete` 안에서

기존 코드:

```cpp
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        CompletePendingIo();
        m_bSendPending = false;
    }
```

아래로 교체:

```cpp
    if (result == SOCKET_ERROR)
    {
        const int sendError = WSAGetLastError();
        if (sendError != WSA_IO_PENDING)
        {
            CompletePendingIo();
            m_bSendPending = false;
            std::cerr << "[Session] send repost failed sid=" << m_sessionId
                      << " wsa=" << sendError << '\n';
            OnDisconnect();
        }
    }
```

근거: OnSendComplete의 하드 WSASend 실패가 무로그 + 무단절 — 실패 패킷이 큐 앞에 남고 m_bSendPending만 풀려 해당 클라이언트 복제가 조용히 정지(클라 화면 프리즈, 서버 흔적 0). recv 대응 경로(OnRecvComplete)는 이미 실패 시 OnDisconnect() 호출 — 정책 비대칭 해소. OnDisconnect는 락을 잡지 않고 idempotent(검증 완료). 의도된 동작 변화: 일시적 WSAENOBUFS도 재시도 대신 단절(표준 IOCP 관행, recv 경로와 동일 정책).

1-6. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

`ProtectLocalMoveYaw` 끝부분에서

삭제할 코드:

```cpp
    if (true)
    {
        char msg[192]{};
        sprintf_s(
            msg,
            "[YawTrace][SnapshotProtect] net=%u seq=%u yaw=%.4f\n",
            netId,
            commandSeq,
            yaw);
    }
```

`OnHello` 안에서

기존 코드:

```cpp
    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifyHelloBuffer(verifier))
    {
        return;
    }
```

아래로 교체:

```cpp
    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifyHelloBuffer(verifier))
    {
        static u32_t s_helloVerifyFailLogCount = 0;
        if (s_helloVerifyFailLogCount < 8)
        {
            char msg[128]{};
            sprintf_s(msg, "[SnapshotApplier] invalid Hello buffer len=%u\n", len);
            OutputDebugStringA(msg);
            ++s_helloVerifyFailLogCount;
        }
        return;
    }
```

`OnHello` 끝부분에서

삭제할 코드:

```cpp
    char msg[160]{};
    sprintf_s(msg,
        "[SnapshotApplier] Hello sid=%u netId=%u tick=%llu champion=%u team=%u\n",
        hello->sessionId(),
        hello->yourNetId(),
        static_cast<unsigned long long>(hello->serverTick()),
        hello->championId(),
        hello->team());
```

`OnSnapshot` 안에서

기존 코드:

```cpp
    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifySnapshotBuffer(verifier))
    {
        return;
    }
```

아래로 교체:

```cpp
    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifySnapshotBuffer(verifier))
    {
        static u32_t s_snapshotVerifyFailLogCount = 0;
        if (s_snapshotVerifyFailLogCount < 8)
        {
            char msg[128]{};
            sprintf_s(msg, "[SnapshotApplier] invalid Snapshot buffer len=%u\n", len);
            OutputDebugStringA(msg);
            ++s_snapshotVerifyFailLogCount;
        }
        return;
    }
```

`OnSnapshot` 의 local net mismatch 블록에서

기존 코드:

```cpp
            char msg[160]{};
            sprintf_s(msg,
                "[SnapshotApplier] snapshot local net mismatch hello=%u snapshot=%u\n",
                m_localNetId,
                snapshotLocalNetId);
            ++s_localNetIdMismatchLogCount;
```

아래로 교체:

```cpp
            char msg[160]{};
            sprintf_s(msg,
                "[SnapshotApplier] snapshot local net mismatch hello=%u snapshot=%u\n",
                m_localNetId,
                snapshotLocalNetId);
            OutputDebugStringA(msg);
            ++s_localNetIdMismatchLogCount;
```

`OnSnapshot` 의 yaw 보호 해제 블록에서 (`m_localMoveYawProtection = {};` 바로 위)

삭제할 코드:

```cpp
                if (true)
                {
                    char msg[256]{};
                    sprintf_s(
                        msg,
                        "[YawTrace][SnapshotProtectClear] tick=%llu net=%u seq=%u actionLocked=%u caught=%u protectedFrames=%u ackedProtectedFrames=%u\n",
                        static_cast<unsigned long long>(m_lastServerTick),
                        es->netId(),
                        m_localMoveYawProtection.commandSeq,
                        bServerActionLocked ? 1u : 0u,
                        bServerCaughtProtectedYaw ? 1u : 0u,
                        static_cast<u32_t>(m_localMoveYawProtection.protectedSnapshotCount),
                        static_cast<u32_t>(m_localMoveYawProtection.ackedProtectedSnapshotCount));
                }
```

stale minion 제거 루프에서 (`staleNetIds.push_back(netId);` 다음)

삭제할 코드:

```cpp
        char msg[160]{};
        sprintf_s(msg,
            "[SnapshotApplier] remove stale minion netId=%u entity=%u\n",
            netId,
            static_cast<u32_t>(entity));
```

`EnsureEntity` 의 champion mismatch 블록에서

기존 코드:

```cpp
            char msg[192]{};
            sprintf_s(msg,
                "[SnapshotApplier] champion mismatch netId=%u entity=%u visual=%u snapshot=%u\n",
                netId,
                static_cast<u32_t>(e),
                currentChampionID,
                championId);

            champ.id = static_cast<eChampion>(championId);
```

아래로 교체:

```cpp
            static u32_t s_championMismatchLogCount = 0;
            if (s_championMismatchLogCount < 8)
            {
                char msg[192]{};
                sprintf_s(msg,
                    "[SnapshotApplier] champion mismatch netId=%u entity=%u visual=%u snapshot=%u\n",
                    netId,
                    static_cast<u32_t>(e),
                    currentChampionID,
                    championId);
                OutputDebugStringA(msg);
                ++s_championMismatchLogCount;
            }

            champ.id = static_cast<eChampion>(championId);
```

`EnsureEntity` 의 신규 엔티티 생성 실패 지점에서

기존 코드:

```cpp
    if (e == NULL_ENTITY)
        return NULL_ENTITY;

    entityMap.Bind(netId, e);
```

아래로 교체:

```cpp
    if (e == NULL_ENTITY)
    {
        static u32_t s_entitySpawnFailLogCount = 0;
        if (s_entitySpawnFailLogCount < 8)
        {
            char msg[160]{};
            sprintf_s(msg,
                "[SnapshotApplier] entity spawn FAILED netId=%u kind=%u champion=%u\n",
                netId,
                static_cast<u32_t>(entityKind),
                static_cast<u32_t>(championId));
            OutputDebugStringA(msg);
            ++s_entitySpawnFailLogCount;
        }
        return NULL_ENTITY;
    }

    entityMap.Bind(netId, e);
```

근거: 이 파일은 1,652줄에 OutputDebugString 호출 0개 — commit 1813b00이 전 로그를 제거하며 sprintf 포맷팅만 남김. 정책: 실패 진단(verify 실패, net-id/champion 불일치, 스폰 실패)은 bounded 재무장, 루틴 트레이스(SnapshotProtect/Hello/stale-minion 포맷팅)는 죽은 코드 삭제. 683-709(minion yaw)/711-770(local yaw) 대형 yaw 계측 블록은 yaw 작업 이력과 얽혀 있어 이번 슬라이스에서 보존(정책 문서에 dead-formatting 사이트로 기록).

1-7. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

`OnEvent` 안에서

기존 코드:

```cpp
    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifyEventPacketBuffer(verifier))
    {
        return;
    }
```

아래로 교체:

```cpp
    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifyEventPacketBuffer(verifier))
    {
        static u32_t s_eventVerifyFailLogCount = 0;
        if (s_eventVerifyFailLogCount < 8)
        {
            char msg[128]{};
            sprintf_s(msg, "[EventApplier] invalid EventPacket buffer len=%u\n", len);
            OutputDebugStringA(msg);
            ++s_eventVerifyFailLogCount;
        }
        return;
    }
```

1-8. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxCuePlayer.cpp

기존 코드:

```cpp
    void LogMissingCue(const char* pszCueName)
    {
        char szBuffer[192]{};
        sprintf_s(szBuffer, "[FxCuePlayer] Missing cue: %s\n", pszCueName ? pszCueName : "(null)");
    }

    void LogSkippedCueEmitter(const char* pszCueName, const FxEmitterDesc& emitter)
    {
        char szBuffer[256]{};
        sprintf_s(szBuffer, "[FxCuePlayer] Skipped cue emitter cue=%s emitter=%s type=%u\n",
            pszCueName ? pszCueName : "(null)",
            emitter.strName.empty() ? "(unnamed)" : emitter.strName.c_str(),
            static_cast<u32_t>(emitter.renderType));
    }
```

아래로 교체:

```cpp
    void LogMissingCue(const char* pszCueName)
    {
        static u32_t s_missingCueLogCount = 0;
        if (s_missingCueLogCount >= 64)
            return;
        char szBuffer[192]{};
        sprintf_s(szBuffer, "[FxCuePlayer] Missing cue: %s\n", pszCueName ? pszCueName : "(null)");
        OutputDebugStringA(szBuffer);
        ++s_missingCueLogCount;
    }

    void LogSkippedCueEmitter(const char* pszCueName, const FxEmitterDesc& emitter)
    {
        static u32_t s_skippedEmitterLogCount = 0;
        if (s_skippedEmitterLogCount >= 64)
            return;
        char szBuffer[256]{};
        sprintf_s(szBuffer, "[FxCuePlayer] Skipped cue emitter cue=%s emitter=%s type=%u\n",
            pszCueName ? pszCueName : "(null)",
            emitter.strName.empty() ? "(unnamed)" : emitter.strName.c_str(),
            static_cast<u32_t>(emitter.renderType));
        OutputDebugStringA(szBuffer);
        ++s_skippedEmitterLogCount;
    }
```

근거: 두 함수 모두 포맷만 하고 출력 없는 no-op — 오탈자 cue 이름이 완전 침묵 no-op("호출은 됐는데 안 보임" 재발 패턴). gotcha 2026-05-26 cue 이름 발견 가능성 규칙 위반 해소.

1-9. C:/Users/user/Desktop/Winters/Client/Private/GameObject/ChampionSpawnService.cpp

`CChampionSpawnService::Spawn` 안에서 (파일은 탭 들여쓰기)

기존 코드:

```cpp
	const ChampionDef* pDef = FindSpawnChampionDef(request.champion);
	if (!pDef || !pDef->fbxPath)
	{
		return result;
	}

	std::unique_ptr<ModelRenderer> pRenderer = std::make_unique<ModelRenderer>();
	if (!pRenderer->Initialize(pDef->fbxPath, pDef->shaderPath))
	{
		return result;
	}
```

아래로 교체:

```cpp
	const ChampionDef* pDef = FindSpawnChampionDef(request.champion);
	if (!pDef || !pDef->fbxPath)
	{
		static u32_t s_spawnNoDefLogCount = 0;
		if (s_spawnNoDefLogCount < 8)
		{
			char msg[128]{};
			sprintf_s(msg, "[ChampionSpawn] FAILED champion=%u reason=no-def\n",
				static_cast<u32_t>(request.champion));
			OutputDebugStringA(msg);
			++s_spawnNoDefLogCount;
		}
		return result;
	}

	std::unique_ptr<ModelRenderer> pRenderer = std::make_unique<ModelRenderer>();
	if (!pRenderer->Initialize(pDef->fbxPath, pDef->shaderPath))
	{
		static u32_t s_spawnInitFailLogCount = 0;
		if (s_spawnInitFailLogCount < 8)
		{
			char msg[512]{};
			sprintf_s(msg, "[ChampionSpawn] FAILED champion=%u reason=renderer-init fbx=%ls\n",
				static_cast<u32_t>(request.champion),
				pDef->fbxPath);
			OutputDebugStringA(msg);
			++s_spawnInitFailLogCount;
		}
		return result;
	}
```

근거: def 누락/모델 초기화 실패가 NULL_ENTITY로만 반환 → SnapshotApplier가 매 스냅샷 무한 재시도(투명 챔피언, 로그 0). 1-6의 EnsureEntity 트레이스와 짝.

1-10. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLifecycle.cpp

`OnEnter` 의 Stage 로드 블록에서

기존 코드:

```cpp
    wchar_t stagePath[MAX_PATH] = {};
    if (CMapDataIO::Get_StagePathW(1, stagePath, MAX_PATH))
    {
        CMapDataIO::Load_Stage(stagePath);
        Winters::DevSmoke::Log("[InGameBootstrap] Stage1 loaded\n");
    }
```

아래로 교체:

```cpp
    wchar_t stagePath[MAX_PATH] = {};
    if (CMapDataIO::Get_StagePathW(1, stagePath, MAX_PATH))
    {
        const HRESULT hrStage = CMapDataIO::Load_Stage(stagePath);
        if (SUCCEEDED(hrStage))
        {
            Winters::DevSmoke::Log("[InGameBootstrap] Stage1 loaded\n");
        }
        else
        {
            char msg[384]{};
            sprintf_s(msg, "[InGameBootstrap] Stage1 load FAILED hr=0x%08X path=%ls\n",
                static_cast<unsigned>(hrStage), stagePath);
            OutputDebugStringA(msg);
        }
    }
    else
    {
        OutputDebugStringA("[InGameBootstrap] Stage1 path resolve FAILED\n");
    }
```

근거: Load_Stage는 HRESULT 반환(7개 E_FAIL 경로, 실패 시 매니저 전부 Clear 상태)인데 결과를 버리고 무조건 "Stage1 loaded" 출력 — 빈 맵 디버깅 시 로그가 거짓말함. 실패 분기는 DevSmoke(현재 no-op 스텁)가 아닌 OutputDebugStringA 직접 사용(실패는 항상 가시).

1-11. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Move/MoveSystem.cpp

waypoint 이동 루프의 회피 방향 소실 지점에서

기존 코드:

```cpp
        const f32_t dirLenSq = dir.x * dir.x + dir.z * dir.z;
        if (dirLenSq <= 0.0001f)
            continue;
```

아래로 교체:

```cpp
        const f32_t dirLenSq = dir.x * dir.x + dir.z * dir.z;
        if (dirLenSq <= 0.0001f)
        {
            static u32_t s_moveSystemStuckTraceCount = 0;
            if (s_moveSystemStuckTraceCount < 32u)
            {
                char msg[224]{};
                sprintf_s(msg,
                    "[MoveSystem][Stuck] tick=%llu entity=%u reason=avoidance-dead-end pos=(%.3f,%.3f,%.3f)\n",
                    static_cast<unsigned long long>(tc.tickIndex),
                    static_cast<u32_t>(entity),
                    pos.x,
                    pos.y,
                    pos.z);
                WintersOutputAIDebugStringA(msg);
                ++s_moveSystemStuckTraceCount;
            }
            continue;
        }
```

근거: ResolveAvoidedDirection이 7개 후보각 전부 차단 시 Vec3{}를 반환하면 bHasTarget 유지 + 무트레이스 continue로 매 틱 동일 실패 반복 — 과거 미니언 프리즈 사고와 동일 클래스인데 챔피언 경로에는 계측이 없음. 동작 변화 없음(트레이스만, 기존 s_moveSystemYawTraceCount 패턴과 동일 bounded 방식). 타겟 자동 해제 정책은 다음 슬라이스(설계 필요).

2. 검증

미검증:
- 런타임에서 각 실패 로그가 실제 실패 상황에 출력되는지 미검증 (실패 유도 필요)
- Session OnSendComplete 단절 정책의 부하 상황 동작 미검증

검증 명령:
- git diff --check
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m

기대 결과:
- 전 프로젝트 빌드 성공 (베이스라인과 동일)
- 정상 런타임(F5, 서버+클라 스모크)에서 새 로그 0줄 — 전부 실패 분기 한정

수동 확인:
- 존재하지 않는 cue 이름으로 스킬 FX 발동 시 `[FxCuePlayer] Missing cue:` 출력
- Stage1.dat 이름 변경 후 인게임 진입 시 `[InGameBootstrap] Stage1 load FAILED` 출력

후속 동기화:
- Engine public header 변경(MCTSSystem.h) 후 `UpdateLib.bat` 실행 필요 (Engine PostBuild가 자동 수행)

롤백 범위:
- 본 계획의 11개 파일 diff만 되돌리면 됨. 동작 변화는 1-1(Change_Scene E_FAIL 반환), 1-5(send 하드 실패 시 단절) 두 곳뿐이고 나머지는 실패 분기 로그 추가/죽은 코드 삭제.

다음 슬라이스 진행 상황 (2026-07-09 같은 날 후속 세션에서 1~4 완료):
1. ~~TransformComponent/VisionComponents/NavAgentComponent → Engine_Defines.h 체인 절단~~ 완료 — Shared TU 오염(dinput.h/using-namespace/global new/OutputDebugStringA 매크로) 제거. 파급 6개 TU는 Shared 소유 `Core/Debug/SimDebugOutput.h`로 해소.
2. ~~Phase 7F 어댑터 include 재라우팅~~ 완료 — `Shared/GameSim/Core/Ecs/` 어댑터 9종 신설, Shared 79개 파일 치환, `Tools/Harness/Check-SharedBoundary.ps1`을 GameSim PreBuild에 연결해 강제. **EngineSDK/inc include 경로 제거와 World=CWorld repoint는 Shared 소유 ECS 백엔드 설계 후** (`WINTERS_DEPENDENCY_MAP.md` §3).
3. ~~CHttpClient 가짜 async 재설계~~ 완료 — future 소유 + RequestSnapshot 복사 + 소멸자 드레인.
4. ~~Pathfinder 실패 reason enum~~ 완료 — `ePathFindResult` out-param + NavigationSystem 트레이스 노출. Server WalkabilityAuthority 결과 구조체 전파는 후속.
5. Change_Scene 실패 시 폴백 씬 정책 (미착수 — 제품 결정 필요)
6. MoveSystem avoidance dead-end 시 타겟 해제 정책 (미착수 — 게임플레이 튜닝 결정 필요)
7. SnapshotApplier 대형 yaw 계측 블록(minion yaw/local yaw) 재무장 or 삭제 결정 (미착수 — yaw 작업 세션에서)
8. Shared 소유 결정론 ECS 백엔드 + EngineSDK/inc 제거 (Phase 7F 최종 단계 — 별도 설계 세션)

검증 결과 (후속 슬라이스 포함, 2026-07-09):
- msbuild Winters.sln Debug x64: exit 0, 에러 0 (SharedBoundary lint PASS가 PreBuild에서 실행됨)
- WintersServer.exe --smoke-seconds=5: exit 0
- SimLab.exe 300 12345: PASS, same-seed hash BB6A67502987351F — 리팩터링 전과 동일 해시 (동작 무변경 증명)
