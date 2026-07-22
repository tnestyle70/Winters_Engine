#!/usr/bin/env python3
import argparse
import json
import math
from pathlib import Path


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def require_close(actual: float, expected: float, message: str) -> None:
    require(math.isclose(actual, expected, rel_tol=0.0, abs_tol=1.0e-5),
            f"{message}: expected={expected} actual={actual}")


def champion_by_name(document, name: str):
    return next(row for row in document["champions"] if row["champion"] == name)


def skill_by_slot(champion, slot: int):
    return next(row for row in champion["skills"] if row["slot"] == slot)


def effect_by_key(document, key: str):
    return next(row for row in document["skillEffects"] if row["key"] == key)


def require_list(actual, expected, message: str) -> None:
    require(len(actual) == len(expected), f"{message}: length mismatch")
    for index, (actual_value, expected_value) in enumerate(zip(actual, expected)):
        require_close(float(actual_value), float(expected_value), f"{message}[{index}]")


def respawn_factor(game_time_sec: float) -> float:
    if game_time_sec <= 900.0:
        return 0.0
    if game_time_sec <= 1800.0:
        return min(0.1275, math.floor((game_time_sec - 900.0) / 30.0) * 0.00425)
    if game_time_sec <= 2700.0:
        return min(0.2175, 0.1275 + math.floor((game_time_sec - 1800.0) / 30.0) * 0.003)
    return min(0.5, 0.2175 + math.floor((game_time_sec - 2700.0) / 30.0) * 0.0145)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    args = parser.parse_args()
    root = Path(args.root).resolve()

    champions = load_json(root / "Data/Gameplay/ChampionGameData/champions.json")
    effects = load_json(root / "Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json")
    generated_skills = load_json(root / "Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json")
    items = load_json(root / "Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json")
    spawn = load_json(root / "Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json")
    economy = load_json(root / "Data/LoL/ServerPrivate/Gameplay/EconomyGameplayDefs.json")
    layout = load_json(root / "Client/Bin/Resource/UI/hud_irelia_layout.json")

    runtime_overlay = (root / "Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp").read_text(
        encoding="utf-8")
    damage_parser = runtime_overlay.split("bool_t TryParseDamageFormula(", 1)[1].split(
        "bool_t TryOverlayUnsigned(", 1)[0]
    for metadata_field in ("cppType", "cppFlags", "rankCount"):
        quoted_field = f'"{metadata_field}"'
        require(damage_parser.count(quoted_field) == 1,
                f"runtime damage parser compatibility key {metadata_field}")
        for access in (f'node["{metadata_field}"]',
                       f'node.value("{metadata_field}"',
                       f'node.contains("{metadata_field}")'):
            require(access not in damage_parser,
                    f"runtime damage parser must ignore metadata {metadata_field}")
    require('node.value("type"' in damage_parser and 'node["flags"]' in damage_parser,
            "runtime damage parser keeps authoring type/flags authoritative")

    item_3153 = next(row for row in items["items"] if row["itemId"] == 3153)
    item_3153_damage = item_3153["onHitDamage"]
    require(item_3153_damage["type"] == "Physical" and
            item_3153_damage["flags"] == [] and
            item_3153_damage["cppType"] == "Physical" and
            item_3153_damage["cppFlags"] == [] and
            item_3153_damage["rankCount"] == 1,
            "item 3153 damage metadata fixture")
    for field in ("flatByRank", "totalAdRatioByRank", "bonusAdRatioByRank",
                  "apRatioByRank", "targetMaxHpRatioByRank",
                  "targetMissingHpRatioByRank"):
        require(len(item_3153_damage[field]) == 1,
                f"item 3153 damage rank count {field}")

	# F4's champion selector is valid only when every canonical champion has a
	# unique 0..4 loadout and an exactly matching skill effect key.
    champion_tokens = set()
    effect_keys = [row["key"] for row in effects["skillEffects"]]
    require(len(effect_keys) == len(set(effect_keys)), "skill effect keys are unique")
    effect_key_set = set(effect_keys)
    for champion in champions["champions"]:
        token = champion["champion"].lower()
        require(token not in champion_tokens, f"duplicate champion token: {token}")
        champion_tokens.add(token)
        slots = [int(skill["slot"]) for skill in champion["skills"]]
        require(sorted(slots) == [0, 1, 2, 3, 4],
                f"{champion['champion']} slots 0..4 exactly once")
        for slot, suffix in enumerate(("basic_attack", "q", "w", "e", "r")):
            key = f"skill.{token}.{suffix}"
            require(key in effect_key_set, f"missing effect key: {key}")
            skill = skill_by_slot(champion, slot)
            rank_count = 1 if slot == 0 else (3 if slot == 4 else 5)
            if "cooldownSecByRank" in skill:
                require(len(skill["cooldownSecByRank"]) == rank_count,
                        f"{key} cooldown rank count")
            else:
                require(isinstance(skill.get("cooldownSec"), (int, float)),
                        f"{key} scalar cooldown fallback")
            require(isinstance(skill.get("rangeMax"), (int, float)) and
                    0.0 <= float(skill["rangeMax"]) <= 500.0,
                    f"{key} canonical range")
            damage = effect_by_key(effects, key)["damage"]
            for field in ("flatByRank", "totalAdRatioByRank", "bonusAdRatioByRank",
                          "apRatioByRank", "targetMaxHpRatioByRank",
                          "targetMissingHpRatioByRank"):
                require(len(damage[field]) == rank_count,
                        f"{key}.{field} rank count")

    # Balance numbers are authoring inputs, not immutable fixtures. Verify that
    # the cooked runtime pack mirrors the current canonical JSON instead of
    # treating a valid F4 edit as a regression.
    generated_by_key = {row["key"]: row for row in generated_skills["skills"]}
    require(len(generated_by_key) == len(generated_skills["skills"]),
            "generated skill keys are unique")
    damage_fields = ("flatByRank", "totalAdRatioByRank", "bonusAdRatioByRank",
                     "apRatioByRank", "targetMaxHpRatioByRank",
                     "targetMissingHpRatioByRank")
    for champion in champions["champions"]:
        token = champion["champion"].lower()
        for slot, suffix in enumerate(("basic_attack", "q", "w", "e", "r")):
            key = f"skill.{token}.{suffix}"
            canonical = skill_by_slot(champion, slot)
            generated = generated_by_key.get(key)
            require(generated is not None, f"generated skill missing: {key}")
            rank_count = 1 if slot == 0 else (3 if slot == 4 else 5)
            canonical_cooldown = canonical.get("cooldownSecByRank")
            if canonical_cooldown is None:
                canonical_cooldown = [canonical["cooldownSec"]] * rank_count
            require_list(generated["cooldown"]["secondsByRank"], canonical_cooldown,
                         f"{key} canonical/generated cooldown parity")
            require_close(float(generated["range"]["maximum"]),
                          float(canonical.get("rangeMax", 0.0)),
                          f"{key} canonical/generated range parity")
            canonical_damage = effect_by_key(effects, key)["damage"]
            generated_damage = generated["effect"]["damage"]
            require(generated_damage["type"] == canonical_damage["type"],
                    f"{key} canonical/generated damage type parity")
            require(generated_damage["flags"] == canonical_damage["flags"],
                    f"{key} canonical/generated damage flags parity")
            for field in damage_fields:
                require_list(generated_damage[field], canonical_damage[field],
                             f"{key}.{field} canonical/generated parity")

            canonical_params = effect_by_key(effects, key).get("params", {})
            generated_params = {
                row["id"]: row.get("values", row.get("value"))
                for row in generated["effect"].get("params", [])
            }
            require(set(generated_params) == set(canonical_params),
                    f"{key} canonical/generated param keys parity")
            for param_name, canonical_value in canonical_params.items():
                generated_value = generated_params[param_name]
                if isinstance(canonical_value, list):
                    require(isinstance(generated_value, list),
                            f"{key}.{param_name} generated ranked shape")
                    require_list(generated_value, canonical_value,
                                 f"{key}.{param_name} canonical/generated parity")
                else:
                    require(not isinstance(generated_value, list),
                            f"{key}.{param_name} generated scalar shape")
                    require_close(generated_value, canonical_value,
                                  f"{key}.{param_name} canonical/generated parity")

    ranked_variant_rank_counts = {
        ("skill.yasuo.q", "tornadoDamage"): 5,
        ("skill.yasuo.q", "dashAreaDamage"): 5,
        ("skill.leesin.q", "baseDamage"): 5,
        ("skill.kalista.e", "damagePerSpear"): 5,
        ("skill.ezreal.r", "nonEpicBaseDamage"): 3,
    }
    for (key, param_name), rank_count in ranked_variant_rank_counts.items():
        values = effect_by_key(effects, key)["params"][param_name]
        require(isinstance(values, list) and len(values) == rank_count,
                f"{key}.{param_name} ranked variant shape")
        require(all(isinstance(value, (int, float)) and
                    math.isfinite(float(value)) and float(value) >= 0.0
                    for value in values),
                f"{key}.{param_name} ranked variant domain")
    require("baseDamage" not in effect_by_key(effects, "skill.yasuo.q")["params"],
            "Yasuo Q1/Q2 uses canonical flat owner")

    fiora_w_effect = effect_by_key(effects, "skill.fiora.w")
    sylas_q_effect = effect_by_key(effects, "skill.sylas.q")
    sylas_w_effect = effect_by_key(effects, "skill.sylas.w")
    require("range" not in fiora_w_effect["params"],
            "Fiora W has one canonical range owner")
    require(sylas_q_effect["damage"]["type"] == "Magic",
            "Sylas Q magic damage")
    require(sylas_w_effect["damage"]["type"] == "Magic",
            "Sylas W magic damage")

    respawn = spawn["spawnLoadout"]["respawnDelaySecByLevel"]
    require_list(respawn, [10, 10, 12, 12, 14, 16, 20, 25, 28, 32.5, 35, 37.5,
                           40, 42.5, 45, 47.5, 50, 52.5], "respawn table")
    for second, expected in ((900, 0.0), (930, 0.00425), (1800, 0.1275),
                             (1830, 0.1305), (2700, 0.2175), (2730, 0.232),
                             (3300, 0.5)):
        require_close(respawn_factor(second), expected, f"respawn factor {second}s")
    require_close(respawn[17] * (1.0 + respawn_factor(3300)), 78.75,
                  "level 18 capped respawn")
    lane_minions = {row["roleType"]: row for row in spawn["minions"]
                    if 0 <= int(row["roleType"]) < 4}
    require(sorted(lane_minions) == [0, 1, 2, 3], "lane minion roles 0..3")
    for role, row in lane_minions.items():
        require(isinstance(row.get("attackRange"), (int, float)) and
                0.1 <= float(row["attackRange"]) <= 100.0,
                f"lane minion role {role} attack range")

    text_rows = {row["ID"]: row for row in layout["texts"]}
    for text_id in ("gold.text", "respawn.text"):
        center = text_rows[text_id]["center"]
        require(len(center) == 2 and
                all(isinstance(value, (int, float)) and math.isfinite(value)
                    for value in center),
                f"{text_id} runtime center")
    require(text_rows["respawn.text"]["bind"] == "respawn", "respawn bind")

    actor_hud = (root / "Engine/Private/Manager/UI/ActorHUDPanel.cpp").read_text(
        encoding="utf-8")
    require('addText("gold.text", "gold", 707.f, 142.f, 0.96f);' in actor_hud,
            "gold authored default")
    require('addText("respawn.text", "respawn", 208.75f, 102.25f, 1.35f);' in actor_hud,
            "respawn authored default")

    scene = (root / "Client/Private/Scene/Scene_InGameImGui.cpp").read_text(encoding="utf-8")
    require("m_bShowBalanceTuner" in scene, "F4 balance tuner state")
    require("CChampionTuner::Open(UI::eBalanceTunerCategory::Champions)" in scene,
            "F4 defaults to Champions")
    require("CStructureTunerPanel::Render" not in scene, "legacy F4 structure renderer removed")
    require(scene.count("CChampionTuner::Render(this)") == 2,
            "exactly guarded F4 and legacy render call sites")
    require("m_bShowUITuner = m_bShowAIDebug" not in scene, "F9 opens AI only")
    ui_tuner_block = scene.split("if (m_bShowUITuner)", 1)[1].split(
        "if (m_bShowWfxEffectTool)", 1)[0]
    require("UI_OnImGui_StatusPanelLayoutTuner" not in ui_tuner_block,
            "F8 opens one UI Manager window")

    tuner = (root / "Client/Private/UI/ChampionTuner.cpp").read_text(encoding="utf-8")
    tuner_surface = tuner.split("namespace UI", 1)[1].split(
        "#if 0 // Superseded current-champion override surface", 1)[0]
    for tab in ('"Champions"', '"Skills"', '"Minions"', '"Towers"'):
        require(tab in tuner_surface, f"F4 data tab {tab}")
    require('BeginCombo("Champion"' in tuner_surface, "all-champion selector")
    require(tuner_surface.count('"Save & Hot Load"') == 1,
            "one Debug authoritative save and hot-load action")
    require('"Reload JSON"' in tuner_surface, "disk draft reload action")
    require('"Reload Draft"' not in tuner_surface, "ambiguous reload label removed")
    require('"Discard unsaved F4 edits?"' in tuner_surface,
            "dirty draft reload confirmation")
    require("bHotLoadPending" in tuner_surface,
            "save and reload lock while authoritative ack is pending")
    require("ResolveHotLoadAvailability" in tuner,
            "one client-known hot-load availability diagnosis")
    require('"Current champion' not in tuner_surface, "F4 is not current-champion bound")
    require("RenderOverrideTable(pScene, state)" not in tuner_surface,
            "raw override table hidden from F4")
    require('"Target NetId' not in tuner_surface, "target net id hidden from F4")
    for label in ("Attack Damage", "Ability Power", "Armor", "Magic Resist",
                  "Attack Speed Ratio", "Resource Regen / Sec", "Cooldown (sec)",
                  "Flat Damage", "Total AD Ratio", "Bonus AD Ratio", "AP Ratio",
                  "Target Max HP Ratio", "Missing HP Ratio", "Max Health"):
        require(f'"{label}"' in tuner_surface, f"F4 field {label}")
    skills_surface = tuner_surface.split('BeginTabItem("Skills")', 1)[1].split(
        'BeginTabItem("Minions")', 1)[0]
    require("kRuntimeFlatOnlySkills" not in skills_surface,
            "obsolete runtime-only damage fork removed")
    require('SeparatorText("Runtime Damage Params")' not in skills_surface,
            "duplicate scalar runtime damage editor removed")
    require('"Passive / Basic Attack", "Q", "W", "E", "R"' in skills_surface,
            "F4 skill selector exposes all five authoritative slots")
    require("draft.skillSlot = skillIndex;" in skills_surface,
            "F4 selector maps directly to slots 0..4")
    for label in ("Q1/Q2 Flat Damage", "Q3 Tornado Flat Damage", "EQ Flat Damage",
                  "Q2 Recast Flat Damage", "Damage Per Spear",
                  "Non-Epic Flat Damage"):
        require(label in tuner, f"ranked variant F4 label {label}")
    require("IsRankedDamageParam(effectKey, pParam)" in skills_surface and
            "RankedDamageParamLabel(effectKey, pParam)" in skills_surface,
            "variant damage rows share the rank table")
    for label in ("Conditional Damage Mechanics", "Vital Target Max HP Ratio",
                  "Petricite Burst Flat Damage", "Petricite Burst AP Ratio",
                  "Contempt Missing HP Ratio", "Target Health Threshold Ratio"):
        require(label in skills_surface, f"F4 conditional damage field {label}")
    missing_execution = skills_surface.split(
        "kDamageExecutionMissing", 1)[1].split("};", 1)[0]
    for key in ("skill.riven.q", "skill.riven.w",
                "skill.masteryi.q", "skill.masteryi.e"):
        require(f'"{key}"' in missing_execution,
                f"F4 missing server execution warning {key}")
    require("Server damage execution: NOT_IMPLEMENTED" in skills_surface and
            "bDisableDamageRows" in skills_surface,
            "missing execution damage rows cannot misrepresent live behavior")

    scene_network = (root / "Client/Private/Scene/Scene_InGameNetwork.cpp").read_text(
        encoding="utf-8")
    command_result = scene_network.split(
        "void CScene_InGame::OnAuthoritativeCommandResult(", 1)[1].split(
        "void CScene_InGame::RebaseNetworkTimeline(", 1)[0]
    require("slot.currentStage = authoritativeSkillStage == 1u ? 1u : 0u;" in
            command_result and
            "stageWindowEndTick > serverTick" in command_result,
            "client command result restores authoritative skill stage/window")
    rebase = scene_network.split(
        "void CScene_InGame::RebaseNetworkTimeline(", 1)[1]
    require("std::fill_n(m_uLastSkillCommandResultSeq, 5u, 0u);" in rebase,
            "replay rebase clears client command result sequence history")

    rank_controls = skills_surface.split("const auto DrawRankRow", 1)[1].split(
        'if (ImGui::BeginTable(', 1)[0]
    require("ImGui::DragFloat" in rank_controls, "F4 rank drag controls")
    require("dragSpeed" in rank_controls and
            "ImGuiSliderFlags_AlwaysClamp" in rank_controls,
            "rank drag speed and direct-input clamp")
    require("ImGui::SliderFloat" not in skills_surface,
            "active F4 skill surface has no slider-thumb controls")
    require('ImGui::InputFloat("##Value"' not in skills_surface,
            "active skill rank values are not raw input boxes")
    for call_contract in (
            '0.1f, 0.f, 300.f, "%.1f s"',
            '1.f, 0.f, 2000.f, "%.0f"',
            '0.01f, 0.f, 5.f, "%.2f"'):
        require(call_contract in skills_surface,
                f"rank DragFloat contract {call_contract}")
    require("kAdRatioAuthoringMax = 10.f" in tuner,
            "F4 AD ratio authoring maximum is 10")
    require(skills_surface.count("kAdRatioAuthoringMax") >= 2,
            "ranked Total/Bonus AD editors use the 10x maximum")
    require("Double-click to type an exact value." in skills_surface,
            "DragFloat exact-input gesture is explained")
    for field_contract in (
            '"rangeMax", "Skill Range (m)"',
            '"radius", "Effect Radius / Half Width (m)"',
            '"formationDelaySec", "Delay (sec)"',
            '"healDamageRatio", "Heal / Damage Ratio"'):
        require(field_contract in skills_surface,
                f"F4 skill mechanics editor {field_contract}")
    for path_token in ("kChampionBalanceDataPath", "kSkillEffectBalanceDataPath",
                       "kSpawnObjectBalanceDataPath"):
        require(path_token in tuner, f"truth JSON path {path_token}")
    require("diskSource != *write.pSource" in tuner,
            "stale external JSON change blocks save")
    require("JSON transaction rolled back" in tuner,
            "multi-file save rollback")
    require("kEconomyBalanceDataPath" in tuner and
            "draft.bEconomyDirty" in tuner and
            "draft.economyPath" in tuner,
            "F4 transaction includes economy JSON")
    require("ePracticeOperation::ReloadGameplayDefinitions" in tuner_surface,
            "single server definition reload")
    minion_surface = tuner_surface.split('BeginTabItem("Minions")', 1)[1].split(
        'BeginTabItem("Towers")', 1)[0]
    require('"attackRange", "Attack Range"' in minion_surface,
            "F4 minion attack range field")
    require("EditDragFloat(" in minion_surface and
            "kMinionAttackRangeMin" in minion_surface and
            "kMinionAttackRangeMax" in minion_surface,
            "F4 minion attack range drag and shared bounds")

    command_schema = (root / "Shared/Schemas/Command.fbs").read_text(encoding="utf-8")
    require("ApplyMinionStatOverride = 30" in command_schema,
            "minion apply operation is append-only")
    require("ClearMinionStatOverrides = 31" in command_schema,
            "minion clear operation is append-only")
    require("RefillJungleHealth = 32" in command_schema and
            "ResetJungleMonster = 33" in command_schema and
            "ClearObjectiveBuffs = 34" in command_schema,
            "objective lab commands are append-only")
    objective_surface = tuner_surface.split('BeginTabItem("Objectives")', 1)[1].split(
        'ImGui::EndTabBar()', 1)[0]
    for field in ("teamGoldPerChampion", "teamLevelGrant", "buffDurationSec",
                  "baronMinionHpMultiplier", "baronMinionAttackDamageMultiplier",
                  "baronMinionScaleMultiplier", "elderAttackDamageMultiplier",
                  "elderExecuteThresholdRatio", "blueManaRegenPerSec",
                  "redHealthRegenPerSec"):
        require(field in objective_surface, f"F4 objective field {field}")
    for section_name in ("jungle", "objectives"):
        for field, value in economy[section_name].items():
            if section_name == "objectives" and field == "teamLevelGrant":
                continue
            require(isinstance(value, (int, float)) and not isinstance(value, bool) and
                    math.isfinite(float(value)) and 0.0 <= float(value) <= 1000000.0,
                    f"{section_name}.{field} editable numeric domain")
    for field in ("elderBurnTickIntervalSec", "redBurnTickIntervalSec"):
        require(float(economy["objectives"][field]) >= 0.001,
                f"objectives.{field} positive interval")
    for field in ("elderBurnTargetMaxHpRatioPerTick", "elderExecuteThresholdRatio"):
        require(float(economy["objectives"][field]) <= 1.0,
                f"objectives.{field} ratio domain")
    team_level_grant = economy["objectives"]["teamLevelGrant"]
    require(isinstance(team_level_grant, int) and not isinstance(team_level_grant, bool) and
            0 <= team_level_grant <= 18,
            "objectives.teamLevelGrant integer domain")

    query_cpp = (root /
        "Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp").read_text(
            encoding="utf-8")
    kindred_cpp = (root /
        "Shared/GameSim/Champions/Kindred/KindredGameSim.cpp").read_text(
            encoding="utf-8")
    kindred_component = (root /
        "Shared/GameSim/Components/KindredSimComponent.h").read_text(
            encoding="utf-8")
    require("uWCastRank = 1u" in kindred_component,
            "Kindred W cast rank is checkpoint-owned state")
    require("uECastRank = 1u" in kindred_component and
            "state.uECastRank = ctx.skillRank > 0u ? ctx.skillRank : 1u" in kindred_cpp,
            "Kindred E captures the cast rank for delayed basic-attack consumption")
    require(
        "TryResolvePracticeSkillEffectOverride" in query_cpp,
        "formula live overrides must bypass authored runtime params",
    )
    for token in (
        "eSkillEffectParamId::TotalAdRatio",
        "eSkillEffectParamId::BonusAdRatio",
        "eSkillEffectParamId::ApRatio",
        "eSkillEffectParamId::TargetMaxHpRatio",
        "eSkillEffectParamId::MissingHealthDamageRatio",
    ):
        require(token in query_cpp, f"missing final formula override: {token}")
    require(
        "request.eSourceKind = eDamageSourceKind::Skill" in kindred_cpp,
        "Kindred W requests must enter the skill formula path explicitly",
    )
    require(
        "state.uWCastRank = ctx.skillRank > 0u ? ctx.skillRank : 1u" in kindred_cpp,
        "Kindred W captures rank at cast time")
    require("ResolveSkillEffectParamRanked" in query_cpp and
            "ResolveSkillFlatDamage" in query_cpp and
            "ResolveSkillTargetMissingHpRatio" in query_cpp,
            "ranked variant and canonical damage queries are centralized")

    consumer_contracts = {
        "Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp": (
            "ResolveYasuoSkillEffectParamRanked", "ResolveSkillFlatDamage"),
        "Shared/GameSim/Champions/LeeSin/LeeSinGameSim.cpp": (
            "ResolveSkillEffectParamRanked",),
        "Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp": (
            "ResolveSkillEffectParamRanked", "ResolveSkillFlatDamage"),
        "Shared/GameSim/Champions/Ezreal/EzrealGameSim.cpp": (
            "ResolveSkillEffectParamRanked", "ResolveSkillFlatDamage"),
        "Shared/GameSim/Champions/Ashe/AsheGameSim.cpp": ("ResolveSkillFlatDamage",),
        "Shared/GameSim/Champions/Fiora/FioraGameSim.cpp": ("ResolveSkillFlatDamage",),
        "Shared/GameSim/Champions/Jax/JaxGameSim.cpp": ("ResolveSkillFlatDamage",),
        "Shared/GameSim/Champions/Kindred/KindredGameSim.cpp": ("ResolveSkillFlatDamage",),
        "Shared/GameSim/Champions/Zed/ZedGameSim.cpp": (
            "ResolveSkillTargetMissingHpRatio",),
    }
    for relative_path, anchors in consumer_contracts.items():
        source = (root / relative_path).read_text(encoding="utf-8")
        for anchor in anchors:
            require(anchor in source, f"{relative_path} consumes {anchor}")
    damage_queue = (root / "Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp").read_text(encoding="utf-8")
    require("resolved.flatAmount *= skillDamageScale" in damage_queue,
            "charge scale applies after formula resolution")
    require("resolved.targetMissingHpRatioOverride *= skillDamageScale" in damage_queue,
            "charge scale covers target HP formula")
    require("Preserve that flat value while still" in damage_queue and
            "request.targetMissingHpRatioOverride" in damage_queue,
            "param-driven variants merge canonical ratios")
    spawn_policy = (root / "Shared/GameSim/Definitions/SpawnLoadoutPolicyDef.h").read_text(
        encoding="utf-8")
    require("bPracticeModeEnabled && pPracticeSecondsByLevel" in spawn_policy,
            "practice respawn guard")
    game_room = (root / "Server/Private/Game/GameRoom.cpp").read_text(encoding="utf-8")
    require("m_PracticeRespawnSecondsByLevel.data()" in game_room,
            "server uses shared respawn policy")
    require("m_PracticeRespawnSecondsByLevel.fill(0.f)" in game_room,
            "match reset clears respawn overrides")
    room_commands = (root / "Server/Private/Game/GameRoomCommands.cpp").read_text(
        encoding="utf-8")
    require(
        "keyframe.practiceRespawnSecondsByLevel = m_PracticeRespawnSecondsByLevel" in
        room_commands,
        "chrono capture preserves respawn overrides")
    require(
        "pKeyframe->practiceRespawnSecondsByLevel" in room_commands,
        "chrono restore preserves respawn overrides")
    require(
        "keyframe.practiceMinionAttackDamageByRole" in room_commands and
        "pKeyframe->practiceMinionAttackDamageByRole" in room_commands,
        "chrono preserves minion attack damage overrides")
    require("ClearPracticeMinionAttackDamageOverrides();" in room_commands,
            "practice disable and clear restore live minions")
    require("SpawnObject definitions normally affect the next spawn" in room_commands,
            "hot reload refreshes existing spawn objects")
    require("MinionStateComponent, MinionComponent, HealthComponent" in room_commands,
            "minion HP mirrors refresh together")
    require("state.type >= PracticeMinionAttackDamagePolicy::kRoleCount" in room_commands,
            "summon role 4 is excluded from lane refresh")
    require("spawnPack.structure.turretAI.nexusAttackDamage" in room_commands,
            "nexus turret damage stays tier-specific")
    require("m_keyframes.clear();" in room_commands and
            "++m_timelineEpoch;" in room_commands,
            "hot reload invalidates old-definition rewind checkpoints")
    reload_slice = room_commands.split(
        "case ePracticeOperation::ReloadGameplayDefinitions:", 1)[1].split(
        "case ePracticeOperation::ApplyJungleStatOverride:", 1)[0]
    require("state.attackRange = combat.attackRange" in reload_slice,
            "hot reload refreshes living minion attack range")
    require("UnapplyAllBaronEmpoweredMinions" in reload_slice and
            "healthRatio" in reload_slice and
            "refreshedJungle" in reload_slice,
            "hot reload preserves HP ratio and refreshes objective actors")

    skill_atom = (root / "Shared/GameSim/Definitions/SkillAtomData.h").read_text(
        encoding="utf-8")
    require("f32_t valueByRank[kSkillRankValueMax]" in skill_atom and
            "sizeof(SkillEffectParam) == 24u" in skill_atom,
            "skill effect params own an explicit ranked ABI")
    generator = (root / "Tools/LoLData/Build-LoLDefinitionPack.py").read_text(
        encoding="utf-8")
    require("RANKED_DAMAGE_PARAM_KEYS" in generator and
            "valueByRank[{rank_index}]" in generator,
            "definition generation preserves ranked variant values")
    require("IsRankedDamageParam" in runtime_overlay and
            "target.valueByRank[rankIndex] = value" in runtime_overlay,
            "runtime hot load preserves ranked variant values")
    require(skill_atom.index("DamageFlatOverride") <
            skill_atom.index("HealDamageRatio"),
            "heal ratio enum is append-only")
    require('"healDamageRatio": "HealDamageRatio"' in generator,
        "heal ratio generator mapping")
    require('{ "healDamageRatio", eSkillEffectParamId::HealDamageRatio }' in
            runtime_overlay, "heal ratio runtime overlay mapping")

    event_schema = (root / "Shared/Schemas/Event.fbs").read_text(encoding="utf-8")
    require("effectLength:float" in event_schema and
            "effectHalfWidth:float" in event_schema,
            "effect event runtime geometry")

    ashe_fx = (root /
        "Client/Private/GameObject/Champion/Ashe/Ashe_FxPresets.cpp").read_text(
            encoding="utf-8")
    require("kPathQDiffuseTex" not in ashe_fx,
            "opaque Ashe Q diffuse ground plane removed")
    ashe_w_hit = load_json(root / "Data/LoL/FX/Champions/Ashe/w_hit.wfx")
    require_close(ashe_w_hit["emitters"][0]["width"], 1.2, "Ashe W hit width")
    require_close(ashe_w_hit["emitters"][0]["height"], 1.2, "Ashe W hit height")

    sylas_q_wfx = load_json(root / "Data/LoL/FX/Champions/Sylas/q_explosion.wfx")
    sylas_crack = next(row for row in sylas_q_wfx["emitters"]
                       if row["name"] == "q_explosion_dark_crack")
    require_close(sylas_crack["width"], 3.3, "Sylas Q decal width")
    require_close(sylas_crack["height"], 3.3, "Sylas Q decal height")

    fiora_cast_wfx = load_json(root / "Data/LoL/FX/Champions/Fiora/w_cast.wfx")
    fiora_indicator = next(row for row in fiora_cast_wfx["emitters"]
                           if row["name"] == "w_cc_indicator_yellow")
    require_close(fiora_indicator["width"], 6.5, "Fiora W indicator length")
    require_close(fiora_indicator["height"], 1.6, "Fiora W indicator width")
    require_close(fiora_indicator["attach_offset"][2], 3.25,
                  "Fiora W indicator midpoint")
    require("RemapDefinitionCooldown" in reload_slice and
            "skillCooldownRefresh=" in reload_slice,
            "hot reload remaps active champion cooldowns")
    require("eSkillSlot::Q" in reload_slice and "eSkillSlot::R" in reload_slice,
            "hot reload covers Q through R")
    require(
        reload_slice.index("previousSkillCooldowns.push_back") <
        reload_slice.index("TryReloadRuntimeGameplayDefinitions") <
        reload_slice.index("RemoveComponent<PracticeSkillEffectOverrideComponent>") <
        reload_slice.index("const GameplayDefinitionPack& reloadedDefinitions") <
        reload_slice.index("RemapDefinitionCooldown"),
        "old override baseline is cached before reload and new resolve")
    simlab = (root / "Tools/SimLab/main.cpp").read_text(encoding="utf-8")
    require("state.uWCastRank" in simlab and
            "wHook.skillRank = 4u" in simlab and
            "request.rank == 4u" in simlab,
            "Kindred W cast hook and request rank route are probed")
    require("RunChampionDamageAuthorityMatrixProbe" in simlab,
            "all champion damage formulas execute through DamageQueue")
    require("RunRankedVariantDamageProbe" in simlab and
            "ResolveSkillEffectParamRanked" in simlab and
            "ResolveSkillFlatDamage" in simlab,
            "ranked special variants and canonical flat ranks are probed")
    require("BuildPackOracle" in simlab and
            "RequestsMatchFormula" in simlab,
            "damage matrix oracle is built directly from pack arrays")
    f4_only = simlab.split(
        'std::strcmp(argv[1], "--f4-balance-only")', 1)[1].split(
            'std::strcmp(argv[1], "--irelia-q-only")', 1)[0]
    require("RunSkillCooldownDefinitionReloadProbe" in simlab and
            "RunSkillCooldownDefinitionReloadProbe" in f4_only,
            "F4 SimLab path probes definition cooldown remap")
    require("CooldownSecOverride" in simlab and
            "RemoveComponent<PracticeSkillEffectOverrideComponent>" in simlab,
            "cooldown reload probe covers override removal order")
    room_spawn = (root / "Server/Private/Game/GameRoomSpawn.cpp").read_text(
        encoding="utf-8")
    require("m_PracticeMinionAttackDamage.Resolve" in room_spawn,
            "new minions use the practice attack damage policy")

    snapshot_schema = (root / "Shared/Schemas/Snapshot.fbs").read_text(encoding="utf-8")
    require("respawnRemainingSec:float" in snapshot_schema, "snapshot remaining field")
    require("respawnDurationSec:float" in snapshot_schema, "snapshot duration field")
    require("objectiveStateFlags:uint" in snapshot_schema and
            "visualScaleMultiplier:float" in snapshot_schema,
            "objective snapshot visual contract")
    for field in ("aiDebugCandidateTick:ulong", "aiDebugSelectionTick:ulong",
                  "aiDebugCandidateKinds:[ubyte]",
                  "aiDebugCandidateFlags:[ubyte]",
                  "aiDebugCandidateScores:[float]",
                  "aiDebugCandidateTermRawValues:[float]",
                  "aiDebugCandidateTermWeights:[float]",
                  "aiDebugCandidateTermContributions:[float]"):
        require(field in snapshot_schema, f"AI behavior evidence field {field}")

    ai_component = (root / "Shared/GameSim/Components/ChampionAIComponent.h").read_text(
        encoding="utf-8")
    resolver = ai_component.split(
        "inline ChampionAITuningParam* ResolveChampionAITuningParam(", 1)[1].split(
        "inline const ChampionAITuningParam* ResolveChampionAITuningParam(", 1)[0]
    tuning_ids = ai_component.split("enum class eChampionAITuningId", 1)[1].split(
        "};", 1)[0]
    for tuning_name in ("SkillCastMinInterval", "FollowWaveSearchRange", "FarmPriority"):
        require(f"eChampionAITuningId::{tuning_name}" in resolver,
                f"central AI tuning resolver maps {tuning_name}")
        require(tuning_name in tuning_ids, f"AI tuning id {tuning_name}")
    require("default:" in resolver and "return nullptr;" in resolver,
            "central AI tuning resolver rejects Count/out-of-range")

    ai_panel = (root / "Client/Private/UI/AIDebugPanel.cpp").read_text(encoding="utf-8")
    active_ai_panel = ai_panel.split(
        "void UI::CAIDebugPanel::Render(CWorld& world, CScene_InGame* pScene)", 1)[1].split(
        "#if 0 // Legacy all-in-one AI inspection surface", 1)[0]
    for label in ("Follow Wave Search (m)", "Farm Priority (x)",
                  "Turret Danger Limit", "Reset Selected Bot"):
        require(label in active_ai_panel, f"active F9 control {label}")
    require("ImGui::IsItemDeactivatedAfterEdit()" in ai_panel,
            "F9 tuning sends once after drag/direct edit")
    require('BeginTable(\n\t\t\t"##TuningRow", 2' in ai_panel and
            'ImGui::TextUnformatted(pLabel)' in ai_panel and
            '"##Value"' in ai_panel,
            "F9 tuning labels use a dedicated responsive table column")
    require("Waiting for authoritative server echo" in ai_panel and
            "No server echo; restored snapshot value" in ai_panel,
            "F9 tuning exposes echo pending and timeout")
    for label in ("Decision Ranking", "SELECTED THIS TICK", "ACTIVE / HELD",
                  "Score Calculations", "Brain order:"):
        require(label in ai_panel, f"F9 behavior rationale {label}")
    require("RenderChampionAIDecisionRanking(debug)" in active_ai_panel,
            "active F9 Observe tab renders behavior ranking")
    require("term.rawValue" in ai_panel and
            "term.weight" in ai_panel and
            "term.contribution" in ai_panel,
            "F9 renders raw x weight = contribution")

    snapshot_builder = (root / "Server/Private/Game/SnapshotBuilder.cpp").read_text(
        encoding="utf-8")
    require("draft.tick == serverTick" in snapshot_builder and
            "latest.tick == serverTick" in snapshot_builder,
            "AI behavior evidence and selection are same-tick authoritative")
    require("#if defined(_DEBUG)" in snapshot_builder and
            "aiDebugCandidateKinds" in snapshot_builder,
            "AI behavior evidence is Debug-only")
    decoder = (root / "Client/Public/Network/Client/AIDebugEvidenceDecoder.h").read_text(
        encoding="utf-8")
    clear_index = decoder.index("outDebug.utilityCandidateTick = 0u")
    validate_index = decoder.index("snapshot.aiDebugCandidateTick() != serverTick")
    require(clear_index < validate_index,
            "client clears old AI evidence before validating current snapshot")
    require("pKinds->size() != kCandidateCount" in decoder and
            "pContributions->size() != kTermCount" in decoder,
            "client validates compact AI evidence vector lengths")

    print("[F4BalanceContracts] PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
