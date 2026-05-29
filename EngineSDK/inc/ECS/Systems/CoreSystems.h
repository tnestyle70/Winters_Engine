#pragma once
#include "ECS/ISystem.h"
#include "ECS/CCommandBuffer.h"

class CInput;

class CPlayerSystem : public ISystem 
{
public: 
	CPlayerSystem(CInput* p) :m_pInput(p) {}
	  uint32_t GetPhase() const override { return 0; } 
	  const char* GetName() const override { return "PlayerSystem"; }
	  void DescribeAccess(CSystemAccessBuilder& builder) const override;
	  void Execute(CWorld& world, float dt) override;

private: 
	CInput* m_pInput;
};

class CAISystem : public ISystem 
{
public: 
	uint32_t GetPhase() const override { return 0; } 
	const char* GetName() const override { return "AISystem"; }
	void DescribeAccess(CSystemAccessBuilder& builder) const override;
	void Execute(CWorld& world, float dt) override;
};

class CMovementSystem : public ISystem
{
public: 
	uint32_t GetPhase() const override { return 1; } 
	const char* GetName() const override { return "MovementSystem"; }
	void DescribeAccess(CSystemAccessBuilder& builder) const override;
	void Execute(CWorld& world, float dt) override;
};

class CCollisionSystem : public ISystem
{
public: 
	uint32_t GetPhase() const override { return 2; } 
	const char* GetName() const override { return "CollisionSystem"; }
	void DescribeAccess(CSystemAccessBuilder& builder) const override;
	void Execute(CWorld& world, float dt) override;
};

class CHealthSystem : public ISystem
{
public: 
	CHealthSystem(CCommandBuffer* p) :m_pCmdBuf(p) {}
	uint32_t GetPhase() const override { return 3; }
	const char* GetName() const override { return "HealthSystem"; }
	void DescribeAccess(CSystemAccessBuilder& builder) const override;
	void Execute(CWorld& world, float dt) override;
private: 
	CCommandBuffer* m_pCmdBuf;
};
