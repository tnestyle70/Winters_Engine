#include "WintersPCH.h"
#include "AI/BehaviorTree.h"

NS_BEGIN(Engine)

eBTStatus CBTSelector::Tick(BTContext& ctx)
{
    for (auto& child : m_children)
    {
        const eBTStatus status = child->Tick(ctx);
        if (status == eBTStatus::Success || status == eBTStatus::Running)
        {
            m_lastStatus = status;
            return status;
        }
    }

    m_lastStatus = eBTStatus::Failure;
    return m_lastStatus;
}

void CBTSelector::Reset()
{
    for (auto& child : m_children)
        child->Reset();
    m_lastStatus = eBTStatus::Invalid;
}

eBTStatus CBTSequence::Tick(BTContext& ctx)
{
    for (auto& child : m_children)
    {
        const eBTStatus status = child->Tick(ctx);
        if (status == eBTStatus::Failure || status == eBTStatus::Running)
        {
            m_lastStatus = status;
            return status;
        }
    }

    m_lastStatus = eBTStatus::Success;
    return m_lastStatus;
}

void CBTSequence::Reset()
{
    for (auto& child : m_children)
        child->Reset();
    m_lastStatus = eBTStatus::Invalid;
}

eBTStatus CBTParallel::Tick(BTContext& ctx)
{
    u32_t successCount = 0;
    bool_t bAnyRunning = false;

    for (auto& child : m_children)
    {
        const eBTStatus status = child->Tick(ctx);
        if (status == eBTStatus::Success)
            ++successCount;
        else if (status == eBTStatus::Running)
            bAnyRunning = true;
    }

    if (successCount >= m_uThreshold)
        m_lastStatus = eBTStatus::Success;
    else if (bAnyRunning)
        m_lastStatus = eBTStatus::Running;
    else
        m_lastStatus = eBTStatus::Failure;

    return m_lastStatus;
}

void CBTParallel::Reset()
{
    for (auto& child : m_children)
        child->Reset();
    m_lastStatus = eBTStatus::Invalid;
}

eBTStatus CBTInverter::Tick(BTContext& ctx)
{
    if (!m_pChild)
    {
        m_lastStatus = eBTStatus::Failure;
        return m_lastStatus;
    }

    const eBTStatus status = m_pChild->Tick(ctx);
    if (status == eBTStatus::Success)
        m_lastStatus = eBTStatus::Failure;
    else if (status == eBTStatus::Failure)
        m_lastStatus = eBTStatus::Success;
    else
        m_lastStatus = status;

    return m_lastStatus;
}

void CBTInverter::Reset()
{
    if (m_pChild)
        m_pChild->Reset();
    m_lastStatus = eBTStatus::Invalid;
}

eBTStatus CBTCooldownDecorator::Tick(BTContext& ctx)
{
    if (m_fCooldownTimer > 0.f)
    {
        m_fCooldownTimer -= ctx.dt;
        if (m_fCooldownTimer < 0.f)
            m_fCooldownTimer = 0.f;
        m_lastStatus = eBTStatus::Failure;
        return m_lastStatus;
    }

    if (!m_pChild)
    {
        m_lastStatus = eBTStatus::Failure;
        return m_lastStatus;
    }

    const eBTStatus status = m_pChild->Tick(ctx);
    if (status == eBTStatus::Success || status == eBTStatus::Failure)
        m_fCooldownTimer = m_fCooldownMax;

    m_lastStatus = status;
    return m_lastStatus;
}

void CBTCooldownDecorator::Reset()
{
    m_fCooldownTimer = 0.f;
    if (m_pChild)
        m_pChild->Reset();
    m_lastStatus = eBTStatus::Invalid;
}

eBTStatus CBTCondition::Tick(BTContext& ctx)
{
    m_lastStatus = m_fn(ctx) ? eBTStatus::Success : eBTStatus::Failure;
    return m_lastStatus;
}

eBTStatus CBTAction::Tick(BTContext& ctx)
{
    m_lastStatus = m_fn(ctx);
    return m_lastStatus;
}

eBTStatus CBehaviorTree::Tick(BTContext& ctx)
{
    if (!m_pRoot)
        return eBTStatus::Failure;
    return m_pRoot->Tick(ctx);
}

void CBehaviorTree::Reset()
{
    if (m_pRoot)
        m_pRoot->Reset();
}

NS_END
