Session - 리신 W1과 동일 커밋에서 흰색으로 덮인 WFX 팔레트를 전수 감사하고 재발 방지 계약까지 복구한다.
목표: 신규 목표 · 축 C5 데이터 시각 정확성, C8 회귀 검증
관련: `CLAUDE_Legacy.md` · `.claude/gotchas.md` · `.md/architecture/WINTERS_CODEBASE_COMPASS.md`

# 2026-07-20 LeeSin/Riven WFX Tint Regression Plan

## 0. 결정 기록

- 문제·제약: `LeeSin.W1.Cast`는 정상 cue와 정상 WFX 파일을 재생하지만 5개 emitter의 RGB가 모두 `[1,1,1]`여서 Additive 합성에서 흰색으로 보인다. 2026-07-14의 공통 흰색 실드 결정은 당시에는 의도적이었으나, 사용자가 2026-07-20에 LeeSin W1과 Riven E 모두를 초록 계열로 되돌리도록 명시해 최신 팔레트 결정이 이를 supersede한다. 실행 중인 서버는 건드리지 않는다.
- 순진한 해법의 실패: 저장소의 모든 흰색 WFX를 일괄 착색하면 의도된 백색 섬광·마스크까지 손상된다. 이력상 “기존 chromatic RGB -> grayscale RGB” 전이가 확인된 파일과 같은 shield palette 안에서 함께 추가된 흰색 레이어만 복구해야 한다.
- 메커니즘: 전체 WFX git 이력을 emitter 이름 기준으로 비교한 결과, chromatic-to-gray 전이는 중복 브랜치 커밋을 제외하면 공통 흰색 실드 작업이 바꾼 `LeeSin/w1_cast.wfx` 5개와 `Riven/Riven_E_Shield.wfx` 5개뿐이다. 현재 Riven에서는 그중 `E_ShieldMult`가 삭제되어 4개가 남고, 같은 흰색 실드 작업에서 흰색 bubble/glow 2개가 추가됐다. 최신 사용자 결정에 따라 현재 파일 기준 LeeSin 5개 + Riven 6개 RGB를 초록 계열 보호 팔레트로 고정한다.
- 대조: lifetime, alpha, texture, lifecycle은 현재 gameplay/visual 지속시간에 맞춘 후속 값이므로 되돌리지 않는다. 색상 RGB만 마지막 정상 팔레트에 맞춘다.
- 대가: exact-palette 계약은 의도적인 재튜닝도 테스트 갱신을 요구한다. 이는 조용한 흰색 정규화를 막기 위한 의도된 review gate다.

## 1. 현재 증거와 소유권

- `Client/Private/GameObject/Champion/LeeSin/LeeSin_Skills.cpp`는 W stage 1에서 `LeeSin.W1.Cast`를 호출한다.
- `Client/Private/GameObject/FX/FxCuePlayer.cpp`는 workspace의 `Data/LoL/FX`를 runtime asset registry에 로드한다. shader fallback 경로가 아니다.
- `.md/build/2026-07-14_SERVER_AUTHORITATIVE_WHITE_SHIELDS_REPORT.md`와 work packet은 LeeSin W1·Riven E의 흰색 RGB를 당시 의도된 공통 실드 팔레트로 기록한다. 이번 변경은 그 사실을 부정하는 롤백이 아니라 사용자의 최신 명시 결정에 따른 champion-specific 초록 팔레트 재도입이다.
- `w2_cast.wfx`는 최초부터 주황색이며 변경 이력이 없다. 이번 수정 대상이 아니다.
- 현재 다른 세션이 수정 중인 Ashe/Fiora/Sylas/Yasuo/Yone/Jungle WFX는 읽기만 하고 수정하지 않는다.

## 2. 반영해야 하는 파일

### 2-1. `Data/LoL/FX/Champions/LeeSin/w1_cast.wfx`

각 emitter의 현재 RGB `[1.0,1.0,1.0]`만 아래 값으로 교체하고 alpha와 나머지 필드는 유지한다.

```json
"w1_floor_gold_teal_ring": [0.72, 1.0, 0.78, 0.38]
"w1_shield_main_bubble": [0.68, 1.0, 0.84, 0.55]
"w1_shield_outer_glow": [0.36, 0.98, 1.0, 0.44]
"w1_gold_outline_pop": [1.0, 0.78, 0.24, 0.58]
"w1_scroll_interior": [0.92, 0.82, 0.44, 0.46]
```

### 2-2. `Data/LoL/FX/Champions/Riven/Riven_E_Shield.wfx`

현재 남은 과거 emitter 4개는 마지막 정상 RGB로 복구한다. 같은 회귀 커밋에서 추가된 bubble/glow는 기존 Riven shield palette의 main/flare RGB를 적용한다. lifetime, alpha, texture, lifecycle은 유지한다.

```json
"E_ShieldMesh": [0.62, 2.45, 0.48, 0.68]
"E_ShieldMainBubble": [0.50, 2.20, 0.40, 0.52]
"E_ShieldOuterGlow": [0.70, 2.55, 0.60, 0.38]
"E_BlueFlare": [0.70, 2.55, 0.60, 0.54]
"E_Rune": [0.72, 2.30, 0.52, 0.52]
"E_GroundFlash": [0.36, 1.85, 0.30, 0.46]
```

### 2-3. 새 파일: `Tools/LoLData/Test-WfxTintRegression.py`

```python
from __future__ import annotations

import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FX_ROOT = ROOT / "Data" / "LoL" / "FX"

PROTECTED_TINTS = {
    "Champions/LeeSin/w1_cast.wfx": {
        "w1_floor_gold_teal_ring": [0.72, 1.0, 0.78, 0.38],
        "w1_shield_main_bubble": [0.68, 1.0, 0.84, 0.55],
        "w1_shield_outer_glow": [0.36, 0.98, 1.0, 0.44],
        "w1_gold_outline_pop": [1.0, 0.78, 0.24, 0.58],
        "w1_scroll_interior": [0.92, 0.82, 0.44, 0.46],
    },
    "Champions/Riven/Riven_E_Shield.wfx": {
        "E_ShieldMesh": [0.62, 2.45, 0.48, 0.68],
        "E_ShieldMainBubble": [0.50, 2.20, 0.40, 0.52],
        "E_ShieldOuterGlow": [0.70, 2.55, 0.60, 0.38],
        "E_BlueFlare": [0.70, 2.55, 0.60, 0.54],
        "E_Rune": [0.72, 2.30, 0.52, 0.52],
        "E_GroundFlash": [0.36, 1.85, 0.30, 0.46],
    },
}


def fail(message: str) -> None:
    raise SystemExit(f"[WfxTintRegression] FAIL: {message}")


def load_wfx(path: Path) -> dict:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail(f"{path.relative_to(ROOT)}: {error}")
    if not isinstance(document.get("emitters"), list):
        fail(f"{path.relative_to(ROOT)}: emitters must be an array")
    return document


def validate_color(path: Path, emitter_name: str, color: object) -> None:
    if not isinstance(color, list) or len(color) != 4:
        fail(f"{path.relative_to(ROOT)}:{emitter_name}: color must have RGBA")
    if not all(
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and math.isfinite(value)
        for value in color
    ):
        fail(f"{path.relative_to(ROOT)}:{emitter_name}: color must be finite numeric RGBA")


def main() -> int:
    wfx_files = sorted(FX_ROOT.rglob("*.wfx"))
    if not wfx_files:
        fail("no WFX files found")

    documents: dict[str, dict] = {}
    emitter_count = 0
    for path in wfx_files:
        document = load_wfx(path)
        key = path.relative_to(FX_ROOT).as_posix()
        documents[key] = document
        for emitter in document["emitters"]:
            if not isinstance(emitter, dict):
                fail(f"{path.relative_to(ROOT)}: every emitter must be an object")
            emitter_name = emitter.get("name", "<unnamed>")
            if "color" in emitter:
                validate_color(path, str(emitter_name), emitter["color"])
            emitter_count += 1

    protected_count = 0
    for relative_path, expected_emitters in PROTECTED_TINTS.items():
        document = documents.get(relative_path)
        if document is None:
            fail(f"missing protected asset: {relative_path}")
        actual_emitters: dict[str, dict] = {}
        for emitter in document["emitters"]:
            emitter_name = emitter.get("name")
            if not isinstance(emitter_name, str):
                continue
            if emitter_name in actual_emitters:
                fail(f"{relative_path}: duplicate protected emitter {emitter_name}")
            actual_emitters[emitter_name] = emitter
        for emitter_name, expected_color in expected_emitters.items():
            emitter = actual_emitters.get(emitter_name)
            if emitter is None:
                fail(f"{relative_path}: missing protected emitter {emitter_name}")
            actual_color = emitter.get("color")
            validate_color(FX_ROOT / relative_path, emitter_name, actual_color)
            if actual_color != expected_color:
                fail(
                    f"{relative_path}:{emitter_name}: "
                    f"expected {expected_color}, actual {actual_color}"
                )
            protected_count += 1

    print(
        f"[WfxTintRegression] PASS: {len(wfx_files)} files / "
        f"{emitter_count} emitters valid; {protected_count} protected tints exact"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

### 2-4. `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1`

기존 `Champion schema mutation contract` 호출 바로 아래에 추가한다.

```powershell
    Invoke-Checked "Protected WFX tint regression" {
        python Tools/LoLData/Test-WfxTintRegression.py
    }
```

### 2-5. `.claude/gotchas.md`

파일 상단 최신 항목에 아래 한 줄을 추가한다.

```text
- 2026-07-20 - [WFX palette ownership] 공통 gameplay shield 작업이 champion-specific WFX까지 일괄 흰색으로 정규화하면 이후 요구 팔레트와 충돌한다(LeeSin W1·Riven E) -> gameplay amount/lifetime과 presentation palette 결정을 분리하고, 보호 팔레트 변경은 최신 명시 요구와 `python Tools/LoLData/Test-WfxTintRegression.py`를 함께 갱신한다.
```

### 2-6. `.md/build/2026-07-14_SERVER_AUTHORITATIVE_WHITE_SHIELDS_REPORT.md`

문서 제목 바로 아래에 current-status note를 추가한다. 공통 shield gameplay, 3초 duration, 흰색 effective-health bar, Yasuo WFX는 유지되지만 LeeSin W1·Riven E WFX palette만 최신 초록 결정으로 supersede됐음을 분리한다.

## 3. 구현·검증 순서

1. 이 계획을 독립 비평해 P0/P1 0건을 확인한다.
2. 두 WFX의 target RGB만 변경한다.
3. 전체 WFX JSON/색상 shape와 11개 보호 tint를 검사하는 테스트를 추가한다.
4. LoL data-driven pipeline에 테스트를 연결하고 gotcha를 기록한다.
5. Python syntax, 신규 test, 전체 WFX history audit, Client Debug/Release x64 build, scoped `git diff --check`를 실행한다.
6. 실행 중인 서버는 유지하고 새 Release 클라이언트 또는 WFX preview에서 LeeSin W1과 Riven E가 chromatic green인지 화면 확인한다. 이 화면 확인 전에는 RESULT를 자동 검증 PASS·시각 확인 필요로 구분한다.
7. 같은 이름의 RESULT에 기대/실제, 전수 감사 결과, 수동 화면 검증 여부를 기록한다.

검증 명령:

```powershell
python -m py_compile Tools/LoLData/Test-WfxTintRegression.py
python Tools/LoLData/Test-WfxTintRegression.py
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug
& 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe' Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /v:minimal
& 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe' Client/Include/Client.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /v:minimal
git diff --check -- Data/LoL/FX/Champions/LeeSin/w1_cast.wfx Data/LoL/FX/Champions/Riven/Riven_E_Shield.wfx Tools/LoLData/Test-WfxTintRegression.py Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 .claude/gotchas.md .md/build/2026-07-14_SERVER_AUTHORITATIVE_WHITE_SHIELDS_REPORT.md
```

`Verify-LoLDataDrivenPipeline.ps1`의 전체 build 단계는 실행 중인 Debug 서버 산출물 lock으로 실패할 수 있다. 이 경우 신규 tint 단계의 PASS와 별도 Client Debug build를 증거로 분리하고 서버 프로세스는 종료하지 않는다.

## 4. 합격 기준

- LeeSin W1 5개 emitter가 마지막 정상 초록·청록·금색 RGB를 사용한다.
- Riven E의 현재 6개 emitter가 일관된 초록 shield RGB를 사용하며 lifetime/lifecycle은 유지한다.
- 전체 저장소 WFX가 JSON/emitters/color RGBA shape 검사를 통과한다.
- 보호 tint 11개 중 하나라도 흰색 또는 다른 값으로 바뀌면 자동 테스트가 실패한다.
- 이력 감사상 다른 chromatic-to-gray 전이는 없음을 RESULT에 기록한다.
- Client Debug/Release build가 성공하고 실행 중인 서버는 유지된다.
- 새 Release 클라이언트 또는 WFX preview에서 LeeSin W1과 Riven E가 흰색이 아닌 chromatic green으로 보인다. 화면 확인을 수행하지 못하면 자동 검증과 시각 검증을 분리해 미종결 상태로 기록한다.

## 5. 독립 서브 에이전트 비평

- 1차 `/root/replay_plan_critique`: P0=0, P1=3, P2=2, FAIL.
  - 수용: 과거 흰색이 의도된 결정이었음을 정정하고 최신 사용자의 LeeSin·Riven 초록 팔레트 명시가 이를 supersede한다고 기록했다.
  - 수용: runtime에서 optional인 일반 emitter `name`/`color`를 전수 강제하지 않는다. emitter object와 존재하는 color shape만 검사하고, 보호 asset에서만 이름·RGBA·중복을 엄격히 검사한다.
  - 수용: Python `bool` 제외, 보호 emitter 중복 검출, Release 화면 수동 gate를 추가했다.
- 2차 `/root/replay_plan_critique`: P0=0, P1=0, PASS. 최신 팔레트 권위, optional runtime shape, protected-only exact gate, Debug 서버 보존과 Release 화면 gate를 확인했다.
