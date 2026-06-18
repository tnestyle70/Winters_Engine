#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

// Tracy 는 Engine DLL 한 곳(TracyClientImpl.cpp)에만 구현부를 두고
// Client/Server 등 나머지 모듈은 import 로 같은 인스턴스에 기록한다.
#ifdef WINTERS_PROFILING
#define TRACY_ENABLE
#define TRACY_ON_DEMAND
#ifdef WINTERS_ENGINE_EXPORTS
#define TRACY_EXPORTS
#else
#define TRACY_IMPORTS
#endif
// Engine_Defines.h의 디버그 new 매크로(DBG_NEW)가 Tracy의 placement new를 깨뜨리므로 격리한다.
#pragma push_macro("new")
#undef new
#include "tracy/Tracy.hpp"
#pragma pop_macro("new")
#endif

#ifdef WINTERS_PROFILING
WINTERS_ENGINE void Winters_Profile_Counter(const char* pName, uint64_t delta);
#define WINTERS_PROFILE_COUNT(name, delta) Winters_Profile_Counter(name, delta)
#else
#define WINTERS_PROFILE_COUNT(name, delta) ((void)0)
#endif

#ifdef WINTERS_PROFILING
WINTERS_ENGINE void Winters_Profile_Push(const char* pName);
WINTERS_ENGINE void Winters_Profile_Pop();

class WINTERS_ENGINE CProfileScope
{
public:
    explicit CProfileScope(const char* pName) { Winters_Profile_Push(pName); }
    ~CProfileScope() { Winters_Profile_Pop(); }
};

// 2단계 CONCAT 으로 __LINE__ 지연 확장
#define WINTERS_PROFILE_CAT_INNER(a, b) a##b
#define WINTERS_PROFILE_CAT(a, b)       WINTERS_PROFILE_CAT_INNER(a, b)

// Tracy zone + 기존 F3 HUD scope 를 동시에 기록한다.
// ZoneNamedN 은 고유 변수명을 받으므로 같은 블록 안 다중 사용이 가능하다.
// name 은 string literal 이어야 한다 (Tracy 정적 source location 요구).
#define WINTERS_PROFILE_SCOPE(name) \
    ZoneNamedN(WINTERS_PROFILE_CAT(_tracyZone_, __LINE__), name, true); \
    ::CProfileScope WINTERS_PROFILE_CAT(_winProfScope_, __LINE__)(name)

#define WINTERS_PROFILE_FRAME_MARK FrameMark
#define WINTERS_PROFILE_THREAD_NAME(name) tracy::SetThreadName(name)

#else
#define WINTERS_PROFILE_SCOPE(name) ((void)0)
#define WINTERS_PROFILE_FRAME_MARK ((void)0)
#define WINTERS_PROFILE_THREAD_NAME(name) ((void)0)
#endif
