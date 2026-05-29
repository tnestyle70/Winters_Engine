#pragma once

class CWorld;
struct TickContext;

class CDeathSystem
{
public:
    static void Execute(CWorld& world, const TickContext& tc);

private:
    CDeathSystem() = delete;
};
