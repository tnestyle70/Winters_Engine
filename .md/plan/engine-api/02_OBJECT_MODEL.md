# 02. Object Model & Reflection — UE5-Style WObject/WCLASS/WPROPERTY

> **UE5 대응**: `UObject`, `UClass`, `UPROPERTY()`, `UFUNCTION()`, `FProperty`, `UStruct`
> **현재 Winters**: 리플렉션 없음, ImGui 노출 수동, 직렬화 수동, 네트워크 pack/unpack 수동
> **목표**: 매크로 기반 컴파일 타임 리플렉션 → 에디터 자동 노출 + 자동 직렬화 + 네트워크 자동 리플리케이션

---

## 1. Architecture Overview

### 1.1 UE5 Reflection 핵심

```
UCLASS()
class AMyActor : public AActor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Replicated)
    float Health;
};

→ UHT (Unreal Header Tool) 가 .generated.h 생성
→ UClass* 에 FProperty 목록 등록
→ 에디터: FProperty 순회 → 자동 UI 생성
→ 네트워크: FRepLayout 이 FProperty 순회 → 자동 delta 직렬화
```

### 1.2 Winters 접근: 매크로 + constexpr 기반 (UHT 대신)

UHT 같은 외부 코드 생성기는 Phase A-3 (Build Pipeline) 에서 도입.
1차로는 **C++20 constexpr + 매크로** 로 컴파일 타임 리플렉션 구현.

```
WCLASS()
class WMyActor : public WActor
{
    WINTERS_GENERATED_BODY(WMyActor)

    WPROPERTY(EditAnywhere, Replicated)
    f32_t m_fHealth = 100.f;

    WPROPERTY(EditAnywhere)
    f32_t m_fMoveSpeed = 340.f;
};
```

---

## 2. 파일 구조

```
Engine/
├── Public/Object/
│   ├── WObject.h              ← 모든 엔진 오브젝트의 기반 클래스
│   ├── WClass.h               ← 클래스 메타데이터 (UClass 대응)
│   ├── WProperty.h            ← 프로퍼티 메타데이터 (FProperty 대응)
│   ├── ObjectMacros.h         ← WCLASS, WPROPERTY, WFUNCTION 매크로
│   ├── PropertyTypes.h        ← 프로퍼티 타입 enum + 타입 traits
│   ├── ObjectRegistry.h       ← 전역 클래스 레지스트리
│   ├── Serialization.h        ← WObject 직렬화/역직렬화
│   └── ObjectPtr.h            ← TWObjectPtr<T> (소프트 참조)
├── Private/Object/
│   ├── WObject.cpp
│   ├── WClass.cpp
│   ├── ObjectRegistry.cpp
│   └── Serialization.cpp
```

---

## 3. 코드 전문

### `Engine/Public/Object/PropertyTypes.h`

```cpp
#pragma once

#include "WintersTypes.h"
#include <string>
#include <typeindex>

/// 프로퍼티 타입 분류
enum class ePropertyType : u8_t
{
    Bool,
    Int32,
    UInt32,
    Int64,
    Float,
    Double,
    String,
    WString,
    Vec3,
    Vec4,
    Quat,
    Matrix,
    Color,
    ObjectPtr,     // TWObjectPtr<T>
    Enum,
    Array,         // std::vector<T>
    Map,           // std::unordered_map<K,V>
    Struct,        // 중첩 구조체
    Custom,
};

/// 프로퍼티 플래그 (UE5 EPropertyFlags 대응)
enum class ePropertyFlags : u32_t
{
    None            = 0,
    EditAnywhere    = 1 << 0,   // 에디터에서 편집 가능
    VisibleAnywhere = 1 << 1,   // 에디터에서 읽기 전용 표시
    Replicated      = 1 << 2,   // 네트워크 리플리케이션 대상
    SaveGame        = 1 << 3,   // 세이브 파일에 포함
    Transient       = 1 << 4,   // 직렬화 제외
    BlueprintReadOnly  = 1 << 5,
    BlueprintReadWrite = 1 << 6,
    Category        = 1 << 7,   // 카테고리 지정됨
    ClampMin        = 1 << 8,   // 최소값 제한
    ClampMax        = 1 << 9,   // 최대값 제한
    Slider          = 1 << 10,  // 슬라이더 UI
    ColorPicker     = 1 << 11,  // 컬러 피커 UI
};

inline ePropertyFlags operator|(ePropertyFlags a, ePropertyFlags b)
{
    return static_cast<ePropertyFlags>(static_cast<u32_t>(a) | static_cast<u32_t>(b));
}

inline bool HasFlag(ePropertyFlags flags, ePropertyFlags test)
{
    return (static_cast<u32_t>(flags) & static_cast<u32_t>(test)) != 0;
}
```

### `Engine/Public/Object/WProperty.h`

```cpp
#pragma once

#include "Object/PropertyTypes.h"
#include <string>
#include <functional>
#include <typeindex>

/// 단일 프로퍼티 메타데이터 (UE5 FProperty 대응)
struct WPropertyMeta
{
    const char*     name = nullptr;          // 변수명
    const char*     displayName = nullptr;   // 에디터 표시명
    const char*     category = nullptr;      // 카테고리 (에디터 그룹)
    const char*     tooltip = nullptr;       // 툴팁
    ePropertyType   type = ePropertyType::Custom;
    ePropertyFlags  flags = ePropertyFlags::None;
    u32_t           offset = 0;              // offsetof(Class, member)
    u32_t           size = 0;                // sizeof(member)
    std::type_index typeIndex = typeid(void);

    // 숫자 타입 범위 제한
    f64_t           clampMin = 0.0;
    f64_t           clampMax = 0.0;
    f64_t           sliderMin = 0.0;
    f64_t           sliderMax = 1.0;

    // 중첩 타입 정보 (Array<T>, Map<K,V>, ObjectPtr<T>)
    std::type_index innerTypeIndex = typeid(void);

    /// 프로퍼티 값 포인터 획득 (오브젝트 인스턴스 기준)
    void* GetValuePtr(void* obj) const
    {
        return reinterpret_cast<u8_t*>(obj) + offset;
    }

    const void* GetValuePtr(const void* obj) const
    {
        return reinterpret_cast<const u8_t*>(obj) + offset;
    }

    /// 타입별 값 접근 헬퍼
    template<typename T>
    T& GetValue(void* obj) const
    {
        return *static_cast<T*>(GetValuePtr(obj));
    }

    template<typename T>
    const T& GetValue(const void* obj) const
    {
        return *static_cast<const T*>(GetValuePtr(obj));
    }
};

/// 프로퍼티 타입 자동 추론
template<typename T> struct PropertyTypeTraits
{
    static constexpr ePropertyType Type = ePropertyType::Custom;
};

template<> struct PropertyTypeTraits<bool>
    { static constexpr ePropertyType Type = ePropertyType::Bool; };
template<> struct PropertyTypeTraits<i32_t>
    { static constexpr ePropertyType Type = ePropertyType::Int32; };
template<> struct PropertyTypeTraits<u32_t>
    { static constexpr ePropertyType Type = ePropertyType::UInt32; };
template<> struct PropertyTypeTraits<i64_t>
    { static constexpr ePropertyType Type = ePropertyType::Int64; };
template<> struct PropertyTypeTraits<f32_t>
    { static constexpr ePropertyType Type = ePropertyType::Float; };
template<> struct PropertyTypeTraits<f64_t>
    { static constexpr ePropertyType Type = ePropertyType::Double; };
template<> struct PropertyTypeTraits<std::string>
    { static constexpr ePropertyType Type = ePropertyType::String; };
template<> struct PropertyTypeTraits<std::wstring>
    { static constexpr ePropertyType Type = ePropertyType::WString; };
```

### `Engine/Public/Object/WClass.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "Object/WProperty.h"

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

class WObject;

/// UE5 UClass 대응 — 클래스의 메타데이터
class WINTERS_API WClass
{
public:
    using FactoryFn = std::function<WObject*()>;

    WClass(const char* className, const char* parentClassName, FactoryFn factory);
    ~WClass() = default;

    const char*                         GetName() const { return m_Name; }
    const char*                         GetParentName() const { return m_ParentName; }
    WClass*                             GetParentClass() const { return m_pParent; }
    const std::vector<WPropertyMeta>&   GetProperties() const { return m_Properties; }

    /// 프로퍼티 이름으로 조회
    const WPropertyMeta* FindProperty(const char* name) const;

    /// 모든 프로퍼티 (상속 포함) 수집
    std::vector<const WPropertyMeta*> GetAllProperties() const;

    /// 인스턴스 생성 (팩토리)
    WObject* CreateInstance() const;

    /// 상속 확인 (IsA)
    bool IsChildOf(const WClass* other) const;

    /// 프로퍼티 등록 (매크로에서 호출)
    void RegisterProperty(const WPropertyMeta& prop);

    /// 부모 클래스 링크 설정 (Registry 에서 호출)
    void SetParentClass(WClass* parent) { m_pParent = parent; }

private:
    const char*                m_Name = nullptr;
    const char*                m_ParentName = nullptr;
    WClass*                    m_pParent = nullptr;
    FactoryFn                  m_Factory;
    std::vector<WPropertyMeta> m_Properties;
};
```

### `Engine/Public/Object/WObject.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "Object/WClass.h"

#include <string>
#include <memory>
#include <atomic>

/// UE5 UObject 대응 — 모든 엔진 오브젝트의 기반 클래스
/// 리플렉션, 직렬화, GC 참조 추적의 루트
class WINTERS_API WObject
{
public:
    virtual ~WObject() = default;

    /// 클래스 메타데이터 조회 (각 클래스가 오버라이드)
    virtual WClass* GetClass() const = 0;

    /// 오브젝트 이름
    const std::string& GetObjectName() const { return m_ObjectName; }
    void SetObjectName(const std::string& name) { m_ObjectName = name; }

    /// 고유 ID
    u64_t GetObjectID() const { return m_ObjectID; }

    /// 타입 확인 (IsA)
    bool IsA(const WClass* cls) const;

    template<typename T>
    bool IsA() const { return IsA(T::StaticClass()); }

    /// 다운캐스트 (UE5 Cast<T> 대응)
    template<typename T>
    T* CastTo()
    {
        return IsA<T>() ? static_cast<T*>(this) : nullptr;
    }

    /// 프로퍼티 직렬화 (바이너리)
    virtual void Serialize(class CArchive& ar);

    /// 프로퍼티 직렬화 (JSON — 에디터/디버그용)
    virtual std::string ToJSON() const;
    virtual void FromJSON(const std::string& json);

    /// 네트워크 리플리케이션 대상 프로퍼티 수집
    void GetReplicatedProperties(std::vector<const WPropertyMeta*>& outProps) const;

    /// 에디터 편집 가능 프로퍼티 수집
    void GetEditableProperties(std::vector<const WPropertyMeta*>& outProps) const;

    /// Dirty flag (네트워크 리플리케이션용)
    bool IsDirty() const { return m_bDirty; }
    void MarkDirty() { m_bDirty = true; }
    void ClearDirty() { m_bDirty = false; }

protected:
    WObject();

private:
    std::string  m_ObjectName;
    u64_t        m_ObjectID = 0;
    bool         m_bDirty = false;

    static std::atomic<u64_t> s_NextObjectID;
};
```

### `Engine/Public/Object/ObjectMacros.h`

```cpp
#pragma once

#include "Object/WObject.h"
#include "Object/WClass.h"
#include "Object/WProperty.h"
#include "Object/ObjectRegistry.h"

#include <cstddef>  // offsetof

/// WCLASS() — 클래스 선언 매크로 (UCLASS 대응)
/// 클래스 앞에 배치. 현재는 marker 역할, 향후 코드 생성기가 파싱.
#define WCLASS(...)

/// WINTERS_GENERATED_BODY — 클래스 내부에 배치 (GENERATED_BODY 대응)
/// StaticClass(), GetClass() 등 필수 메서드를 자동 생성
#define WINTERS_GENERATED_BODY(ClassName)                                     \
public:                                                                       \
    static WClass* StaticClass()                                              \
    {                                                                         \
        static WClass s_Class(                                                \
            #ClassName,                                                       \
            ClassName::Super ? ClassName::Super::StaticClass()->GetName()      \
                             : nullptr,                                       \
            []() -> WObject* { return new ClassName(); }                      \
        );                                                                    \
        static bool s_bRegistered = false;                                    \
        if (!s_bRegistered)                                                   \
        {                                                                     \
            s_bRegistered = true;                                             \
            ClassName::RegisterProperties(&s_Class);                          \
            CObjectRegistry::Get().RegisterClass(&s_Class);                   \
        }                                                                     \
        return &s_Class;                                                      \
    }                                                                         \
    WClass* GetClass() const override { return StaticClass(); }               \
private:                                                                      \
    static void RegisterProperties(WClass* cls);                              \
public:

/// WPROPERTY() — 프로퍼티 선언 매크로 (UPROPERTY 대응)
/// 변수 선언 앞에 배치. RegisterProperties 에서 수동 등록.
/// 향후 코드 생성기가 자동 생성.
#define WPROPERTY(...)

/// WFUNCTION() — 함수 선언 매크로 (UFUNCTION 대응)
/// 향후 RPC / 에디터 호출 가능 함수 태깅용
#define WFUNCTION(...)

/// 프로퍼티 등록 헬퍼 매크로
/// RegisterProperties 구현 내에서 사용
#define REGISTER_PROPERTY(ClassName, MemberName, Flags)                       \
    do {                                                                      \
        WPropertyMeta prop;                                                   \
        prop.name = #MemberName;                                              \
        prop.displayName = #MemberName;                                       \
        prop.type = PropertyTypeTraits<                                       \
            std::remove_reference_t<                                          \
                decltype(std::declval<ClassName>().MemberName)                 \
            >>::Type;                                                         \
        prop.flags = Flags;                                                   \
        prop.offset = static_cast<u32_t>(                                     \
            offsetof(ClassName, MemberName));                                  \
        prop.size = static_cast<u32_t>(                                       \
            sizeof(std::declval<ClassName>().MemberName));                     \
        prop.typeIndex = typeid(                                              \
            decltype(std::declval<ClassName>().MemberName));                   \
        cls->RegisterProperty(prop);                                          \
    } while(0)

/// 범위 제한 포함 프로퍼티 등록
#define REGISTER_PROPERTY_RANGE(ClassName, MemberName, Flags, Min, Max)       \
    do {                                                                      \
        WPropertyMeta prop;                                                   \
        prop.name = #MemberName;                                              \
        prop.displayName = #MemberName;                                       \
        prop.type = PropertyTypeTraits<                                       \
            std::remove_reference_t<                                          \
                decltype(std::declval<ClassName>().MemberName)                 \
            >>::Type;                                                         \
        prop.flags = Flags | ePropertyFlags::ClampMin                         \
                          | ePropertyFlags::ClampMax;                         \
        prop.offset = static_cast<u32_t>(                                     \
            offsetof(ClassName, MemberName));                                  \
        prop.size = static_cast<u32_t>(                                       \
            sizeof(std::declval<ClassName>().MemberName));                     \
        prop.typeIndex = typeid(                                              \
            decltype(std::declval<ClassName>().MemberName));                   \
        prop.clampMin = static_cast<f64_t>(Min);                              \
        prop.clampMax = static_cast<f64_t>(Max);                              \
        cls->RegisterProperty(prop);                                          \
    } while(0)
```

### `Engine/Public/Object/ObjectRegistry.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include <string>
#include <unordered_map>
#include <vector>

class WClass;
class WObject;

/// 전역 클래스 레지스트리 (UE5 GetTransientPackage() + class registry 대응)
class WINTERS_API CObjectRegistry
{
public:
    static CObjectRegistry& Get();

    /// 클래스 등록
    void RegisterClass(WClass* cls);

    /// 이름으로 클래스 조회
    WClass* FindClass(const char* className) const;

    /// 클래스 이름으로 인스턴스 생성
    WObject* CreateObject(const char* className) const;

    /// 타입으로 인스턴스 생성
    template<typename T>
    T* CreateObject()
    {
        WObject* obj = CreateObject(T::StaticClass()->GetName());
        return obj ? static_cast<T*>(obj) : nullptr;
    }

    /// 등록된 모든 클래스 이름
    std::vector<const char*> GetAllClassNames() const;

    /// 특정 부모 클래스의 자식 클래스 목록
    std::vector<WClass*> GetDerivedClasses(const char* parentName) const;

private:
    CObjectRegistry() = default;
    std::unordered_map<std::string, WClass*> m_Classes;
};
```

### `Engine/Private/Object/WObject.cpp`

```cpp
#include "Object/WObject.h"
#include "Object/WProperty.h"

#include <sstream>

std::atomic<u64_t> WObject::s_NextObjectID{1};

WObject::WObject()
    : m_ObjectID(s_NextObjectID.fetch_add(1, std::memory_order_relaxed))
{
}

bool WObject::IsA(const WClass* cls) const
{
    if (!cls) return false;
    return GetClass()->IsChildOf(cls);
}

void WObject::GetReplicatedProperties(
    std::vector<const WPropertyMeta*>& outProps) const
{
    auto allProps = GetClass()->GetAllProperties();
    for (auto* prop : allProps)
    {
        if (HasFlag(prop->flags, ePropertyFlags::Replicated))
            outProps.push_back(prop);
    }
}

void WObject::GetEditableProperties(
    std::vector<const WPropertyMeta*>& outProps) const
{
    auto allProps = GetClass()->GetAllProperties();
    for (auto* prop : allProps)
    {
        if (HasFlag(prop->flags, ePropertyFlags::EditAnywhere) ||
            HasFlag(prop->flags, ePropertyFlags::VisibleAnywhere))
            outProps.push_back(prop);
    }
}

void WObject::Serialize(CArchive& ar)
{
    auto allProps = GetClass()->GetAllProperties();
    for (auto* prop : allProps)
    {
        if (HasFlag(prop->flags, ePropertyFlags::Transient))
            continue;
        // 타입별 직렬화는 CArchive 가 dispatch
        // ar.SerializeProperty(*prop, const_cast<void*>(
        //     prop->GetValuePtr(this)));
    }
}

std::string WObject::ToJSON() const
{
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"class\": \"" << GetClass()->GetName() << "\",\n";
    ss << "  \"id\": " << m_ObjectID << ",\n";
    ss << "  \"properties\": {\n";

    auto allProps = GetClass()->GetAllProperties();
    for (size_t i = 0; i < allProps.size(); ++i)
    {
        auto* prop = allProps[i];
        ss << "    \"" << prop->name << "\": ";

        switch (prop->type)
        {
        case ePropertyType::Float:
            ss << prop->GetValue<f32_t>(this);
            break;
        case ePropertyType::Int32:
            ss << prop->GetValue<i32_t>(this);
            break;
        case ePropertyType::Bool:
            ss << (prop->GetValue<bool>(this) ? "true" : "false");
            break;
        case ePropertyType::String:
            ss << "\"" << prop->GetValue<std::string>(this) << "\"";
            break;
        default:
            ss << "null";
            break;
        }

        if (i + 1 < allProps.size())
            ss << ",";
        ss << "\n";
    }

    ss << "  }\n}";
    return ss.str();
}

void WObject::FromJSON(const std::string& json)
{
    // JSON 파싱 → 프로퍼티 값 설정
    // nlohmann/json 또는 자체 파서 사용
}
```

### `Engine/Private/Object/WClass.cpp`

```cpp
#include "Object/WClass.h"
#include "Object/WObject.h"

WClass::WClass(const char* className, const char* parentClassName,
               FactoryFn factory)
    : m_Name(className)
    , m_ParentName(parentClassName)
    , m_Factory(std::move(factory))
{
}

const WPropertyMeta* WClass::FindProperty(const char* name) const
{
    for (auto& prop : m_Properties)
    {
        if (strcmp(prop.name, name) == 0)
            return &prop;
    }
    if (m_pParent)
        return m_pParent->FindProperty(name);
    return nullptr;
}

std::vector<const WPropertyMeta*> WClass::GetAllProperties() const
{
    std::vector<const WPropertyMeta*> result;

    // 부모 프로퍼티 먼저
    if (m_pParent)
    {
        auto parentProps = m_pParent->GetAllProperties();
        result.insert(result.end(), parentProps.begin(), parentProps.end());
    }

    // 자기 프로퍼티
    for (auto& prop : m_Properties)
        result.push_back(&prop);

    return result;
}

WObject* WClass::CreateInstance() const
{
    return m_Factory ? m_Factory() : nullptr;
}

bool WClass::IsChildOf(const WClass* other) const
{
    if (this == other) return true;
    if (m_pParent) return m_pParent->IsChildOf(other);
    return false;
}

void WClass::RegisterProperty(const WPropertyMeta& prop)
{
    m_Properties.push_back(prop);
}
```

### `Engine/Private/Object/ObjectRegistry.cpp`

```cpp
#include "Object/ObjectRegistry.h"
#include "Object/WClass.h"
#include "Object/WObject.h"

CObjectRegistry& CObjectRegistry::Get()
{
    static CObjectRegistry s_Instance;
    return s_Instance;
}

void CObjectRegistry::RegisterClass(WClass* cls)
{
    m_Classes[cls->GetName()] = cls;

    // 부모 클래스 링크
    if (cls->GetParentName())
    {
        auto* parent = FindClass(cls->GetParentName());
        if (parent)
            cls->SetParentClass(parent);
    }
}

WClass* CObjectRegistry::FindClass(const char* className) const
{
    auto it = m_Classes.find(className);
    return (it != m_Classes.end()) ? it->second : nullptr;
}

WObject* CObjectRegistry::CreateObject(const char* className) const
{
    auto* cls = FindClass(className);
    return cls ? cls->CreateInstance() : nullptr;
}

std::vector<const char*> CObjectRegistry::GetAllClassNames() const
{
    std::vector<const char*> names;
    for (auto& [name, _] : m_Classes)
        names.push_back(name.c_str());
    return names;
}

std::vector<WClass*> CObjectRegistry::GetDerivedClasses(
    const char* parentName) const
{
    auto* parent = FindClass(parentName);
    if (!parent) return {};

    std::vector<WClass*> result;
    for (auto& [_, cls] : m_Classes)
    {
        if (cls != parent && cls->IsChildOf(parent))
            result.push_back(cls);
    }
    return result;
}
```

---

## 4. 사용 예시

### 4.1 게임 클래스 정의

```cpp
// Client/Public/GameObject/Champion/ChampionCharacter.h
#pragma once

#include "Object/ObjectMacros.h"
#include "Actor/WActor.h"

WCLASS()
class AChampionCharacter : public WActor
{
    using Super = WActor;
    WINTERS_GENERATED_BODY(AChampionCharacter)

public:
    WPROPERTY(EditAnywhere, Category = "Stats")
    f32_t m_fMaxHealth = 1500.f;

    WPROPERTY(EditAnywhere, Replicated, Category = "Stats")
    f32_t m_fHealth = 1500.f;

    WPROPERTY(VisibleAnywhere, Replicated)
    f32_t m_fMoveSpeed = 340.f;

    WPROPERTY(EditAnywhere, Slider, Category = "Combat")
    f32_t m_fAttackDamage = 65.f;

    WPROPERTY(EditAnywhere, Category = "Combat")
    f32_t m_fAttackSpeed = 0.625f;
};

// RegisterProperties 구현 (향후 코드 생성기가 자동 생성)
// ChampionCharacter.cpp
void AChampionCharacter::RegisterProperties(WClass* cls)
{
    REGISTER_PROPERTY(AChampionCharacter, m_fMaxHealth,
                      ePropertyFlags::EditAnywhere);
    REGISTER_PROPERTY(AChampionCharacter, m_fHealth,
                      ePropertyFlags::EditAnywhere | ePropertyFlags::Replicated);
    REGISTER_PROPERTY(AChampionCharacter, m_fMoveSpeed,
                      ePropertyFlags::VisibleAnywhere | ePropertyFlags::Replicated);
    REGISTER_PROPERTY_RANGE(AChampionCharacter, m_fAttackDamage,
                            ePropertyFlags::EditAnywhere, 0.0, 500.0);
    REGISTER_PROPERTY_RANGE(AChampionCharacter, m_fAttackSpeed,
                            ePropertyFlags::EditAnywhere, 0.1, 5.0);
}
```

### 4.2 에디터에서 자동 프로퍼티 UI 생성

```cpp
// 에디터 코드 (09_EDITOR_FRAMEWORK 에서 상세)
void CDetailsPanel::DrawObject(WObject* obj)
{
    auto allProps = obj->GetClass()->GetAllProperties();
    for (auto* prop : allProps)
    {
        if (!HasFlag(prop->flags, ePropertyFlags::EditAnywhere) &&
            !HasFlag(prop->flags, ePropertyFlags::VisibleAnywhere))
            continue;

        bool readOnly = HasFlag(prop->flags, ePropertyFlags::VisibleAnywhere)
                     && !HasFlag(prop->flags, ePropertyFlags::EditAnywhere);

        switch (prop->type)
        {
        case ePropertyType::Float:
        {
            auto& val = prop->GetValue<f32_t>(obj);
            f32_t min = static_cast<f32_t>(prop->clampMin);
            f32_t max = static_cast<f32_t>(prop->clampMax);

            if (HasFlag(prop->flags, ePropertyFlags::Slider))
                ImGui::SliderFloat(prop->displayName, &val, min, max);
            else if (HasFlag(prop->flags, ePropertyFlags::ClampMin))
                ImGui::DragFloat(prop->displayName, &val, 0.1f, min, max);
            else
                ImGui::DragFloat(prop->displayName, &val);

            if (readOnly) ImGui::SameLine(), ImGui::TextDisabled("(read-only)");
            break;
        }
        case ePropertyType::Bool:
        {
            auto& val = prop->GetValue<bool>(obj);
            ImGui::Checkbox(prop->displayName, &val);
            break;
        }
        // ... 다른 타입들
        }
    }
}
```

---

## 5. Verification Checklist

```
[ ] WClass::RegisterProperty 로 프로퍼티 등록 → GetAllProperties 에서 조회
[ ] WObject::IsA<T>() 상속 체인 확인
[ ] CObjectRegistry::CreateObject("AChampionCharacter") → 인스턴스 생성
[ ] GetReplicatedProperties → Replicated 플래그 필터링
[ ] GetEditableProperties → EditAnywhere/VisibleAnywhere 필터링
[ ] WObject::ToJSON() → 프로퍼티 값 JSON 출력
[ ] REGISTER_PROPERTY_RANGE → clampMin/clampMax 설정
[ ] 부모 클래스 프로퍼티가 GetAllProperties 에 포함
[ ] StaticClass() 중복 호출 → 동일 WClass* 반환 (정적 변수)
```
