#pragma once
#include "Engine_Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"   // 폴더-정규화 경로: Engine(..\Public) + SDK(EngineSDK\inc) 양쪽 resolve
#include <vector>

//TransformComponent
//ECS 친화 Plain Struct Pos/Rot/Scale -> parent entity
//Dirty Flag : m_bLocalDirty : local -> world 재계산
//hierarchy - parent == null entity일 경우 world = local 
//struct의 멤버를 직접 쓰지 않고 헬퍼 함수(set position) 사용

struct TransformComponent
{
	//Euler Radians
	Vec3 m_LocalPosition{ 0.f, 0.f, 0.f };
	Vec3 m_LocalRotation{ 0.f, 0.f, 0.f };
	Vec3 m_LocalScale{ 1.f, 1.f, 1.f };
	//Cached Matrix(Transform System이 계산)
	Mat4 m_LocalMatrix{}; //Scale Rot Translation
	Mat4 m_WorldMatrix{}; //parent world * local
	//Hierarchy
	EntityID m_Parent{ NULL_ENTITY };
	std::vector<EntityID> m_vecChildren{};
	//Dirty Flags
	bool m_bLocalDirty{ true }; //localmatrix 재계산 필요
	bool m_bWorldDirty{ true }; //부모 혹은 자기 자신 local 변경 worldmatrix 재계산

	//Helper Setters
	void SetPosition(const Vec3& p)
	{
		m_LocalPosition = p;
		m_bLocalDirty = true;
		m_bWorldDirty = true;
	}
	void SetRotation(const Vec3& r)
	{
		m_LocalRotation = r;
		m_bLocalDirty = true;
		m_bWorldDirty = true;
	}
	void SetScale(const Vec3& s)
	{
		m_LocalScale = s;
		m_bLocalDirty = true;
		m_bWorldDirty = true;
	}
	void SetPosition(f32_t x, f32_t y, f32_t z) { SetPosition(Vec3{ x, y, z }); }
	void SetScale(f32_t uniform) { SetScale(Vec3{ uniform, uniform, uniform }); }

	//Getters
	const Mat4& GetWorldMatrix() const { return m_WorldMatrix; }
	const Mat4& GetLocalMatrix() const { return m_LocalMatrix; }

	// 값 반환 (임시 객체라 참조 불가)
	Vec3 GetWorldPosition() const
	{
		return { m_WorldMatrix.m._41, m_WorldMatrix.m._42, m_WorldMatrix.m._43 };
	}

	// 로컬 원본은 멤버가 존재하니 const 참조 OK
	const Vec3& GetLocalPosition() const { return m_LocalPosition; }
	const Vec3& GetPosition()      const { return m_LocalPosition; }  // alias
	const Vec3& GetRotation()      const { return m_LocalRotation; }
	const Vec3& GetScale()         const { return m_LocalScale; }
};
