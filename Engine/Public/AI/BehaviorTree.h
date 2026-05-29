#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "AI/Blackboard.h"
#include "ECS/Entity.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class CWorld;

NS_BEGIN(Engine)

enum class eBTStatus : u8_t
{
    Invalid = 0,
    Running,
    Success,
    Failure
};

struct BTContext
{
    CWorld* pWorld = nullptr;
    EntityID self = NULL_ENTITY;
    CBlackboard* pBB = nullptr;
    CBlackboard* pTeamBB = nullptr;
    f32_t dt = 0.f;
};

class WINTERS_ENGINE CBTNode
{
public:
    CBTNode() = default;
    virtual ~CBTNode() = default;

    CBTNode(const CBTNode&) = delete;
    CBTNode& operator=(const CBTNode&) = delete;
    CBTNode(CBTNode&&) noexcept = default;
    CBTNode& operator=(CBTNode&&) noexcept = default;

    virtual eBTStatus Tick(BTContext& ctx) = 0;
    virtual const char* GetName() const = 0;
    virtual void Reset() {}

    virtual size_t GetChildCount() const { return 0; }
    virtual CBTNode* GetChild(size_t) const { return nullptr; }
    eBTStatus GetLastStatus() const { return m_lastStatus; }

protected:
    eBTStatus m_lastStatus = eBTStatus::Invalid;
};

class WINTERS_ENGINE CBTSelector final : public CBTNode
{
public:
    CBTSelector() = default;
    ~CBTSelector() override = default;
    CBTSelector(const CBTSelector&) = delete;
    CBTSelector& operator=(const CBTSelector&) = delete;
    CBTSelector(CBTSelector&&) noexcept = default;
    CBTSelector& operator=(CBTSelector&&) noexcept = default;

    void AddChild(std::unique_ptr<CBTNode> child) { m_children.push_back(std::move(child)); }
    eBTStatus Tick(BTContext& ctx) override;
    const char* GetName() const override { return "Selector"; }
    size_t GetChildCount() const override { return m_children.size(); }
    CBTNode* GetChild(size_t i) const override { return m_children[i].get(); }
    void Reset() override;

private:
    std::vector<std::unique_ptr<CBTNode>> m_children;
};

class WINTERS_ENGINE CBTSequence final : public CBTNode
{
public:
    CBTSequence() = default;
    ~CBTSequence() override = default;
    CBTSequence(const CBTSequence&) = delete;
    CBTSequence& operator=(const CBTSequence&) = delete;
    CBTSequence(CBTSequence&&) noexcept = default;
    CBTSequence& operator=(CBTSequence&&) noexcept = default;

    void AddChild(std::unique_ptr<CBTNode> child) { m_children.push_back(std::move(child)); }
    eBTStatus Tick(BTContext& ctx) override;
    const char* GetName() const override { return "Sequence"; }
    size_t GetChildCount() const override { return m_children.size(); }
    CBTNode* GetChild(size_t i) const override { return m_children[i].get(); }
    void Reset() override;

private:
    std::vector<std::unique_ptr<CBTNode>> m_children;
};

class WINTERS_ENGINE CBTParallel final : public CBTNode
{
public:
    explicit CBTParallel(u32_t successThreshold) : m_uThreshold(successThreshold) {}
    ~CBTParallel() override = default;
    CBTParallel(const CBTParallel&) = delete;
    CBTParallel& operator=(const CBTParallel&) = delete;
    CBTParallel(CBTParallel&&) noexcept = default;
    CBTParallel& operator=(CBTParallel&&) noexcept = default;

    void AddChild(std::unique_ptr<CBTNode> child) { m_children.push_back(std::move(child)); }
    eBTStatus Tick(BTContext& ctx) override;
    const char* GetName() const override { return "Parallel"; }
    size_t GetChildCount() const override { return m_children.size(); }
    CBTNode* GetChild(size_t i) const override { return m_children[i].get(); }
    void Reset() override;

private:
    std::vector<std::unique_ptr<CBTNode>> m_children;
    u32_t m_uThreshold = 1;
};

class WINTERS_ENGINE CBTInverter final : public CBTNode
{
public:
    CBTInverter() = default;
    ~CBTInverter() override = default;
    CBTInverter(const CBTInverter&) = delete;
    CBTInverter& operator=(const CBTInverter&) = delete;
    CBTInverter(CBTInverter&&) noexcept = default;
    CBTInverter& operator=(CBTInverter&&) noexcept = default;

    void SetChild(std::unique_ptr<CBTNode> child) { m_pChild = std::move(child); }
    eBTStatus Tick(BTContext& ctx) override;
    const char* GetName() const override { return "Inverter"; }
    size_t GetChildCount() const override { return m_pChild ? 1u : 0u; }
    CBTNode* GetChild(size_t) const override { return m_pChild.get(); }
    void Reset() override;

private:
    std::unique_ptr<CBTNode> m_pChild;
};

class WINTERS_ENGINE CBTCooldownDecorator final : public CBTNode
{
public:
    explicit CBTCooldownDecorator(f32_t cooldownSec) : m_fCooldownMax(cooldownSec) {}
    ~CBTCooldownDecorator() override = default;
    CBTCooldownDecorator(const CBTCooldownDecorator&) = delete;
    CBTCooldownDecorator& operator=(const CBTCooldownDecorator&) = delete;
    CBTCooldownDecorator(CBTCooldownDecorator&&) noexcept = default;
    CBTCooldownDecorator& operator=(CBTCooldownDecorator&&) noexcept = default;

    void SetChild(std::unique_ptr<CBTNode> child) { m_pChild = std::move(child); }
    eBTStatus Tick(BTContext& ctx) override;
    const char* GetName() const override { return "Cooldown"; }
    size_t GetChildCount() const override { return m_pChild ? 1u : 0u; }
    CBTNode* GetChild(size_t) const override { return m_pChild.get(); }
    void Reset() override;

private:
    std::unique_ptr<CBTNode> m_pChild;
    f32_t m_fCooldownMax = 0.f;
    f32_t m_fCooldownTimer = 0.f;
};

using BTConditionFn = std::function<bool_t(BTContext&)>;
using BTActionFn = std::function<eBTStatus(BTContext&)>;

class WINTERS_ENGINE CBTCondition final : public CBTNode
{
public:
    CBTCondition(std::string name, BTConditionFn fn)
        : m_strName(std::move(name)), m_fn(std::move(fn)) {}

    eBTStatus Tick(BTContext& ctx) override;
    const char* GetName() const override { return m_strName.c_str(); }

private:
    std::string m_strName;
    BTConditionFn m_fn;
};

class WINTERS_ENGINE CBTAction final : public CBTNode
{
public:
    CBTAction(std::string name, BTActionFn fn)
        : m_strName(std::move(name)), m_fn(std::move(fn)) {}

    eBTStatus Tick(BTContext& ctx) override;
    const char* GetName() const override { return m_strName.c_str(); }

private:
    std::string m_strName;
    BTActionFn m_fn;
};

class WINTERS_ENGINE CBehaviorTree
{
public:
    CBehaviorTree() = default;
    ~CBehaviorTree() = default;

    CBehaviorTree(const CBehaviorTree&) = delete;
    CBehaviorTree& operator=(const CBehaviorTree&) = delete;
    CBehaviorTree(CBehaviorTree&&) noexcept = default;
    CBehaviorTree& operator=(CBehaviorTree&&) noexcept = default;

    void SetRoot(std::unique_ptr<CBTNode> root) { m_pRoot = std::move(root); }
    eBTStatus Tick(BTContext& ctx);
    void Reset();
    CBTNode* GetRoot() const { return m_pRoot.get(); }

private:
    std::unique_ptr<CBTNode> m_pRoot;
};

NS_END
