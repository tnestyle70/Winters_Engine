// Engine/Include/ProfilerAPI.h 교체
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

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
#define WINTERS_PROFILE_SCOPE(name) \
    ::CProfileScope WINTERS_PROFILE_CAT(_winProfScope_, __LINE__)(name)

#else
#define WINTERS_PROFILE_SCOPE(name) ((void)0)
#endif