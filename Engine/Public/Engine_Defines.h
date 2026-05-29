#ifndef Engine_Defines_h__
#define Engine_Defines_h__

#include <DirectXMath.h>

using namespace DirectX;

#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include <functional>

#define DIRECTINPUT_VERSION	0x0800
#include <dinput.h>

using namespace std;

#include <string>
#include <unordered_map>
#include <ctime>
#include <memory>

#include "Engine_Enum.h"
#include "Engine_Macro.h"
#include "Engine_Struct.h"
#include "Engine_Typedef.h"
#include "Engine_Function.h"

#pragma warning(disable : 4251)

#ifdef _DEBUG

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif

#ifndef WINTERS_ENABLE_NON_AI_DEBUG_STRING
#define WINTERS_ENABLE_NON_AI_DEBUG_STRING 0
#endif

#ifndef WINTERS_ENABLE_AI_DEBUG_STRING
#define WINTERS_ENABLE_AI_DEBUG_STRING 1
#endif

#ifndef WINTERS_DEBUG_STRING_GATE_DEFINED
#define WINTERS_DEBUG_STRING_GATE_DEFINED
inline void WintersOutputDebugStringA(const char* pText)
{
#if WINTERS_ENABLE_NON_AI_DEBUG_STRING
    ::OutputDebugStringA(pText);
#else
    (void)pText;
#endif
}

inline void WintersOutputDebugStringW(const wchar_t* pText)
{
#if WINTERS_ENABLE_NON_AI_DEBUG_STRING
    ::OutputDebugStringW(pText);
#else
    (void)pText;
#endif
}

inline void WintersOutputAIDebugStringA(const char* pText)
{
#if WINTERS_ENABLE_AI_DEBUG_STRING
    ::OutputDebugStringA(pText);
#else
    (void)pText;
#endif
}

inline void WintersOutputAIDebugStringW(const wchar_t* pText)
{
#if WINTERS_ENABLE_AI_DEBUG_STRING
    ::OutputDebugStringW(pText);
#else
    (void)pText;
#endif
}
#endif

#ifndef WINTERS_KEEP_RAW_OUTPUT_DEBUG_STRING
#ifndef OutputDebugStringA
#define OutputDebugStringA WintersOutputDebugStringA
#endif
#ifndef OutputDebugStringW
#define OutputDebugStringW WintersOutputDebugStringW
#endif
#endif

using namespace Engine;

#endif // Engine_Defines_h__
