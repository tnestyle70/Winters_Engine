#pragma once

class CWorld;
struct TickContext;

class CDamageQueueSystem
{
public:
    static void Execute(CWorld& world, const TickContext& tc);

private:
    CDamageQueueSystem() = delete;
};
