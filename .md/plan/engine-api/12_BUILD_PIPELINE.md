# 12. Build Pipeline & Code Generation -- WHT / Module Build / Plugin / Asset Cook

> **UE5 대응**: `UnrealBuildTool (UBT)`, `UnrealHeaderTool (UHT)`, `.uplugin`, `Asset Cooker`
> **현재 Winters**: 단일 Engine.vcxproj + Client.vcxproj, 코드 생성 없음, UpdateLib.bat 수동 SDK 동기화
> **목표**: WHT (Winters Header Tool) 자동 리플렉션 코드 생성, 모듈별 빌드, 플러그인 시스템, 에셋 쿠킹

---

## 1. Architecture Overview

### 1.1 UE5 Build Pipeline

```
.h (UCLASS/UPROPERTY) → UHT parse → .generated.h → Compile → UBT link modules → Cook assets → Package
```

### 1.2 Winters Build Pipeline (목표)

```
.h (WCLASS/WPROPERTY) → WHT parse → .winters.gen.h → MSBuild per-module → Link → Cook → Package
                                     ↑
                              PreBuild Event 또는 CMake Custom Command
```

---

## 2. 파일 구조

```
Tools/
├── WHT/                              ← Winters Header Tool
│   ├── wht.py                        ← 메인 파서 (Python 3.10+)
│   ├── wht_codegen.py                ← 코드 생성기
│   ├── wht_types.py                  ← 파싱 타입 정의
│   └── wht_tests.py                  ← 단위 테스트
├── WintersBuild/                      ← 빌드 자동화
│   ├── generate_module_vcxproj.py     ← 모듈별 vcxproj 생성
│   ├── module_template.vcxproj.in     ← 템플릿
│   └── build_all_modules.bat          ← 전체 빌드 스크립트
├── AssetCooker/                       ← 에셋 쿠킹
│   ├── cook.py                        ← 마스터 쿠킹 스크립트
│   ├── shader_cook.py                 ← HLSL → CSO + 리플렉션
│   └── asset_manifest.py             ← 에셋 매니페스트 생성
```

---

## 3. WHT (Winters Header Tool) — 코드 생성기

### 3.1 `Tools/WHT/wht_types.py`

```python
"""WHT 파싱 타입 정의"""
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Optional

class PropertyType(Enum):
    BOOL = auto()
    INT32 = auto()
    UINT32 = auto()
    INT64 = auto()
    FLOAT = auto()
    DOUBLE = auto()
    STRING = auto()
    WSTRING = auto()
    VEC3 = auto()
    VEC4 = auto()
    OBJECT_PTR = auto()
    ENUM = auto()
    ARRAY = auto()
    CUSTOM = auto()

@dataclass
class PropertyInfo:
    name: str
    cpp_type: str
    prop_type: PropertyType
    flags: list[str] = field(default_factory=list)
    category: str = ""
    tooltip: str = ""
    clamp_min: Optional[float] = None
    clamp_max: Optional[float] = None
    offset_expr: str = ""  # offsetof(Class, member) 생성용

@dataclass
class ClassInfo:
    name: str
    parent: str
    properties: list[PropertyInfo] = field(default_factory=list)
    functions: list['FunctionInfo'] = field(default_factory=list)
    is_abstract: bool = False
    source_file: str = ""
    line_number: int = 0

@dataclass
class FunctionInfo:
    name: str
    return_type: str
    params: list[tuple[str, str]]  # (type, name) pairs
    flags: list[str] = field(default_factory=list)  # Server, Client, etc.

# C++ 타입 → PropertyType 매핑
TYPE_MAP = {
    'bool': PropertyType.BOOL,
    'bool_t': PropertyType.BOOL,
    'i32_t': PropertyType.INT32,
    'int32_t': PropertyType.INT32,
    'u32_t': PropertyType.UINT32,
    'uint32_t': PropertyType.UINT32,
    'i64_t': PropertyType.INT64,
    'f32_t': PropertyType.FLOAT,
    'float': PropertyType.FLOAT,
    'f64_t': PropertyType.DOUBLE,
    'double': PropertyType.DOUBLE,
    'std::string': PropertyType.STRING,
    'std::wstring': PropertyType.WSTRING,
    'wstring_t': PropertyType.WSTRING,
    'Vec3': PropertyType.VEC3,
    'Vec4': PropertyType.VEC4,
}
```

### 3.2 `Tools/WHT/wht.py` — 메인 파서

```python
"""
Winters Header Tool (WHT) — WCLASS/WPROPERTY/WFUNCTION 매크로 파서
UE5 UHT 의 경량 대응. C++ 헤더를 파싱하여 리플렉션 등록 코드 생성.

사용법:
  python wht.py --input Engine/Public/ Client/Public/ --output Generated/
  python wht.py --input Client/Public/GameObject/Champion/ --output Generated/
"""

import re
import os
import sys
import argparse
from pathlib import Path
from wht_types import ClassInfo, PropertyInfo, FunctionInfo, TYPE_MAP, PropertyType
from wht_codegen import generate_registration_code

# WCLASS() 매크로 패턴
RE_WCLASS = re.compile(
    r'WCLASS\s*\(([^)]*)\)\s*\n'
    r'\s*class\s+(?:\w+\s+)*(\w+)\s*'
    r'(?::\s*public\s+(\w+))?',
    re.MULTILINE
)

# WPROPERTY() 매크로 + 다음 줄의 변수 선언
RE_WPROPERTY = re.compile(
    r'WPROPERTY\s*\(([^)]*)\)\s*\n'
    r'\s+(\w[\w:<>,\s]*?)\s+(\w+)\s*(?:=\s*[^;]+)?;',
    re.MULTILINE
)

# WFUNCTION() 매크로 + 함수 선언
RE_WFUNCTION = re.compile(
    r'WFUNCTION\s*\(([^)]*)\)\s*\n'
    r'\s+(?:virtual\s+)?(\w[\w:*&<>,\s]*?)\s+(\w+)\s*\(([^)]*)\)',
    re.MULTILINE
)

# WINTERS_GENERATED_BODY(ClassName) — 확인용
RE_GENERATED_BODY = re.compile(
    r'WINTERS_GENERATED_BODY\s*\(\s*(\w+)\s*\)'
)


def parse_flags(flag_str: str) -> list[str]:
    """WPROPERTY(EditAnywhere, Replicated, Category="Stats") 파싱"""
    flags = []
    for part in flag_str.split(','):
        part = part.strip()
        if part:
            flags.append(part)
    return flags


def extract_category(flags: list[str]) -> str:
    for f in flags:
        if f.startswith('Category'):
            # Category="Stats" 또는 Category = "Stats"
            match = re.search(r'"([^"]*)"', f)
            return match.group(1) if match else ""
    return ""


def resolve_property_type(cpp_type: str) -> PropertyType:
    clean = cpp_type.strip()
    if clean in TYPE_MAP:
        return TYPE_MAP[clean]
    if 'vector' in clean.lower() or 'std::vector' in clean:
        return PropertyType.ARRAY
    if clean.startswith('TWObjectPtr'):
        return PropertyType.OBJECT_PTR
    return PropertyType.CUSTOM


def parse_header(filepath: str) -> list[ClassInfo]:
    """단일 헤더 파일에서 WCLASS 클래스 추출"""
    with open(filepath, 'r', encoding='utf-8-sig') as f:
        content = f.read()

    classes = []

    for m in RE_WCLASS.finditer(content):
        class_flags = parse_flags(m.group(1))
        class_name = m.group(2)
        parent_name = m.group(3) or "WObject"

        # WINTERS_GENERATED_BODY 확인
        if not RE_GENERATED_BODY.search(content):
            print(f"  WARNING: {class_name} in {filepath} missing "
                  f"WINTERS_GENERATED_BODY", file=sys.stderr)

        cls = ClassInfo(
            name=class_name,
            parent=parent_name,
            source_file=filepath,
            line_number=content[:m.start()].count('\n') + 1,
        )

        # 해당 클래스 범위 내 WPROPERTY 수집
        # 간이 범위: WCLASS ~ 다음 WCLASS 또는 파일 끝
        class_start = m.end()
        next_class = RE_WCLASS.search(content, class_start)
        class_end = next_class.start() if next_class else len(content)
        class_body = content[class_start:class_end]

        for pm in RE_WPROPERTY.finditer(class_body):
            flags = parse_flags(pm.group(1))
            cpp_type = pm.group(2).strip()
            var_name = pm.group(3).strip()

            prop = PropertyInfo(
                name=var_name,
                cpp_type=cpp_type,
                prop_type=resolve_property_type(cpp_type),
                flags=flags,
                category=extract_category(flags),
                offset_expr=f"offsetof({class_name}, {var_name})",
            )
            cls.properties.append(prop)

        # WFUNCTION 수집
        for fm in RE_WFUNCTION.finditer(class_body):
            flags = parse_flags(fm.group(1))
            ret_type = fm.group(2).strip()
            func_name = fm.group(3).strip()
            params_str = fm.group(4).strip()

            params = []
            if params_str:
                for p in params_str.split(','):
                    p = p.strip()
                    parts = p.rsplit(None, 1)
                    if len(parts) == 2:
                        params.append((parts[0], parts[1]))

            func = FunctionInfo(
                name=func_name,
                return_type=ret_type,
                params=params,
                flags=flags,
            )
            cls.functions.append(func)

        classes.append(cls)

    return classes


def scan_directory(dirs: list[str]) -> list[ClassInfo]:
    """디렉토리 재귀 스캔하여 모든 WCLASS 수집"""
    all_classes = []
    for d in dirs:
        for root, _, files in os.walk(d):
            for fname in files:
                if fname.endswith('.h'):
                    filepath = os.path.join(root, fname)
                    classes = parse_header(filepath)
                    if classes:
                        print(f"  Found {len(classes)} class(es) in "
                              f"{filepath}")
                        all_classes.extend(classes)
    return all_classes


def main():
    parser = argparse.ArgumentParser(
        description='Winters Header Tool (WHT) — '
                    'WCLASS/WPROPERTY reflection code generator')
    parser.add_argument('--input', nargs='+', required=True,
                        help='Input directories to scan')
    parser.add_argument('--output', required=True,
                        help='Output directory for .winters.gen.h files')
    parser.add_argument('--dry-run', action='store_true',
                        help='Parse only, do not generate')
    args = parser.parse_args()

    print(f"[WHT] Scanning {len(args.input)} input dirs...")
    classes = scan_directory(args.input)
    print(f"[WHT] Found {len(classes)} reflected classes, "
          f"{sum(len(c.properties) for c in classes)} properties, "
          f"{sum(len(c.functions) for c in classes)} functions")

    if args.dry_run:
        for cls in classes:
            print(f"  {cls.name} : {cls.parent} "
                  f"({len(cls.properties)} props, "
                  f"{len(cls.functions)} funcs)")
        return

    os.makedirs(args.output, exist_ok=True)
    generated_count = 0

    for cls in classes:
        out_path = os.path.join(args.output,
                                f"{cls.name}.winters.gen.h")
        code = generate_registration_code(cls)
        with open(out_path, 'w', encoding='utf-8') as f:
            f.write(code)
        generated_count += 1

    print(f"[WHT] Generated {generated_count} files in {args.output}")


if __name__ == '__main__':
    main()
```

### 3.3 `Tools/WHT/wht_codegen.py` — 코드 생성

```python
"""WHT 코드 생성기 — ClassInfo → .winters.gen.h C++ 코드"""

from wht_types import ClassInfo, PropertyInfo, PropertyType

# PropertyType → ePropertyType C++ enum
PROP_TYPE_CPP = {
    PropertyType.BOOL: "ePropertyType::Bool",
    PropertyType.INT32: "ePropertyType::Int32",
    PropertyType.UINT32: "ePropertyType::UInt32",
    PropertyType.INT64: "ePropertyType::Int64",
    PropertyType.FLOAT: "ePropertyType::Float",
    PropertyType.DOUBLE: "ePropertyType::Double",
    PropertyType.STRING: "ePropertyType::String",
    PropertyType.WSTRING: "ePropertyType::WString",
    PropertyType.VEC3: "ePropertyType::Vec3",
    PropertyType.VEC4: "ePropertyType::Vec4",
    PropertyType.OBJECT_PTR: "ePropertyType::ObjectPtr",
    PropertyType.ENUM: "ePropertyType::Enum",
    PropertyType.ARRAY: "ePropertyType::Array",
    PropertyType.CUSTOM: "ePropertyType::Custom",
}


def flags_to_cpp(flags: list[str]) -> str:
    """플래그 리스트 → ePropertyFlags 조합"""
    flag_map = {
        'EditAnywhere': 'ePropertyFlags::EditAnywhere',
        'VisibleAnywhere': 'ePropertyFlags::VisibleAnywhere',
        'Replicated': 'ePropertyFlags::Replicated',
        'SaveGame': 'ePropertyFlags::SaveGame',
        'Transient': 'ePropertyFlags::Transient',
        'Slider': 'ePropertyFlags::Slider',
        'ColorPicker': 'ePropertyFlags::ColorPicker',
    }
    cpp_flags = []
    for f in flags:
        key = f.split('=')[0].strip().strip('"')
        if key in flag_map:
            cpp_flags.append(flag_map[key])
    if not cpp_flags:
        return 'ePropertyFlags::None'
    return ' | '.join(cpp_flags)


def generate_registration_code(cls: ClassInfo) -> str:
    """ClassInfo → RegisterProperties 구현 코드"""
    lines = []
    lines.append(f"// AUTO-GENERATED by WHT — do not edit")
    lines.append(f"// Source: {cls.source_file}:{cls.line_number}")
    lines.append(f"// Class: {cls.name} : {cls.parent}")
    lines.append(f"#pragma once\n")
    lines.append(f'#include "Object/WProperty.h"')
    lines.append(f'#include "Object/WClass.h"')
    lines.append(f'#include <cstddef>\n')
    lines.append(
        f"void {cls.name}::RegisterProperties(WClass* cls)")
    lines.append("{")

    for prop in cls.properties:
        prop_type = PROP_TYPE_CPP.get(prop.prop_type,
                                       "ePropertyType::Custom")
        flags_cpp = flags_to_cpp(prop.flags)
        category = prop.category or "Default"

        lines.append(f"    // {prop.cpp_type} {prop.name}")
        lines.append(f"    {{")
        lines.append(f"        WPropertyMeta p;")
        lines.append(f'        p.name = "{prop.name}";')
        lines.append(f'        p.displayName = "{prop.name}";')
        lines.append(f'        p.category = "{category}";')
        lines.append(f"        p.type = {prop_type};")
        lines.append(f"        p.flags = {flags_cpp};")
        lines.append(f"        p.offset = static_cast<u32_t>("
                     f"{prop.offset_expr});")
        lines.append(f"        p.size = static_cast<u32_t>("
                     f"sizeof(std::declval<{cls.name}>()."
                     f"{prop.name}));")
        lines.append(f"        p.typeIndex = typeid("
                     f"decltype(std::declval<{cls.name}>()."
                     f"{prop.name}));")

        if prop.clamp_min is not None:
            lines.append(f"        p.clampMin = {prop.clamp_min};")
            lines.append(
                f"        p.flags = p.flags "
                f"| ePropertyFlags::ClampMin;")
        if prop.clamp_max is not None:
            lines.append(f"        p.clampMax = {prop.clamp_max};")
            lines.append(
                f"        p.flags = p.flags "
                f"| ePropertyFlags::ClampMax;")

        lines.append(f"        cls->RegisterProperty(p);")
        lines.append(f"    }}")
        lines.append("")

    # RPC 함수 등록 (향후 06_NETWORK_REPLICATION 연동)
    for func in cls.functions:
        rpc_flags = [f for f in func.flags
                     if f in ('Server', 'Client', 'NetMulticast')]
        if rpc_flags:
            lines.append(f"    // RPC: {func.name} "
                         f"[{', '.join(rpc_flags)}]")
            lines.append(f"    // TODO: Register RPC dispatch entry")

    lines.append("}")
    lines.append("")

    return '\n'.join(lines)
```

---

## 4. 모듈별 빌드 자동화

### 4.1 `Tools/WintersBuild/generate_module_vcxproj.py`

```python
"""모듈별 vcxproj 생성 스크립트
Engine 을 Core/RHI/Renderer/Network/Editor 모듈로 분리 시 사용"""

import os
import sys
from pathlib import Path

VCXPROJ_TEMPLATE = r'''<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{{{guid}}}</ProjectGuid>
    <RootNamespace>{module_name}</RootNamespace>
    <ProjectName>{module_name}</ProjectName>
    <ConfigurationType>DynamicLibrary</ConfigurationType>
    <PlatformToolset>v143</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>

  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>
        $(SolutionDir)Engine\Include;
        $(SolutionDir)Engine\Public;
        $(SolutionDir)Shared;
        $(SolutionDir)Engine\ThirdPartyLib\include;
        %(AdditionalIncludeDirectories)
      </AdditionalIncludeDirectories>
      <PreprocessorDefinitions>
        WINTERS_ENGINE_EXPORTS;
        {module_define};
        %(PreprocessorDefinitions)
      </PreprocessorDefinitions>
      <LanguageStandard>stdcpp20</LanguageStandard>
      <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
    </ClCompile>
    <Link>
      <AdditionalDependencies>
        {dependencies};%(AdditionalDependencies)
      </AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>

  <ItemGroup>
{source_items}
  </ItemGroup>

  <ItemGroup>
{header_items}
  </ItemGroup>

  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />

  <Target Name="WHT" BeforeTargets="ClCompile">
    <Exec Command="python $(SolutionDir)Tools\WHT\wht.py --input {public_dir} --output {gen_dir}" />
  </Target>
</Project>
'''


def find_sources(src_dir: str, pub_dir: str):
    """소스/헤더 파일 수집"""
    sources, headers = [], []
    for d in [src_dir, pub_dir]:
        if not os.path.isdir(d):
            continue
        for root, _, files in os.walk(d):
            for f in files:
                rel = os.path.relpath(os.path.join(root, f))
                if f.endswith('.cpp'):
                    sources.append(rel)
                elif f.endswith('.h'):
                    headers.append(rel)
    return sources, headers


def generate(module_name: str, src_dir: str, pub_dir: str,
             deps: list[str], guid: str):
    sources, headers = find_sources(src_dir, pub_dir)

    src_items = '\n'.join(
        f'    <ClCompile Include="{s}" />' for s in sources)
    hdr_items = '\n'.join(
        f'    <ClInclude Include="{h}" />' for h in headers)
    dep_libs = ';'.join(f'{d}.lib' for d in deps)
    module_def = f"{module_name.upper()}_EXPORTS"

    content = VCXPROJ_TEMPLATE.format(
        guid=guid,
        module_name=module_name,
        module_define=module_def,
        dependencies=dep_libs,
        source_items=src_items,
        header_items=hdr_items,
        public_dir=pub_dir.replace('\\', '/'),
        gen_dir=f"Generated/{module_name}",
    )

    out_path = f"{module_name}.vcxproj"
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Generated {out_path} ({len(sources)} cpp, "
          f"{len(headers)} h)")
```

---

## 5. 셰이더 쿠킹 파이프라인

### 5.1 `Tools/AssetCooker/shader_cook.py`

```python
"""HLSL 셰이더 오프라인 컴파일 + 리플렉션 데이터 추출
배포 빌드에서 D3DCompileFromFile 런타임 호출 제거용"""

import subprocess
import os
import json
import hashlib
from pathlib import Path

FXC_PATH = r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\fxc.exe"

SHADER_PROFILES = {
    'VS': 'vs_5_0',
    'PS': 'ps_5_0',
    'CS': 'cs_5_0',
}

SHADER_DIR = "Shaders"
OUTPUT_DIR = "Cooked/Shaders"


def compile_shader(hlsl_path: str, entry: str, profile: str,
                   output_cso: str, defines: list[str] = None):
    """fxc.exe 로 오프라인 컴파일"""
    cmd = [
        FXC_PATH,
        '/T', profile,
        '/E', entry,
        '/Fo', output_cso,
        '/O3',  # 최적화
        '/Zi',  # 디버그 정보 (PDB)
    ]
    if defines:
        for d in defines:
            cmd.extend(['/D', d])
    cmd.append(hlsl_path)

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  ERROR compiling {hlsl_path} [{entry}]:")
        print(result.stderr)
        return False

    print(f"  Compiled: {hlsl_path} [{entry}] -> {output_cso}")
    return True


def compute_hash(filepath: str) -> str:
    with open(filepath, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def cook_all_shaders():
    """모든 .hlsl 파일 컴파일"""
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    manifest = {}

    for hlsl_file in Path(SHADER_DIR).rglob('*.hlsl'):
        name = hlsl_file.stem
        rel = str(hlsl_file)

        # VS + PS 컴파일
        for entry, profile in [('VS', 'vs_5_0'), ('PS', 'ps_5_0')]:
            cso_name = f"{name}_{entry}.cso"
            cso_path = os.path.join(OUTPUT_DIR, cso_name)

            if compile_shader(str(hlsl_file), entry, profile,
                              cso_path):
                manifest[cso_name] = {
                    'source': rel,
                    'entry': entry,
                    'profile': profile,
                    'hash': compute_hash(cso_path),
                }

    # 매니페스트 저장
    manifest_path = os.path.join(OUTPUT_DIR, "shader_manifest.json")
    with open(manifest_path, 'w') as f:
        json.dump(manifest, f, indent=2)
    print(f"\nManifest: {manifest_path} "
          f"({len(manifest)} entries)")


if __name__ == '__main__':
    cook_all_shaders()
```

---

## 6. 에셋 쿠킹 파이프라인

### 6.1 `Tools/AssetCooker/cook.py`

```python
"""에셋 쿠킹 마스터 스크립트
원본 에셋 → 최적화된 런타임 포맷"""

import os
import json
import hashlib
from pathlib import Path

ASSET_ROOTS = [
    "Client/Bin/Resource/",
]

COOKED_DIR = "Cooked/Assets"


def cook_textures(src_dir: str, out_dir: str):
    """PNG/BMP → DDS 변환 (texconv 사용)"""
    count = 0
    for tex in Path(src_dir).rglob('*.png'):
        rel = tex.relative_to(src_dir)
        out_path = Path(out_dir) / rel.with_suffix('.dds')
        out_path.parent.mkdir(parents=True, exist_ok=True)

        # texconv 호출 (DirectXTex 도구)
        # 실제 구현: subprocess.run(["texconv", ...])
        print(f"  Cook texture: {tex} -> {out_path}")
        count += 1
    return count


def cook_meshes(src_dir: str, out_dir: str):
    """FBX → .wmesh/.wskel/.wanim (WintersAssetConverter 사용)"""
    converter = "Tools/WintersAssetConverter/WintersAssetConverter.exe"
    count = 0
    for fbx in Path(src_dir).rglob('*.fbx'):
        rel = fbx.relative_to(src_dir)
        out_path = Path(out_dir) / rel.with_suffix('.wmesh')
        out_path.parent.mkdir(parents=True, exist_ok=True)

        # subprocess.run([converter, str(fbx), str(out_path)])
        print(f"  Cook mesh: {fbx} -> {out_path}")
        count += 1
    return count


def generate_manifest(cooked_dir: str):
    """쿠킹된 에셋 매니페스트 생성"""
    manifest = {}
    for f in Path(cooked_dir).rglob('*'):
        if f.is_file():
            rel = str(f.relative_to(cooked_dir))
            with open(f, 'rb') as fp:
                h = hashlib.sha256(fp.read()).hexdigest()
            manifest[rel] = {
                'size': f.stat().st_size,
                'hash': h,
            }

    out = os.path.join(cooked_dir, "asset_manifest.json")
    with open(out, 'w') as fp:
        json.dump(manifest, fp, indent=2)
    print(f"\nAsset manifest: {out} ({len(manifest)} entries)")


def main():
    print("[AssetCooker] Starting...")
    os.makedirs(COOKED_DIR, exist_ok=True)

    total = 0
    for root in ASSET_ROOTS:
        if os.path.isdir(root):
            total += cook_textures(root, COOKED_DIR)
            total += cook_meshes(root, COOKED_DIR)

    generate_manifest(COOKED_DIR)
    print(f"[AssetCooker] Done. {total} assets cooked.")


if __name__ == '__main__':
    main()
```

---

## 7. PreBuild Event 통합

### 7.1 Engine.vcxproj PreBuild Event

```xml
<Target Name="WHT_PreBuild" BeforeTargets="ClCompile">
  <Message Text="[WHT] Scanning for WCLASS/WPROPERTY..." Importance="high" />
  <Exec Command="python &quot;$(SolutionDir)Tools\WHT\wht.py&quot; --input &quot;$(SolutionDir)Engine\Public&quot; &quot;$(SolutionDir)Client\Public&quot; --output &quot;$(SolutionDir)Generated&quot;" />
</Target>
```

### 7.2 전체 빌드 플로우

```
1. WHT PreBuild
   ├── Scan Engine/Public/*.h + Client/Public/*.h
   ├── Parse WCLASS/WPROPERTY/WFUNCTION
   └── Generate Generated/*.winters.gen.h

2. MSBuild Compile
   ├── Engine.vcxproj → WintersEngine.dll
   │   └── #include "Generated/WActor.winters.gen.h" (자동 포함)
   └── Client.vcxproj → WintersLOL.exe

3. PostBuild
   ├── UpdateLib.bat → EngineSDK/inc/ 동기화
   ├── xcopy Shaders → OutDir
   └── xcopy ThirdPartyLib DLLs → OutDir

4. Cook (Release 배포 시)
   ├── shader_cook.py → Cooked/Shaders/*.cso
   ├── cook.py → Cooked/Assets/ (DDS, wmesh)
   └── Package → zip/installer
```

---

## 8. 플러그인 시스템

### 8.1 플러그인 기술자 (`*.wplugin`)

```json
{
    "name": "CustomChampions",
    "version": "1.0.0",
    "type": "Game",
    "description": "Additional champions for WintersLOL",
    "author": "TeamWinters",
    "modules": [
        {
            "name": "CustomChampions",
            "type": "Game",
            "loadPhase": "PostDefault",
            "dependencies": ["WintersCore", "WintersRenderer"]
        }
    ],
    "contentDirs": ["Content/CustomChampions/"],
    "shaders": ["Shaders/Custom/"]
}
```

### 8.2 플러그인 로더 (`Engine/Public/Plugin/PluginManager.h`)

```cpp
#pragma once

#include "WintersAPI.h"
#include "Module/ModuleManager.h"
#include <string>
#include <vector>
#include <memory>

struct PluginDescriptor
{
    std::string name;
    std::string version;
    std::string type;        // "Game", "Editor", "Content"
    std::string description;
    std::vector<std::string> moduleNames;
    std::vector<std::string> contentDirs;
};

class WINTERS_API CPluginManager final
{
public:
    static CPluginManager& Get();

    /// Plugins/ 디렉토리 스캔하여 .wplugin 파일 로드
    void DiscoverPlugins(const std::wstring& pluginDir);

    /// 발견된 플러그인의 모듈 로드
    void LoadAllPlugins();

    /// 특정 플러그인 활성화/비활성화
    void EnablePlugin(const std::string& name);
    void DisablePlugin(const std::string& name);

    /// 활성 플러그인 목록
    const std::vector<PluginDescriptor>& GetPlugins() const
    { return m_Plugins; }

private:
    CPluginManager() = default;

    PluginDescriptor ParsePluginFile(const std::wstring& path);

    std::vector<PluginDescriptor> m_Plugins;
    std::vector<std::string>      m_EnabledPlugins;
};
```

### 8.3 `Engine/Private/Plugin/PluginManager.cpp`

```cpp
#include "Plugin/PluginManager.h"
#include "Module/ModuleManager.h"

#include <fstream>
#include <filesystem>
// nlohmann/json 또는 자체 JSON 파서
// #include <nlohmann/json.hpp>

namespace fs = std::filesystem;

CPluginManager& CPluginManager::Get()
{
    static CPluginManager s_Instance;
    return s_Instance;
}

void CPluginManager::DiscoverPlugins(const std::wstring& pluginDir)
{
    if (!fs::exists(pluginDir))
        return;

    for (auto& entry : fs::recursive_directory_iterator(pluginDir))
    {
        if (entry.path().extension() == L".wplugin")
        {
            auto desc = ParsePluginFile(entry.path().wstring());
            if (!desc.name.empty())
            {
                m_Plugins.push_back(std::move(desc));
            }
        }
    }
}

void CPluginManager::LoadAllPlugins()
{
    auto& mm = CModuleManager::Get();

    for (auto& plugin : m_Plugins)
    {
        for (auto& modName : plugin.moduleNames)
        {
            mm.LoadModule(modName.c_str());
        }
        m_EnabledPlugins.push_back(plugin.name);
    }
}

void CPluginManager::EnablePlugin(const std::string& name)
{
    for (auto& plugin : m_Plugins)
    {
        if (plugin.name == name)
        {
            auto& mm = CModuleManager::Get();
            for (auto& mod : plugin.moduleNames)
                mm.LoadModule(mod.c_str());
            m_EnabledPlugins.push_back(name);
            return;
        }
    }
}

void CPluginManager::DisablePlugin(const std::string& name)
{
    for (auto& plugin : m_Plugins)
    {
        if (plugin.name == name)
        {
            auto& mm = CModuleManager::Get();
            for (auto it = plugin.moduleNames.rbegin();
                 it != plugin.moduleNames.rend(); ++it)
                mm.UnloadModule(it->c_str());

            m_EnabledPlugins.erase(
                std::remove(m_EnabledPlugins.begin(),
                            m_EnabledPlugins.end(), name),
                m_EnabledPlugins.end());
            return;
        }
    }
}

PluginDescriptor CPluginManager::ParsePluginFile(
    const std::wstring& path)
{
    PluginDescriptor desc;
    // JSON 파싱 → desc 채우기
    // 실제 구현은 nlohmann/json 또는 자체 파서
    return desc;
}
```

---

## 9. CI/CD 통합

### 9.1 GitHub Actions 빌드 워크플로우 (예시)

```yaml
# .github/workflows/build.yml
name: Winters Engine Build

on:
  push:
    branches: [main, feature/*]
  pull_request:
    branches: [main]

jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4

      - name: Setup Python (WHT)
        uses: actions/setup-python@v5
        with:
          python-version: '3.12'

      - name: Setup MSVC
        uses: microsoft/setup-msbuild@v2

      - name: WHT Code Generation
        run: python Tools/WHT/wht.py --input Engine/Public Client/Public --output Generated

      - name: Build Engine (Debug)
        run: msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m

      - name: Build Engine (Release)
        run: msbuild Winters.sln /p:Configuration=Release /p:Platform=x64 /m

      - name: Cook Shaders
        run: python Tools/AssetCooker/shader_cook.py

      - name: Upload Artifacts
        uses: actions/upload-artifact@v4
        with:
          name: winters-build
          path: |
            Client/Bin/Release/
            Cooked/
```

---

## 10. Verification Checklist

```
[ ] WHT: Python 스크립트로 WCLASS/WPROPERTY 포함 헤더 파싱
[ ] WHT: .winters.gen.h 파일 생성 → RegisterProperties 코드 포함
[ ] WHT: 생성된 코드 컴파일 성공 (Engine.vcxproj PreBuild)
[ ] WHT: 프로퍼티 타입 자동 추론 (f32_t → Float, Vec3 → Vec3)
[ ] WHT: 플래그 파싱 (EditAnywhere, Replicated, Category)
[ ] 모듈 빌드: Engine → Core/RHI/Renderer 분리 vcxproj 생성
[ ] 셰이더 쿠킹: fxc.exe → .cso + shader_manifest.json
[ ] 에셋 쿠킹: PNG → DDS, FBX → .wmesh 변환 확인
[ ] 플러그인: .wplugin 파싱 → 모듈 동적 로드
[ ] CI/CD: GitHub Actions 빌드 워크플로우 통과
```

---

## 11. 기존 코드 마이그레이션

| 현재 | 마이그 후 |
|------|----------|
| `RegisterProperties()` 수동 구현 | WHT 가 자동 생성 |
| 단일 Engine.vcxproj | 모듈별 vcxproj (선택적) |
| `D3DCompileFromFile` 런타임 | Release: CSO 로드, Debug: 런타임 유지 |
| `UpdateLib.bat` 수동 | PreBuild + PostBuild 자동화 |
| 플러그인 없음 | .wplugin + CPluginManager |
| 에셋 원본 배포 | 쿠킹 후 최적화 포맷 배포 |
