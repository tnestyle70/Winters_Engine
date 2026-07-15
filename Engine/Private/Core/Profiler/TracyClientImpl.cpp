// Tracy client 구현부. Engine DLL 에 단일 TU 로 컴파일한다.
// 다른 모듈(Client/Server)은 ProfilerAPI.h 의 TRACY_IMPORTS 경유로
// 이 DLL 이 export 한 Tracy 심볼을 사용한다.
#ifdef WINTERS_PROFILING
#define TRACY_ENABLE
#define TRACY_ON_DEMAND
#define TRACY_FIBERS
#define TRACY_EXPORTS
#include "TracyClient.cpp"
#endif
