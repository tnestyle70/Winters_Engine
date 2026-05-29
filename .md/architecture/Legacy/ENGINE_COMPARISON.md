# 경쟁 게임 엔진 모듈 시스템 비교 — Winters 청사진을 위한 input

> **작성일**: 2026-05-03
> **목적**: UE5 외 5 엔진 (Unity / id Tech / Source 2 / O3DE / Bevy) 의 모듈 시스템 패턴 추출. Winters Blueprint v1 의 의사결정 input.
> **결론**: 7 공통 패턴 → Winters 가 채택할 5개 명시.

---

## §1. Unity (DOTS / 2023+)

### 모듈 시스템

```
Packages/com.unity.X/
├── package.json              ← {"dependencies": {...}}
├── *.asmdef                  ← Assembly Definition (컴파일 단위)
└── Documentation~/           ← npm 패턴
```

- **Package Manager** = visual dependency graph
- **Assembly Definitions (.asmdef)** = 컴파일 격리 + cycle 차단 (IDE 강제)
- **DOTS (Data-Oriented Tech Stack)** = ECS — System / Component 단위 분할

### 핵심 패턴

| 패턴 | 의미 |
|---|---|
| Package = 1 폴더 = 1 manifest (package.json) | 모듈 단위 명확 |
| asmdef references = compile-time 의존성 | cycle 자동 차단 |
| `internal` 키워드 + asmdef 의 `noEngineReferences` | API 노출 강제 |
| `Documentation~/` (~ prefix) — Unity 가 무시하는 폴더 | 문서 분리 표준 |
| Custom Package Registry | 사내 모듈 배포 |

→ **Winters 채택**: package.json 같은 `_MODULE.md` + asmdef 같은 의존성 명시.

---

## §2. id Tech 7 (Doom Eternal / 2020)

### 특징 (id Software 공개 정보 기반)

- **C++ 모놀리식** — 단일 솔루션, 모듈 격리 약함
- **Megatexture** — 텍스처 streaming engine 일체화
- **Vulkan-only** (PC) — RHI 추상 최소
- **idStudio** — 자체 에디터 (Unreal Editor 같은)
- **Decl 시스템** — 텍스트 기반 데이터 정의 (TweakDB 와 비슷)

### 핵심 패턴

| 패턴 | 의미 |
|---|---|
| Decl (.decl) 파일 = data-as-compass | TweakDB 와 같은 schema-as-compass |
| 단일 .sln → 모듈 격리는 코드 컨벤션으로 | Winters 가 피해야 할 패턴 (1000 파일 도달 시 막힘) |
| 60 FPS 강제 — 성능 최적화 우선 | Performance budget 박제 의무 |

→ **Winters 차용 X** (모놀리식 X), **단 Decl 패턴 차용** (게임 데이터를 텍스트 박제 = SkillTable.cpp 와 비슷).

---

## §3. Source 2 (DOTA 2 / CS2 / 2015~)

### 특징 (Valve 의 GDC talk 기반)

- **Hammer 5 에디터** = 게임 도구 분리 (실시간 수정 + Hot reload)
- **Tools API** — 에디터/게임 IPC 통신
- **Panorama UI** — JS-like UI 시스템 (자체)
- **EconLib** — Steam 경제 시스템 모듈화

### 핵심 패턴

| 패턴 | 의미 |
|---|---|
| 도구 ↔ 게임 IPC 분리 | 에디터 충돌 시 게임 안 죽음 |
| Hot Reload (셰이더 / asset / script) | 빌드 1회로 튜닝 (Winters 의 ImGui 정책과 동일) |
| Panorama (HTML/CSS like UI) | 외부 표준 차용 |

→ **Winters 채택**: Hot Reload (이미 부분 적용, ImGui 슬라이더 패턴과 통합 강화).

---

## §4. O3DE (Open 3D Engine — Amazon Lumberyard 후신 / 2021~)

### 모듈 시스템 — Gem

```
Gems/
├── Atom/                     ← AAA 렌더러 Gem
│   ├── gem.json              ← manifest
│   ├── Code/
│   └── Assets/
├── Audio/
└── ScriptCanvas/
```

- **Gem = Plugin per feature** — Atom (렌더러) / Audio / Animation / 모두 Gem
- **gem.json** = dependencies + LoadingPhase + Platform
- **Engine Core 자체가 Gem 의 합** — 극단적 모듈화

### 핵심 패턴

| 패턴 | 의미 |
|---|---|
| 모든 기능이 Gem (UE5 보다 더 극단) | Plugin-first architecture |
| Engine Core = 최소 + Gem 합산 | 사용자가 게임별 사이즈 조절 |
| Open Source (Apache 2) | Winters 도 일부 공개 가능 |

→ **Winters 차용**: "기능 = Plugin" 의 극단까지는 가지 않되, **Phase 4 (Beyond 100) 에서 Gem-like 시스템 검토**.

---

## §5. Bevy (Rust ECS / 2020~)

### 특징

- **Pure ECS** — Entity-Component-System 가장 순수한 구현
- **Plugin trait** = `impl Plugin for X` — 모든 기능이 Plugin
- **Crate 단위** = Rust 의 모듈 시스템 + Cargo.toml dependency

```rust
App::new()
    .add_plugins(DefaultPlugins)        // 기본 plugin set
    .add_plugins(MyGamePlugin)          // 게임 plugin
    .run();
```

### 핵심 패턴

| 패턴 | 의미 |
|---|---|
| `add_plugins(X)` — 명시적 활성화 | Winters 의 ECS Schedule 과 비슷 |
| Cargo.toml = package + dependency | npm/asmdef 패턴 |
| Compile-time 검증 (Rust 타입 시스템) | Winters 는 C++ — 부분적 (concepts) |

→ **Winters 차용**: Plugin trait 같은 명시적 활성화 패턴.

---

## §6. Godot 4 (오픈소스 / 2023~)

### 특징

- **GDExtension** = 외부 Plugin (C++/Rust/Python 자유)
- **Scene = .tscn 파일** = 직렬화된 트리 (Winters Scene 과 다름)
- **Modulus 시스템** — 핵심 모듈도 빌드 시 활성/비활성 가능

### 핵심 패턴

| 패턴 | 의미 |
|---|---|
| Build-time module 활성화 (SCons) | 게임별 binary 사이즈 최적화 |
| GDExtension manifest (.gdextension) | UE5 의 .uplugin 패턴 |

→ **Winters 채택**: Build-time module flag — Phase 4 의 Beyond 100 (CMake 옵션 또는 vcxproj configuration).

---

## §7. 7 공통 패턴 + Winters 채택 결정

### 7-1. 패턴 매트릭스

| 패턴 | UE5 | Unity | Source2 | O3DE | Bevy | Godot | Winters 채택 |
|---|---|---|---|---|---|---|---|
| **Per-module manifest** | ✓ Build.cs | ✓ asmdef | (소스 비공개) | ✓ gem.json | ✓ Cargo.toml | ✓ .gdextension | ✓ **`_MODULE.md`** |
| **Public/Private 경계** | ✓ 폴더 | ✓ internal | ? | ✓ | ✓ pub() | ✓ | ✓ 이미 있음 |
| **Plugin 격리** | ✓ uplugin | ✓ Package | ✓ IPC | ✓ Gem | ✓ trait | ✓ GDExtension | ✓ **Phase 4 신규** |
| **Dependency 명시** | ✓ Build.cs | ✓ asmdef ref | ? | ✓ | ✓ | ✓ | ✓ **`_MODULE.md` 3 분류** |
| **AI 친화 entry point** | (Confluence) | Documentation~/ | ? | (부분) | rustdoc | (부분) | ✓ **`_MODULE.md` 의 Common Tasks** |
| **Hot Reload** | (부분) | (부분) | ✓ Hammer | ✓ | ✗ | (부분) | ✓ **ImGui + Compile-time 결합 강화** |
| **Build-time module flag** | (부분) | ✓ Define | (부분) | ✓ | ✓ feature | ✓ SCons | ✓ **Phase 4 (vcxproj configuration)** |

### 7-2. Winters 의 5 핵심 채택

1. **`_MODULE.md`** = UE5 Build.cs + Unity asmdef + O3DE gem.json 의 markdown 통합본
2. **의존성 3 분류** (Public / Private / Forward-Decl) = UE5 Build.cs 패턴
3. **Plugin 시스템 (Phase 4)** = `_PLUGIN.md` + LoadingPhase + EnabledByDefault — UE5 uplugin + O3DE gem.json 차용
4. **Hot Reload 강화** = ImGui 정책 + 셰이더 hot reload + Lua script reload (Source 2 패턴)
5. **Build-time module flag** = vcxproj configuration 별 module 활성/비활성 — Phase 4 (Beyond 100)

### 7-3. Winters 가 채택 X 한 패턴

- **단일 .sln 모놀리식 (id Tech)** — 1000 파일 도달 시 막힘
- **Pure ECS (Bevy 순수성)** — Winters 는 ECS + 기존 패턴 mix (ChampionDef 같은 OOP 잔존)
- **자체 UI 표준 (Panorama / UMG)** — ImGui 가 충분 (에디터+게임 통합 목표)

---

## §8. 한 줄 요약

**UE5 의 Build.cs (per-module + 3 분류 의존성) + Unity asmdef (compile-time 강제) + O3DE Gem (Plugin per feature) + Source 2 Hot Reload + Godot Build-time flag — 5 패턴을 Winters `_MODULE.md` + Phase 4 Plugin 시스템 + ImGui 기반 Hot Reload 로 통합. id Tech 모놀리식 + Bevy 순수 ECS + Panorama 자체 UI 는 채택 X. 결과: 100 모듈 도달 시 UE5 와 동급 확장성 + Unity 보다 명확한 의존성 박제 + O3DE 보다 단순한 Plugin 시스템.**
