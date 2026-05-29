# 03. Actor-Component Model -- UE5-Style WActor / WActorComponent / WSceneComponent

> **UE5 대응**: `AActor`, `UActorComponent`, `USceneComponent`, `UStaticMeshComponent`, `USkeletalMeshComponent`, `CreateDefaultSubobject<T>`
> **현재 Winters**: CTransform + CWorld(ECS) + ModelRenderer 수동 조합, Scene_InGame 에서 챔프마다 `ModelRenderer m_Irelia; CTransform m_IreliaTransform;` 나열
> **목표**: WActor 기반 게임 오브젝트 = 컴포넌트 트리 자동 구성, 스폰 1줄, 렌더 자동 수집, 트랜스폼 계층 자동 갱신

---

## 1. Architecture Overview

### 1.1 UE5 Actor-Component 핵심

```
AActor
  ├── USceneComponent (RootComponent, transform hierarchy)
  │   ├── USkeletalMeshComponent (auto SceneProxy registration)
  │   └── USceneComponent (socket attach point)
  ├── UActorComponent (no transform, pure logic)
  │   └── UHealthComponent, UBuffComponent, etc.
  └── CreateDefaultSubobject<T>("Name") in constructor
      → UObject factory with Outer ownership
      → auto-registered in Components array
```

### 1.2 현재 Winters 문제점

```
Scene_InGame.h (634줄 헤더, 3000줄 cpp):
  ModelRenderer   m_Irelia;   CTransform m_IreliaTransform;    // 수동 쌍
  ModelRenderer   m_Yasuo;    CTransform m_YasuoTransform;     // 7 챔프 × 2
  ModelRenderer   m_Sylas;    CTransform m_SylasTransform;
  ...
  EntityID m_IreliaEntity = NULL_ENTITY;                        // ECS 수동 매핑
  std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_ChampionRenderers;

문제:
  1. 챔프 추가 = Scene_InGame 에 멤버 2개 + OnEnter 30줄 + OnRender 1줄 추가
  2. CTransform 과 ECS TransformComponent 이중 관리 (SyncECSTransformsFromLegacy)
  3. ModelRenderer::Render(worldMat) 수동 호출 = 챔프마다 나열
  4. 컴포넌트 = POD 구조체 (HealthComponent, ChampionComponent) → 행동 없음
```

### 1.3 Winters Actor-Component 설계

```
WActor (WObject 상속)
  ├── WSceneComponent* m_pRootComponent (트랜스폼 계층 루트)
  │   ├── WSkeletalMeshComponent (ModelRenderer 래핑 → SceneProxy 자동 등록)
  │   └── WSceneComponent (소켓 부착점)
  ├── WActorComponent (틱 + 라이프사이클, 트랜스폼 없음)
  │   └── WHealthComponent, WSkillComponent, WBuffComponent
  └── CreateDefaultSubobject<T>("Name") in constructor
      → m_Components 에 자동 등록
      → WWorld::SpawnActor<T> 시 InitializeComponents 자동 호출
```

---

## 2. 파일 구조

```
Engine/
├── Public/Actor/
│   ├── WActor.h                  -- 게임 오브젝트 기반 클래스
│   ├── WActorComponent.h         -- 논리 컴포넌트 기반
│   ├── WSceneComponent.h         -- 트랜스폼 + 계층 구조
│   ├── WMeshComponent.h          -- 스태틱 메시 (ModelRenderer 래핑)
│   └── WSkeletalMeshComponent.h  -- 스켈레탈 메시 (CModel+CAnimator 래핑)
├── Private/Actor/
│   ├── WActor.cpp
│   ├── WActorComponent.cpp
│   ├── WSceneComponent.cpp
│   ├── WMeshComponent.cpp
│   └── WSkeletalMeshComponent.cpp
```

---

## 3. 코드 전문

### `Engine/Public/Actor/WActorComponent.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Object/ObjectMacros.h"

#include <string>
#include <vector>

class WActor;

/// UE5 UActorComponent 대응
/// 트랜스폼 없는 논리 컴포넌트. Tick + 라이프사이클 제공.
/// WSceneComponent 는 이 클래스를 상속하여 트랜스폼을 추가.
WCLASS()
class WINTERS_API WActorComponent : public WObject
{
    using Super = WObject;
    WINTERS_GENERATED_BODY(WActorComponent)

public:
    virtual ~WActorComponent();

    /// 라이프사이클 (WWorld 가 WActor 경유로 호출)
    virtual void InitializeComponent();
    virtual void BeginPlay();
    virtual void EndPlay();
    virtual void TickComponent(f32_t fDeltaTime);
    virtual void OnDestroyComponent();

    /// Tick 활성화 제어
    bool IsTickEnabled() const { return m_bTickEnabled; }
    void SetTickEnabled(bool bEnable) { m_bTickEnabled = bEnable; }

    /// 소유 액터
    WActor* GetOwner() const { return m_pOwner; }

    /// 컴포넌트 이름 (디버그/에디터)
    const std::string& GetComponentName() const { return m_ComponentName; }
    void SetComponentName(const std::string& name) { m_ComponentName = name; }

    /// 활성 상태
    bool IsActive() const { return m_bActive; }
    void SetActive(bool bActive);

    /// 등록 상태 (WWorld 씬에 등록되었는지)
    bool IsRegistered() const { return m_bRegistered; }

protected:
    WActorComponent();

private:
    friend class WActor;
    friend class WWorld;

    WActor*     m_pOwner = nullptr;
    std::string m_ComponentName;
    bool        m_bTickEnabled = true;
    bool        m_bActive = true;
    bool        m_bRegistered = false;
    bool        m_bBegunPlay = false;
};
```

### `Engine/Private/Actor/WActorComponent.cpp`

```cpp
#include "Actor/WActorComponent.h"

// -- WActorComponent RegisterProperties (reflection, 02_OBJECT_MODEL 연동) --
void WActorComponent::RegisterProperties(WClass* cls)
{
    // base 컴포넌트는 에디터 노출 프로퍼티 없음 (서브클래스가 등록)
}

WActorComponent::WActorComponent()
{
}

WActorComponent::~WActorComponent()
{
}

void WActorComponent::InitializeComponent()
{
    // 서브클래스 오버라이드: 초기 설정
}

void WActorComponent::BeginPlay()
{
    m_bBegunPlay = true;
}

void WActorComponent::EndPlay()
{
    m_bBegunPlay = false;
}

void WActorComponent::TickComponent(f32_t fDeltaTime)
{
    // 서브클래스 오버라이드
}

void WActorComponent::OnDestroyComponent()
{
    EndPlay();
    m_bRegistered = false;
    m_pOwner = nullptr;
}

void WActorComponent::SetActive(bool bActive)
{
    if (m_bActive == bActive) return;
    m_bActive = bActive;
    // 향후: 활성 변경 이벤트 브로드캐스트
}
```

### `Engine/Public/Actor/WSceneComponent.h`

```cpp
#pragma once

#include "Actor/WActorComponent.h"
#include "WintersMath.h"

#include <vector>

/// UE5 USceneComponent 대응
/// 트랜스폼 계층 (부모-자식) 을 가진 컴포넌트.
/// WActor::RootComponent 는 WSceneComponent.
/// WMeshComponent, WSkeletalMeshComponent 가 이를 상속.
WCLASS()
class WINTERS_API WSceneComponent : public WActorComponent
{
    using Super = WActorComponent;
    WINTERS_GENERATED_BODY(WSceneComponent)

public:
    virtual ~WSceneComponent();

    // ---- Transform (Local, relative to parent) ----

    const Vec3& GetRelativePosition() const { return m_vRelativePosition; }
    void SetRelativePosition(const Vec3& pos);

    const Vec3& GetRelativeRotation() const { return m_vRelativeRotation; }
    void SetRelativeRotation(const Vec3& rot);

    const Vec3& GetRelativeScale() const { return m_vRelativeScale; }
    void SetRelativeScale(const Vec3& scale);

    // ---- Transform (World) ----

    Vec3 GetWorldPosition() const;
    Vec3 GetWorldRotation() const;
    Vec3 GetWorldScale() const;

    void SetWorldPosition(const Vec3& pos);
    void SetWorldRotation(const Vec3& rot);

    /// 월드 매트릭스 (렌더링용). 캐시 + dirty flag.
    const Mat4& GetWorldMatrix();

    /// forward / right / up 벡터 (yaw 기반)
    Vec3 GetForwardVector() const;
    Vec3 GetRightVector() const;
    Vec3 GetUpVector() const;

    // ---- Hierarchy ----

    WSceneComponent* GetAttachParent() const { return m_pAttachParent; }
    const std::vector<WSceneComponent*>& GetAttachChildren() const { return m_AttachChildren; }

    /// 부모에 부착 (detach from current parent first)
    void AttachTo(WSceneComponent* pNewParent);

    /// 부모에서 분리
    void DetachFromParent();

    /// 자식 수
    u32_t GetChildCount() const { return static_cast<u32_t>(m_AttachChildren.size()); }

    // ---- Dirty ----

    bool IsTransformDirty() const { return m_bTransformDirty; }

protected:
    WSceneComponent();

    /// dirty 전파 (자식 전체에 재귀)
    void MarkTransformDirty();

    /// 월드 매트릭스 재계산
    void UpdateWorldMatrix();

private:
    WPROPERTY(EditAnywhere, Category = "Transform")
    Vec3 m_vRelativePosition{ 0.f, 0.f, 0.f };

    WPROPERTY(EditAnywhere, Category = "Transform")
    Vec3 m_vRelativeRotation{ 0.f, 0.f, 0.f };

    WPROPERTY(EditAnywhere, Category = "Transform")
    Vec3 m_vRelativeScale{ 1.f, 1.f, 1.f };

    Mat4 m_WorldMatrix{};
    bool m_bTransformDirty = true;

    WSceneComponent* m_pAttachParent = nullptr;
    std::vector<WSceneComponent*> m_AttachChildren;
};
```

### `Engine/Private/Actor/WSceneComponent.cpp`

```cpp
#include "Actor/WSceneComponent.h"

#include <DirectXMath.h>
using namespace DirectX;

void WSceneComponent::RegisterProperties(WClass* cls)
{
    REGISTER_PROPERTY(WSceneComponent, m_vRelativePosition,
                      ePropertyFlags::EditAnywhere);
    REGISTER_PROPERTY(WSceneComponent, m_vRelativeRotation,
                      ePropertyFlags::EditAnywhere);
    REGISTER_PROPERTY(WSceneComponent, m_vRelativeScale,
                      ePropertyFlags::EditAnywhere);
}

WSceneComponent::WSceneComponent()
{
    // identity matrix
    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&m_WorldMatrix),
                    XMMatrixIdentity());
}

WSceneComponent::~WSceneComponent()
{
    DetachFromParent();

    // 자식들의 부모 참조 해제
    for (auto* child : m_AttachChildren)
        child->m_pAttachParent = nullptr;
    m_AttachChildren.clear();
}

void WSceneComponent::SetRelativePosition(const Vec3& pos)
{
    m_vRelativePosition = pos;
    MarkTransformDirty();
}

void WSceneComponent::SetRelativeRotation(const Vec3& rot)
{
    m_vRelativeRotation = rot;
    MarkTransformDirty();
}

void WSceneComponent::SetRelativeScale(const Vec3& scale)
{
    m_vRelativeScale = scale;
    MarkTransformDirty();
}

Vec3 WSceneComponent::GetWorldPosition() const
{
    if (m_pAttachParent)
    {
        // 부모 월드 매트릭스 × 로컬 위치
        // 간이 구현: 부모 위치 + 로컬 오프셋
        Vec3 parentPos = m_pAttachParent->GetWorldPosition();
        return { parentPos.x + m_vRelativePosition.x,
                 parentPos.y + m_vRelativePosition.y,
                 parentPos.z + m_vRelativePosition.z };
    }
    return m_vRelativePosition;
}

Vec3 WSceneComponent::GetWorldRotation() const
{
    if (m_pAttachParent)
    {
        Vec3 parentRot = m_pAttachParent->GetWorldRotation();
        return { parentRot.x + m_vRelativeRotation.x,
                 parentRot.y + m_vRelativeRotation.y,
                 parentRot.z + m_vRelativeRotation.z };
    }
    return m_vRelativeRotation;
}

Vec3 WSceneComponent::GetWorldScale() const
{
    if (m_pAttachParent)
    {
        Vec3 parentScale = m_pAttachParent->GetWorldScale();
        return { parentScale.x * m_vRelativeScale.x,
                 parentScale.y * m_vRelativeScale.y,
                 parentScale.z * m_vRelativeScale.z };
    }
    return m_vRelativeScale;
}

void WSceneComponent::SetWorldPosition(const Vec3& pos)
{
    if (m_pAttachParent)
    {
        Vec3 parentPos = m_pAttachParent->GetWorldPosition();
        m_vRelativePosition = { pos.x - parentPos.x,
                                pos.y - parentPos.y,
                                pos.z - parentPos.z };
    }
    else
    {
        m_vRelativePosition = pos;
    }
    MarkTransformDirty();
}

void WSceneComponent::SetWorldRotation(const Vec3& rot)
{
    if (m_pAttachParent)
    {
        Vec3 parentRot = m_pAttachParent->GetWorldRotation();
        m_vRelativeRotation = { rot.x - parentRot.x,
                                rot.y - parentRot.y,
                                rot.z - parentRot.z };
    }
    else
    {
        m_vRelativeRotation = rot;
    }
    MarkTransformDirty();
}

const Mat4& WSceneComponent::GetWorldMatrix()
{
    if (m_bTransformDirty)
        UpdateWorldMatrix();
    return m_WorldMatrix;
}

Vec3 WSceneComponent::GetForwardVector() const
{
    f32_t yaw = GetWorldRotation().y;
    return { -sinf(yaw), 0.f, -cosf(yaw) };
}

Vec3 WSceneComponent::GetRightVector() const
{
    f32_t yaw = GetWorldRotation().y;
    return { cosf(yaw), 0.f, -sinf(yaw) };
}

Vec3 WSceneComponent::GetUpVector() const
{
    return { 0.f, 1.f, 0.f };
}

void WSceneComponent::AttachTo(WSceneComponent* pNewParent)
{
    if (m_pAttachParent == pNewParent) return;

    DetachFromParent();

    m_pAttachParent = pNewParent;
    if (pNewParent)
        pNewParent->m_AttachChildren.push_back(this);

    MarkTransformDirty();
}

void WSceneComponent::DetachFromParent()
{
    if (!m_pAttachParent) return;

    auto& siblings = m_pAttachParent->m_AttachChildren;
    siblings.erase(
        std::remove(siblings.begin(), siblings.end(), this),
        siblings.end());

    m_pAttachParent = nullptr;
    MarkTransformDirty();
}

void WSceneComponent::MarkTransformDirty()
{
    m_bTransformDirty = true;
    for (auto* child : m_AttachChildren)
        child->MarkTransformDirty();
}

void WSceneComponent::UpdateWorldMatrix()
{
    Vec3 worldPos = GetWorldPosition();
    Vec3 worldRot = GetWorldRotation();
    Vec3 worldScale = GetWorldScale();

    XMMATRIX S = XMMatrixScaling(worldScale.x, worldScale.y, worldScale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(worldRot.x, worldRot.y, worldRot.z);
    XMMATRIX T = XMMatrixTranslation(worldPos.x, worldPos.y, worldPos.z);

    XMMATRIX W = S * R * T;
    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&m_WorldMatrix), W);

    m_bTransformDirty = false;
}
```

### `Engine/Public/Actor/WActor.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Object/ObjectMacros.h"
#include "Actor/WSceneComponent.h"

#include <string>
#include <vector>
#include <memory>
#include <typeindex>

class WWorld;
class FSceneProxy;

/// UE5 AActor 대응
/// 월드에 배치되는 모든 게임 오브젝트의 기반 클래스.
/// 컴포넌트 트리를 소유하며, 스폰/파괴 라이프사이클을 관리.
/// 렌더링: WMeshComponent 가 FSceneProxy 를 자동 등록 (05_RENDERING_PIPELINE).
/// 네트워크: WPROPERTY(Replicated) 필드가 자동 리플리케이트 (06_NETWORK_REPLICATION).
WCLASS()
class WINTERS_API WActor : public WObject
{
    using Super = WObject;
    WINTERS_GENERATED_BODY(WActor)

public:
    virtual ~WActor();

    // ---- Lifecycle (WWorld 가 호출) ----

    /// 스폰 직후, 컴포넌트 InitializeComponent 후 호출
    virtual void BeginPlay();

    /// 파괴 직전 호출
    virtual void EndPlay();

    /// 프레임 틱. 기본은 모든 컴포넌트 TickComponent 호출.
    virtual void Tick(f32_t fDeltaTime);

    /// 파괴 예약 (현재 프레임 끝에 WWorld 가 제거)
    void Destroy();

    /// 파괴 예약 상태
    bool IsPendingDestroy() const { return m_bPendingDestroy; }

    // ---- Root Component ----

    WSceneComponent* GetRootComponent() const { return m_pRootComponent; }
    void SetRootComponent(WSceneComponent* pComp);

    // ---- Transform 바로가기 (RootComponent 위임) ----

    Vec3 GetActorPosition() const;
    void SetActorPosition(const Vec3& pos);
    Vec3 GetActorRotation() const;
    void SetActorRotation(const Vec3& rot);
    Vec3 GetActorScale() const;
    void SetActorScale(const Vec3& scale);
    Vec3 GetActorForward() const;
    const Mat4& GetActorWorldMatrix();

    // ---- Component Management ----

    /// UE5 CreateDefaultSubobject 대응.
    /// 생성자에서 호출하여 기본 컴포넌트 구성.
    /// T 는 WActorComponent 파생 클래스.
    template<typename T>
    T* CreateDefaultSubobject(const std::string& name)
    {
        static_assert(std::is_base_of_v<WActorComponent, T>,
                      "T must derive from WActorComponent");

        auto comp = std::make_unique<T>();
        T* pRaw = comp.get();
        pRaw->m_pOwner = this;
        pRaw->SetComponentName(name);
        m_OwnedComponents.push_back(std::move(comp));
        return pRaw;
    }

    /// 타입으로 컴포넌트 조회 (첫 번째 매칭)
    template<typename T>
    T* FindComponentByClass() const
    {
        for (auto& comp : m_OwnedComponents)
        {
            T* casted = dynamic_cast<T*>(comp.get());
            if (casted) return casted;
        }
        return nullptr;
    }

    /// 타입으로 모든 컴포넌트 수집
    template<typename T>
    void GetComponentsByClass(std::vector<T*>& outComps) const
    {
        for (auto& comp : m_OwnedComponents)
        {
            T* casted = dynamic_cast<T*>(comp.get());
            if (casted) outComps.push_back(casted);
        }
    }

    /// 이름으로 컴포넌트 조회
    WActorComponent* FindComponentByName(const std::string& name) const;

    /// 모든 컴포넌트
    const std::vector<std::unique_ptr<WActorComponent>>& GetComponents() const
    {
        return m_OwnedComponents;
    }

    // ---- World 참조 ----

    WWorld* GetWorld() const { return m_pWorld; }

    // ---- Network ----

    /// 리플리케이션 활성화 여부 (기본 false)
    WPROPERTY(EditAnywhere, Category = "Replication")
    bool m_bReplicates = false;

    /// 네트워크 ID (서버가 할당, 0 = 미할당)
    u32_t GetNetID() const { return m_NetID; }

    // ---- ECS Bridge (과도기 호환) ----

    /// 기존 ECS EntityID 와의 브리지. 마이그레이션 완료 후 제거.
    EntityID GetBridgeEntityID() const { return m_BridgeEntityID; }
    void SetBridgeEntityID(EntityID id) { m_BridgeEntityID = id; }

protected:
    WActor();

    /// 컴포넌트 초기화 (SpawnActor 에서 BeginPlay 전에 호출)
    void InitializeComponents();

private:
    friend class WWorld;

    WSceneComponent* m_pRootComponent = nullptr;  // non-owning, m_OwnedComponents 안에 있음
    std::vector<std::unique_ptr<WActorComponent>> m_OwnedComponents;

    WWorld*  m_pWorld = nullptr;
    u32_t    m_NetID = 0;
    bool     m_bPendingDestroy = false;
    bool     m_bBegunPlay = false;

    // ECS bridge (과도기)
    EntityID m_BridgeEntityID = 0;  // NULL_ENTITY = 0
};
```

### `Engine/Private/Actor/WActor.cpp`

```cpp
#include "Actor/WActor.h"
#include "Actor/WActorComponent.h"

void WActor::RegisterProperties(WClass* cls)
{
    REGISTER_PROPERTY(WActor, m_bReplicates,
                      ePropertyFlags::EditAnywhere);
}

WActor::WActor()
{
}

WActor::~WActor()
{
    // 컴포넌트는 unique_ptr 이 자동 소멸
}

void WActor::BeginPlay()
{
    m_bBegunPlay = true;
    for (auto& comp : m_OwnedComponents)
    {
        if (comp->IsActive())
            comp->BeginPlay();
    }
}

void WActor::EndPlay()
{
    for (auto& comp : m_OwnedComponents)
    {
        if (comp->m_bBegunPlay)
            comp->EndPlay();
    }
    m_bBegunPlay = false;
}

void WActor::Tick(f32_t fDeltaTime)
{
    for (auto& comp : m_OwnedComponents)
    {
        if (comp->IsActive() && comp->IsTickEnabled())
            comp->TickComponent(fDeltaTime);
    }
}

void WActor::Destroy()
{
    m_bPendingDestroy = true;
}

void WActor::SetRootComponent(WSceneComponent* pComp)
{
    m_pRootComponent = pComp;
}

Vec3 WActor::GetActorPosition() const
{
    return m_pRootComponent ? m_pRootComponent->GetWorldPosition()
                            : Vec3{ 0.f, 0.f, 0.f };
}

void WActor::SetActorPosition(const Vec3& pos)
{
    if (m_pRootComponent)
        m_pRootComponent->SetWorldPosition(pos);
}

Vec3 WActor::GetActorRotation() const
{
    return m_pRootComponent ? m_pRootComponent->GetWorldRotation()
                            : Vec3{ 0.f, 0.f, 0.f };
}

void WActor::SetActorRotation(const Vec3& rot)
{
    if (m_pRootComponent)
        m_pRootComponent->SetWorldRotation(rot);
}

Vec3 WActor::GetActorScale() const
{
    return m_pRootComponent ? m_pRootComponent->GetWorldScale()
                            : Vec3{ 1.f, 1.f, 1.f };
}

void WActor::SetActorScale(const Vec3& scale)
{
    if (m_pRootComponent)
        m_pRootComponent->SetRelativeScale(scale);
}

Vec3 WActor::GetActorForward() const
{
    return m_pRootComponent ? m_pRootComponent->GetForwardVector()
                            : Vec3{ 0.f, 0.f, -1.f };
}

const Mat4& WActor::GetActorWorldMatrix()
{
    static Mat4 identity{};
    return m_pRootComponent ? m_pRootComponent->GetWorldMatrix() : identity;
}

WActorComponent* WActor::FindComponentByName(const std::string& name) const
{
    for (auto& comp : m_OwnedComponents)
    {
        if (comp->GetComponentName() == name)
            return comp.get();
    }
    return nullptr;
}

void WActor::InitializeComponents()
{
    for (auto& comp : m_OwnedComponents)
    {
        comp->m_bRegistered = true;
        comp->InitializeComponent();
    }
}
```

### `Engine/Public/Actor/WMeshComponent.h`

```cpp
#pragma once

#include "Actor/WSceneComponent.h"

#include <string>
#include <memory>

class ModelRenderer;

/// UE5 UStaticMeshComponent 대응
/// ModelRenderer (스태틱 메시) 를 래핑하여 자동 렌더 수집 지원.
/// SceneProxy 를 통해 RenderGraph 에 MeshDrawCommand 자동 등록.
WCLASS()
class WINTERS_API WMeshComponent : public WSceneComponent
{
    using Super = WSceneComponent;
    WINTERS_GENERATED_BODY(WMeshComponent)

public:
    virtual ~WMeshComponent();

    /// 메시 에셋 로드 (wmesh / fbx 경로)
    bool LoadMesh(const std::string& strMeshPath);

    /// 셰이더 경로 설정 (기본: Shaders/Mesh3D.hlsl)
    void SetShaderPath(const std::wstring& strPath) { m_ShaderPath = strPath; }

    /// 텍스처 설정
    bool LoadTexture(u32_t iMeshIndex, const std::wstring& strPath);
    void LoadTextureForAll(const std::wstring& strPath);

    /// 내부 ModelRenderer 접근 (과도기 호환)
    ModelRenderer* GetModelRenderer() const { return m_pRenderer.get(); }

    /// 렌더링 가시성
    bool IsVisible() const { return m_bVisible; }
    void SetVisible(bool bVisible) { m_bVisible = bVisible; }

    /// TickComponent 오버라이드: 월드 매트릭스를 ModelRenderer 에 전달
    void TickComponent(f32_t fDeltaTime) override;

    /// 렌더 호출 (05_RENDERING_PIPELINE 에서 SceneProxy 로 교체 예정)
    void Render();

protected:
    WMeshComponent();

private:
    std::unique_ptr<ModelRenderer> m_pRenderer;
    std::wstring m_ShaderPath = L"Shaders/Mesh3D.hlsl";

    WPROPERTY(VisibleAnywhere, Category = "Rendering")
    bool m_bVisible = true;
};
```

### `Engine/Private/Actor/WMeshComponent.cpp`

```cpp
#include "Actor/WMeshComponent.h"
#include "Renderer/ModelRenderer.h"

void WMeshComponent::RegisterProperties(WClass* cls)
{
    REGISTER_PROPERTY(WMeshComponent, m_bVisible,
                      ePropertyFlags::VisibleAnywhere);
}

WMeshComponent::WMeshComponent()
{
}

WMeshComponent::~WMeshComponent()
{
}

bool WMeshComponent::LoadMesh(const std::string& strMeshPath)
{
    m_pRenderer = std::make_unique<ModelRenderer>();
    return m_pRenderer->Init(strMeshPath, m_ShaderPath.c_str());
}

bool WMeshComponent::LoadTexture(u32_t iMeshIndex, const std::wstring& strPath)
{
    if (!m_pRenderer) return false;
    return m_pRenderer->LoadMeshTexture(iMeshIndex, strPath);
}

void WMeshComponent::LoadTextureForAll(const std::wstring& strPath)
{
    if (m_pRenderer)
        m_pRenderer->LoadTextureForAllMeshes(strPath);
}

void WMeshComponent::TickComponent(f32_t fDeltaTime)
{
    if (!m_pRenderer || !m_bVisible) return;

    // 매 프레임 월드 매트릭스 동기화
    m_pRenderer->UpdateTransform(GetWorldMatrix());
}

void WMeshComponent::Render()
{
    if (!m_pRenderer || !m_bVisible) return;
    m_pRenderer->Render();
}
```

### `Engine/Public/Actor/WSkeletalMeshComponent.h`

```cpp
#pragma once

#include "Actor/WMeshComponent.h"

#include <string>

namespace Engine { class CAnimator; }

/// UE5 USkeletalMeshComponent 대응
/// 스켈레탈 메시 + 애니메이션 지원.
/// ModelRenderer 의 스켈레톤/애니메이션 기능을 래핑.
WCLASS()
class WINTERS_API WSkeletalMeshComponent : public WMeshComponent
{
    using Super = WMeshComponent;
    WINTERS_GENERATED_BODY(WSkeletalMeshComponent)

public:
    virtual ~WSkeletalMeshComponent();

    /// 애니메이션 재생 (이름 키워드 매칭)
    void PlayAnimation(const std::string& strKeyword, bool bLoop = false);

    /// 애니메이션 재생 (인덱스)
    void PlayAnimationByIndex(u32_t iIndex);

    /// 스켈레톤 보유 여부
    bool HasSkeleton() const;

    /// 애니메이션 수
    u32_t GetAnimationCount() const;

    /// Animator 접근 (프레임 이벤트 감지 등)
    const Engine::CAnimator* GetAnimator() const;
    Engine::CAnimator* GetAnimator();

    /// TickComponent 오버라이드: 애니메이션 업데이트
    void TickComponent(f32_t fDeltaTime) override;

    /// 애니메이션 재생 속도 배율
    WPROPERTY(EditAnywhere, Category = "Animation")
    f32_t m_fAnimPlayRate = 1.f;

protected:
    WSkeletalMeshComponent();
};
```

### `Engine/Private/Actor/WSkeletalMeshComponent.cpp`

```cpp
#include "Actor/WSkeletalMeshComponent.h"
#include "Renderer/ModelRenderer.h"

void WSkeletalMeshComponent::RegisterProperties(WClass* cls)
{
    REGISTER_PROPERTY_RANGE(WSkeletalMeshComponent, m_fAnimPlayRate,
                            ePropertyFlags::EditAnywhere, 0.01, 10.0);
}

WSkeletalMeshComponent::WSkeletalMeshComponent()
{
}

WSkeletalMeshComponent::~WSkeletalMeshComponent()
{
}

void WSkeletalMeshComponent::PlayAnimation(const std::string& strKeyword, bool bLoop)
{
    auto* pRenderer = GetModelRenderer();
    if (pRenderer)
        pRenderer->PlayAnimationByName(strKeyword, bLoop);
}

void WSkeletalMeshComponent::PlayAnimationByIndex(u32_t iIndex)
{
    auto* pRenderer = GetModelRenderer();
    if (pRenderer)
        pRenderer->PlayAnimation(iIndex);
}

bool WSkeletalMeshComponent::HasSkeleton() const
{
    auto* pRenderer = GetModelRenderer();
    return pRenderer ? pRenderer->HasSkeleton() : false;
}

u32_t WSkeletalMeshComponent::GetAnimationCount() const
{
    auto* pRenderer = GetModelRenderer();
    return pRenderer ? pRenderer->GetAnimationCount() : 0;
}

const Engine::CAnimator* WSkeletalMeshComponent::GetAnimator() const
{
    auto* pRenderer = GetModelRenderer();
    return pRenderer ? pRenderer->GetAnimator() : nullptr;
}

Engine::CAnimator* WSkeletalMeshComponent::GetAnimator()
{
    auto* pRenderer = GetModelRenderer();
    return pRenderer ? pRenderer->GetAnimator() : nullptr;
}

void WSkeletalMeshComponent::TickComponent(f32_t fDeltaTime)
{
    // 부모: 월드 매트릭스 동기화
    WMeshComponent::TickComponent(fDeltaTime);

    // 애니메이션 업데이트
    auto* pRenderer = GetModelRenderer();
    if (pRenderer && HasSkeleton())
        pRenderer->Update(fDeltaTime * m_fAnimPlayRate);
}
```

---

## 4. 사용 예시

### 4.1 Before (현재 Scene_InGame)

```cpp
// Scene_InGame.h — 멤버 선언
ModelRenderer   m_Irelia;   CTransform m_IreliaTransform;
EntityID m_IreliaEntity = NULL_ENTITY;

// Scene_InGame.cpp — OnEnter (30줄 per champion)
bool CScene_InGame::OnEnter()
{
    m_Irelia.Init("Irelia/irelia.wmesh");
    m_Irelia.LoadMeshTexture(0, L"Irelia/irelia_base.png");
    m_IreliaTransform.SetPosition({0, 0, 0});
    m_IreliaTransform.SetScale({0.01f, 0.01f, 0.01f});
    m_IreliaEntity = m_World.CreateEntity();
    m_World.AddComponent<TransformComponent>(m_IreliaEntity, {{0,0,0},{0,0,0},{0.01f,0.01f,0.01f}});
    m_World.AddComponent<HealthComponent>(m_IreliaEntity, {1500.f, 1500.f});
    m_World.AddComponent<ChampionComponent>(m_IreliaEntity, {eChampionId::Irelia, eTeam::Blue});
    // ...repeat for 7 champions
    return true;
}

// OnRender — 수동 나열
void CScene_InGame::OnRender()
{
    m_Irelia.Render();
    m_Yasuo.Render();
    m_Sylas.Render();
    // ...
}

// 매 프레임 동기화 (ECS <-> Legacy)
void CScene_InGame::SyncECSTransformsFromLegacy()
{
    auto& tc = m_World.GetComponent<TransformComponent>(m_IreliaEntity);
    tc.position = m_IreliaTransform.GetPosition();
    // ...repeat for 7 champions
}
```

### 4.2 After (WActor-Component 모델)

```cpp
// Client/Public/GameObject/Champion/WChampionActor.h
#pragma once
#include "Actor/WActor.h"
#include "Actor/WSkeletalMeshComponent.h"

WCLASS()
class WChampionActor : public WActor
{
    using Super = WActor;
    WINTERS_GENERATED_BODY(WChampionActor)

public:
    void BeginPlay() override;
    void Tick(f32_t fDeltaTime) override;

    // 챔피언 초기화 (메시/텍스처/스탯 로드)
    void InitChampion(eChampion eId, eTeam eTeam);

    WSkeletalMeshComponent* GetMeshComp() const { return m_pMeshComp; }

private:
    WChampionActor();

    WSkeletalMeshComponent* m_pMeshComp = nullptr;
    // 향후: WHealthComponent*, WSkillComponent*, WBuffComponent*

    WPROPERTY(EditAnywhere, Replicated)
    f32_t m_fHealth = 1500.f;

    WPROPERTY(EditAnywhere, Replicated)
    f32_t m_fMoveSpeed = 340.f;

    WPROPERTY(VisibleAnywhere)
    eChampion m_eChampionId = eChampion::Irelia;

    WPROPERTY(VisibleAnywhere)
    eTeam m_eTeam = eTeam::Blue;
};

// WChampionActor.cpp
WChampionActor::WChampionActor()
{
    m_pMeshComp = CreateDefaultSubobject<WSkeletalMeshComponent>("ChampionMesh");
    SetRootComponent(m_pMeshComp);
    m_bReplicates = true;
}

void WChampionActor::InitChampion(eChampion eId, eTeam eTeam)
{
    m_eChampionId = eId;
    m_eTeam = eTeam;

    // ChampionTable 에서 메시 경로/텍스처 경로/스탯 로드
    // auto& def = ChampionTable::Get(eId);
    // m_pMeshComp->LoadMesh(def.meshPath);
    // m_pMeshComp->LoadTexture(0, def.texturePath);
    // m_pMeshComp->SetRelativeScale({0.01f, 0.01f, 0.01f});
    // m_fHealth = def.baseHealth;
    // m_fMoveSpeed = def.baseMoveSpeed;
}

void WChampionActor::BeginPlay()
{
    WActor::BeginPlay();
    m_pMeshComp->PlayAnimation("idle", true);
}

void WChampionActor::Tick(f32_t fDeltaTime)
{
    WActor::Tick(fDeltaTime);  // 컴포넌트 Tick 자동 호출
    // 챔피언 전용 로직 (이동, 스킬, 전투)
}
```

```cpp
// Scene 코드 (04_WORLD_SUBSYSTEM 의 WWorld 사용)
// OnEnter: 1줄 per champion
auto* irelia = pWorld->SpawnActor<WChampionActor>({0, 0, 0});
irelia->InitChampion(eChampion::Irelia, eTeam::Blue);

// OnRender: 자동 (WWorld 가 모든 WMeshComponent 순회 → Render)
// OnUpdate: 자동 (WWorld 가 모든 WActor::Tick 호출)
// ECS 동기화: 불필요 (WActor 가 트랜스폼 단일 소유)
```

---

## 5. CTransform -> WSceneComponent 마이그레이션 전략

### 5.1 단계별 전환

```
Phase 1: WActor/WSceneComponent 병존
  - 기존 CTransform + ModelRenderer 유지
  - 신규 챔프만 WChampionActor 사용
  - ECS bridge: WActor::m_BridgeEntityID 로 기존 시스템과 연결

Phase 2: 기존 챔프 순차 전환 (챔프당 30분)
  - m_Irelia + m_IreliaTransform + m_IreliaEntity
    -> auto* irelia = SpawnActor<WChampionActor>({...});
  - SyncECSTransformsFromLegacy() 에서 해당 챔프 제거

Phase 3: Scene_InGame 정리
  - 모든 ModelRenderer / CTransform 멤버 제거
  - Scene_InGame 은 WWorld + Subsystem 으로 대체 (04_WORLD_SUBSYSTEM)
  - ECS CWorld 는 서버 전용 또는 물리 전용으로 보존
```

### 5.2 어댑터 패턴 (CTransform <-> WSceneComponent)

```cpp
/// 기존 코드가 CTransform* 를 기대할 때 shim
class WTransformAdapter
{
public:
    static void SyncFromLegacy(WSceneComponent* pComp, const CTransform& legacy)
    {
        pComp->SetRelativePosition(legacy.GetPosition());
        pComp->SetRelativeRotation(legacy.GetRotation());
        pComp->SetRelativeScale(legacy.GetScale());
    }

    static void SyncToLegacy(const WSceneComponent* pComp, CTransform& legacy)
    {
        legacy.SetPosition(pComp->GetWorldPosition());
        legacy.SetRotation(pComp->GetWorldRotation());
        legacy.SetScale(pComp->GetWorldScale());
    }
};
```

---

## 6. Verification Checklist

```
[ ] WActor 생성자에서 CreateDefaultSubobject<WSceneComponent>("Root") 호출 성공
[ ] WSceneComponent 부모-자식 계층: AttachTo / DetachFromParent
[ ] WSceneComponent::SetRelativePosition -> MarkTransformDirty -> 자식 전파
[ ] WSceneComponent::GetWorldMatrix() 캐시 + dirty 재계산
[ ] WActor::Tick -> 모든 컴포넌트 TickComponent 호출
[ ] WActor::BeginPlay -> 모든 컴포넌트 BeginPlay 호출
[ ] WMeshComponent::LoadMesh -> ModelRenderer::Init 래핑 성공
[ ] WMeshComponent::TickComponent -> ModelRenderer::UpdateTransform 자동 동기화
[ ] WSkeletalMeshComponent::PlayAnimation -> ModelRenderer::PlayAnimationByName
[ ] WSkeletalMeshComponent::TickComponent -> ModelRenderer::Update (애니메이션 갱신)
[ ] WChampionActor 스폰 + InitChampion -> 메시 로드 + idle 재생
[ ] WActor::FindComponentByClass<WSkeletalMeshComponent>() 타입 조회
[ ] WActor::Destroy() -> m_bPendingDestroy = true, WWorld 가 프레임 끝에 제거
[ ] ECS bridge: SetBridgeEntityID -> 기존 시스템과 연결 (과도기)
[ ] LoL 빌드 통과 (기존 코드 무변경, 신규 WActor 추가만)
```
