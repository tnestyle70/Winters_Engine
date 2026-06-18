# HKX Animation To Winters `.wanim` Pipeline

## 목적

Elden Ring 캐릭터 애니메이션은 HKX(Havok) 포맷이라 FLVER 메시 파이프라인만으로는 `.wanim`이 나오지 않는다.
이 문서는 anibnd HKX를 Soulstruct Blender add-on으로 디코딩해 Winters `.wanim`까지 연결하는 샘플 파이프라인을 기록한다.

## 체인

```text
chr/<id>.anibnd.dcx
  -> DivBinder (skeleton.hkx + *.compendium + a*.hkx, _divXX 자동 병합)
  -> Soulstruct read_skeleton_hkx_entry / read_animation_hkx_entry
  -> SoulstructAnimation.new_from_hkx_animation (Blender Action 생성)
  -> Blender FBX export (Armature only, bake all actions, leaf bone off)
  -> WintersAssetConverter anim <fbx> --skel <cooked wskel> -o <Animation dir>
  -> converter info 검증 (skel_hash 일치)
```

핵심 결정:

| 결정 | 이유 |
|---|---|
| 아마추어 소스 = run-full이 cook한 chrbnd FBX 재임포트 | `.wskel`을 만든 것과 동일한 본 계층이라 skel hash가 일치한다 |
| `DivBinder` 사용 | ER `_div00/_div01` 보조 anibnd와 compendium을 자동 병합한다 |
| Blender 오퍼레이터 대신 `SoulstructAnimation` API 직접 호출 | headless `--factory-startup --background`에서 poll/팝업 의존성을 제거한다 |
| spline-compressed HKX는 `to_interleaved_hkx()` 경유 | Soulstruct 내부에서 자동 처리, 단 변환 시간이 든다 |

## 명령

```bat
python Tools\EldenAssetPipeline\elden_pipeline.py convert-hkx-anim ^
  --game-root "C:\Program Files (x86)\Steam\steamapps\common\ELDEN RING\Game" ^
  --resource-root "Client\Bin\Resource\EldenRing" ^
  --work-root "C:\Users\tnest\Desktop\EldenRingExtract\_full_pipeline" ^
  --blender "C:\Users\tnest\Downloads\blender-4.2.18-windows-x64\blender-4.2.18-windows-x64\blender.exe" ^
  --converter "Tools\Bin\Debug\WintersAssetConverter.exe" ^
  --character c2060 --character c2010 ^
  --max-anims 6 ^
  --out "C:\Users\tnest\Desktop\EldenRingExtract\_full_pipeline\runs\hkx_anim_sample.json"
```

전제 조건:

1. 대상 캐릭터의 chrbnd가 `run-full-pipeline`으로 먼저 cook되어 있어야 한다.
   (`FullGame/chr/character-binder/chr_<id>.chrbnd/Model/`에 FBX + `.wskel` 존재)
2. Soulstruct add-on 루트는 `--soulstruct-root`로 지정 (기본값: Downloads의 io_soulstruct 2.5.0).

산출 위치:

```text
Client/Bin/Resource/EldenRing/FullGame/chr/character-binder/chr_<id>.chrbnd/Animation/*.wanim
```

## 합격 기준

| 항목 | 기준 |
|---|---|
| Blender HKX import | `animsImported > 0`, 실패 엔트리는 `failedEntries`에 기록 |
| FBX export | armature-only, bake_anim, all actions |
| `.wanim` 생성 | `wanimCount > 0` |
| skel hash | 첫 `.wanim`의 `skel_hash` == cooked `.wskel`의 `hash` (`validation.skelHashMatches`) |
| channels | 본 수와 같은 차수의 채널 수 (`validation.channels`) |

## 검증 결과 (2026-06-11 샘플)

| 캐릭터 | HKX import | `.wanim` | channels | skel hash 일치 |
|---|---|---|---:|---|
| c2060 (139 bones) | 6/6 | 6개 생성 | 137 | `0x9acb9793ac7a1853` 일치 |
| c2010 (356 bones) | 6/6 | 6개 생성 | 357 | `0x58c08123463ddd91` 일치 |

c2060은 현 스킨드 셰이더 한계(256 bones) 안이라 `wmesh + wskel + wanim + wmat + DDS` 전 체인이 런타임 사용 가능한 첫 캐릭터다.

주의: Blender armature-only FBX export는 Assimp가 거부한다. 반드시 메시를 포함해 export한다
(`object_types={"ARMATURE","MESH"}`) — 레거시 `anim3010.fbx`(mesh 1 포함)와 같은 구조.

## 알려진 한계

1. `c0000`(플레이어)은 sub-ANIBND(`c0000_a0x.anibnd`) 구조라 이 샘플 명령이 아직 다루지 않는다.
2. Root motion은 Soulstruct가 아마추어 오브젝트 레벨에 적용하며, FBX bake 시 본 단위 root motion 트랙으로는 분리되지 않는다.
3. HKX 스켈레톤의 `Twist` 본 등 FLVER 아마추어에 없는 본의 트랙은 경고 후 폐기된다(정상 동작).
4. spline-compressed 애니메이션 디코딩은 순수 Python이라 클립당 수 초가 걸린다. 전 캐릭터 배치는 시간 예산이 필요하다.
5. TAE 이벤트(콤보 windows, 사운드 트리거)는 아직 `.wanim` 이벤트로 변환하지 않는다 — `AnimationRaw/`에 수집된 `.tae`가 소스다.

## 다음 단계

1. 샘플 → 배치: cook된 전 캐릭터에 `--max-anims 0`으로 확장, run-full과 같은 resume/status 구조 추가.
2. `c0000` sub-ANIBND 분기 지원.
3. TAE 파서로 `.wanim` 이벤트 채우기.
4. 런타임: `CModel` wskel+wanim 로드 후 bind pose 탈출 확인 (`02` 문서 7단계 기준 재사용).
