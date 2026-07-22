Session - Projectile Yaw Root Fix and Champion WFX Pipeline

## 1. 반영해야 하는 코드

### 본질 원인

`CFxCuePlayer`가 WFX mesh rotation을 적용한 뒤에도 복제 발사체는 `EventApplier::EnsureProjectilePresentation`에서 방향 yaw와 `ProjectileVisualDesc::fYawOffset`으로 다시 덮어쓴다. 따라서 WFX만 0 또는 임의 각도로 바꾼 이전 수정은 최종 렌더 경로에 반영되지 않은 실패다. 정상인 칼리스타 BA/Q가 카탈로그에 `PI`를 가진 것과 같은 방식으로 최종 소유자를 수정한다.

### 기존 파일: `Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp`

```cpp
constexpr ProjectileVisualDesc kEzrealGlobalBeamVisual{
    "Ezreal.R.Missile", "Ezreal.R.Hit", nullptr, nullptr, nullptr, nullptr,
    WintersMath::kPi
};
constexpr ProjectileVisualDesc kAsheBasicAttackVisual{
    "Ashe.BA.Arrow", nullptr, "Ashe.BA.Hit", nullptr, nullptr, nullptr,
    -WintersMath::kPi * 0.5f
};
constexpr ProjectileVisualDesc kAsheVolleyArrowVisual{
    "Ashe.W.Arrow", "Ashe.W.Hit", nullptr, nullptr, nullptr, nullptr,
    -WintersMath::kPi * 0.5f
};
constexpr ProjectileVisualDesc kAsheHawkshotVisual{
    "Ashe.E.Hawkshot", nullptr, nullptr, nullptr, nullptr, nullptr,
    -WintersMath::kPi * 0.5f
};
constexpr ProjectileVisualDesc kAsheCrystalArrowVisual{
    "Ashe.R.Arrow", "Ashe.R.Hit", nullptr, nullptr, nullptr, nullptr,
    -WintersMath::kPi * 0.5f
};
```

### 기존 파일: `Data/LoL/FX/Champions/Ashe/*.wfx`

복제 최종 카탈로그와 local/non-replicated 재생을 일치시킨다.

```json
{"BA/W scale":0.0105,"E scale":0.021,"R scale":0.0315,"mesh_rotation_y":-1.5708}
```

### 기존 파일: `Data/LoL/FX/Champions/Ezreal/q_projectile.wfx`

기존 mesh/head/trail 조합을 삭제하고 `ba_cast.wfx`의 whisp 텍스처를 폭 `0.50`, 높이 `2.06`으로 3장 겹친다.

```json
{"schema":"WintersWfx","version":1,"name":"Ezreal.Q.Projectile","emitters":[{"name":"q_ba_whisp_left","render_type":"Billboard","blend_mode":"Additive","depth_mode":"DepthTestWriteOff","texture":"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_ba_whisps.png","lifetime":0.62,"start_delay":0.00,"fade_in":0.01,"fade_out":0.22,"width":0.50,"height":2.06,"color":[0.45,1.25,1.45,0.62],"attach_offset":[-0.10,1.08,0.02],"billboard":true,"blockable_by_wind_wall":true,"alpha_clip":0.015},{"name":"q_ba_whisp_center","render_type":"Billboard","blend_mode":"Additive","depth_mode":"DepthTestWriteOff","texture":"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_ba_whisps.png","lifetime":0.62,"start_delay":0.025,"fade_in":0.01,"fade_out":0.22,"width":0.50,"height":2.06,"color":[1.20,0.96,0.30,0.72],"attach_offset":[0.00,1.08,0.10],"billboard":true,"blockable_by_wind_wall":true,"alpha_clip":0.015},{"name":"q_ba_whisp_right","render_type":"Billboard","blend_mode":"Additive","depth_mode":"DepthTestWriteOff","texture":"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_ba_whisps.png","lifetime":0.62,"start_delay":0.05,"fade_in":0.01,"fade_out":0.22,"width":0.50,"height":2.06,"color":[0.45,1.25,1.45,0.62],"attach_offset":[0.10,1.08,0.02],"billboard":true,"blockable_by_wind_wall":true,"alpha_clip":0.015}]}
```

### 새 파일: `Data/LoL/FX/Champions/Fiora/ba_hit.wfx`

```json
{"schema":"WintersWfx","version":1,"name":"Fiora.BA.Hit","emitters":[{"name":"ba_hit_spark","render_type":"Billboard","blend_mode":"Additive","depth_mode":"DepthTestWriteOff","texture":"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_e_hit_spark_yellow.png","lifetime":0.40,"fade_in":0.01,"fade_out":0.18,"width":1.40,"height":1.40,"color":[1.10,0.95,0.45,1.00],"attach_offset":[0.00,1.00,0.00],"billboard":true}]}
```

### 새 파일: `Data/LoL/FX/Champions/Fiora/q_cast.wfx`

```json
{"schema":"WintersWfx","version":1,"name":"Fiora.Q.Cast","emitters":[{"name":"q_slash","render_type":"Billboard","blend_mode":"Additive","depth_mode":"DepthTestWriteOff","texture":"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_q_slash.png","lifetime":0.40,"fade_in":0.01,"fade_out":0.18,"width":2.40,"height":1.60,"color":[0.95,0.85,0.55,1.00],"attach_offset":[0.00,1.00,0.00],"billboard":true},{"name":"q_sword_glow","render_type":"Billboard","blend_mode":"Additive","depth_mode":"DepthTestWriteOff","texture":"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_q_swordglow.png","lifetime":0.30,"start_delay":0.02,"fade_in":0.01,"fade_out":0.14,"width":1.40,"height":1.40,"color":[1.00,0.95,0.70,1.00],"attach_offset":[0.00,1.10,0.20],"billboard":true}]}
```

### 새 파일: `Data/LoL/FX/Champions/Fiora/w_cast.wfx`

```json
{"schema":"WintersWfx","version":1,"name":"Fiora.W.Cast","emitters":[{"name":"w_block_glow","render_type":"Billboard","blend_mode":"AlphaBlend","depth_mode":"DepthTestWriteOff","texture":"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_w_block_glow.png","lifetime":1.50,"fade_in":0.04,"fade_out":0.40,"width":2.00,"height":2.00,"color":[1.10,0.90,0.40,0.85],"attach_offset":[0.00,1.00,0.00],"billboard":true},{"name":"w_block_flash","render_type":"Billboard","blend_mode":"Additive","depth_mode":"DepthTestWriteOff","texture":"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_w_block_flash.png","lifetime":0.34,"fade_in":0.01,"fade_out":0.20,"width":2.60,"height":2.60,"color":[1.40,1.20,0.60,1.00],"attach_offset":[0.00,1.20,0.00],"billboard":true}]}
```

### 새 파일: `Data/LoL/FX/Champions/Fiora/e_buff.wfx`

```json
{"schema":"WintersWfx","version":1,"name":"Fiora.E.Buff","emitters":[{"name":"e_buff","render_type":"Billboard","blend_mode":"Additive","depth_mode":"DepthTestWriteOff","texture":"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_e_buff_mult_yellow.png","lifetime":5.00,"fade_in":0.05,"fade_out":0.50,"width":1.60,"height":1.60,"color":[1.00,0.85,0.30,0.90],"attach_offset":[0.00,1.30,0.00],"billboard":true},{"name":"e_sword_glow","render_type":"Billboard","blend_mode":"Additive","depth_mode":"DepthTestWriteOff","texture":"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_e_sword_glow.png","lifetime":5.00,"fade_in":0.05,"fade_out":0.50,"width":0.70,"height":2.20,"color":[1.20,0.95,0.35,0.78],"attach_offset":[0.00,1.15,0.15],"billboard":true}]}
```

### 새 파일: `Data/LoL/FX/Champions/Fiora/r_mark.wfx`

```json
{"schema":"WintersWfx","version":1,"name":"Fiora.R.Mark","emitters":[{"name":"r_crest_glow","render_type":"Billboard","blend_mode":"Additive","depth_mode":"DepthTestWriteOff","texture":"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_crest_glow.png","lifetime":8.00,"fade_in":0.08,"fade_out":0.60,"width":1.00,"height":1.40,"color":[1.20,0.90,0.30,1.00],"attach_offset":[0.00,2.50,0.00],"billboard":true}]}
```

### 새 파일: `Data/LoL/FX/Champions/Fiora/r_heal.wfx`

```json
{"schema":"WintersWfx","version":1,"name":"Fiora.R.Heal","emitters":[{"name":"r_heal_zone","render_type":"Billboard","blend_mode":"AlphaBlend","depth_mode":"DepthTestWriteOff","texture":"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_healzone.png","lifetime":4.00,"fade_in":0.10,"fade_out":0.80,"width":6.00,"height":6.00,"yaw":0.00,"color":[0.85,1.00,0.55,0.70],"attach_offset":[0.00,0.05,0.00],"billboard":false}]}
```

### 기존 파일: `Client/Private/GameObject/Champion/Fiora/Fiora_FxPresets.cpp`

직접 billboard component를 만드는 경로를 `CFxCuePlayer::PlayAll`로 교체하여 위 WFX가 실제 런타임 경로가 되게 한다.

```cpp
FxCueContext cue{};
cue.attachTo = owner;
cue.vWorldPos = ResolvePosition(world, owner);
cue.vForward = ResolveForward(direction);
cue.bOverrideLifetime = lifetime > 0.f;
cue.fLifetimeOverride = lifetime;
CFxCuePlayer::PlayAll(world, cueName, cue, nullptr);
```

## 2. 검증

```powershell
python -m json.tool Data/LoL/FX/Champions/Ezreal/q_projectile.wfx
Get-ChildItem Data/LoL/FX/Champions/Fiora/*.wfx | ForEach-Object { python -m json.tool $_.FullName > $null }
msbuild Client/Include/Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64
```

인게임 gate는 애쉬 BA/W/E/R 진행 방향, 이즈리얼 R 방향, 이즈리얼 Q 3겹, 피오라 BA/Q/W/E/R cue 가시성이다.
