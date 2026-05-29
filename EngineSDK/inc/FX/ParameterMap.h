#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>

enum class eFxNamespace : u8_t
{
    System,
    Emitter,
    Particle,
    User,
    Event,
};

enum class eFxParameterType : u8_t
{
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    UInt,
    Bool,
    String,
    WideString,
};

inline u32_t FxHashName(std::string_view name)
{
    u32_t hash = 2166136261u;
    for (char ch : name)
    {
        hash ^= static_cast<u8_t>(ch);
        hash *= 16777619u;
    }
    return hash;
}

struct FxParameterID
{
    eFxNamespace ns = eFxNamespace::User;
    u32_t nameHash = 0;
    eFxParameterType type = eFxParameterType::Float;

    u64_t ToKey() const
    {
        return (static_cast<u64_t>(ns) << 56) |
            (static_cast<u64_t>(type) << 48) |
            static_cast<u64_t>(nameHash);
    }

    static FxParameterID Make(eFxNamespace valueNamespace,
        std::string_view name,
        eFxParameterType valueType)
    {
        FxParameterID id{};
        id.ns = valueNamespace;
        id.nameHash = FxHashName(name);
        id.type = valueType;
        return id;
    }

    bool operator==(const FxParameterID& rhs) const
    {
        return ns == rhs.ns && nameHash == rhs.nameHash && type == rhs.type;
    }
};

using FxValue = std::variant<
    f32_t,
    Vec2,
    Vec3,
    Vec4,
    i32_t,
    u32_t,
    bool_t,
    std::string,
    wstring_t>;

template<typename T>
inline constexpr eFxParameterType FxParameterTypeOf()
{
    if constexpr (std::is_same_v<T, f32_t>)
        return eFxParameterType::Float;
    else if constexpr (std::is_same_v<T, Vec2>)
        return eFxParameterType::Float2;
    else if constexpr (std::is_same_v<T, Vec3>)
        return eFxParameterType::Float3;
    else if constexpr (std::is_same_v<T, Vec4>)
        return eFxParameterType::Float4;
    else if constexpr (std::is_same_v<T, i32_t>)
        return eFxParameterType::Int;
    else if constexpr (std::is_same_v<T, u32_t>)
        return eFxParameterType::UInt;
    else if constexpr (std::is_same_v<T, bool_t>)
        return eFxParameterType::Bool;
    else if constexpr (std::is_same_v<T, std::string>)
        return eFxParameterType::String;
    else if constexpr (std::is_same_v<T, wstring_t>)
        return eFxParameterType::WideString;
    else
        static_assert(sizeof(T) == 0, "Unsupported FX parameter type");
}

class CFxParameterMap final
{
public:
    template<typename T>
    void Set(eFxNamespace valueNamespace, std::string_view name, const T& value)
    {
        const FxParameterID id =
            FxParameterID::Make(valueNamespace, name, FxParameterTypeOf<T>());
        m_Values[id.ToKey()] = value;
    }

    template<typename T>
    T Get(eFxNamespace valueNamespace,
        std::string_view name,
        const T& fallback = {}) const
    {
        const FxParameterID id =
            FxParameterID::Make(valueNamespace, name, FxParameterTypeOf<T>());
        const auto it = m_Values.find(id.ToKey());
        if (it == m_Values.end())
            return fallback;

        if (const T* pValue = std::get_if<T>(&it->second))
            return *pValue;

        return fallback;
    }

    template<typename T>
    bool_t Has(eFxNamespace valueNamespace, std::string_view name) const
    {
        const FxParameterID id =
            FxParameterID::Make(valueNamespace, name, FxParameterTypeOf<T>());
        return m_Values.find(id.ToKey()) != m_Values.end();
    }

    void Clear()
    {
        m_Values.clear();
    }

private:
    std::unordered_map<u64_t, FxValue> m_Values;
};
