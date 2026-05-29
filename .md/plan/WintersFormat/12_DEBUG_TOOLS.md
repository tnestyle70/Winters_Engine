# Stage 11 — Debug Tools (ImGui 에셋 뷰어 + 해시 패널 + 대시보드)

> **목표**: Winters 포맷 레이어를 블랙박스로 두지 않음. ImGui 로 **모든 로드/변환/검증 상태** 를 실시간 조회. CLAUDE.md "빌드 1번으로 모든 값 튜닝" 철학 준수.

---

## 1. CLAUDE.md ImGui 정책 연동

"신규 시스템 작성 시 튜닝 파라미터는 ImGui 슬라이더 노출 의무" — AssetFormat 은 튜닝 파라미터는 적지만 **가시화 니즈가 큼**. 에셋 100~1000개 상태를 한 화면에.

---

## 2. AssetFormat Debugger 메인 창

```
┌─ AssetFormat Debugger ───────────────────────────────────────┐
│ Tabs: [Bundle] [Resources] [Loader Stats] [Integrity] [Tamper]│
│       [Converter] [Hash Explorer] [Memory]                    │
├──────────────────────────────────────────────────────────────┤
│ [Pause IO]  [Reload All]  [Profile: ▶ 60 fps]                │
└──────────────────────────────────────────────────────────────┘
```

키: `F10` 로 토글 (CLAUDE.md Gotcha — F12 금지, F10 재사용).

---

## 3. Bundle 탭

열린 번들 목록 + 내부 에셋 트리.

```
Open Bundles
├─ Content.winters (584 MB, 234 assets)
│   ├─ Mesh: 48         [expand]
│   ├─ Anim: 312        [expand]
│   ├─ Skel: 5          [expand]
│   ├─ Texture: 180     [expand]
│   ├─ Material: 62     [expand]
│   └─ Map: 2           [expand]
└─ Content_Dev.winters (12 MB, 43 assets, DEV)

Selected: Characters/Irelia/body.wmesh
  Size: 524 KB → 312 KB (LZ4)
  SHA256: 3f2a7e12... [Copy]
  Bundle: Content.winters
  Loaded: 3 (alive)
  [Extract to Disk]  [View Hex]  [Unload]
```

### 3.1 에셋 단일 정보 (오른쪽 Inspector)

```
[body.wmesh] — Mesh
──────────────────────────
Magic: WINT (WMSH)
Version: 1.0
Flags: LZ4
Vertex format: VF_SKINNED (76B)
Submeshes: 4
  0: Body       (vert 3240, idx 9720)
  1: Blade_L    (vert 810,  idx 2430)
  2: Blade_R    (vert 810,  idx 2430)
  3: FX_Core    (vert 120,  idx 360)
Bones: 52
AABB: min=(-1.2,-0.1,-0.8) max=(1.2,1.9,0.8)
SHA256: 3f2a7e12b4...
[Hot Reload from Disk]  [Show Bones]
```

---

## 4. Resources 탭 — 살아있는 리소스

```
Live Resources (ResourceCache)
┌ Type     ┬ Name                         ┬ Refs ┬ Size  ┬ Loaded ─┐
│ Mesh     │ Characters/Irelia/body       │  3   │ 2.4MB │ 0:02:14 │
│ Texture  │ Characters/Irelia/body_diff  │  4   │ 8.3MB │ 0:02:14 │
│ Anim     │ Irelia/q_cast                │  1   │  68KB │ 0:01:05 │
│ Mat      │ Irelia/body                  │  3   │  1.1KB│ 0:02:14 │
└──────────┴──────────────────────────────┴──────┴───────┴─────────┘
Total:  142 resources,  380 MB

[Evict Unreferenced]  [Dump to CSV]
```

---

## 5. Loader Stats 탭

```
Last Frame
  Load requests:  3
  Cache hits:     2
  Cache misses:   1
  Bundle extractions: 1
  Disk fallbacks: 0
  SHA256 checks:  1  (0.18 ms avg)

Total Session
  Loaded:   142 assets
  Evicted:   23
  Failed:     2 (see Tamper tab)

Load time histogram (last 60s)
  Mesh:     █████▒▒▒▒▒       avg 0.6ms  p99 1.2ms
  Texture:  ██▒▒▒▒▒▒▒▒       avg 2.1ms  p99 4.3ms
  Anim:     ███▒▒▒▒▒▒▒       avg 0.1ms  p99 0.3ms
```

---

## 6. Integrity 탭

Stage 8 서명 / 해시 검증 상태 대시보드.

```
Integrity Status
  Bundle: Content.winters
    Signature:   VALID (Ed25519, key v1)
    Signed at:   2026-05-01 14:23:12 UTC
    Publisher:   1 (Winters Studios)

  Hash checks (this session):
    Passed:  142
    Failed:   0
    Skipped:  0  (disabled in debug)

  Per-key verify count
    Key v1:  142 verifies
    Key v2:    0

[Verify All Loaded]  [Force Re-check]
```

### 6.1 "Verify All Loaded" 버튼 동작

모든 살아있는 에셋의 원본 바이트를 번들에서 재조회 → SHA256 재계산 → DLL 내부 공개키로 서명 재검증.
큰 비용이므로 개발/QA 용. Release 빌드에선 숨김.

---

## 7. Tamper 탭

`CTamperDetector::Report` 가 쌓이는 이벤트 리스트.

```
Tamper Events (session)
┌ Time     ┬ Reason             ┬ Asset hash         ┬ Reported ─┐
│ 14:23:01 │ SHA_MISMATCH       │ 0x3f2a7e12...      │ Yes       │
│ 14:23:45 │ SIGNATURE_FAILED   │ 0xa4b281f0...      │ Yes       │
└──────────┴────────────────────┴────────────────────┴───────────┘

Session Total: 2 events
  (5+ triggers auto-disconnect)

[Clear (dev only)]  [Export Report]
```

주의 (CLAUDE.md 보안 §5):
- Release UI 에서는 hash 전체값 대신 첫 8바이트만 표시
- "Export Report" 도 로컬 저장만, 외부 전송 금지

---

## 8. Converter 탭 (개발 빌드만)

```
WintersAssetConverter 원격 호출

Source file: [ Bin/Resource/Chars/Irelia/body.fbx  ] [Browse]
Output:       [ auto (body.wmesh)                  ]
Action:       [ mesh ▼ ]
Options:
  [x] Compress (LZ4)
  [ ] Mirror X
  [ ] Flip V
  Scale: [ 1.0 ]

[Run Converter]         Progress: ████████░░ 83%

Output:
  [OK] body.wmesh (524 KB → 312 KB, 0.42s)
  [INFO] SHA256: 3f2a...
  [Hot Reload in Engine]
```

Engine 이 `ShellExecute` 로 `WintersAssetConverter.exe` 를 호출. 완료 후 `ResourceCache` 에 hot reload.

---

## 9. Hash Explorer 탭

FNV-1a 해시 역검색 + 충돌 검사.

```
Hash Explorer

Input string: [ Characters/Irelia/body.wmesh ]
  FNV-1a:        0x3f2a7e12b4c891f0
  SHA256:        3f2a7e12...
  CRC32:         0xa4b281f0

Loaded name → hash map (234 entries)
  [search box]
  ─────────────────────────────
  0x3f2a...  Characters/Irelia/body.wmesh
  0x4b8c...  Characters/Yasuo/body.wmesh
  ...

Collision check:  0 conflicts  ✅
```

---

## 10. Memory 탭

```
AssetFormat Memory Budget

mmap view (Content.winters)   584 MB  ████████████
Decompress buffers             12 MB  ▒
Temp upload staging             8 MB  ▒
Total                         604 MB

By asset type:
  Mesh (GPU)    28 MB
  Texture (GPU) 210 MB
  Anim (RAM)    14 MB
  Other          8 MB

[Detailed Breakdown]
```

`Detailed Breakdown` 는 에셋 개별 크기 정렬 테이블 → 가장 큰 에셋 상위 20개.

---

## 11. 단축키 / 핫 리로드

```cpp
// Engine/Private/Editor/AssetDebugPanel.cpp
void CAssetDebugPanel::HandleHotkeys()
{
    if (ImGui::IsKeyPressed(ImGuiKey_F10))       Toggle();
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false) &&
        ImGui::GetIO().KeyCtrl)                  ReloadSelected();
}

void CAssetDebugPanel::ReloadSelected()
{
    if (m_selectedPath.empty()) return;

    // Disk 에서 재로드 → 새 버전으로 교체
    CGameInstance::Get()->Get_ResourceCache()->ForceReload(m_selectedPath);
}
```

---

## 12. 구현 지점

### 12.1 ECS 통합

Debug Panel 은 World/System 과 무관. 단순히 `CGameInstance::Get()->Get_ResourceCache()` / `Get_Bundle()` 조회:

```cpp
// Engine/Public/Editor/AssetDebugPanel.h
#pragma once
#include "WintersAPI.h"

namespace Winters::Editor
{
    class WINTERS_API CAssetDebugPanel
    {
    public:
        void OnImGui();        // CImGuiLayer 가 매 프레임 호출
        void Toggle();

    private:
        bool_t m_bOpen = false;
        int    m_activeTab = 0;
        std::wstring m_selectedPath;

        void DrawBundleTab();
        void DrawResourceTab();
        void DrawStatsTab();
        void DrawIntegrityTab();
        void DrawTamperTab();
        void DrawConverterTab();
        void DrawHashTab();
        void DrawMemoryTab();
    };
}
```

### 12.2 등록

```cpp
// Client/Private/CGameApp.cpp
void CGameApp::OnImGui()
{
    // 기존 SceneManager::OnImGui() ...

#ifdef _DEBUG
    static Winters::Editor::CAssetDebugPanel s_assetPanel;
    s_assetPanel.OnImGui();
#endif
}
```

---

## 13. DebugDraw — 본 / AABB 시각화

`.wmesh` 로드한 메시의 Bone / AABB / SubMesh 외곽 시각화:

```cpp
void DrawMeshDebug(const CMesh* mesh, const XMMATRIX& world)
{
    for (uint32_t i = 0; i < mesh->GetSubMeshCount(); ++i) {
        const auto& b = mesh->GetSubMeshBounds(i);
        DebugDraw::AABB(TransformAABB(b, world), RGB(0,255,0));
    }
    for (const auto& bone : mesh->GetBones()) {
        DebugDraw::Sphere(TransformPoint(bone.pos, world), 0.02f, RGB(255,255,0));
        if (bone.parent_index >= 0)
            DebugDraw::Line(bone.pos, mesh->GetBones()[bone.parent_index].pos);
    }
}
```

Inspector 에 `[Show Bones]` 버튼 → DebugDraw 토글.

---

## 14. Converter Live Preview (개발 빌드)

Scene_Editor 에서:
1. 우클릭 mesh → "Convert Source FBX"
2. 백그라운드에서 `WintersAssetConverter.exe mesh ... --quick`
3. 완료 시 자동 `ResourceCache::ForceReload` → 화면 즉시 갱신

`--quick` 옵션 쓰면 BC7 fast 모드로 1초 내 프리뷰 가능. 최종 빌드는 `--slow`.

---

## 15. 해시 충돌 경고

FNV-1a 는 64 bit 이지만 텍스처 1만개 넘으면 생일 문제로 충돌 가능성 ~1%. 번들 빌드 시:

```cpp
// Tools/WintersAssetConverter/Commands/BundleCommand.cpp
void DetectHashCollisions(const std::vector<TOCEntry>& toc)
{
    std::unordered_map<uint64_t, std::string> seen;
    for (const auto& e : toc) {
        auto it = seen.find(e.name_hash);
        if (it != seen.end()) {
            CLogger::Warn("Hash collision: %s <-> %s (0x%llx)",
                           it->second.c_str(), name, e.name_hash);
            // 선택: 자동 suffix 추가 ("body.wmesh" → "body.wmesh#1")
        }
        seen[e.name_hash] = name;
    }
}
```

Debug 탭의 `Hash Explorer` 에도 충돌 표시.

---

## 16. Profiler 연계

Phase 3-C (Profiler) 에 AssetFormat 카테고리 등록:

```cpp
// Engine/Public/Profiler/ProfileCategories.h
enum eProfileCategory : uint32_t
{
    PROFILE_AssetFormat_Load_Mesh     = 100,
    PROFILE_AssetFormat_Load_Texture  = 101,
    PROFILE_AssetFormat_Load_Anim     = 102,
    PROFILE_AssetFormat_SHA256        = 103,
    PROFILE_AssetFormat_Ed25519       = 104,
    PROFILE_AssetFormat_LZ4_Decomp    = 105,
    PROFILE_AssetFormat_BundleOpen    = 106,
};
```

Profiler 패널의 "AssetFormat" 카테고리 선택 시 해당 스코프만 표시.

---

## 17. Release 모드 동작

CLAUDE.md 보안 §5 / §§12 준수:
- Debug Panel 전체 `#ifdef _DEBUG` 래핑 → Release 빌드 자체에서 제외
- 서명 / 해시 실패 이벤트는 기록되지만 UI 표시 X
- `OutputDebugString` 등 로그 0

---

## 18. 완료 기준

- [ ] `CAssetDebugPanel` 기본 프레임
- [ ] 8 탭 전부 구현 (Bundle / Resources / Stats / Integrity / Tamper / Converter / Hash / Memory)
- [ ] Hot Reload 동작 (단일 에셋 / 디렉토리)
- [ ] DebugDraw 본/AABB
- [ ] Profiler 카테고리 등록 + 히스토그램
- [ ] 해시 충돌 감지 + Hash Explorer
- [ ] Release 빌드 UI 제거 확인
- [ ] F10 토글 + Ctrl+F5 재로드 바인딩

---

## 19. 전체 Stage 완료

Stage 1 ~ 11 전부 완료 시 Winters Format 기반 에셋 파이프라인이 **Unreal `.uasset` / Valve `.vpk` / id Tech `.bsp` 급** 의 자립성과 런타임 성능을 가짐. CLAUDE.md 의 핵심 주제 (자체 렌더링 / 자체 Physics / 자체 AI) 에 **자체 AssetFormat** 이 합류.

다음 로드맵 (INDEX §전체 로드맵 참조):
- Phase 3-B NavMesh 통합 (Stage 6 `.wmap` 선행 — ✅ 완료 시)
- Phase 5 Fiber & JobSystem (챔피언 병렬 로드)
- Phase 4 Network (번들 서명 + 해시 검증 이미 연동)
- Phase D Physics (공유 BVH)
- Phase E Graphics (공유 Monte Carlo / PBR)
- Phase F AI (공유 A*)
