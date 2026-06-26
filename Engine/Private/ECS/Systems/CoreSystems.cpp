#include "WintersPCH.h"
#include "ECS/Systems/CoreSystems.h"
#include "ECS/World.h"
#include "ECS/Components/CoreComponents.h"
#include "Core/CInput.h"
#include <cmath>

void CPlayerSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
	builder.Read<PlayerTag>().Write<VelocityComponent>();
}

void CPlayerSystem::Execute(CWorld& world, float dt)
{
	world.ForEach<PlayerTag, VelocityComponent>
		(
			[this](EntityID, PlayerTag&, VelocityComponent& vel)
			{
				Vec3 vDir{ 0.f, 0.f, 0.f };
				if (m_pInput->IsKeyDown('W'))
					vDir.z += 1.f;
				if (m_pInput->IsKeyDown('S'))
					vDir.z -= 1.f;
				if (m_pInput->IsKeyDown('A'))
					vDir.x -= 1.f;
				if (m_pInput->IsKeyDown('D'))
					vDir.x += 1.f;
				vel.vDirection = vDir;
				vel.fSpeed = (vDir.x != 0.f || vDir.z != 0.f) ? 5.f : 0.f;
			}
		);
}

void CAISystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
	builder.Write<AIStateComponent>()
		.Read<TransformComponent>()
		.Write<VelocityComponent>();
}

void CAISystem::Execute(CWorld& world, float dt)
{
	world.ForEach<AIStateComponent, TransformComponent, VelocityComponent>
		(
			[&world](EntityID entity, AIStateComponent& ai, TransformComponent& tf,
				VelocityComponent& vel)
			{
				if (ai.target == NULL_ENTITY || !world.IsAlive(ai.target))
				{
					ai.current = AIStateComponent::State::Idle;
					vel.fSpeed = 0.f;
					return;
				}
				auto& target = world.GetComponent<TransformComponent>(ai.target);
				float fDx = target.m_LocalPosition.x - tf.m_LocalPosition.x;
				float fDz = target.m_LocalPosition.z - tf.m_LocalPosition.z;
				float fDist = sqrtf(fDx * fDx + fDz * fDz);
				if (fDist <= ai.attackRange)
				{
					ai.current = AIStateComponent::State::Attack;
					vel.fSpeed = 0.f;
				}
				else if (fDist <= ai.detectRange)
				{
					ai.current = AIStateComponent::State::Chase;
					vel.vDirection = { fDx / fDist,0.f,fDz / fDist };
					vel.fSpeed = 3.f;
				}
				else
				{
					ai.current = AIStateComponent::State::Idle;
					vel.fSpeed = 0.f;
				}
			});
}

void CMovementSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
	builder.Write<TransformComponent>().Read<VelocityComponent>();
}

void CMovementSystem::Execute(CWorld& world, float dt)
{
	world.ForEach<TransformComponent, VelocityComponent>(
		[dt](EntityID, TransformComponent& tf, VelocityComponent& vel) {
			if (vel.fSpeed > 0.f) {
				Vec3 pos = tf.m_LocalPosition;
				pos.x += vel.vDirection.x * vel.fSpeed * dt;
				pos.y += vel.vDirection.y * vel.fSpeed * dt;
				pos.z += vel.vDirection.z * vel.fSpeed * dt;
				tf.SetPosition(pos);   // dirty 플래그 자동 설정 (local + world)
			}
		});
}

void CCollisionSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
	builder.UnknownWritesAll();
}

void CCollisionSystem::Execute(CWorld& world, float dt)
{
	// TODO: AABB O(n^2) -> Spatial Hash -> Jolt Physics (07. Physics)
}

void CHealthSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
	builder.Write<HealthComponent>().CreatesOrDestroysEntities();
}

void CHealthSystem::Execute(CWorld& world, float dt)
{
	world.ForEach<HealthComponent>
		([this](EntityID e, HealthComponent& hp)
			{
				if (hp.fCurrent <= 0.f && !hp.bIsDead)
				{
					hp.bIsDead = true;
					m_pCmdBuf->DeferDestroy(e);
				}
			});
}

