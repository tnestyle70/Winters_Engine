#pragma once

// ─────────────────────────────────────────────────────────────
// JobDecl — 함수포인터 기반 Job 선언.
// 5-A 에서는 주로 std::function<void()> Submit 을 쓰므로 이 구조체는
// 5-B 또는 대량 Submit 시 오버헤드 감소용으로만 사용.
// ─────────────────────────────────────────────────────────────
using JobFn = void(*)(void* pData);

struct JobDecl
{
    JobFn       pFn = nullptr;
    void* pData = nullptr;
    const char* pszName = "unnamed";
};