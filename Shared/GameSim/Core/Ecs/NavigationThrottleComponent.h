#pragma once

// Phase 7F adapter: Shared/GameSim 코드는 Engine ECS 헤더를 직접 include하지 않고
// 이 어댑터를 경유한다. 백엔드 교체 시 이 파일만 바꾼다 (WINTERS_DEPENDENCY_MAP.md §3).
#include "ECS/Components/NavigationThrottleComponent.h"
