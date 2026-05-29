# Yasuo WFX Pipeline And Tuning Guide

## Goal

Yasuo skill visuals should be authored as data-first WFX cues. Gameplay and cue names stay in code, while shape, color, timing, and layer composition live in `Data/LoL/FX/Champions/Yasuo/*.wfx`.

## Runtime Path

`YasuoFxPresets.cpp` plays these cue names:

- `Yasuo.Q.Slash` -> `q_slash.wfx`
- `Yasuo.Q.BuildUp` -> `q_build_up.wfx`
- `Yasuo.Q.Tornado` -> `q_tornado.wfx`
- `Yasuo.W.WindWall` -> `w_windwall.wfx`
- `Yasuo.E.DashTrail` -> `e_dash_trail.wfx`
- `Yasuo.EQ.Ring` -> `eq_ring.wfx`
- `Yasuo.EQ.InnerWind` -> `eq_inner_wind.wfx`
- `Yasuo.R.LandImpact` -> `r_land_impact.wfx`
- `Yasuo.R.SwordGlow` -> `r_sword_glow.wfx`

The cue registry loads WFX files from `Data/LoL/FX`, and texture/model paths in WFX should point at runtime resources under `Client/Bin/Resource`.

## Source References

Use screenshots from:

`Client/Bin/Resource/Texture/UI/이펙트 이미지/Yasuo`

Use actual effect resources from:

`Client/Bin/Resource/Texture/Character/Yasuo/particles`

Do not use the screenshot folder as runtime effect texture input. It is only visual reference.

## Layering Rules

- Q slash: short forward sword arc, thin blue slash, ground dirt/rock flash, hit spark feel.
- Q build-up: small hand/sword wind charge and passive wind wrap.
- Q tornado: moving wind column with mesh blade spin, ground swirl, vertical soft column, pale cyan/blue tint.
- W wind wall: broad wall mesh, wispy vertical sheet, top/side support layers, ground crack/dust.
- E dash: afterimage, black/white dash streak, smoke, ring, timer flash.
- EQ: circular slash ring, black slash underside, fill circle, inner wind column.
- R: land ring/ground crack/blast plus attached sword wind glow and hit streaks.

## Tuning Knobs

- Size: edit `width`, `height`, or mesh `scale`.
- Duration: edit `lifetime`, `fade_in`, `fade_out`, `start_delay`.
- Directional placement: edit `attach_offset`; Z is forward from the cue context.
- Brightness: keep additive RGB mostly <= 1.0 to avoid white saturation.
- Ground effects: use `GroundDecal` and `billboard: false`.
- Forward slash/beam effects: use `Billboard` with `billboard: false` or `Beam`.
- Tornado/sword/wall meshes: use `MeshParticle` and tune `scale`, `rotation`, `world_yaw_spin_speed`.

## Validation

- Run `git diff --check`.
- Build Client Debug x64.
- In game, check each skill once before tuning values.
- If a layer is invisible, first verify texture/model path, then size, then alpha/color.
