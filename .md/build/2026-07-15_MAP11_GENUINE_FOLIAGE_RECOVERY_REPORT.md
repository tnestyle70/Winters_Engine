# GEOMETRY PASS / COLOR INCOMPLETE

이 보고서의 803개 genuine foliage mesh 복구와 Stage `B0` 판정은 유효하다. 다만 이 단계는 `VertexDeform_inst`의 diffuse 연결까지만 복구했으며, Riot 재질의 `USE_GRASS_TINT_MAP`와 Map11 `GrassTint_SRX` 공간 색상 단계를 복구하지 못했다. 따라서 geometry 복구를 color 복구 완료로 해석하면 안 된다. 색상 교정은 `2026-07-15_ASHE_PROJECTILE_ZERO_YAW_GRASS_TINT_CORRECTIVE_REPORT.md`를 따른다.

# Session - Map11 Genuine Foliage Recovery

Date: 2026-07-15

## Outcome

- S003의 PNG billboard/windgrass crossed-card 방식은 **시각적 실패이며 정상 F5 대안이 아니다**. S004의 퇴출 결정을 유지한다.
- Stage1은 현재 `v5/B0`이다. 이전 billboard bush 엔트리는 다시 들어오지 않았다.
- 블루 우물 기준으로 보이던 흰 foliage 문제는 S004에서 해결된 것이 아니었다. 정상 맵 WMesh가 GLB 노드 배치와 `VertexDeform` 재질 연결을 잃어, foliage mesh를 원점 부근에 흰 기본 텍스처로 그릴 수 있는 상태였다.
- 진짜 bush/foliage geometry는 별도 FBX가 아니라 이미 `sr_base_flip.glb`에 있다. Direct Children의 `VertexDeform` foliage 803개가 대상이며, `base.materials.bin`의 실제 연결은 `VertexDeform_inst -> ASSETS/Maps/KitPieces/SRX/textures/SRU_Brush.tex`이다.
- 기존 `base.mapgeo`, `base.materials.bin`, GLB가 온전하므로 Obsidian 재추출이나 FBX 우회는 첫 단계가 아니다.

## Implemented Boundary

- `WintersAssetConverter mesh --pretransform`
  - Assimp `aiProcess_PreTransformVertices`와 `AI_CONFIG_PP_PTV_KEEP_HIERARCHY`를 map static cook에만 선택적으로 적용한다.
  - 스킨/애니메이션 cook 기본 경로에는 적용하지 않는다.
- `--material-remap <material>=<runtime-path>`
  - Engine writer는 제품 경로를 알지 않고, map cook script가 `VertexDeform_inst`를 `sru_brush.png`에 연결한다.
- `--exclude-material <name>`
  - 높이 샘플러용 `sr_base_flip_surface.wmesh`에서 시각 foliage만 제외한다.
- 런타임 정의
  - `baseMapMesh = Texture/MAP/output/sr_base_flip.wmesh`
  - `baseMapSurface = Texture/MAP/output/sr_base_flip_surface.wmesh`
- 회귀 검사
  - `Tools/audit_map11_foliage.py`가 foliage 배치 범위, empty diffuse 사용, surface 분리, Stage B0를 함께 검사한다.

## Cooked Runtime Evidence

| Asset | Size | SHA-256 |
|---|---:|---|
| `sr_base_flip.wmesh` | 42,126,580 | `67CD8062E147B0E6ED4917A75353A97E68CD93692F8BB15C016E6D126FA2607D` |
| `sr_base_flip.wmat` | 756,348 | `41CE17715D5E002CB0C531793641234E018F3F65ED0B4244992A1095E0B57B30` |
| `sr_base_flip_surface.wmesh` | 21,131,192 | `88A681D77AF3C6CCFF4F1851833A893B400B969C1474CE1E810CF0116914DEED` |
| `sr_base_flip_surface.wmat` | 756,348 | `81A3FC26318B9E71E8F128464CFFBDB2EAA0A649125DA9B2BC1559962E400D68` |

Audit result:

```text
PASS visual=S1080/V729060/I1759152 foliage=803 span=(13461.4,13449.7) surface=S277/V375147/I774927 stage=v5/B0
```

Interpretation:

- visual foliage 803개가 X/Z 각각 약 13.4k 범위로 펼쳐져 노드 배치가 bake됐다.
- visual map에서 사용 중인 empty diffuse submesh는 0개다.
- surface map은 foliage 803개만 제외한 277 submesh다.
- Stage 기반 billboard/독립 bush renderer는 정상 F5에서 계속 B0다.

## Verification

```powershell
& Tools/convert_all_assets.ps1 -Mode maps
py -3 Tools/audit_map11_foliage.py
py -3 Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
```

- converter Debug x64 build: PASS
- Client Debug x64 build: PASS (기존 DLL interface/sprintf warning은 남아 있으나 error 없음)
- map cook 재실행: `UP-TO-DATE`, `FAIL=0`
- definition pack check: PASS, `0x10774DA5`
- targeted `git diff --check`: PASS

## Visual Gate / Handoff

서버가 떠 있지 않은 환경에서 자동 인게임 눈 검증은 완료하지 않았다. 사용자가 정상 서버/F5 흐름에서 직접 확인한다.

확인 순서:

1. 블루 우물 주변 원점성 흰 풀 덩어리가 사라졌는지 본다.
2. 탑/미드/봇/강가 bush가 카메라를 돌려도 단일 billboard가 아니라 여러 방향의 실제 mesh 실루엣으로 유지되는지 본다.
3. bush 통과 시 챔피언 Y와 이동 경로가 잎 표면으로 튀지 않는지 본다.
4. 전체 맵 좌우 방향과 구조물 정렬이 이전 정상 배치와 맞는지 본다.
5. 기대와 다르면 Obsidian/FBX로 돌아가기 전에 이 report의 audit 출력과 해당 위치 화면 캡처를 먼저 비교한다.
