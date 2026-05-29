#pragma once

class CWorld;
struct TickContext;

class CRecallSystem
{
public:
	static void Execute(CWorld& world, const TickContext& tc);

private:
	CRecallSystem() = delete;
};