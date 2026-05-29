#pragma once
#include "Core/CTimer.h"

NS_BEGIN(Engine)

class CTimer_Manager
{
private:
	CTimer_Manager();
public:
	~CTimer_Manager();

public:
	f32_t			Get_TimeDelta(const wstring_t& strTimerTag);

public:
	HRESULT			Add_Timer(const wstring_t& strTimerTag);
	void			UpdateTimeDelta(const wstring_t& strTimerTag);

	static unique_ptr<CTimer_Manager> Create();

private:
	CTimer* Find_Timer(const wstring_t& strTimerTag);

private:
	map<wstring_t, unique_ptr<CTimer>>		m_Timers;
};

NS_END
