# Stage 9 — `WintersAssetConverter.exe` 통합 CLI

> **목표**: Stage 2~8 의 모든 쓰기 파이프라인을 **단일 실행파일** 로 통합. 빌드 시스템에서 한 명령으로 FBX→`.wmesh`, PNG→`.wtex`, .dat→`.wmap`, 디렉토리→`.winters` 전체 처리.

> **기존 상태** (CLAUDE.md 언급): `Tools/WintersAssetConverter/` 스켈레톤만 존재. 본 Stage 에서 완성.

---

## 1. 왜 단일 CLI

- 빌드 스크립트 단순화 (한 exe, 여러 subcommand)
- 공통 옵션 (signing key, log level) 한 곳에서 관리
- CI/CD 파이프라인 병렬 처리 용이 (`for` 루프 + `start /b`)
- 에러 리포팅 통일

---

## 2. 디렉토리 구조

```
Tools/WintersAssetConverter/
├── main.cpp                   ← CLI 엔트리 (subcommand dispatch)
├── CLI/
│   ├── ArgParser.h            ← argv → 구조체
│   ├── Logger.h               ← 색상 + 진행률 출력
│   └── ReturnCodes.h          ← exit code 표준화
├── Commands/
│   ├── MeshCommand.cpp        ← mesh subcommand (Stage 2)
│   ├── AnimCommand.cpp        ← anim + skel (Stage 3)
│   ├── TexCommand.cpp         ← tex (Stage 4)
│   ├── MatCommand.cpp         ← mat (Stage 5)
│   ├── MapCommand.cpp         ← map (Stage 6)
│   ├── BundleCommand.cpp      ← bundle + sign (Stage 7/8)
│   ├── VerifyCommand.cpp      ← hash/signature 검증
│   ├── InfoCommand.cpp        ← 헤더 덤프
│   └── BatchCommand.cpp       ← 디렉토리 전체 자동 변환
├── Pipeline/
│   ├── AssimpImporter.cpp     ← FBX/glb 공통 로딩
│   ├── DirectXTexWrapper.cpp
│   ├── Nlohmann_Json.hpp
│   └── SigningKeyLoader.cpp
├── WintersAssetConverter.vcxproj
└── WintersAssetConverter.vcxproj.filters
```

Engine 과 **독립 프로젝트** — Debug/Release 빌드 모두 가능. Tools 는 `.exe` 단독 (Engine.dll 의존 X, 빌드 서버 배치 편의).

---

## 3. 명령어 체계

### 3.1 개요

```
WintersAssetConverter.exe <command> [options] <input> [-o output]

Commands:
  mesh       FBX/glb → .wmesh
  anim       FBX → .wanim (per-animation)
  skel       FBX → .wskel
  tex        PNG/TGA/DDS/HDR → .wtex
  mat        JSON → .wmat
  map        .dat → .wmap (migration)
  bundle     Directory → .winters
  sign       Sign existing .winters or .w* asset
  verify     Verify SHA256 and/or Ed25519
  info       Dump header info for any .w* or .winters
  batch      Run full pipeline on a directory (auto-detect)
  help       Show help for a specific command
```

### 3.2 글로벌 옵션

```
--log-level <quiet|error|warn|info|debug>   (default: info)
--color / --no-color                        (ANSI color output)
--threads <N>                               (parallel conversion; default: CPU cores)
--config <path>                             (load .conf settings.json)
--output-dir <path>                         (base output dir override)
```

---

## 4. 서브커맨드 상세

### 4.1 `mesh`

```
WintersAssetConverter.exe mesh <input.fbx> [-o <output.wmesh>] [options]

Options:
  --compress            LZ4 payload compression (default: on)
  --no-compress         Disable LZ4
  --flip-v              Flip V (glb → DX)
  --mirror-x            X-axis mirror (lol2gltf workaround)
  --scale <f>           Uniform rebake scale (default: 1.0)
  --generate-mat-stub   Create .mat.json stub per submesh
  --strip-anim          Remove animation channels (mesh-only)
```

예:
```
:: Single champion
WintersAssetConverter.exe mesh \
    "Bin\Resource\Characters\Irelia\body.fbx" \
    -o "Bin\Resource\Characters\Irelia\body.wmesh" \
    --compress --generate-mat-stub

:: Map (lol2gltf X-flip workaround)
WintersAssetConverter.exe mesh \
    "Bin\Resource\Map\SummonersRift.glb" \
    -o "Bin\Resource\Map\SummonersRift.wmesh" \
    --mirror-x --scale 0.01
```

### 4.2 `anim` / `skel`

```
WintersAssetConverter.exe skel <input.fbx> -o <skeleton.wskel>

WintersAssetConverter.exe anim <input.fbx> \
    --skeleton <skeleton.wskel> \
    --out-dir <anims/> \
    [--split]                     Split multi-clip FBX into per-clip .wanim
    [--events <events.json>]      Attach animation events
    [--loop-pattern "idle,run"]   Names matching → is_loop=1
```

예:
```
:: Skeleton (1 file)
WintersAssetConverter.exe skel "Irelia/rig.fbx" -o "Irelia/skeleton.wskel"

:: All animations split into per-clip .wanim
WintersAssetConverter.exe anim "Irelia/anims.fbx" \
    --skeleton "Irelia/skeleton.wskel" \
    --out-dir "Irelia/anims/" \
    --split \
    --loop-pattern "idle,run,walk"
```

### 4.3 `tex`

```
WintersAssetConverter.exe tex <input.png> [-o <output.wtex>] [options]

Options:
  --format <BC1|BC3|BC4|BC5|BC6H|BC7|RGBA8|RGBA16F>   (default: BC7)
  --srgb / --no-srgb     (default: BC7 on, others off)
  --mips                  Generate full mip chain
  --max-mip <N>           Stop at mip level N
  --quick / --slow        BC7 quality preset (default: default)
  --preserve-alpha        Use BC7 mode 6/7
  --output-cube           Treat 6 input files as cubemap faces
```

예:
```
:: Diffuse (BC7 SRGB)
WintersAssetConverter.exe tex body_diffuse.png --format BC7 --srgb --mips --slow

:: Normal (BC5)
WintersAssetConverter.exe tex body_normal.png --format BC5 --no-srgb --mips

:: HDR Skybox (BC6H)
WintersAssetConverter.exe tex skybox_hdr.exr --format BC6H --no-srgb --mips
```

### 4.4 `mat`

```
WintersAssetConverter.exe mat <input.mat.json> [-o <output.wmat>]

Options:
  --validate           Verify shader + texture references exist
```

### 4.5 `map`

```
WintersAssetConverter.exe map <input.dat> [-o <output.wmap>] \
    [--nav-grid <navgrid.bin>]                Separate NavGrid source
    [--nav-cells <navcells.json>]             NavMesh triangulation (Phase C-4)
    [--scene-id <id>]                         Override scene_id (default: 1)
```

### 4.6 `bundle`

```
WintersAssetConverter.exe bundle --input <dir> [-o <output.winters>] [options]

Options:
  --recursive            Recursive collection (default: on)
  --filter <glob>        "*.wmesh" "*.wtex" etc. (multi allowed)
  --compress-all         Force LZ4 on all assets
  --no-strings           Strip string table (Release)
  --sign <seed.key>      Ed25519 sign the bundle (Stage 8)
  --publisher <id>       Set publisher_id
```

예:
```
:: Dev bundle (loose-file fallback OK)
WintersAssetConverter.exe bundle --input Bin\Resource\Content\ \
    -o Bin\Content_Dev.winters --compress-all

:: Release signed bundle
WintersAssetConverter.exe bundle --input Bin\Resource\Content\ \
    -o Bin\Content.winters --compress-all --no-strings \
    --sign C:\secrets\prod.seed.key --publisher 1
```

### 4.7 `sign`

```
WintersAssetConverter.exe sign <file.winters|file.w*> --key <seed.key>
```

### 4.8 `verify`

```
WintersAssetConverter.exe verify <file.winters|file.w*> \
    [--pubkey <pub.key>]
    [--deep]             Recursively verify all assets in a bundle

Exits 0 on success, non-zero on failure.
```

### 4.9 `info`

```
WintersAssetConverter.exe info <file>

:: Output varies by format
$ info body.wmesh
  Magic: WINT, Version: 1.0
  Content size: 418,240 bytes
  Flags: LZ4
  Mesh:
    submeshes: 4
    bones: 52
    vertex_format: VF_SKINNED (76 B stride)
    total_vertices: 10,532
    total_indices: 31,596
  SHA256: 3f2a...

$ info Content.winters
  Magic: WINTPACK, Version: 1.0
  TOC entries: 234
    Mesh:     48
    Anim:    312
    Texture: 180
    ...
  Signed: YES (Ed25519)
  Created: 2026-05-01 14:23:12 UTC
```

### 4.10 `batch` — 가장 중요

디렉토리 전체를 자동 인식 후 변환:

```
WintersAssetConverter.exe batch <input-dir> [-o <output-dir>] [options]

Options:
  --rule <rules.json>          매핑 규칙 파일
  --champions-only             Only process Characters/ subtree
  --threads <N>                Parallel count
  --continue-on-error          Don't abort on single file failure
  --dry-run                    Print plan without executing
```

**rules.json 예**:
```json
{
  "rules": [
    { "match": "*.fbx",  "action": "mesh+skel+anim",
      "options": { "compress": true } },
    { "match": "*_diffuse.png", "action": "tex",
      "options": { "format": "BC7", "srgb": true, "mips": true } },
    { "match": "*_normal.png",  "action": "tex",
      "options": { "format": "BC5", "srgb": false, "mips": true } },
    { "match": "*.mat.json",    "action": "mat" },
    { "match": "Data/*.dat",    "action": "map" }
  ]
}
```

**사용**:
```
WintersAssetConverter.exe batch "Bin\Resource\" --threads 8 --continue-on-error
```

---

## 5. 빌드 시스템 통합

### 5.1 Post-Build Event

```
:: Client.vcxproj PostBuildEvent (Release 빌드 시)
"$(SolutionDir)Tools\Bin\WintersAssetConverter.exe" batch \
    "$(SolutionDir)Client\Bin\Resource\" \
    --rule "$(SolutionDir)Tools\asset_rules.json" \
    --continue-on-error \
    --threads 8

"$(SolutionDir)Tools\Bin\WintersAssetConverter.exe" bundle \
    --input "$(SolutionDir)Client\Bin\Resource\" \
    -o "$(OutDir)Content.winters" \
    --compress-all --no-strings
```

### 5.2 CI/CD (GitHub Actions 예)

```yaml
- name: Build assets
  run: Tools\Bin\WintersAssetConverter.exe batch Bin\Resource\ --threads 8

- name: Sign bundle
  env:
    WINTERS_PROD_SEED: ${{ secrets.WINTERS_PROD_SEED }}
  run: |
    echo "$env:WINTERS_PROD_SEED" > prod.seed.key
    Tools\Bin\WintersAssetConverter.exe sign Bin\Content.winters --key prod.seed.key
    Remove-Item prod.seed.key
```

---

## 6. 로거

```cpp
// Tools/WintersAssetConverter/CLI/Logger.h
namespace Winters::Tool
{
    enum eLogLevel : uint8_t { LOG_QUIET, LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG };

    class CLogger
    {
    public:
        static void SetLevel(eLogLevel l);
        static void SetColor(bool_t b);
        static void Info(const char* fmt, ...);
        static void Warn(const char* fmt, ...);
        static void Error(const char* fmt, ...);
        static void Debug(const char* fmt, ...);
        static void Progress(const char* label, uint32_t current, uint32_t total);
    };
}
```

출력 예 (ANSI):
```
[INFO] Converting 428 assets (8 threads)
[WARN] body_diffuse.png is 4096x4096 — consider reducing for BC7
[OK]   1/428 Irelia/body.wmesh (0.42s, 512KB → 312KB LZ4)
[ERR]  2/428 Bushes/bush_001.fbx — Aborted: FBX has no skeleton
...
[INFO] Done. 426 succeeded, 2 failed. Total time: 2m 14s
```

---

## 7. Return Codes

```cpp
// Tools/WintersAssetConverter/CLI/ReturnCodes.h
enum eExitCode : int
{
    EXIT_OK              = 0,
    EXIT_USAGE           = 1,
    EXIT_INPUT_NOT_FOUND = 2,
    EXIT_PARSE_FAIL      = 3,
    EXIT_WRITE_FAIL      = 4,
    EXIT_HASH_MISMATCH   = 10,
    EXIT_SIGN_FAIL       = 11,
    EXIT_VERIFY_FAIL     = 12,
    EXIT_INTERNAL        = 99,
};
```

CI 파이프라인이 exit code 로 실패 분기.

---

## 8. 병렬 처리

```cpp
// Tools/WintersAssetConverter/Commands/BatchCommand.cpp
#include <execution>
#include <algorithm>

void RunBatch(std::vector<BatchTask>& tasks, uint32_t threads)
{
    std::atomic<uint32_t> done{0};

    std::for_each(std::execution::par, tasks.begin(), tasks.end(),
                   [&](const BatchTask& t) {
        try {
            ExecuteTask(t);
            CLogger::Progress(t.name.c_str(), ++done, (uint32_t)tasks.size());
        } catch (const std::exception& e) {
            CLogger::Error("%s: %s", t.name.c_str(), e.what());
        }
    });
}
```

Assimp / DirectXTex 는 스레드 세이프. 파일 I/O 는 각 스레드가 독립 파일 오픈.

---

## 9. 구성 파일 (`.winters-asset.conf`)

```ini
[global]
log_level    = info
threads      = 0                     ; 0 = CPU cores
output_dir   = Bin/Resource/

[mesh]
compress     = true
generate_mat = true

[tex]
format_diffuse = BC7
format_normal  = BC5
srgb_diffuse   = true
mips           = true
bc7_quality    = slow                ; default|slow|quick

[bundle]
publisher     = 1
compress_all  = true
sign_key      = C:/secrets/prod.seed.key

[map]
default_scene_id = 1
```

```
WintersAssetConverter.exe --config Tools/asset.conf batch Bin/Resource/
```

---

## 10. 에러 리포팅

변환 실패 시 상세 로그:

```
[ERR] Characters/Viego/body.fbx
      Stage: assimp-import
      Reason: Assimp failed to open file
      Detail: aiLoad() returned nullptr, possibly corrupt FBX
      Suggestion: re-export from Blender with "ASCII" format disabled
```

대응 가능한 에러는 suggestion 포함.

---

## 11. Unit Test

각 subcommand 가 호출 가능한 라이브러리 형태 + CLI 는 그 위의 shim:

```cpp
// Tools/WintersAssetConverter/Commands/MeshCommand.h
namespace Winters::Tool
{
    struct MeshCommandInput
    {
        std::wstring inputPath;
        std::wstring outputPath;
        bool_t bCompress, bFlipV, bMirrorX;
        float fScale;
    };

    int RunMeshCommand(const MeshCommandInput& in);
}

// GoogleTest
TEST(MeshCommand, RoundTripIrelia)
{
    MeshCommandInput in{ L"tests/data/irelia.fbx", L"tests/out/irelia.wmesh", true };
    EXPECT_EQ(Winters::Tool::RunMeshCommand(in), 0);

    // 로드 검증
    CWintersFile file;
    EXPECT_TRUE(SUCCEEDED(CWintersFile::LoadFromDisk(in.outputPath.c_str(), file)));
}
```

---

## 12. Release 패키징

```
Tools/Bin/
├── WintersAssetConverter.exe
├── WintersAssetConverter.pdb        (Debug 빌드만)
├── Assimp.dll
├── DirectXTex.dll (또는 static link)
└── LICENSE.txt
```

빌드 출력 `Tools/Bin/Release/*.exe` → CI 가 artifact 로 업로드.

---

## 13. 완료 기준

- [ ] `WintersAssetConverter.vcxproj` 생성 + 빌드
- [ ] 10 subcommand 전부 동작 (mesh/anim/skel/tex/mat/map/bundle/sign/verify/info/batch)
- [ ] `rules.json` 기반 batch 자동 변환
- [ ] Client PostBuild 통합
- [ ] 병렬 처리 (8 core 기준 4배 이상 가속)
- [ ] CI (GitHub Actions) 빌드 스크립트
- [ ] Logger 진행률 + 색상 출력
- [ ] 에러 Return Code 표준화
- [ ] GoogleTest subcommand 단위 테스트

---

## 14. 다음 단계

Stage 10 (Versioning) 으로 이동 — Major/Minor 호환성 + VersionMigrator 런타임.
