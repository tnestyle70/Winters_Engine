#pragma once

class CWorld;
struct TickContext;

class CMoveSystem
{
public:
    static void Execute(CWorld& world, const TickContext& tc);

private:
    CMoveSystem() = delete;
};
