#include "Shared/GameSim/Definitions/ItemDef.h"

CItemRegistry& CItemRegistry::Instance()
{
    static CItemRegistry s_Instance;
    return s_Instance;
}

const ItemDef* CItemRegistry::Find(u16_t itemId) const
{
    for (const ItemDef& item : m_Items)
    {
        if (item.itemId == itemId)
            return &item;
    }
    return nullptr;
}

bool_t CItemRegistry::LoadFromItemDefs(const ItemDef* items, std::size_t count)
{
    if (!items || count == 0u)
    {
        m_Items.clear();
        return false;
    }
    m_Items.assign(items, items + count);
    return true;
}

void CItemRegistry::Clear()
{
    m_Items.clear();
}
