#pragma once

#include "WintersTypes.h"
#include "RHI/RHIHandles.h"
#include <cassert>
#include <thread>
#include <vector>

template<typename TResource, typename TTag>
class CRHIResourceTable final
{
public:
    using HandleType = RHIHandle<TTag>;

    CRHIResourceTable()
        : m_RenderThreadId(std::this_thread::get_id())
    {
    }

    CRHIResourceTable(const CRHIResourceTable&) = delete;
    CRHIResourceTable& operator=(const CRHIResourceTable&) = delete;

    ~CRHIResourceTable()
    {
        Clear();
    }

    HandleType Insert(TResource* pResource)
    {
        AssertRenderThread();
        if (!pResource)
            return {};

        u32_t index = 0;
        if (!m_FreeList.empty())
        {
            index = m_FreeList.back();
            m_FreeList.pop_back();
            m_Slots[index].pResource = pResource;
            ++m_Slots[index].generation;
            if (m_Slots[index].generation == 0)
                m_Slots[index].generation = 1;
        }
        else
        {
            index = static_cast<u32_t>(m_Slots.size());
            m_Slots.push_back({ pResource, 1u });
        }

        return HandleType::Make(index, m_Slots[index].generation);
    }

    TResource* Lookup(HandleType handle)
    {
        AssertRenderThread();
        if (!handle.IsValid())
            return nullptr;

        const u32_t index = handle.Index();
        if (index >= m_Slots.size())
            return nullptr;

        const Slot& slot = m_Slots[index];
        if (slot.generation != handle.Generation())
            return nullptr;

        return slot.pResource;
    }

    const TResource* Lookup(HandleType handle) const
    {
        AssertRenderThread();
        if (!handle.IsValid())
            return nullptr;

        const u32_t index = handle.Index();
        if (index >= m_Slots.size())
            return nullptr;

        const Slot& slot = m_Slots[index];
        if (slot.generation != handle.Generation())
            return nullptr;

        return slot.pResource;
    }

    void Erase(HandleType handle)
    {
        AssertRenderThread();
        if (!handle.IsValid())
            return;

        const u32_t index = handle.Index();
        if (index >= m_Slots.size())
            return;

        Slot& slot = m_Slots[index];
        if (slot.generation != handle.Generation())
            return;

        delete slot.pResource;
        slot.pResource = nullptr;
        m_FreeList.push_back(index);
    }

    void Clear()
    {
        AssertRenderThread();
        for (Slot& slot : m_Slots)
        {
            delete slot.pResource;
            slot.pResource = nullptr;
        }
        m_Slots.clear();
        m_FreeList.clear();
    }

private:
    struct Slot
    {
        TResource* pResource = nullptr;
        u32_t generation = 0;
    };

    void AssertRenderThread() const
    {
#ifdef _DEBUG
        assert(std::this_thread::get_id() == m_RenderThreadId &&
            "CRHIResourceTable must be accessed from the render thread");
#endif
    }

    std::vector<Slot> m_Slots;
    std::vector<u32_t> m_FreeList;
    std::thread::id m_RenderThreadId;
};
