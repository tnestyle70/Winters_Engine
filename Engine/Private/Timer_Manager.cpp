#include "Core/Timer_Manager.h"
#include "Core/CTimer.h"

CTimer_Manager::CTimer_Manager()
{}

CTimer_Manager::~CTimer_Manager()
{}

f32_t CTimer_Manager::Get_TimeDelta(const wstring_t& strTimerTag)
{
	CTimer* pTimer = Find_Timer(strTimerTag);
	if (nullptr == pTimer)
		return 0.f;

	return pTimer->Get_TimeDelta();
}

void CTimer_Manager::UpdateTimeDelta(const wstring_t& strTimerTag)
{
	CTimer* pTimer = Find_Timer(strTimerTag);
	if (nullptr == pTimer)
		return;

	pTimer->Update_Timer();
}

HRESULT CTimer_Manager::Add_Timer(const wstring_t& strTimerTag)
{
	//타이머가 이미 존재하는 경우 
	if (nullptr != Find_Timer(strTimerTag))
		return E_FAIL;

	unique_ptr<CTimer> pTimer = CTimer::Create();
	if (pTimer == nullptr)
		return E_FAIL;
	m_Timers.emplace(strTimerTag, std::move(pTimer));

	return S_OK;
}

CTimer* CTimer_Manager::Find_Timer(const wstring_t& strTimerTag)
{
	auto iter = m_Timers.find(strTimerTag);
	//timer가 존재하지 않을 경우, 타이머 생성해주기
	if (iter == m_Timers.end())
		return nullptr;
	//timer 있으면 해당 타이머의 원시 포인터 반환
	return iter->second.get();
}

unique_ptr<CTimer_Manager> CTimer_Manager::Create()
{
	auto pInstance = unique_ptr<CTimer_Manager>(new CTimer_Manager());

	return pInstance;
}
