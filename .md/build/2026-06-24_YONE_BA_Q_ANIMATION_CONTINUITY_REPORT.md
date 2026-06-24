# Yone BA/Q Animation Continuity Report

Date: 2026-06-24
Plan: `.md/plan/2026-06-24_YONE_BA_Q_ANIMATION_CONTINUITY_PLAN.md`

## 1. 작업 결과

### 원인 판정

Anim Mesh Binary Data 추출 누락 가능성은 낮다.

검증 근거:

- `Yone.wskel`
  - bones: 172
  - hash: `0xda105f3000f5b6cc`
- `skinned_mesh_yone_attack1.wanim`
  - channels: 169
  - duration: `0.333s`
  - keys: 4563
  - skel_hash: `0xda105f3000f5b6cc`
- `skinned_mesh_yone_spell1_a1.wanim`
  - channels: 169
  - duration: `0.233s`
  - keys: 3042
  - skel_hash: `0xda105f3000f5b6cc`

실제 원인은 짧은 요네 BA/Q 액션 클립을 클라이언트 네트워크 로코모션 상태 머신이 서버 action lock `0.9s` 동안 붙잡는 구조였다. 기존 이벤트 재생 경로는 생성 데이터의 `animPlaySpeed = 0.85` 대신 레거시 등록값 `1.0`을 사용해 이 현상을 더 크게 만들었다.

### 코드 반영

반영 파일:

- `Client/Private/Network/Client/EventApplier.cpp`
- `Client/Private/Scene/Scene_InGameNetwork.cpp`
- `Client/Private/GameObject/Champion/Yone/Yone_Registration.cpp`

반영 내용:

- 서버 액션 애니메이션 재생 속도는 `ChampionGameDataDB` 생성 데이터를 우선 사용한다.
- 네트워크 액션 시각화 타이머는 `min(serverLockSec, animationDurationSec / playSpeed)`로 계산한다.
- Jax E 같은 loop action은 기존 lock 기준을 유지한다.
- 요네 BA/Q 레거시 등록값을 generated timing과 맞췄다.
  - BA/Q `lockDurationSec = 0.9f`
  - BA/Q `animPlaySpeed = 0.85f`
- 요네 BA/Q 후속 전환 클립을 짧게 연결했다.
  - BA: `attack1_toidle1`, `attack1_towalk1`, `0.18f`
  - Q run: `spell1a_towalk1`, `0.16f`

## 2. 검증 결과

### WAnim/Skeleton Evidence

Commands:

```powershell
Tools\Bin\Debug\WintersAssetConverter.exe info Client\Bin\Resource\Texture\Character\Yone\Yone.wskel
Tools\Bin\Debug\WintersAssetConverter.exe info Client\Bin\Resource\Texture\Character\Yone\anims\skinned_mesh_yone_attack1.wanim
Tools\Bin\Debug\WintersAssetConverter.exe info Client\Bin\Resource\Texture\Character\Yone\anims\skinned_mesh_yone_spell1_a1.wanim
```

Result:

- PASS
- `.wskel`과 BA/Q `.wanim`의 skeleton hash가 모두 `0xda105f3000f5b6cc`로 일치했다.
- BA/Q `.wanim`에 channel/key/duration 정보가 존재했다.

### Definition Pack Freshness

Command:

```powershell
python Tools\LoLData\Build-LoLDefinitionPack.py --check
```

Result:

- PASS
- Definition pack: `0x42EA0952`
- Champions: 17, skills: 85, summoner spells: 1

### Whitespace

Command:

```powershell
git diff --check
```

Result:

- PASS
- 기존 작업트리의 LF/CRLF 경고만 출력됐고 whitespace error는 없었다.

### Client Debug x64 Build

Command:

```powershell
MSBuild Client\Include\Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

Result:

- PASS
- Output: `Client/Bin/Debug/WintersGame.exe`
- 기존 `ISystem` DLL interface warning 계열은 남아 있으나, 이번 변경 파일은 컴파일/링크를 통과했다.

## 3. 수동 확인 항목

1. 요네 BA/Q 입력 시 액션 클립이 서버 lock 끝까지 마지막 프레임에 멈춰 보이지 않는지 확인한다.
2. 요네 Q 후 이동 중이면 `spell1a_towalk1` 전환 후 run으로 복귀하는지 확인한다.
3. BA/Q damage, FX cue, cooldown은 기존 서버 권위 결과와 동일한지 확인한다.
