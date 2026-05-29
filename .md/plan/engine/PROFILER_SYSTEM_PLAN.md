# Winters Engine — AAA급 성능 프로파일링 시스템 구현 계획서

> **참고 엔진:** Unreal Insights (Trace 기반), Frostbite PerfOverlay, Naughty Dog Fiber Profiler,
> id Tech Timeline, Tracy/Optick (오픈소스), Chrome Tracing (JSON 포맷)

---

## 0. 아키텍처 총괄

```
┌──────────────────────────────────────────────────────────────────┐
│                    ProfilerAPI.h (공개 매크로)                    │
│  WINTERS_PROFILE_SCOPE / WINTERS_GPU_PROFILE_SCOPE / ...        │
├──────────────────────────────────────────────────────────────────┤
│                     CProfiler (마스터 싱글톤)                     │
│  ┌───────────┐ ┌───────────┐ ┌──────────────┐ ┌──────────────┐  │
│  │CCPUProfiler│ │CGPUProfiler│ │CMemoryProfiler│ │CJobProfiler │  │
│  └───────────┘ └───────────┘ └──────────────┘ └──────────────┘  │
│  ┌──────────────┐ ┌──────────────┐ ┌───────────────────┐        │
│  │CFrameInspector│ │CNetworkProfiler│ │CProfilerExporter │        │
│  └──────────────┘ └──────────────┘ └───────────────────┘        │
│  ┌──────────────┐                                                │
│  │CProfilerOverlay│  ← F3 토글, 실시간 온스크린 HUD             │
│  └──────────────┘                                                │
├──────────────────────────────────────────────────────────────────┤
│  CRingBuffer<T, N>  — 스레드별 SPSC 락프리 링 버퍼              │
│  ProfilerTypes.h    — 공용 enum, struct, 상수                    │
└──────────────────────────────────────────────────────────────────┘
```

### 레이어 배치: `01. Core\Profiler`

프로파일러는 **01. Core** 필터 내 서브필터로 배치한다.
이유: 모든 상위 레이어(03. Renderer, 05. ECS, 10. JobSystem 등)가
`WINTERS_PROFILE_SCOPE` 매크로를 호출할 수 있어야 하므로,
최하위 Core 레이어에 위치해야 순방향 의존성을 위반하지 않는다.

### 조건부 컴파일

```cpp
#ifdef WINTERS_PROFILING   // Debug/Development 빌드에서 정의
// 프로파일링 코드 활성
#else
// 모든 매크로 → ((void)0), Shipping 빌드 오버헤드 제로
#endif
```

---

## 1. 디렉토리 및 파일 구조

```
Engine/
  Include/
    ProfilerAPI.h                    ← 공개 API (DLL 경계, 매크로 정의)
  Header/
    Core/Profiler/
      ProfilerTypes.h                ← 공용 상수·열거형·구조체
      CRingBuffer.h                  ← 락프리 SPSC 링 버퍼 (헤더 온리 템플릿)
      CCPUProfiler.h                 ← 계층적 CPU 스코프 타이머
      CGPUProfiler.h                 ← DX11 타임스탬프 쿼리
      CMemoryProfiler.h             ← 할당 추적 + 누수 감지
      CFrameInspector.h             ← 프레임별 통계 집계 + 버짓 분석
      CNetworkProfiler.h            ← 대역폭/RTT (스텁, 09.Network 대기)
      CJobProfiler.h                ← JobSystem/ECS 시스템별 타이밍
      CProfilerOverlay.h            ← 온스크린 HUD (텍스트 기반 → ImGui)
      CProfilerExporter.h           ← Chrome Tracing JSON + CSV 내보내기
      CProfiler.h                   ← 마스터 싱글톤
  Code/
    Core/Profiler/
      CCPUProfiler.cpp
      CGPUProfiler.cpp
      CMemoryProfiler.cpp
      CFrameInspector.cpp
      CNetworkProfiler.cpp
      CJobProfiler.cpp
      CProfilerOverlay.cpp
      CProfilerExporter.cpp
      CProfiler.cpp
```

---

## 2. ProfilerTypes.h — 공용 데이터 타입

**경로:** `Engine/Header/Core/Profiler/ProfilerTypes.h`

```cpp
#pragma once
#include "WintersTypes.h"
#include <atomic>
#include <vector>
#include <string>

// ─────────────────────────────────────────────────────────────────
// 상수
// ─────────────────────────────────────────────────────────────────
constexpr uint32 PROFILER_MAX_EVENTS_PER_THREAD = 8192;   // 링 버퍼 용량
constexpr uint32 PROFILER_MAX_THREADS           = 32;
constexpr uint32 PROFILER_MAX_GPU_QUERIES       = 256;    // 프레임당 GPU 쿼리
constexpr uint32 PROFILER_FRAME_HISTORY         = 300;    // ~5초 @ 60fps
constexpr uint32 PROFILER_MAX_NAME_LENGTH       = 64;     // 이벤트 이름 최대 길이
constexpr uint32 PROFILER_GPU_FRAME_LATENCY     = 3;      // 트리플 버퍼링

// ─────────────────────────────────────────────────────────────────
// 프로파일링 카테고리 (시스템별 분류)
// ─────────────────────────────────────────────────────────────────
enum class EProfilerCategory : uint8
{
    General     = 0,
    Render      = 1,
    Physics     = 2,
    AI          = 3,
    Network     = 4,
    Audio       = 5,
    ECS         = 6,
    JobSystem   = 7,
    Input       = 8,
    Script      = 9,   // Lua
    Memory      = 10,
    GPU         = 11,
    Count
};

// ─────────────────────────────────────────────────────────────────
// 이벤트 타입
// ─────────────────────────────────────────────────────────────────
enum class EProfilerEventType : uint8
{
    Begin   = 0,    // 스코프 시작
    End     = 1,    // 스코프 종료
    Instant = 2     // 단발성 마커 (프레임 경계 등)
};

// ─────────────────────────────────────────────────────────────────
// CPU 프로파일링 이벤트 (per-thread 링 버퍼에 저장)
// ─────────────────────────────────────────────────────────────────
struct ProfilerEvent
{
    int64               timestamp;                          // QPC 틱
    char                name[PROFILER_MAX_NAME_LENGTH]{};   // 인라인 문자열 (힙 할당 없음)
    uint32              threadId    = 0;
    uint16              depth       = 0;                    // 중첩 깊이 (계층 구조)
    EProfilerCategory   category    = EProfilerCategory::General;
    EProfilerEventType  type        = EProfilerEventType::Begin;
};

// ─────────────────────────────────────────────────────────────────
// GPU 프로파일링 이벤트 (리졸브 완료 후)
// ─────────────────────────────────────────────────────────────────
struct GPUProfilerEvent
{
    char                name[PROFILER_MAX_NAME_LENGTH]{};
    float64             startMs     = 0.0;    // 프레임 시작 기준 밀리초
    float64             durationMs  = 0.0;
    EProfilerCategory   category    = EProfilerCategory::GPU;
};

// ─────────────────────────────────────────────────────────────────
// 메모리 이벤트
// ─────────────────────────────────────────────────────────────────
struct MemoryEvent
{
    void*               address      = nullptr;
    uint64              size         = 0;
    EProfilerCategory   category     = EProfilerCategory::Memory;
    bool                isAllocation = true;
    uint32              callsiteHash = 0;   // Hash(__FILE__:__LINE__)
};

// ─────────────────────────────────────────────────────────────────
// 프레임 통계 (프레임 인스펙터가 매 프레임 집계)
// ─────────────────────────────────────────────────────────────────
struct FrameStats
{
    uint64  frameIndex       = 0;
    float64 cpuFrameMs       = 0.0;    // 전체 CPU 프레임 시간
    float64 gpuFrameMs       = 0.0;    // 전체 GPU 프레임 시간
    float64 categoryMs[static_cast<uint32>(EProfilerCategory::Count)]{};
    uint64  memoryUsedBytes  = 0;
    uint64  memoryAllocCount = 0;
    uint32  drawCallCount    = 0;
    uint32  triangleCount    = 0;
    float64 networkBytesSent = 0.0;
    float64 networkBytesRecv = 0.0;
    float64 rttMs            = 0.0;
};

// ─────────────────────────────────────────────────────────────────
// 프레임 버짓 (경고/위험 임계값)
// ─────────────────────────────────────────────────────────────────
struct FrameBudget
{
    float64 targetMs   = 16.667;  // 60fps
    float64 warningMs  = 14.0;    // 84% 사용 시 노란색
    float64 criticalMs = 16.0;    // 96% 사용 시 빨간색
};
```

---

## 3. CRingBuffer — 락프리 SPSC 링 버퍼

**경로:** `Engine/Header/Core/Profiler/CRingBuffer.h`

**설계 근거:** 글로벌 MPSC 큐는 12+ 워커 스레드의 CAS 충돌로 캐시라인 바운싱 발생.
스레드별 SPSC 링 버퍼는 쓰기 시 경합 제로. Tracy, Optick, Unreal Insights 동일 접근법.

```cpp
#pragma once
#include "WintersTypes.h"
#include <atomic>
#include <array>

using std::atomic;
using std::memory_order_relaxed;
using std::memory_order_release;
using std::memory_order_acquire;

template<typename T, uint32 Capacity>
class CRingBuffer
{
public:
    CRingBuffer() = default;

    // ── Producer (소유 스레드에서만 호출, 락 불필요) ──────────
    bool Push(const T& item)
    {
        const uint32 writePos = m_writeIndex.load(memory_order_relaxed);
        const uint32 nextPos  = (writePos + 1) % Capacity;
        // 가득 차면 가장 오래된 데이터 덮어씀 (프로파일링에서 허용 가능)
        m_data[writePos] = item;
        m_writeIndex.store(nextPos, memory_order_release);
        return true;
    }

    // ── Consumer (집계 스레드, 프레임 끝에서 호출) ────────────
    // 사용 가능한 모든 이벤트를 드레인하여 func에 전달
    template<typename Func>
    uint32 DrainAll(Func&& func)
    {
        uint32 readPos  = m_readIndex;
        uint32 writePos = m_writeIndex.load(memory_order_acquire);
        uint32 count    = 0;

        while (readPos != writePos)
        {
            func(m_data[readPos]);
            readPos = (readPos + 1) % Capacity;
            ++count;
        }
        m_readIndex = readPos;
        return count;
    }

    uint32 Size() const
    {
        const uint32 w = m_writeIndex.load(memory_order_acquire);
        const uint32 r = m_readIndex;
        return (w >= r) ? (w - r) : (Capacity - r + w);
    }

    void Clear()
    {
        m_readIndex = 0;
        m_writeIndex.store(0, memory_order_release);
    }

private:
    std::array<T, Capacity>  m_data{};
    uint32                   m_readIndex = 0;        // Consumer만 접근
    atomic<uint32>           m_writeIndex{ 0 };      // Producer 쓰기, Consumer 읽기
};
```

---

## 4. CCPUProfiler — 계층적 스코프 CPU 타이머

**경로:** `Engine/Header/Core/Profiler/CCPUProfiler.h`

Unreal의 `SCOPE_CYCLE_COUNTER`와 동일한 패턴. RAII 가드로 스코프 진입/퇴장 시
자동으로 Begin/End 이벤트를 스레드 로컬 링 버퍼에 기록.

```cpp
#pragma once
#include "Core/Profiler/ProfilerTypes.h"
#include "Core/Profiler/CRingBuffer.h"
#include <mutex>
#include <vector>

using std::mutex;
using std::vector;
using std::lock_guard;

// ─────────────────────────────────────────────────────────────────
// 스레드별 프로파일러 상태
// ─────────────────────────────────────────────────────────────────
struct ThreadProfilerState
{
    CRingBuffer<ProfilerEvent, PROFILER_MAX_EVENTS_PER_THREAD> ringBuffer;
    uint16 currentDepth = 0;
    uint32 threadId     = 0;
};

// ─────────────────────────────────────────────────────────────────
class CCPUProfiler
{
public:
    void Initialize();
    void Shutdown();

    // ── 계측 API (모든 스레드에서 호출 가능) ─────────────────
    void BeginEvent(const char* name, EProfilerCategory category);
    void EndEvent();
    void InstantEvent(const char* name, EProfilerCategory category);

    // ── 프레임 경계 (메인 스레드, 프레임 끝) ─────────────────
    void CollectFrameEvents(vector<ProfilerEvent>& outEvents);

    // ── QPC 유틸 ─────────────────────────────────────────────
    static int64   GetTimestamp();
    float64        TicksToMs(int64 ticks) const;

private:
    ThreadProfilerState& GetThreadState();

    int64 m_frequency = 0;   // QueryPerformanceFrequency

    // thread_local로 스레드별 상태 접근 (첫 접근 시 할당, 등록)
    static thread_local ThreadProfilerState* t_pState;

    mutex                       m_mtxThreadStates;
    vector<ThreadProfilerState*> m_vecAllThreadStates;   // 수집용
};

// ─────────────────────────────────────────────────────────────────
// RAII 스코프 가드
// ─────────────────────────────────────────────────────────────────
class CScopedCPUTimer
{
public:
    CScopedCPUTimer(const char* name, EProfilerCategory category,
                    CCPUProfiler& profiler)
        : m_profiler(profiler)
    {
        m_profiler.BeginEvent(name, category);
    }

    ~CScopedCPUTimer()
    {
        m_profiler.EndEvent();
    }

    CScopedCPUTimer(const CScopedCPUTimer&)            = delete;
    CScopedCPUTimer& operator=(const CScopedCPUTimer&) = delete;

private:
    CCPUProfiler& m_profiler;
};
```

**구현:** `Engine/Code/Core/Profiler/CCPUProfiler.cpp`

```cpp
#include "WintersPCH.h"
#include "Core/Profiler/CCPUProfiler.h"
#include <cstring>   // strncpy_s

#ifdef WINTERS_PROFILING

thread_local ThreadProfilerState* CCPUProfiler::t_pState = nullptr;

void CCPUProfiler::Initialize()
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_frequency = freq.QuadPart;
}

void CCPUProfiler::Shutdown()
{
    lock_guard<mutex> lock(m_mtxThreadStates);
    for (auto* state : m_vecAllThreadStates)
        delete state;
    m_vecAllThreadStates.clear();
}

ThreadProfilerState& CCPUProfiler::GetThreadState()
{
    if (!t_pState)
    {
        t_pState = new ThreadProfilerState{};
        t_pState->threadId = GetCurrentThreadId();

        lock_guard<mutex> lock(m_mtxThreadStates);
        m_vecAllThreadStates.push_back(t_pState);
    }
    return *t_pState;
}

int64 CCPUProfiler::GetTimestamp()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

float64 CCPUProfiler::TicksToMs(int64 ticks) const
{
    return (static_cast<float64>(ticks) / static_cast<float64>(m_frequency)) * 1000.0;
}

void CCPUProfiler::BeginEvent(const char* name, EProfilerCategory category)
{
    auto& state = GetThreadState();

    ProfilerEvent evt{};
    evt.timestamp = GetTimestamp();
    strncpy_s(evt.name, name, PROFILER_MAX_NAME_LENGTH - 1);
    evt.threadId  = state.threadId;
    evt.depth     = state.currentDepth;
    evt.category  = category;
    evt.type      = EProfilerEventType::Begin;

    state.ringBuffer.Push(evt);
    ++state.currentDepth;
}

void CCPUProfiler::EndEvent()
{
    auto& state = GetThreadState();
    if (state.currentDepth > 0)
        --state.currentDepth;

    ProfilerEvent evt{};
    evt.timestamp = GetTimestamp();
    evt.name[0]   = '\0';   // End 이벤트는 이름 불필요 (Begin과 depth로 매칭)
    evt.threadId  = state.threadId;
    evt.depth     = state.currentDepth;
    evt.category  = EProfilerCategory::General;
    evt.type      = EProfilerEventType::End;

    state.ringBuffer.Push(evt);
}

void CCPUProfiler::InstantEvent(const char* name, EProfilerCategory category)
{
    auto& state = GetThreadState();

    ProfilerEvent evt{};
    evt.timestamp = GetTimestamp();
    strncpy_s(evt.name, name, PROFILER_MAX_NAME_LENGTH - 1);
    evt.threadId  = state.threadId;
    evt.depth     = state.currentDepth;
    evt.category  = category;
    evt.type      = EProfilerEventType::Instant;

    state.ringBuffer.Push(evt);
}

void CCPUProfiler::CollectFrameEvents(vector<ProfilerEvent>& outEvents)
{
    lock_guard<mutex> lock(m_mtxThreadStates);
    for (auto* state : m_vecAllThreadStates)
    {
        state->ringBuffer.DrainAll([&outEvents](const ProfilerEvent& evt)
        {
            outEvents.push_back(evt);
        });
    }
}

#endif // WINTERS_PROFILING
```

---

## 5. CGPUProfiler — DX11 타임스탬프 쿼리

**경로:** `Engine/Header/Core/Profiler/CGPUProfiler.h`

**설계:** D3D11_QUERY_TIMESTAMP + D3D11_QUERY_TIMESTAMP_DISJOINT 사용.
트리플 버퍼링: 현재 프레임(N) 기록, N-2 프레임 읽기. GPU가 2프레임 이상
앞서 실행을 완료하므로 GetData 스톨 없음. Frostbite 동일 접근법.

```cpp
#pragma once
#include "Core/Profiler/ProfilerTypes.h"

// DX11 전방 선언 (CDX11Device.h 포함하지 않음 → 의존성 규칙 준수)
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Query;

// ─────────────────────────────────────────────────────────────────
// GPU 쿼리 쌍 (Begin/End 타임스탬프)
// ─────────────────────────────────────────────────────────────────
struct GPUQueryPair
{
    ID3D11Query* pBeginQuery = nullptr;
    ID3D11Query* pEndQuery   = nullptr;
    char         name[PROFILER_MAX_NAME_LENGTH]{};
    EProfilerCategory category = EProfilerCategory::GPU;
    bool         active      = false;
};

// ─────────────────────────────────────────────────────────────────
// 프레임별 쿼리 세트
// ─────────────────────────────────────────────────────────────────
struct GPUFrameQueries
{
    ID3D11Query*  pDisjointQuery = nullptr;
    GPUQueryPair  queries[PROFILER_MAX_GPU_QUERIES]{};
    uint32        queryCount = 0;
    bool          submitted  = false;
};

// ─────────────────────────────────────────────────────────────────
class CGPUProfiler
{
public:
    void Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    void Shutdown();

    // ── 프레임 경계 ──────────────────────────────────────────
    void BeginFrame();    // Disjoint 쿼리 시작
    void EndFrame();      // Disjoint 쿼리 종료

    // ── 스코프 GPU 타이밍 ────────────────────────────────────
    void BeginGPUEvent(const char* name,
                       EProfilerCategory category = EProfilerCategory::GPU);
    void EndGPUEvent();

    // ── 결과 수집 (N-2 프레임, 스톨 없음) ────────────────────
    bool CollectResults(vector<GPUProfilerEvent>& outEvents);

    float64 GetLastFrameGPUTimeMs() const { return m_lastFrameGPUMs; }

private:
    bool CreateQueries(GPUFrameQueries& frame);
    void DestroyQueries(GPUFrameQueries& frame);

    ID3D11Device*        m_pDevice  = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;

    GPUFrameQueries m_frames[PROFILER_GPU_FRAME_LATENCY]{};
    uint32          m_currentFrame     = 0;
    uint32          m_activeQueryIndex = 0;

    float64         m_lastFrameGPUMs = 0.0;
};

// ─────────────────────────────────────────────────────────────────
// RAII GPU 스코프 가드
// ─────────────────────────────────────────────────────────────────
class CScopedGPUTimer
{
public:
    CScopedGPUTimer(const char* name, EProfilerCategory category,
                    CGPUProfiler& profiler)
        : m_profiler(profiler)
    {
        m_profiler.BeginGPUEvent(name, category);
    }

    ~CScopedGPUTimer()
    {
        m_profiler.EndGPUEvent();
    }

    CScopedGPUTimer(const CScopedGPUTimer&)            = delete;
    CScopedGPUTimer& operator=(const CScopedGPUTimer&) = delete;

private:
    CGPUProfiler& m_profiler;
};
```

**구현:** `Engine/Code/Core/Profiler/CGPUProfiler.cpp`

```cpp
#include "WintersPCH.h"
#include "Core/Profiler/CGPUProfiler.h"
#include <d3d11.h>
#include <cstring>

#ifdef WINTERS_PROFILING

void CGPUProfiler::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    m_pDevice  = pDevice;
    m_pContext = pContext;

    for (uint32 i = 0; i < PROFILER_GPU_FRAME_LATENCY; ++i)
        CreateQueries(m_frames[i]);
}

void CGPUProfiler::Shutdown()
{
    for (uint32 i = 0; i < PROFILER_GPU_FRAME_LATENCY; ++i)
        DestroyQueries(m_frames[i]);
    m_pDevice  = nullptr;
    m_pContext = nullptr;
}

bool CGPUProfiler::CreateQueries(GPUFrameQueries& frame)
{
    // Disjoint 쿼리 생성
    D3D11_QUERY_DESC desc{};
    desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    HRESULT hr = m_pDevice->CreateQuery(&desc, &frame.pDisjointQuery);
    if (FAILED(hr)) return false;

    // 타임스탬프 쿼리 쌍 생성
    desc.Query = D3D11_QUERY_TIMESTAMP;
    for (uint32 i = 0; i < PROFILER_MAX_GPU_QUERIES; ++i)
    {
        hr = m_pDevice->CreateQuery(&desc, &frame.queries[i].pBeginQuery);
        if (FAILED(hr)) return false;
        hr = m_pDevice->CreateQuery(&desc, &frame.queries[i].pEndQuery);
        if (FAILED(hr)) return false;
    }
    return true;
}

void CGPUProfiler::DestroyQueries(GPUFrameQueries& frame)
{
    if (frame.pDisjointQuery) { frame.pDisjointQuery->Release(); frame.pDisjointQuery = nullptr; }
    for (uint32 i = 0; i < PROFILER_MAX_GPU_QUERIES; ++i)
    {
        if (frame.queries[i].pBeginQuery) { frame.queries[i].pBeginQuery->Release(); frame.queries[i].pBeginQuery = nullptr; }
        if (frame.queries[i].pEndQuery)   { frame.queries[i].pEndQuery->Release();   frame.queries[i].pEndQuery   = nullptr; }
    }
}

void CGPUProfiler::BeginFrame()
{
    auto& frame = m_frames[m_currentFrame];
    frame.queryCount = 0;
    frame.submitted  = false;
    m_activeQueryIndex = 0;

    m_pContext->Begin(frame.pDisjointQuery);
}

void CGPUProfiler::EndFrame()
{
    auto& frame = m_frames[m_currentFrame];
    m_pContext->End(frame.pDisjointQuery);
    frame.submitted = true;

    m_currentFrame = (m_currentFrame + 1) % PROFILER_GPU_FRAME_LATENCY;
}

void CGPUProfiler::BeginGPUEvent(const char* name, EProfilerCategory category)
{
    auto& frame = m_frames[m_currentFrame];
    if (frame.queryCount >= PROFILER_MAX_GPU_QUERIES) return;

    auto& pair = frame.queries[frame.queryCount];
    strncpy_s(pair.name, name, PROFILER_MAX_NAME_LENGTH - 1);
    pair.category = category;
    pair.active   = true;

    // DX11 타임스탬프 쿼리는 End()로 기록 (Begin 아님)
    m_pContext->End(pair.pBeginQuery);
    m_activeQueryIndex = frame.queryCount;
}

void CGPUProfiler::EndGPUEvent()
{
    auto& frame = m_frames[m_currentFrame];
    if (m_activeQueryIndex >= PROFILER_MAX_GPU_QUERIES) return;

    auto& pair = frame.queries[m_activeQueryIndex];
    m_pContext->End(pair.pEndQuery);

    ++frame.queryCount;
}

bool CGPUProfiler::CollectResults(vector<GPUProfilerEvent>& outEvents)
{
    // N-LATENCY 프레임 읽기 (트리플 버퍼링)
    uint32 readFrame = (m_currentFrame + PROFILER_GPU_FRAME_LATENCY - PROFILER_GPU_FRAME_LATENCY)
                       % PROFILER_GPU_FRAME_LATENCY;
    // 첫 LATENCY 프레임은 아직 제출 안 됐을 수 있음
    // 보정: 가장 오래된 제출 완료 프레임 읽기
    readFrame = (m_currentFrame) % PROFILER_GPU_FRAME_LATENCY;  // circular: 현재 위치가 가장 오래됨

    auto& frame = m_frames[readFrame];
    if (!frame.submitted || frame.queryCount == 0) return false;

    // Disjoint 결과 가져오기
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData{};
    HRESULT hr = m_pContext->GetData(frame.pDisjointQuery, &disjointData,
                                     sizeof(disjointData), D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (hr != S_OK) return false;   // 아직 준비 안 됨
    if (disjointData.Disjoint) return false;  // 타이밍 불안정 (주파수 변동)

    const float64 gpuFreq = static_cast<float64>(disjointData.Frequency);

    // 프레임 시작 타임스탬프 (첫 번째 쿼리의 Begin)
    uint64 frameStartTick = 0;
    m_pContext->GetData(frame.queries[0].pBeginQuery, &frameStartTick,
                        sizeof(uint64), D3D11_ASYNC_GETDATA_DONOTFLUSH);

    float64 totalGpuMs = 0.0;

    for (uint32 i = 0; i < frame.queryCount; ++i)
    {
        auto& pair = frame.queries[i];
        if (!pair.active) continue;

        uint64 beginTick = 0, endTick = 0;
        hr = m_pContext->GetData(pair.pBeginQuery, &beginTick, sizeof(uint64),
                                 D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (hr != S_OK) continue;
        hr = m_pContext->GetData(pair.pEndQuery, &endTick, sizeof(uint64),
                                 D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (hr != S_OK) continue;

        GPUProfilerEvent gpuEvt{};
        strncpy_s(gpuEvt.name, pair.name, PROFILER_MAX_NAME_LENGTH - 1);
        gpuEvt.startMs    = (static_cast<float64>(beginTick - frameStartTick) / gpuFreq) * 1000.0;
        gpuEvt.durationMs = (static_cast<float64>(endTick - beginTick) / gpuFreq) * 1000.0;
        gpuEvt.category   = pair.category;

        totalGpuMs += gpuEvt.durationMs;
        outEvents.push_back(gpuEvt);

        pair.active = false;
    }

    m_lastFrameGPUMs = totalGpuMs;
    frame.submitted  = false;
    return true;
}

#endif // WINTERS_PROFILING
```

---

## 6. CMemoryProfiler — 할당 추적 + 누수 감지

**경로:** `Engine/Header/Core/Profiler/CMemoryProfiler.h`

```cpp
#pragma once
#include "Core/Profiler/ProfilerTypes.h"
#include <mutex>
#include <unordered_map>
#include <atomic>

using std::atomic;
using std::mutex;
using std::lock_guard;
using std::unordered_map;

// ─────────────────────────────────────────────────────────────────
// 카테고리별 메모리 통계 (Atomic → 락프리)
// ─────────────────────────────────────────────────────────────────
struct MemoryCategoryStats
{
    atomic<int64>  currentBytes{ 0 };
    atomic<int64>  peakBytes{ 0 };           // 하이워터마크
    atomic<uint64> totalAllocCount{ 0 };
    atomic<uint64> totalFreeCount{ 0 };
};

// ─────────────────────────────────────────────────────────────────
// 개별 할당 기록 (누수 감지용)
// ─────────────────────────────────────────────────────────────────
struct AllocationRecord
{
    uint64            size         = 0;
    EProfilerCategory category     = EProfilerCategory::Memory;
    uint32            callsiteHash = 0;
};

// ─────────────────────────────────────────────────────────────────
class CMemoryProfiler
{
public:
    void Initialize();
    void Shutdown();

    // ── 할당/해제 추적 (커스텀 할당자에서 호출) ──────────────
    void OnAlloc(void* address, uint64 size, EProfilerCategory category,
                 const char* file, int32 line);
    void OnFree(void* address);

    // ── 통계 접근 ────────────────────────────────────────────
    const MemoryCategoryStats& GetCategoryStats(EProfilerCategory cat) const;
    uint64 GetTotalUsedBytes() const;
    uint64 GetTotalAllocCount() const;

    // ── 누수 감지 (Shutdown 시 호출) ─────────────────────────
    uint32 ReportLeaks();   // 미해제 할당 수 반환

    // ── 카테고리별 버짓 ──────────────────────────────────────
    void SetBudget(EProfilerCategory category, uint64 maxBytes);
    bool IsOverBudget(EProfilerCategory category) const;

private:
    MemoryCategoryStats m_categoryStats[static_cast<uint32>(EProfilerCategory::Count)]{};
    uint64              m_budgets[static_cast<uint32>(EProfilerCategory::Count)]{};

    // 활성 할당 맵 (Debug/Dev 빌드 전용, 누수 감지)
    mutex                                      m_mtxAllocations;
    unordered_map<uintptr_t, AllocationRecord>  m_activeAllocations;
};

// ─────────────────────────────────────────────────────────────────
// 매크로
// ─────────────────────────────────────────────────────────────────
#ifdef WINTERS_PROFILING
#define WINTERS_ALLOC_TRACKED(profiler, ptr, size, cat) \
    (profiler).OnAlloc((ptr), (size), (cat), __FILE__, __LINE__)
#define WINTERS_FREE_TRACKED(profiler, ptr) \
    (profiler).OnFree((ptr))
#else
#define WINTERS_ALLOC_TRACKED(profiler, ptr, size, cat) ((void)0)
#define WINTERS_FREE_TRACKED(profiler, ptr)             ((void)0)
#endif
```

**구현:** `Engine/Code/Core/Profiler/CMemoryProfiler.cpp`

```cpp
#include "WintersPCH.h"
#include "Core/Profiler/CMemoryProfiler.h"

#ifdef WINTERS_PROFILING

void CMemoryProfiler::Initialize()
{
    for (auto& stat : m_categoryStats)
    {
        stat.currentBytes.store(0);
        stat.peakBytes.store(0);
        stat.totalAllocCount.store(0);
        stat.totalFreeCount.store(0);
    }
    std::fill(std::begin(m_budgets), std::end(m_budgets), 0);
}

void CMemoryProfiler::Shutdown()
{
    ReportLeaks();
    lock_guard<mutex> lock(m_mtxAllocations);
    m_activeAllocations.clear();
}

void CMemoryProfiler::OnAlloc(void* address, uint64 size,
                                EProfilerCategory category,
                                const char* file, int32 line)
{
    const uint32 catIdx = static_cast<uint32>(category);
    auto& stat = m_categoryStats[catIdx];

    const int64 newCurrent = stat.currentBytes.fetch_add(
        static_cast<int64>(size), std::memory_order_relaxed) + static_cast<int64>(size);

    // 하이워터마크 갱신 (CAS 루프)
    int64 peak = stat.peakBytes.load(std::memory_order_relaxed);
    while (newCurrent > peak)
    {
        if (stat.peakBytes.compare_exchange_weak(peak, newCurrent,
            std::memory_order_relaxed)) break;
    }

    stat.totalAllocCount.fetch_add(1, std::memory_order_relaxed);

    // 활성 할당 맵에 등록
    AllocationRecord record{};
    record.size         = size;
    record.category     = category;
    record.callsiteHash = static_cast<uint32>(
        std::hash<std::string>{}(std::string(file) + ":" + std::to_string(line)));

    lock_guard<mutex> lock(m_mtxAllocations);
    m_activeAllocations[reinterpret_cast<uintptr_t>(address)] = record;
}

void CMemoryProfiler::OnFree(void* address)
{
    lock_guard<mutex> lock(m_mtxAllocations);
    auto it = m_activeAllocations.find(reinterpret_cast<uintptr_t>(address));
    if (it == m_activeAllocations.end()) return;

    const auto& record = it->second;
    const uint32 catIdx = static_cast<uint32>(record.category);
    m_categoryStats[catIdx].currentBytes.fetch_sub(
        static_cast<int64>(record.size), std::memory_order_relaxed);
    m_categoryStats[catIdx].totalFreeCount.fetch_add(1, std::memory_order_relaxed);

    m_activeAllocations.erase(it);
}

const MemoryCategoryStats& CMemoryProfiler::GetCategoryStats(EProfilerCategory cat) const
{
    return m_categoryStats[static_cast<uint32>(cat)];
}

uint64 CMemoryProfiler::GetTotalUsedBytes() const
{
    uint64 total = 0;
    for (const auto& stat : m_categoryStats)
        total += static_cast<uint64>(stat.currentBytes.load(std::memory_order_relaxed));
    return total;
}

uint64 CMemoryProfiler::GetTotalAllocCount() const
{
    uint64 total = 0;
    for (const auto& stat : m_categoryStats)
        total += stat.totalAllocCount.load(std::memory_order_relaxed);
    return total;
}

uint32 CMemoryProfiler::ReportLeaks()
{
    lock_guard<mutex> lock(m_mtxAllocations);
    const uint32 leakCount = static_cast<uint32>(m_activeAllocations.size());

    if (leakCount > 0)
    {
        char buf[256];
        sprintf_s(buf, "[WintersProfiler] MEMORY LEAK: %u unfreed allocations!\n", leakCount);
        OutputDebugStringA(buf);

        uint32 shown = 0;
        for (const auto& [addr, record] : m_activeAllocations)
        {
            if (shown++ >= 20) { OutputDebugStringA("  ... (truncated)\n"); break; }
            sprintf_s(buf, "  Leak: addr=0x%p, size=%llu bytes, category=%u\n",
                      reinterpret_cast<void*>(addr), record.size,
                      static_cast<uint32>(record.category));
            OutputDebugStringA(buf);
        }
    }
    return leakCount;
}

void CMemoryProfiler::SetBudget(EProfilerCategory category, uint64 maxBytes)
{
    m_budgets[static_cast<uint32>(category)] = maxBytes;
}

bool CMemoryProfiler::IsOverBudget(EProfilerCategory category) const
{
    const uint32 idx = static_cast<uint32>(category);
    if (m_budgets[idx] == 0) return false;  // 버짓 미설정
    return static_cast<uint64>(
        m_categoryStats[idx].currentBytes.load(std::memory_order_relaxed))
        > m_budgets[idx];
}

#endif // WINTERS_PROFILING
```

---

## 7. CFrameInspector — 프레임별 통계 집계

**경로:** `Engine/Header/Core/Profiler/CFrameInspector.h`

```cpp
#pragma once
#include "Core/Profiler/ProfilerTypes.h"
#include <span>

using std::vector;
using std::span;

// ─────────────────────────────────────────────────────────────────
// 프레임 타임라인 (시각화용 원본 이벤트 보관)
// ─────────────────────────────────────────────────────────────────
struct FrameTimeline
{
    vector<ProfilerEvent>     cpuEvents;
    vector<GPUProfilerEvent>  gpuEvents;
};

// ─────────────────────────────────────────────────────────────────
class CFrameInspector
{
public:
    void Initialize();

    // ── 매 프레임 호출 (수집된 데이터 전달) ──────────────────
    void ProcessFrame(uint64 frameIndex,
                      const vector<ProfilerEvent>& cpuEvents,
                      const vector<GPUProfilerEvent>& gpuEvents,
                      uint64 memUsedBytes, uint64 memAllocCount);

    // ── 통계 접근 ────────────────────────────────────────────
    const FrameStats& GetCurrentFrameStats() const;
    const FrameStats& GetFrameStats(uint32 framesAgo) const;
    span<const FrameStats> GetFrameHistory() const;

    // ── 버짓 ─────────────────────────────────────────────────
    void SetBudget(const FrameBudget& budget);
    bool IsOverBudget() const;
    bool IsWarning() const;

    const FrameTimeline& GetCurrentTimeline() const { return m_currentTimeline; }

private:
    void AggregateStats(const vector<ProfilerEvent>& cpuEvents,
                        const vector<GPUProfilerEvent>& gpuEvents,
                        FrameStats& outStats);

    FrameStats    m_frameHistory[PROFILER_FRAME_HISTORY]{};
    FrameTimeline m_currentTimeline;
    uint32        m_historyIndex = 0;
    uint32        m_frameCount   = 0;
    FrameBudget   m_budget;
};
```

**구현:** `Engine/Code/Core/Profiler/CFrameInspector.cpp`

```cpp
#include "WintersPCH.h"
#include "Core/Profiler/CFrameInspector.h"
#include "Core/Profiler/CCPUProfiler.h"
#include <algorithm>

#ifdef WINTERS_PROFILING

void CFrameInspector::Initialize()
{
    std::fill(std::begin(m_frameHistory), std::end(m_frameHistory), FrameStats{});
    m_historyIndex = 0;
    m_frameCount   = 0;
}

void CFrameInspector::ProcessFrame(uint64 frameIndex,
                                    const vector<ProfilerEvent>& cpuEvents,
                                    const vector<GPUProfilerEvent>& gpuEvents,
                                    uint64 memUsedBytes, uint64 memAllocCount)
{
    auto& stats = m_frameHistory[m_historyIndex];
    stats = FrameStats{};
    stats.frameIndex       = frameIndex;
    stats.memoryUsedBytes  = memUsedBytes;
    stats.memoryAllocCount = memAllocCount;

    AggregateStats(cpuEvents, gpuEvents, stats);

    // 타임라인 보관
    m_currentTimeline.cpuEvents = cpuEvents;
    m_currentTimeline.gpuEvents = gpuEvents;

    m_historyIndex = (m_historyIndex + 1) % PROFILER_FRAME_HISTORY;
    ++m_frameCount;
}

void CFrameInspector::AggregateStats(const vector<ProfilerEvent>& cpuEvents,
                                      const vector<GPUProfilerEvent>& gpuEvents,
                                      FrameStats& outStats)
{
    // CPU 카테고리별 시간 집계 (Begin/End 쌍 매칭)
    // 최상위(depth==0) Begin~End 사이의 시간을 카테고리에 누적
    struct PendingScope { int64 startTick; EProfilerCategory category; };
    std::vector<PendingScope> stack;

    int64 frameStartTick = 0;
    int64 frameEndTick   = 0;

    for (const auto& evt : cpuEvents)
    {
        if (evt.type == EProfilerEventType::Begin)
        {
            if (frameStartTick == 0) frameStartTick = evt.timestamp;
            stack.push_back({ evt.timestamp, evt.category });
        }
        else if (evt.type == EProfilerEventType::End && !stack.empty())
        {
            auto& scope = stack.back();
            const int64 duration = evt.timestamp - scope.startTick;
            const uint32 catIdx  = static_cast<uint32>(scope.category);

            // 최상위 스코프만 카테고리에 누적 (이중 계산 방지)
            if (stack.size() == 1)
                outStats.categoryMs[catIdx] += CCPUProfiler::GetTimestamp(); // placeholder
            // 실제: TicksToMs 변환 필요 (CProfiler 경유)

            stack.pop_back();
            frameEndTick = evt.timestamp;
        }
    }

    // TODO: TicksToMs 변환은 CProfiler::EndFrame에서 frequency 전달받아 처리

    // GPU 총 시간
    float64 gpuTotal = 0.0;
    for (const auto& gpu : gpuEvents)
        gpuTotal += gpu.durationMs;
    outStats.gpuFrameMs = gpuTotal;
}

const FrameStats& CFrameInspector::GetCurrentFrameStats() const
{
    uint32 idx = (m_historyIndex == 0) ? (PROFILER_FRAME_HISTORY - 1) : (m_historyIndex - 1);
    return m_frameHistory[idx];
}

const FrameStats& CFrameInspector::GetFrameStats(uint32 framesAgo) const
{
    if (framesAgo >= PROFILER_FRAME_HISTORY) framesAgo = PROFILER_FRAME_HISTORY - 1;
    uint32 idx = (m_historyIndex + PROFILER_FRAME_HISTORY - 1 - framesAgo) % PROFILER_FRAME_HISTORY;
    return m_frameHistory[idx];
}

span<const FrameStats> CFrameInspector::GetFrameHistory() const
{
    return span<const FrameStats>(m_frameHistory, PROFILER_FRAME_HISTORY);
}

void CFrameInspector::SetBudget(const FrameBudget& budget)
{
    m_budget = budget;
}

bool CFrameInspector::IsOverBudget() const
{
    const auto& stats = GetCurrentFrameStats();
    const float64 maxMs = std::max(stats.cpuFrameMs, stats.gpuFrameMs);
    return maxMs > m_budget.targetMs;
}

bool CFrameInspector::IsWarning() const
{
    const auto& stats = GetCurrentFrameStats();
    const float64 maxMs = std::max(stats.cpuFrameMs, stats.gpuFrameMs);
    return maxMs > m_budget.warningMs && maxMs <= m_budget.targetMs;
}

#endif // WINTERS_PROFILING
```

---

## 8. CJobProfiler — JobSystem/ECS 시스템 타이밍

**경로:** `Engine/Header/Core/Profiler/CJobProfiler.h`

```cpp
#pragma once
#include "Core/Profiler/ProfilerTypes.h"
#include <mutex>
#include <vector>
#include <atomic>

using std::atomic;
using std::mutex;
using std::lock_guard;
using std::vector;

// ─────────────────────────────────────────────────────────────────
// ECS 시스템별 타이밍 기록
// ─────────────────────────────────────────────────────────────────
struct SystemTimingRecord
{
    char    name[PROFILER_MAX_NAME_LENGTH]{};
    float64 durationMs      = 0.0;
    uint32  phase           = 0;
    uint32  executionThread = 0;
};

// ─────────────────────────────────────────────────────────────────
// JobSystem 전체 통계
// ─────────────────────────────────────────────────────────────────
struct JobSystemStats
{
    uint32  activeThreads           = 0;
    uint32  totalThreads            = 0;
    float64 utilizationPercent      = 0.0;    // (busy / total) × 100
    uint32  queueDepth              = 0;
    uint32  jobsCompletedThisFrame  = 0;
    vector<SystemTimingRecord> systemTimings;
};

// ─────────────────────────────────────────────────────────────────
class CJobProfiler
{
public:
    void Initialize(uint32 workerThreadCount);
    void Shutdown();

    // ── ECS 시스템 타이밍 (SystemScheduler에서 호출) ─────────
    void BeginSystemTiming(const char* systemName, uint32 phase);
    void EndSystemTiming();

    // ── JobSystem 워커 타이밍 ────────────────────────────────
    void OnJobBegin(uint32 threadIndex);
    void OnJobEnd(uint32 threadIndex);
    void RecordQueueDepth(uint32 depth);

    // ── 프레임 집계 ──────────────────────────────────────────
    void EndFrame();

    const JobSystemStats& GetStats() const { return m_stats; }

private:
    struct ThreadBusyTracker
    {
        atomic<int64> busyTicks{ 0 };
        int64         frameStartTick = 0;
        int64         jobStartTick   = 0;
    };

    JobSystemStats              m_stats{};
    vector<ThreadBusyTracker>   m_threadTrackers;
    int64                       m_frequency = 0;

    // 시스템 타이밍 수집
    static thread_local char    t_systemName[PROFILER_MAX_NAME_LENGTH];
    static thread_local int64   t_systemStartTick;
    static thread_local uint32  t_systemPhase;

    mutex                       m_mtxSystemTimings;
    vector<SystemTimingRecord>  m_pendingTimings;
};
```

**구현:** `Engine/Code/Core/Profiler/CJobProfiler.cpp`

```cpp
#include "WintersPCH.h"
#include "Core/Profiler/CJobProfiler.h"
#include "Core/Profiler/CCPUProfiler.h"
#include <cstring>

#ifdef WINTERS_PROFILING

thread_local char   CJobProfiler::t_systemName[PROFILER_MAX_NAME_LENGTH] = {};
thread_local int64  CJobProfiler::t_systemStartTick = 0;
thread_local uint32 CJobProfiler::t_systemPhase = 0;

void CJobProfiler::Initialize(uint32 workerThreadCount)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_frequency = freq.QuadPart;

    m_threadTrackers.resize(workerThreadCount);
    m_stats.totalThreads = workerThreadCount;
}

void CJobProfiler::Shutdown()
{
    m_threadTrackers.clear();
    m_pendingTimings.clear();
}

void CJobProfiler::BeginSystemTiming(const char* systemName, uint32 phase)
{
    strncpy_s(t_systemName, systemName, PROFILER_MAX_NAME_LENGTH - 1);
    t_systemPhase     = phase;
    t_systemStartTick = CCPUProfiler::GetTimestamp();
}

void CJobProfiler::EndSystemTiming()
{
    const int64 endTick = CCPUProfiler::GetTimestamp();
    const int64 delta   = endTick - t_systemStartTick;
    const float64 ms    = (static_cast<float64>(delta) / static_cast<float64>(m_frequency)) * 1000.0;

    SystemTimingRecord record{};
    strncpy_s(record.name, t_systemName, PROFILER_MAX_NAME_LENGTH - 1);
    record.durationMs      = ms;
    record.phase           = t_systemPhase;
    record.executionThread = GetCurrentThreadId();

    lock_guard<mutex> lock(m_mtxSystemTimings);
    m_pendingTimings.push_back(record);
}

void CJobProfiler::OnJobBegin(uint32 threadIndex)
{
    if (threadIndex >= m_threadTrackers.size()) return;
    m_threadTrackers[threadIndex].jobStartTick = CCPUProfiler::GetTimestamp();
}

void CJobProfiler::OnJobEnd(uint32 threadIndex)
{
    if (threadIndex >= m_threadTrackers.size()) return;
    auto& tracker = m_threadTrackers[threadIndex];
    const int64 elapsed = CCPUProfiler::GetTimestamp() - tracker.jobStartTick;
    tracker.busyTicks.fetch_add(elapsed, std::memory_order_relaxed);
}

void CJobProfiler::RecordQueueDepth(uint32 depth)
{
    m_stats.queueDepth = depth;
}

void CJobProfiler::EndFrame()
{
    // 시스템 타이밍 수집
    {
        lock_guard<mutex> lock(m_mtxSystemTimings);
        m_stats.systemTimings = std::move(m_pendingTimings);
        m_pendingTimings.clear();
    }

    // 스레드 활용률 계산
    uint32 active = 0;
    float64 totalBusy = 0.0;
    for (auto& tracker : m_threadTrackers)
    {
        const int64 busy = tracker.busyTicks.exchange(0, std::memory_order_relaxed);
        if (busy > 0) ++active;
        totalBusy += static_cast<float64>(busy);
    }

    m_stats.activeThreads = active;

    if (m_stats.totalThreads > 0 && m_frequency > 0)
    {
        // 추정: 1 프레임 ≈ 16.67ms → 실제는 CTimer 델타 사용 권장
        const float64 frameTicks = static_cast<float64>(m_frequency) / 60.0;
        const float64 maxBusy   = frameTicks * m_stats.totalThreads;
        m_stats.utilizationPercent = (totalBusy / maxBusy) * 100.0;
    }
}

#endif // WINTERS_PROFILING
```

---

## 9. CNetworkProfiler — 네트워크 프로파일링 (스텁)

**경로:** `Engine/Header/Core/Profiler/CNetworkProfiler.h`

```cpp
#pragma once
#include "Core/Profiler/ProfilerTypes.h"

// ─────────────────────────────────────────────────────────────────
// 네트워크 통계
// ─────────────────────────────────────────────────────────────────
struct NetworkStats
{
    float64 bytesSentPerSec    = 0.0;
    float64 bytesRecvPerSec    = 0.0;
    uint32  packetsSentPerSec  = 0;
    uint32  packetsRecvPerSec  = 0;
    float64 rttMs              = 0.0;
    float64 jitterMs           = 0.0;
    // 패킷 크기 히스토그램 [0-64, 64-128, ..., 4096+]
    uint32  packetSizeHistogram[8]{};
};

// ─────────────────────────────────────────────────────────────────
class CNetworkProfiler
{
public:
    void Initialize();
    void Shutdown();

    // ── 네트워크 레이어 (09. Network) 에서 호출 ──────────────
    void OnPacketSent(uint32 sizeBytes);
    void OnPacketReceived(uint32 sizeBytes);
    void OnRTTSample(float64 rttMs);

    // ── 프레임 틱 ────────────────────────────────────────────
    void Tick(float64 deltaTime);

    const NetworkStats& GetStats() const { return m_stats; }

private:
    NetworkStats m_stats{};
    uint64       m_bytesSentAccum   = 0;
    uint64       m_bytesRecvAccum   = 0;
    uint32       m_packetsSentAccum = 0;
    uint32       m_packetsRecvAccum = 0;
    float64      m_accumTime        = 0.0;
    float64      m_rttEMA           = 0.0;
    float64      m_rttVariance      = 0.0;
};
```

**구현:** `Engine/Code/Core/Profiler/CNetworkProfiler.cpp`

```cpp
#include "WintersPCH.h"
#include "Core/Profiler/CNetworkProfiler.h"
#include <cmath>

#ifdef WINTERS_PROFILING

void CNetworkProfiler::Initialize()
{
    m_stats = NetworkStats{};
    m_bytesSentAccum = m_bytesRecvAccum = 0;
    m_packetsSentAccum = m_packetsRecvAccum = 0;
    m_accumTime = 0.0;
    m_rttEMA = m_rttVariance = 0.0;
}

void CNetworkProfiler::Shutdown()
{
    // nothing to clean
}

void CNetworkProfiler::OnPacketSent(uint32 sizeBytes)
{
    m_bytesSentAccum += sizeBytes;
    ++m_packetsSentAccum;

    // 히스토그램 버킷
    uint32 bucket = 0;
    uint32 threshold = 64;
    while (bucket < 7 && sizeBytes >= threshold)
    {
        threshold <<= 1;
        ++bucket;
    }
    m_stats.packetSizeHistogram[bucket]++;
}

void CNetworkProfiler::OnPacketReceived(uint32 sizeBytes)
{
    m_bytesRecvAccum += sizeBytes;
    ++m_packetsRecvAccum;
}

void CNetworkProfiler::OnRTTSample(float64 rttMs)
{
    // 지수이동평균 (EMA, alpha = 0.1)
    constexpr float64 alpha = 0.1;
    m_rttEMA = m_rttEMA * (1.0 - alpha) + rttMs * alpha;

    const float64 diff = rttMs - m_rttEMA;
    m_rttVariance = m_rttVariance * (1.0 - alpha) + (diff * diff) * alpha;

    m_stats.rttMs    = m_rttEMA;
    m_stats.jitterMs = std::sqrt(m_rttVariance);
}

void CNetworkProfiler::Tick(float64 deltaTime)
{
    m_accumTime += deltaTime;
    if (m_accumTime >= 1.0)   // 1초마다 갱신
    {
        m_stats.bytesSentPerSec   = static_cast<float64>(m_bytesSentAccum) / m_accumTime;
        m_stats.bytesRecvPerSec   = static_cast<float64>(m_bytesRecvAccum) / m_accumTime;
        m_stats.packetsSentPerSec = static_cast<uint32>(m_packetsSentAccum / m_accumTime);
        m_stats.packetsRecvPerSec = static_cast<uint32>(m_packetsRecvAccum / m_accumTime);

        m_bytesSentAccum = m_bytesRecvAccum = 0;
        m_packetsSentAccum = m_packetsRecvAccum = 0;
        m_accumTime = 0.0;
    }
}

#endif // WINTERS_PROFILING
```

---

## 10. CProfilerOverlay — 실시간 온스크린 HUD

**경로:** `Engine/Header/Core/Profiler/CProfilerOverlay.h`

```cpp
#pragma once
#include "Core/Profiler/ProfilerTypes.h"
#include "Core/Profiler/CJobProfiler.h"
#include "Core/Profiler/CNetworkProfiler.h"
#include <string>

using std::string;

// ─────────────────────────────────────────────────────────────────
// 오버레이 표시 모드 (F3으로 순환)
// ─────────────────────────────────────────────────────────────────
enum class EOverlayMode : uint8
{
    Off      = 0,   // 표시 없음
    Minimal  = 1,   // FPS + 프레임 시간 + GPU 시간
    Detailed = 2,   // + 카테고리별 분석
    Timeline = 3,   // + 프레임 타임라인 바
    Full     = 4,   // + 히스토리 그래프
    Count
};

// ─────────────────────────────────────────────────────────────────
class CProfilerOverlay
{
public:
    void Initialize();
    void Shutdown();

    // ── 모드 전환 ────────────────────────────────────────────
    void Toggle();
    void SetMode(EOverlayMode mode);
    EOverlayMode GetMode() const { return m_mode; }

    // ── 매 프레임 업데이트 ───────────────────────────────────
    void Update(const FrameStats& stats,
                const JobSystemStats& jobStats,
                const NetworkStats& netStats,
                const FrameBudget& budget);

    // ── 텍스트 출력 (DebugRender 또는 ImGui) ────────────────
    const string& GetOverlayText() const { return m_overlayText; }

private:
    void BuildMinimalText(const FrameStats& stats, const FrameBudget& budget);
    void BuildDetailedText(const FrameStats& stats,
                           const JobSystemStats& jobStats,
                           const NetworkStats& netStats);
    void BuildTimelineText(const FrameStats& stats);
    void BuildFullText(const FrameStats& stats);

    // 카테고리 이름 문자열
    static const char* GetCategoryName(EProfilerCategory cat);
    // 버짓 상태 문자열 [OK] [WARNING] [OVER]
    static const char* GetBudgetStatusTag(float64 ms, const FrameBudget& budget);

    EOverlayMode m_mode = EOverlayMode::Off;
    string       m_overlayText;
};
```

**구현:** `Engine/Code/Core/Profiler/CProfilerOverlay.cpp`

```cpp
#include "WintersPCH.h"
#include "Core/Profiler/CProfilerOverlay.h"
#include <cstdio>

#ifdef WINTERS_PROFILING

void CProfilerOverlay::Initialize()
{
    m_mode = EOverlayMode::Off;
    m_overlayText.reserve(4096);
}

void CProfilerOverlay::Shutdown() {}

void CProfilerOverlay::Toggle()
{
    uint8 next = static_cast<uint8>(m_mode) + 1;
    if (next >= static_cast<uint8>(EOverlayMode::Count))
        next = 0;
    m_mode = static_cast<EOverlayMode>(next);
}

void CProfilerOverlay::SetMode(EOverlayMode mode)
{
    m_mode = mode;
}

void CProfilerOverlay::Update(const FrameStats& stats,
                               const JobSystemStats& jobStats,
                               const NetworkStats& netStats,
                               const FrameBudget& budget)
{
    m_overlayText.clear();
    if (m_mode == EOverlayMode::Off) return;

    BuildMinimalText(stats, budget);

    if (m_mode >= EOverlayMode::Detailed)
        BuildDetailedText(stats, jobStats, netStats);

    if (m_mode >= EOverlayMode::Timeline)
        BuildTimelineText(stats);

    if (m_mode >= EOverlayMode::Full)
        BuildFullText(stats);

    // Debug Output에도 출력 (Visual Studio Output 창)
    if (!m_overlayText.empty())
        OutputDebugStringA(m_overlayText.c_str());
}

void CProfilerOverlay::BuildMinimalText(const FrameStats& stats,
                                         const FrameBudget& budget)
{
    char buf[512];
    const float64 maxMs = (stats.cpuFrameMs > stats.gpuFrameMs)
                          ? stats.cpuFrameMs : stats.gpuFrameMs;
    const float64 fps   = (maxMs > 0.001) ? (1000.0 / maxMs) : 9999.0;

    sprintf_s(buf,
        "=== WINTERS PROFILER ===\n"
        "FPS: %.1f  %s\n"
        "CPU: %.2f ms | GPU: %.2f ms\n"
        "Frame: %llu\n",
        fps, GetBudgetStatusTag(maxMs, budget),
        stats.cpuFrameMs, stats.gpuFrameMs,
        stats.frameIndex);

    m_overlayText += buf;
}

void CProfilerOverlay::BuildDetailedText(const FrameStats& stats,
                                          const JobSystemStats& jobStats,
                                          const NetworkStats& netStats)
{
    char buf[1024];

    m_overlayText += "--- Category Breakdown ---\n";
    for (uint32 i = 0; i < static_cast<uint32>(EProfilerCategory::Count); ++i)
    {
        if (stats.categoryMs[i] > 0.001)
        {
            sprintf_s(buf, "  %-12s: %.2f ms\n",
                      GetCategoryName(static_cast<EProfilerCategory>(i)),
                      stats.categoryMs[i]);
            m_overlayText += buf;
        }
    }

    sprintf_s(buf,
        "--- Memory ---\n"
        "  Used: %.2f MB | Allocs: %llu\n"
        "--- Jobs ---\n"
        "  Workers: %u/%u | Util: %.1f%% | Queue: %u\n",
        stats.memoryUsedBytes / (1024.0 * 1024.0),
        stats.memoryAllocCount,
        jobStats.activeThreads, jobStats.totalThreads,
        jobStats.utilizationPercent, jobStats.queueDepth);

    m_overlayText += buf;

    // ECS 시스템별 타이밍
    if (!jobStats.systemTimings.empty())
    {
        m_overlayText += "--- ECS Systems ---\n";
        for (const auto& sys : jobStats.systemTimings)
        {
            sprintf_s(buf, "  [P%u] %-20s: %.3f ms\n",
                      sys.phase, sys.name, sys.durationMs);
            m_overlayText += buf;
        }
    }

    // 네트워크 (있을 때만)
    if (netStats.rttMs > 0.0)
    {
        sprintf_s(buf,
            "--- Network ---\n"
            "  RTT: %.1f ms | Jitter: %.1f ms\n"
            "  Send: %.1f KB/s | Recv: %.1f KB/s\n",
            netStats.rttMs, netStats.jitterMs,
            netStats.bytesSentPerSec / 1024.0,
            netStats.bytesRecvPerSec / 1024.0);
        m_overlayText += buf;
    }
}

void CProfilerOverlay::BuildTimelineText(const FrameStats& stats)
{
    // ASCII 프레임 타임라인 바 (16.67ms = 60fps 기준)
    char buf[256];
    const float64 targetMs = 16.667;

    m_overlayText += "--- Frame Timeline ---\n";

    auto drawBar = [&](const char* label, float64 ms)
    {
        const uint32 barLen = static_cast<uint32>((ms / targetMs) * 40.0);
        const uint32 capped = (barLen > 60) ? 60 : barLen;

        sprintf_s(buf, "  %-6s [", label);
        m_overlayText += buf;
        for (uint32 i = 0; i < capped; ++i)
            m_overlayText += (i < 40) ? '#' : '!';  // 40 이후 = over budget
        for (uint32 i = capped; i < 40; ++i)
            m_overlayText += '.';
        sprintf_s(buf, "] %.2f ms\n", ms);
        m_overlayText += buf;
    };

    drawBar("CPU", stats.cpuFrameMs);
    drawBar("GPU", stats.gpuFrameMs);
}

void CProfilerOverlay::BuildFullText(const FrameStats& stats)
{
    // TODO: 히스토리 그래프 (최근 60프레임 ASCII 스파크라인)
    m_overlayText += "--- Frame History ---\n";
    m_overlayText += "  (Reserved for ImGui frame graph)\n";
}

const char* CProfilerOverlay::GetCategoryName(EProfilerCategory cat)
{
    static const char* names[] = {
        "General", "Render", "Physics", "AI", "Network",
        "Audio", "ECS", "JobSystem", "Input", "Script",
        "Memory", "GPU"
    };
    const uint32 idx = static_cast<uint32>(cat);
    return (idx < static_cast<uint32>(EProfilerCategory::Count)) ? names[idx] : "Unknown";
}

const char* CProfilerOverlay::GetBudgetStatusTag(float64 ms, const FrameBudget& budget)
{
    if (ms > budget.targetMs)   return "[OVER BUDGET]";
    if (ms > budget.warningMs)  return "[WARNING]";
    return "[OK]";
}

#endif // WINTERS_PROFILING
```

---

## 11. CProfilerExporter — Chrome Tracing JSON + CSV

**경로:** `Engine/Header/Core/Profiler/CProfilerExporter.h`

```cpp
#pragma once
#include "Core/Profiler/ProfilerTypes.h"
#include <span>
#include <vector>

using std::vector;
using std::span;

// ─────────────────────────────────────────────────────────────────
class CProfilerExporter
{
public:
    // ── Chrome Tracing JSON (chrome://tracing 또는 ui.perfetto.dev) ──
    static bool ExportChromeTracing(const char* filepath,
                                     const vector<ProfilerEvent>& cpuEvents,
                                     const vector<GPUProfilerEvent>& gpuEvents,
                                     int64 qpcFrequency);

    // ── CSV (스프레드시트 분석용) ────────────────────────────
    static bool ExportFrameStatsCSV(const char* filepath,
                                     span<const FrameStats> frameHistory);

    // ── 바이너리 덤프 (고속 읽기/쓰기) ──────────────────────
    static bool ExportBinary(const char* filepath,
                              const vector<ProfilerEvent>& cpuEvents,
                              int64 qpcFrequency);
};
```

**구현:** `Engine/Code/Core/Profiler/CProfilerExporter.cpp`

```cpp
#include "WintersPCH.h"
#include "Core/Profiler/CProfilerExporter.h"
#include <fstream>
#include <cstdio>

#ifdef WINTERS_PROFILING

// Chrome Tracing JSON 포맷 사양:
// https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU
//
// ph: "B" = Begin, "E" = End, "X" = Complete, "i" = Instant
// ts: 마이크로초 단위 타임스탬프
// pid: 프로세스 ID (1 = 게임)
// tid: 스레드 ID
// cat: 카테고리 문자열

static const char* CategoryToString(EProfilerCategory cat)
{
    static const char* names[] = {
        "General","Render","Physics","AI","Network",
        "Audio","ECS","JobSystem","Input","Script","Memory","GPU"
    };
    return names[static_cast<uint32>(cat)];
}

bool CProfilerExporter::ExportChromeTracing(const char* filepath,
                                             const vector<ProfilerEvent>& cpuEvents,
                                             const vector<GPUProfilerEvent>& gpuEvents,
                                             int64 qpcFrequency)
{
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    const float64 tickToUs = 1000000.0 / static_cast<float64>(qpcFrequency);

    file << "{\"traceEvents\":[\n";

    bool first = true;

    // CPU 이벤트
    for (const auto& evt : cpuEvents)
    {
        if (!first) file << ",\n";
        first = false;

        const char* ph = "i";
        if (evt.type == EProfilerEventType::Begin) ph = "B";
        else if (evt.type == EProfilerEventType::End) ph = "E";

        const float64 tsUs = static_cast<float64>(evt.timestamp) * tickToUs;

        char buf[512];
        sprintf_s(buf,
            "{\"name\":\"%s\",\"cat\":\"%s\",\"ph\":\"%s\","
            "\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
            evt.name, CategoryToString(evt.category),
            ph, tsUs, evt.threadId);

        file << buf;
    }

    // GPU 이벤트 (가상 tid = 0, "GPU" 레인)
    for (const auto& gpu : gpuEvents)
    {
        if (!first) file << ",\n";
        first = false;

        const float64 startUs = gpu.startMs * 1000.0;  // ms → us
        const float64 durUs   = gpu.durationMs * 1000.0;

        char buf[512];
        sprintf_s(buf,
            "{\"name\":\"%s\",\"cat\":\"%s\",\"ph\":\"X\","
            "\"ts\":%.3f,\"dur\":%.3f,\"pid\":1,\"tid\":0}",
            gpu.name, CategoryToString(gpu.category),
            startUs, durUs);

        file << buf;
    }

    file << "\n]}\n";
    file.close();
    return true;
}

bool CProfilerExporter::ExportFrameStatsCSV(const char* filepath,
                                             span<const FrameStats> frameHistory)
{
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    // 헤더
    file << "FrameIndex,CPU_ms,GPU_ms,Memory_MB,AllocCount,"
            "DrawCalls,Triangles,NetSend_KB,NetRecv_KB,RTT_ms\n";

    for (const auto& stats : frameHistory)
    {
        if (stats.frameIndex == 0) continue;  // 미사용 슬롯 건너뜀

        char buf[256];
        sprintf_s(buf,
            "%llu,%.3f,%.3f,%.2f,%llu,%u,%u,%.2f,%.2f,%.1f\n",
            stats.frameIndex,
            stats.cpuFrameMs, stats.gpuFrameMs,
            stats.memoryUsedBytes / (1024.0 * 1024.0),
            stats.memoryAllocCount,
            stats.drawCallCount, stats.triangleCount,
            stats.networkBytesSent / 1024.0,
            stats.networkBytesRecv / 1024.0,
            stats.rttMs);
        file << buf;
    }

    file.close();
    return true;
}

bool CProfilerExporter::ExportBinary(const char* filepath,
                                      const vector<ProfilerEvent>& cpuEvents,
                                      int64 qpcFrequency)
{
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) return false;

    // 헤더: 매직 넘버 + 버전 + QPC 주파수 + 이벤트 수
    const uint32 magic   = 0x57505246;  // "WPRF"
    const uint32 version = 1;
    const uint32 count   = static_cast<uint32>(cpuEvents.size());

    file.write(reinterpret_cast<const char*>(&magic),        sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version),      sizeof(version));
    file.write(reinterpret_cast<const char*>(&qpcFrequency), sizeof(qpcFrequency));
    file.write(reinterpret_cast<const char*>(&count),        sizeof(count));

    // 이벤트 데이터
    file.write(reinterpret_cast<const char*>(cpuEvents.data()),
               cpuEvents.size() * sizeof(ProfilerEvent));

    file.close();
    return true;
}

#endif // WINTERS_PROFILING
```

---

## 12. CProfiler — 마스터 싱글톤

**경로:** `Engine/Header/Core/Profiler/CProfiler.h`

```cpp
#pragma once
#include "Core/Profiler/CCPUProfiler.h"
#include "Core/Profiler/CGPUProfiler.h"
#include "Core/Profiler/CMemoryProfiler.h"
#include "Core/Profiler/CFrameInspector.h"
#include "Core/Profiler/CNetworkProfiler.h"
#include "Core/Profiler/CJobProfiler.h"
#include "Core/Profiler/CProfilerOverlay.h"
#include "Core/Profiler/CProfilerExporter.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

// ─────────────────────────────────────────────────────────────────
class CProfiler
{
public:
    static CProfiler& Get()
    {
        static CProfiler instance;
        return instance;
    }

    void Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
                    uint32 workerThreadCount);
    void Shutdown();

    // ── 프레임 경계 (CEngineApp에서 호출) ────────────────────
    void BeginFrame();
    void EndFrame();

    // ── 서브시스템 접근 ──────────────────────────────────────
    CCPUProfiler&       GetCPU()         { return m_cpuProfiler; }
    CGPUProfiler&       GetGPU()         { return m_gpuProfiler; }
    CMemoryProfiler&    GetMemory()      { return m_memProfiler; }
    CFrameInspector&    GetInspector()   { return m_frameInspector; }
    CNetworkProfiler&   GetNetwork()     { return m_netProfiler; }
    CJobProfiler&       GetJobProfiler() { return m_jobProfiler; }
    CProfilerOverlay&   GetOverlay()     { return m_overlay; }

    // ── 내보내기 (핫키 또는 콘솔 명령) ───────────────────────
    void ExportChromeTracing(const char* filepath);
    void ExportCSV(const char* filepath);

    bool   IsEnabled() const { return m_bEnabled; }
    void   SetEnabled(bool enabled) { m_bEnabled = enabled; }
    uint64 GetFrameIndex() const { return m_frameIndex; }

private:
    CProfiler()  = default;
    ~CProfiler() = default;
    CProfiler(const CProfiler&)            = delete;
    CProfiler& operator=(const CProfiler&) = delete;

    CCPUProfiler       m_cpuProfiler;
    CGPUProfiler       m_gpuProfiler;
    CMemoryProfiler    m_memProfiler;
    CFrameInspector    m_frameInspector;
    CNetworkProfiler   m_netProfiler;
    CJobProfiler       m_jobProfiler;
    CProfilerOverlay   m_overlay;

    // 프레임 이벤트 임시 버퍼 (매 프레임 재사용)
    std::vector<ProfilerEvent>    m_cpuEventsBuffer;
    std::vector<GPUProfilerEvent> m_gpuEventsBuffer;

    uint64 m_frameIndex = 0;
    bool   m_bEnabled   = true;
    int64  m_qpcFrequency = 0;
};
```

**구현:** `Engine/Code/Core/Profiler/CProfiler.cpp`

```cpp
#include "WintersPCH.h"
#include "Core/Profiler/CProfiler.h"

#ifdef WINTERS_PROFILING

void CProfiler::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
                             uint32 workerThreadCount)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_qpcFrequency = freq.QuadPart;

    m_cpuProfiler.Initialize();
    m_gpuProfiler.Initialize(pDevice, pContext);
    m_memProfiler.Initialize();
    m_frameInspector.Initialize();
    m_netProfiler.Initialize();
    m_jobProfiler.Initialize(workerThreadCount);
    m_overlay.Initialize();

    m_cpuEventsBuffer.reserve(4096);
    m_gpuEventsBuffer.reserve(256);

    m_frameIndex = 0;
    m_bEnabled   = true;

    OutputDebugStringA("[WintersProfiler] Initialized.\n");
}

void CProfiler::Shutdown()
{
    OutputDebugStringA("[WintersProfiler] Shutting down...\n");

    m_overlay.Shutdown();
    m_netProfiler.Shutdown();
    m_jobProfiler.Shutdown();
    m_gpuProfiler.Shutdown();

    const uint32 leaks = m_memProfiler.ReportLeaks();
    m_memProfiler.Shutdown();

    m_cpuProfiler.Shutdown();

    if (leaks > 0)
    {
        char buf[128];
        sprintf_s(buf, "[WintersProfiler] %u memory leaks detected.\n", leaks);
        OutputDebugStringA(buf);
    }

    OutputDebugStringA("[WintersProfiler] Shutdown complete.\n");
}

void CProfiler::BeginFrame()
{
    if (!m_bEnabled) return;

    m_cpuProfiler.BeginEvent("Frame", EProfilerCategory::General);
    m_gpuProfiler.BeginFrame();
}

void CProfiler::EndFrame()
{
    if (!m_bEnabled) return;

    m_gpuProfiler.EndFrame();
    m_cpuProfiler.EndEvent();   // "Frame" 스코프 종료

    // ── CPU 이벤트 수집 ──────────────────────────────────────
    m_cpuEventsBuffer.clear();
    m_cpuProfiler.CollectFrameEvents(m_cpuEventsBuffer);

    // ── GPU 결과 수집 (N-2 프레임) ───────────────────────────
    m_gpuEventsBuffer.clear();
    m_gpuProfiler.CollectResults(m_gpuEventsBuffer);

    // ── Job/ECS 프레임 집계 ──────────────────────────────────
    m_jobProfiler.EndFrame();

    // ── 프레임 인스펙터 처리 ─────────────────────────────────
    m_frameInspector.ProcessFrame(
        m_frameIndex,
        m_cpuEventsBuffer,
        m_gpuEventsBuffer,
        m_memProfiler.GetTotalUsedBytes(),
        m_memProfiler.GetTotalAllocCount());

    // ── 오버레이 업데이트 ────────────────────────────────────
    m_overlay.Update(
        m_frameInspector.GetCurrentFrameStats(),
        m_jobProfiler.GetStats(),
        m_netProfiler.GetStats(),
        FrameBudget{});

    ++m_frameIndex;
}

void CProfiler::ExportChromeTracing(const char* filepath)
{
    CProfilerExporter::ExportChromeTracing(
        filepath, m_cpuEventsBuffer, m_gpuEventsBuffer, m_qpcFrequency);

    char buf[256];
    sprintf_s(buf, "[WintersProfiler] Exported Chrome Tracing to: %s\n", filepath);
    OutputDebugStringA(buf);
}

void CProfiler::ExportCSV(const char* filepath)
{
    CProfilerExporter::ExportFrameStatsCSV(
        filepath, m_frameInspector.GetFrameHistory());

    char buf[256];
    sprintf_s(buf, "[WintersProfiler] Exported CSV to: %s\n", filepath);
    OutputDebugStringA(buf);
}

#endif // WINTERS_PROFILING
```

---

## 13. ProfilerAPI.h — 공개 매크로 (DLL 경계)

**경로:** `Engine/Include/ProfilerAPI.h`

Client가 직접 포함하는 유일한 프로파일러 헤더. Shipping에서 모든 매크로 → no-op.

```cpp
#pragma once
#include "WintersAPI.h"

// ─────────────────────────────────────────────────────────────────
// WINTERS_PROFILING 은 Debug/Development vcxproj에서 정의
// Shipping 빌드에서는 모든 매크로가 ((void)0)으로 컴파일
// ─────────────────────────────────────────────────────────────────

#ifdef WINTERS_PROFILING

// 전방 참조
class CProfiler;
class CCPUProfiler;
class CGPUProfiler;
class CScopedCPUTimer;
class CScopedGPUTimer;
enum class EProfilerCategory : uint8_t;

// ─── CPU 프로파일링 ──────────────────────────────────────────
// 사용법:
//   void MyFunction() {
//       WINTERS_PROFILE_SCOPE("MyFunction", EProfilerCategory::Render);
//       // ... 코드 ...
//   }
//
//   void AnotherFunction() {
//       WINTERS_PROFILE_FUNCTION();  // 자동으로 __FUNCTION__ 이름 사용
//       // ... 코드 ...
//   }

#define WINTERS_PROFILE_SCOPE(name, category) \
    CScopedCPUTimer WINTERS_CONCAT(_profScope_, __LINE__)(name, category, CProfiler::Get().GetCPU())

#define WINTERS_PROFILE_FUNCTION() \
    WINTERS_PROFILE_SCOPE(__FUNCTION__, EProfilerCategory::General)

// ─── GPU 프로파일링 ──────────────────────────────────────────
#define WINTERS_GPU_PROFILE_SCOPE(name, category) \
    CScopedGPUTimer WINTERS_CONCAT(_gpuScope_, __LINE__)(name, category, CProfiler::Get().GetGPU())

#define WINTERS_GPU_PROFILE_FUNCTION() \
    WINTERS_GPU_PROFILE_SCOPE(__FUNCTION__, EProfilerCategory::GPU)

// ─── 메모리 추적 ────────────────────────────────────────────
#define WINTERS_PROFILE_ALLOC(ptr, size, cat) \
    CProfiler::Get().GetMemory().OnAlloc((ptr), (size), (cat), __FILE__, __LINE__)
#define WINTERS_PROFILE_FREE(ptr) \
    CProfiler::Get().GetMemory().OnFree((ptr))

// ─── 단발성 마커 ────────────────────────────────────────────
#define WINTERS_PROFILE_MARKER(name, category) \
    CProfiler::Get().GetCPU().InstantEvent(name, category)

// ─── 토큰 연결 헬퍼 ─────────────────────────────────────────
#define WINTERS_CONCAT_INNER(a, b) a ## b
#define WINTERS_CONCAT(a, b) WINTERS_CONCAT_INNER(a, b)

#else  // ── Shipping: 모든 매크로 no-op ───────────────────────

#define WINTERS_PROFILE_SCOPE(name, category)       ((void)0)
#define WINTERS_PROFILE_FUNCTION()                   ((void)0)
#define WINTERS_GPU_PROFILE_SCOPE(name, category)   ((void)0)
#define WINTERS_GPU_PROFILE_FUNCTION()               ((void)0)
#define WINTERS_PROFILE_ALLOC(ptr, size, cat)        ((void)0)
#define WINTERS_PROFILE_FREE(ptr)                    ((void)0)
#define WINTERS_PROFILE_MARKER(name, category)       ((void)0)

#endif // WINTERS_PROFILING
```

---

## 14. 기존 코드 수정 상세

### 14-1. Engine/Include/EngineConfig.h

**수정 전 (기존 구조체 끝):**
```cpp
    bool vsync     = true;
    int32 targetFPS = 60;
};
```

**수정 후:**
```cpp
    bool vsync     = true;
    int32 targetFPS = 60;

    // ── Profiling ──────────────────────────────────────────
    bool enableProfiling = true;   // 런타임 토글 (컴파일타임은 WINTERS_PROFILING)
};
```

### 14-2. Engine/Header/Framework/CEngineApp.h

**추가 (#include 섹션):**
```cpp
#ifdef WINTERS_PROFILING
#include "Core/Profiler/CProfiler.h"
#endif
```

### 14-3. Engine/Code/Framework/CEngineApp.cpp

**Initialize() — CDX11Device 초기화 직후, app->OnInit() 직전:**
```cpp
    // ── Profiler 초기화 ─────────────────────────────────────
#ifdef WINTERS_PROFILING
    if (config.enableProfiling)
    {
        CProfiler::Get().Initialize(
            m_Device.GetDevice(),
            m_Device.GetContext(),
            0  // workerThreadCount: 0 = 자동 감지
        );
    }
#endif
```

**Run() — 게임 루프 내:**
```cpp
    // ── 프레임 시작 ─────────────────────────────────────────
#ifdef WINTERS_PROFILING
    CProfiler::Get().BeginFrame();
#endif

    m_Timer.Tick();
    Update(m_Timer.GetDeltaTime());
    Render();

    // ── 프레임 종료 ─────────────────────────────────────────
#ifdef WINTERS_PROFILING
    CProfiler::Get().EndFrame();

    // F3: 오버레이 토글
    static bool prevF3 = false;
    bool currF3 = CInput::Get().IsKeyDown(VK_F3);
    if (currF3 && !prevF3)
        CProfiler::Get().GetOverlay().Toggle();
    prevF3 = currF3;

    // F5: Chrome Tracing 내보내기
    static bool prevF5 = false;
    bool currF5 = CInput::Get().IsKeyDown(VK_F5);
    if (currF5 && !prevF5)
        CProfiler::Get().ExportChromeTracing("profiler_trace.json");
    prevF5 = currF5;
#endif

    CInput::Get().EndFrame();
```

**Update() — 스코프 계측 추가:**
```cpp
void CEngineApp::Update(float32 deltaTime)
{
    WINTERS_PROFILE_SCOPE("Update", EProfilerCategory::General);

#ifdef WINTERS_PROFILING
    CProfiler::Get().GetNetwork().Tick(static_cast<float64>(deltaTime));
#endif

    if (m_pGameApp)
        m_pGameApp->OnUpdate(deltaTime);
}
```

**Render() — CPU/GPU 스코프 계측 추가:**
```cpp
void CEngineApp::Render()
{
    WINTERS_PROFILE_SCOPE("Render", EProfilerCategory::Render);

    m_Device.BeginFrame(0.08f, 0.08f, 0.12f, 1.f);

    {
        WINTERS_GPU_PROFILE_SCOPE("FullFrame", EProfilerCategory::Render);
        if (m_pGameApp)
            m_pGameApp->OnRender();
    }

    m_Device.EndFrame();
}
```

**Shutdown() — 프로파일러 종료:**
```cpp
    // ── Profiler 종료 (Device 파괴 전) ──────────────────────
#ifdef WINTERS_PROFILING
    CProfiler::Get().Shutdown();
#endif
```

### 14-4. Engine/Code/ECS/SystemScheduler.cpp

**시스템 실행 래핑:**
```cpp
// 각 시스템 Execute 호출부:
#ifdef WINTERS_PROFILING
    CProfiler::Get().GetJobProfiler().BeginSystemTiming(sys->GetName(), phase);
#endif

    sys->Execute(world, fTimeDelta);

#ifdef WINTERS_PROFILING
    CProfiler::Get().GetJobProfiler().EndSystemTiming();
#endif
```

> **참고:** ISystem에 `virtual const char* GetName() const = 0;` 추가 필요.

### 14-5. Engine/Code/Core/JobSystem.cpp

**워커 스레드 잡 실행 래핑:**
```cpp
// 워커 루프 내 job() 호출부:
#ifdef WINTERS_PROFILING
    CProfiler::Get().GetJobProfiler().OnJobBegin(threadIndex);
#endif

    job();

#ifdef WINTERS_PROFILING
    CProfiler::Get().GetJobProfiler().OnJobEnd(threadIndex);
#endif
```

> **참고:** 워커 스레드 생성 시 threadIndex를 캡처하도록 람다 수정 필요.

---

## 15. vcxproj.filters 등록

```xml
<!-- Engine/Include/Engine.vcxproj.filters -->

<!-- 새 필터 추가 -->
<Filter Include="01. Core\Profiler">
  <UniqueIdentifier>{새GUID생성}</UniqueIdentifier>
</Filter>

<!-- 공개 헤더 -->
<ClInclude Include="Include\ProfilerAPI.h">
  <Filter>Include</Filter>
</ClInclude>

<!-- 내부 헤더 (Header/Core/Profiler/*.h) -->
<ClInclude Include="Header\Core\Profiler\ProfilerTypes.h">
  <Filter>01. Core\Profiler</Filter>
</ClInclude>
<!-- ... 나머지 10개 .h 파일 동일 패턴 ... -->

<!-- 구현 (Code/Core/Profiler/*.cpp) -->
<ClCompile Include="Code\Core\Profiler\CCPUProfiler.cpp">
  <Filter>01. Core\Profiler</Filter>
</ClCompile>
<!-- ... 나머지 8개 .cpp 파일 동일 패턴 ... -->
```

---

## 16. 성능 오버헤드 분석

| 연산 | 비용 | 전략 |
|------|------|------|
| `WINTERS_PROFILE_SCOPE` | ~50ns (QPC + 링버퍼 Push) | thread_local, 락 없음 |
| `WINTERS_GPU_PROFILE_SCOPE` | ~0 CPU (GPU측 타임스탬프) | DX11 쿼리 = GPU 전용 |
| Memory `OnAlloc/OnFree` | ~200ns (atomic + map lookup) | Map mutex는 Debug만 |
| 프레임 끝 수집 | ~100μs (전 스레드 링버퍼 드레인) | 메인 스레드, 1회/프레임 |
| 오버레이 텍스트 빌드 | ~50μs (sprintf) | 1회/프레임 |
| **Shipping 빌드** | **0** | **모든 매크로 `((void)0)`** |

**총 오버헤드 목표:** Development 빌드에서 프레임당 0.5ms 미만. Shipping에서 제로.

---

## 17. 구현 순서 (5단계)

```
Phase A: Foundation (먼저 구현, 모든 것이 의존)
  1. ProfilerTypes.h
  2. CRingBuffer.h
  3. CCPUProfiler.h/.cpp
  4. CProfiler.h/.cpp (초기엔 CPU만)
  5. ProfilerAPI.h (매크로)
  6. CEngineApp 통합 (Init/Run/Shutdown)
  → 검증: WINTERS_PROFILE_SCOPE로 Update/Render 계측, OutputDebugString 확인

Phase B: GPU + Memory
  7. CGPUProfiler.h/.cpp (DX11 타임스탬프 쿼리)
  8. CMemoryProfiler.h/.cpp (할당 추적)
  9. CDX11Device BeginFrame/EndFrame 통합
  → 검증: GPU 시간 측정, 메모리 누수 감지

Phase C: 집계 + 시각화
  10. CFrameInspector.h/.cpp (프레임별 통계)
  11. CProfilerOverlay.h/.cpp (온스크린 HUD)
  12. F3 핫키 통합
  → 검증: F3으로 오버레이 토글, FPS/프레임시간/GPU시간 표시

Phase D: Job/ECS + 내보내기
  13. CJobProfiler.h/.cpp (워커 스레드 타이밍)
  14. SystemScheduler/JobSystem 통합
  15. CProfilerExporter.h/.cpp (Chrome Tracing JSON)
  → 검증: F5로 profiler_trace.json 내보내기, chrome://tracing에서 열기

Phase E: 네트워크 (스텁)
  16. CNetworkProfiler.h/.cpp (대역폭/RTT 스텁)
  → 검증: 09. Network 레이어 구현 시 OnPacketSent/Received 호출 확인
```

---

## 18. 검증 계획

| 검증 항목 | 방법 | 기대 결과 |
|-----------|------|-----------|
| CPU 프로파일링 | Update/Render에 SCOPE 매크로 삽입 | OutputDebugString에 계층적 이벤트 로그 |
| GPU 프로파일링 | BeginFrame/EndFrame 사이 GPU 스코프 | GPU 시간(ms) 정상 측정 |
| 메모리 누수 감지 | 의도적 누수 코드 삽입 후 Shutdown | "MEMORY LEAK" 메시지 출력 |
| 오버레이 HUD | F3 키 반복 입력 | Off→Minimal→Detailed→Timeline→Full 순환 |
| Chrome Tracing | F5 키 입력 | profiler_trace.json 생성, chrome://tracing에서 정상 로드 |
| Shipping 빌드 | WINTERS_PROFILING 미정의 | 컴파일 성공, 프로파일링 코드 0바이트 |
| 멀티스레드 안정성 | JobSystem 활성 상태에서 프레임 100+ 실행 | 크래시/데드락 없음 |

---

## 19. 설계 근거 요약

| 결정 | 이유 |
|------|------|
| 스레드별 SPSC 링 버퍼 (글로벌 MPSC 아님) | 12+ 워커 스레드 CAS 충돌 방지, 캐시라인 바운싱 제거. Tracy/Optick/Unreal Insights 동일 접근법 |
| GPU 트리플 버퍼링 | DX11 GetData 스톨 방지 (N-2 프레임 읽기). Frostbite 동일 |
| 인라인 char[64] (const char* 아님) | 동적 문자열 댕글링 포인터 방지, 링 버퍼 자체 완결성 보장 |
| 01. Core 배치 (신규 필터 아님) | 모든 상위 레이어가 매크로 호출 가능. 순방향 의존성 유지 |
| `#ifdef WINTERS_PROFILING` 전체 게이트 | Shipping 오버헤드 제로 보장 |
| CProfiler 싱글톤 | CEngineApp 패턴 일관성, 전역 접근 편의 |
