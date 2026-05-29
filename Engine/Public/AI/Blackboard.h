#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

class CBlackboard
{
public:
    using Value = std::variant<bool_t, i32_t, f32_t, Vec3, std::string, uint64_t>;

    void Set(const std::string& key, Value value) { m_mapValues[key] = std::move(value); }
    bool Has(const std::string& key) const { return m_mapValues.find(key) != m_mapValues.end(); }
    void Remove(const std::string& key) { m_mapValues.erase(key); }
    void Clear() { m_mapValues.clear(); }

    template<typename T>
    T Get(const std::string& key, T fallback) const
    {
        auto it = m_mapValues.find(key);
        if (it == m_mapValues.end())
            return fallback;
        if (const auto* value = std::get_if<T>(&it->second))
            return *value;
        return fallback;
    }

    bool_t GetBool(const std::string& key, bool_t fallback = false) const
    {
        return Get<bool_t>(key, fallback);
    }

    i32_t GetInt(const std::string& key, i32_t fallback = 0) const
    {
        return Get<i32_t>(key, fallback);
    }

    f32_t GetFloat(const std::string& key, f32_t fallback = 0.f) const
    {
        return Get<f32_t>(key, fallback);
    }

    Vec3 GetVec3(const std::string& key, Vec3 fallback = {}) const
    {
        return Get<Vec3>(key, fallback);
    }

    std::string GetString(const std::string& key, const std::string& fallback = "") const
    {
        return Get<std::string>(key, fallback);
    }

    EntityID GetEntity(const std::string& key, EntityID fallback = NULL_ENTITY) const
    {
        return static_cast<EntityID>(Get<uint64_t>(key, static_cast<uint64_t>(fallback)));
    }

private:
    std::unordered_map<std::string, Value> m_mapValues;
};

struct BlackboardComponent
{
    CBlackboard bb;
};

struct TeamBlackboardComponent
{
    CBlackboard bb;
    u8_t team = 0;
};
