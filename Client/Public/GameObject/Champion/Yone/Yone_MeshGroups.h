#pragma once

#include "ECS/Components/MeshGroupVisibilityComponent.h"

namespace Yone::MeshGroups
{
    enum class eYoneSubmesh : u32_t
    {
        DefaultMaterial = 0,
        GhostKatana = 1,
        Skirt = 2,
        SkirtInner = 3,
        Sheath = 4,
        Body = 5,
        Katana = 6,
        KatanaSmear = 7,
        GhostKatanaSmear = 8,
        Azakana = 9,
        Sushi = 10,
        Fish = 11,
        Instrument = 12,
        BowSmear = 13,
        Count = 14
    };

    inline VisibilityMask MaskNone()
    {
        return VisibilityMask{};
    }

    inline void Add(VisibilityMask& mask, eYoneSubmesh part)
    {
        SetSubmeshVisible(mask, static_cast<u32_t>(part), true);
    }

    inline VisibilityMask MaskBaseDefault()
    {
        VisibilityMask mask = MaskNone();
        Add(mask, eYoneSubmesh::Skirt);
        Add(mask, eYoneSubmesh::SkirtInner);
        Add(mask, eYoneSubmesh::Sheath);
        Add(mask, eYoneSubmesh::Body);
        Add(mask, eYoneSubmesh::Katana);
        Add(mask, eYoneSubmesh::Azakana);
        return mask;
    }

    inline VisibilityMask MaskESpiritOnly()
    {
        VisibilityMask mask = MaskNone();
        Add(mask, eYoneSubmesh::GhostKatana);
        Add(mask, eYoneSubmesh::GhostKatanaSmear);
        return mask;
    }

    inline VisibilityMask MaskSkillSmearOnly()
    {
        VisibilityMask mask = MaskNone();
        Add(mask, eYoneSubmesh::KatanaSmear);
        Add(mask, eYoneSubmesh::GhostKatanaSmear);
        Add(mask, eYoneSubmesh::BowSmear);
        return mask;
    }

    inline VisibilityMask MaskDebugAll()
    {
        return MakeAllVisibleMask();
    }
}
