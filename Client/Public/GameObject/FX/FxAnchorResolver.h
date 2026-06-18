#pragma once

#include "ECS/Components/RenderComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Entity.h"
#include "ECS/World.h"
#include "FX/FxAsset.h"
#include "Renderer/ModelRenderer.h"

#include <DirectXMath.h>

namespace FxAnchor
{
    inline bool_t IsZero(const Vec3& v)
    {
        return v.x == 0.f && v.y == 0.f && v.z == 0.f;
    }

    inline Vec3 Add(const Vec3& a, const Vec3& b)
    {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    inline Vec3 ResolveLegacyOffset(const FxAnchorDesc& anchor, const Vec3& vLegacyOffset)
    {
        if (anchor.eAnchorType != eFxAnchorType::Entity || !IsZero(anchor.vAnchorOffset))
            return anchor.vAnchorOffset;
        return vLegacyOffset;
    }

    inline void FlushTransformForFx(TransformComponent& tf)
    {
        if (tf.m_bLocalDirty)
        {
            DirectX::XMVECTOR scale = DirectX::XMLoadFloat3(
                reinterpret_cast<const DirectX::XMFLOAT3*>(&tf.m_LocalScale));
            DirectX::XMVECTOR rot = DirectX::XMQuaternionRotationRollPitchYaw(
                tf.m_LocalRotation.x,
                tf.m_LocalRotation.y,
                tf.m_LocalRotation.z);
            DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(
                reinterpret_cast<const DirectX::XMFLOAT3*>(&tf.m_LocalPosition));
            DirectX::XMMATRIX local =
                DirectX::XMMatrixAffineTransformation(scale, DirectX::XMVectorZero(), rot, pos);
            DirectX::XMStoreFloat4x4(
                reinterpret_cast<DirectX::XMFLOAT4X4*>(&tf.m_LocalMatrix), local);
            tf.m_bLocalDirty = false;
            tf.m_bWorldDirty = true;
        }

        if (tf.m_bWorldDirty)
        {
            tf.m_WorldMatrix = tf.m_LocalMatrix;
            tf.m_bWorldDirty = false;
        }
    }

    inline bool_t TryResolveEntityWorldPosition(CWorld& world,
        EntityID entity,
        const Vec3& vOffset,
        Vec3& vOutWorldPos)
    {
        if (entity == NULL_ENTITY || !world.HasComponent<TransformComponent>(entity))
            return false;

        TransformComponent& tf = world.GetComponent<TransformComponent>(entity);
        FlushTransformForFx(tf);
        vOutWorldPos = Add(tf.GetWorldPosition(), vOffset);
        return true;
    }

    inline bool_t TryResolveWorldPosition(CWorld& world,
        EntityID attachTo,
        const FxAnchorDesc& anchor,
        const Vec3& vLegacyOffset,
        const Vec3& vCurrentWorldPos,
        Vec3& vOutWorldPos)
    {
        if (anchor.eAnchorType == eFxAnchorType::World)
        {
            vOutWorldPos = vCurrentWorldPos;
            return true;
        }

        if (anchor.eAnchorType == eFxAnchorType::Bone)
        {
            if (attachTo != NULL_ENTITY &&
                world.HasComponent<TransformComponent>(attachTo) &&
                world.HasComponent<RenderComponent>(attachTo))
            {
                TransformComponent& tf = world.GetComponent<TransformComponent>(attachTo);
                RenderComponent& rc = world.GetComponent<RenderComponent>(attachTo);
                FlushTransformForFx(tf);

                if (rc.pRenderer &&
                    rc.pRenderer->TryResolveBoneWorldPosition(
                        anchor.strAnchorName,
                        tf.GetWorldMatrix(),
                        anchor.vAnchorOffset,
                        vOutWorldPos))
                {
                    return true;
                }
            }

            if (anchor.eFallback == eFxAnchorFallback::None)
                return false;

            return TryResolveEntityWorldPosition(world, attachTo, vLegacyOffset, vOutWorldPos);
        }

        if (anchor.eAnchorType == eFxAnchorType::Socket ||
            anchor.eAnchorType == eFxAnchorType::Submesh ||
            anchor.eAnchorType == eFxAnchorType::TargetSegment)
        {
            if (anchor.eFallback == eFxAnchorFallback::None)
                return false;
        }

        return TryResolveEntityWorldPosition(
            world,
            attachTo,
            ResolveLegacyOffset(anchor, vLegacyOffset),
            vOutWorldPos);
    }
}
