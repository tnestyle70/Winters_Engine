#include "Core/CTimer.h"

HRESULT CTimer::Initialize()
{
	QueryPerformanceCounter(&m_FrameTime);			// 1077
	QueryPerformanceCounter(&m_LastTime);			// 1085
	QueryPerformanceCounter(&m_FixTime);			// 1090

	QueryPerformanceFrequency(&m_CpuTick);		// cpu tick 값을 얻어오는 함수

	return S_OK;
}

void CTimer::Update_Timer()
{
	QueryPerformanceCounter(&m_FrameTime);			// 1500

	if (m_FrameTime.QuadPart - m_FixTime.QuadPart >= m_CpuTick.QuadPart)
	{
		QueryPerformanceFrequency(&m_CpuTick);
		m_FixTime = m_FrameTime;
	}

	m_fTimeDelta = (m_FrameTime.QuadPart - m_LastTime.QuadPart) / (f32_t)m_CpuTick.QuadPart;

	// 스파이크 보호 상한만 둔다. 과거 16.7ms 하드 클램프는 프레임이 60fps 경계를
	// 넘는 순간 게임 시간이 실제 시간보다 느려지는 슬로모션 스터터를 만들었다.
	if (m_fTimeDelta >= 0.1f)
	{
		m_fTimeDelta = 0.1f;
	}
	if (m_fTimeDelta < 0.f)
	{
		m_fTimeDelta = 0.f;
	}

	m_LastTime = m_FrameTime;
}

unique_ptr<CTimer> CTimer::Create()
{
	auto pInstance = unique_ptr<CTimer>(new CTimer());
	
	if (FAILED(pInstance->Initialize()))
		return nullptr;

	return pInstance;
}
