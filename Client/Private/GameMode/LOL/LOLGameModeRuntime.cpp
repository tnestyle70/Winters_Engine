#include "GameMode/LOL/LOLGameModeRuntime.h"

std::unique_ptr<CLOLGameModeRuntime> CLOLGameModeRuntime::Create(const GameModeDef& def)
{
	return std::unique_ptr<CLOLGameModeRuntime>(new CLOLGameModeRuntime(def));
}

CLOLGameModeRuntime::CLOLGameModeRuntime(const GameModeDef& def)
	: m_Def(def)
{
}

bool_t CLOLGameModeRuntime::OnLoadContent()
{
	return true;
}

bool_t CLOLGameModeRuntime::OnCreateWorld(CWorld& /*world*/)
{
	return true;
}

void CLOLGameModeRuntime::OnMatchStart()
{
	m_bGameOver = false;
}

void CLOLGameModeRuntime::Tick(f32_t /*dt*/)
{
}

void CLOLGameModeRuntime::OnEntityKilled(EntityID /*victim*/, EntityID /*killer*/)
{
}
