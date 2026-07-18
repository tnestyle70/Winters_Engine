#include "Client/Private/Data/RuntimeVisualDefinitionOverlay.h"

#include "Network/Backend/json.hpp"

#include <Windows.h>

#include <atomic>
#include <cmath>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

namespace
{
    using json = nlohmann::json;

    struct OwnedModel
    {
        ClientData::ChampionModelVisualDefinition value{};
        std::string displayName;
        std::string animPrefix;
        std::string idleAnimation;
        std::string runAnimation;
        std::string basicAttackAnimation;
        std::string mesh;
        std::wstring shader;
        std::wstring defaultTexture;
        std::wstring textureSlots[ClientData::kChampionModelTextureSlotCount];
    };

    struct OwnedUi
    {
        ClientData::ChampionUiVisualDefinition value{};
        std::wstring loadscreen;
        std::wstring portrait;
    };

    struct RuntimeVisualStorage
    {
        std::vector<ClientData::ChampionVisualDefinition> champions;
        std::vector<OwnedModel> ownedModels;
        std::vector<ClientData::ChampionModelVisualDefinition> models;
        ClientData::ChampionModelVisualPack modelPack{};
        std::vector<OwnedUi> ownedUi;
        std::vector<ClientData::ChampionUiVisualDefinition> ui;
        json objectVisualDocument;
    };

    std::atomic<const RuntimeVisualStorage*> g_active{ nullptr };
    std::atomic<u32_t> g_revision{ 0u };

    std::mutex& ReloadMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    std::vector<std::unique_ptr<RuntimeVisualStorage>>& Generations()
    {
        static std::vector<std::unique_ptr<RuntimeVisualStorage>> values;
        return values;
    }

    u32_t Fnv1a32(const std::string& text)
    {
        u32_t value = 2166136261u;
        for (char ch : text)
        {
            value ^= static_cast<u8_t>(ch);
            value *= 16777619u;
        }
        return value;
    }

    std::string ToLowerAscii(std::string value)
    {
        for (char& ch : value)
        {
            if (ch >= 'A' && ch <= 'Z')
                ch = static_cast<char>(ch - 'A' + 'a');
        }
        return value;
    }

    std::wstring WidenAscii(const std::string& value)
    {
        return std::wstring(value.begin(), value.end());
    }

    bool_t ResolveWorkspaceFile(const wchar_t* relative, std::wstring& outPath)
    {
        wchar_t cwd[MAX_PATH]{};
        if (GetCurrentDirectoryW(MAX_PATH, cwd) == 0u)
            return false;
        std::wstring directory = cwd;
        for (u32_t depth = 0u; depth < 8u; ++depth)
        {
            if (GetFileAttributesW((directory + L"\\Winters.sln").c_str()) != INVALID_FILE_ATTRIBUTES)
            {
                outPath = directory + L"\\" + relative;
                return GetFileAttributesW(outPath.c_str()) != INVALID_FILE_ATTRIBUTES;
            }
            const std::size_t slash = directory.find_last_of(L"\\/");
            if (slash == std::wstring::npos)
                break;
            directory.resize(slash);
        }
        return false;
    }

    bool_t ReadJson(const wchar_t* relative, json& outValue, std::string& outError)
    {
        std::wstring path;
        if (!ResolveWorkspaceFile(relative, path))
        {
            outError = "workspace path not found";
            return false;
        }
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            outError = "read failed";
            return false;
        }
        std::ostringstream stream;
        stream << file.rdbuf();
        outValue = json::parse(stream.str(), nullptr, false);
        if (outValue.is_discarded())
        {
            outError = "json parse failed";
            return false;
        }
        return true;
    }

    bool_t ResolveSkillSlot(const std::string& key, u8_t& outSlot)
    {
        if (key.ends_with(".basic_attack")) outSlot = 0u;
        else if (key.ends_with(".q")) outSlot = 1u;
        else if (key.ends_with(".w")) outSlot = 2u;
        else if (key.ends_with(".e")) outSlot = 3u;
        else if (key.ends_with(".r")) outSlot = 4u;
        else return false;
        return true;
    }

    bool_t ParseChampionVisuals(
        const json& root,
        RuntimeVisualStorage& storage,
        std::string& outError)
    {
        if (!root.contains("champions") || !root["champions"].is_array() ||
            root["champions"].size() != 17u)
        {
            outError = "ChampionVisualDefs champions coverage must be 17";
            return false;
        }
        storage.champions.clear();
        for (const json& entry : root["champions"])
        {
            if (!entry.is_object() || !entry.contains("key") || !entry["key"].is_string() ||
                !entry.contains("modelYawOffsetRadians") || !entry["modelYawOffsetRadians"].is_number() ||
                !entry.contains("skills") || !entry["skills"].is_array() || entry["skills"].size() != 5u)
            {
                outError = "invalid champion visual entry";
                return false;
            }
            ClientData::ChampionVisualDefinition value{};
            value.key = Fnv1a32(entry["key"].get<std::string>());
            value.legacyChampion = ClientData::ResolveChampionFromDefinitionKey(value.key);
            value.modelYawOffsetRadians = entry["modelYawOffsetRadians"].get<f32_t>();
            if (value.legacyChampion == eChampion::END || !std::isfinite(value.modelYawOffsetRadians))
            {
                outError = "unknown champion visual key";
                return false;
            }
            for (const json& skill : entry["skills"])
            {
                if (!skill.is_object() || !skill.contains("key") || !skill["key"].is_string() ||
                    !skill.contains("replicatedCueId") || !skill["replicatedCueId"].is_number_unsigned() ||
                    !skill.contains("stages") || !skill["stages"].is_array() ||
                    skill["stages"].empty() || skill["stages"].size() > ClientData::kVisualSkillStageCount)
                {
                    outError = "invalid skill visual entry";
                    return false;
                }
                u8_t slot = 0u;
                if (!ResolveSkillSlot(skill["key"].get<std::string>(), slot))
                {
                    outError = "invalid skill visual key";
                    return false;
                }
                ClientData::SkillVisualDefinition& output = value.skills[slot];
                output.stageCount = static_cast<u8_t>(skill["stages"].size());
                output.replicatedCueId = skill["replicatedCueId"].get<u32_t>();
                for (std::size_t index = 0u; index < skill["stages"].size(); ++index)
                {
                    const json& stage = skill["stages"][index];
                    if (!stage.contains("animationPlaybackSpeed") || !stage["animationPlaybackSpeed"].is_number() ||
                        !stage.contains("castFrame") || !stage["castFrame"].is_number() ||
                        !stage.contains("recoveryFrame") || !stage["recoveryFrame"].is_number())
                    {
                        outError = "invalid skill visual stage";
                        return false;
                    }
                    ClientData::SkillVisualStageDef parsed{};
                    parsed.animationPlaybackSpeed = stage["animationPlaybackSpeed"].get<f32_t>();
                    parsed.castFrame = stage["castFrame"].get<f32_t>();
                    parsed.recoveryFrame = stage["recoveryFrame"].get<f32_t>();
                    if (!std::isfinite(parsed.animationPlaybackSpeed) || parsed.animationPlaybackSpeed <= 0.f ||
                        !std::isfinite(parsed.castFrame) || parsed.castFrame < 0.f ||
                        !std::isfinite(parsed.recoveryFrame) || parsed.recoveryFrame < 0.f)
                    {
                        outError = "skill visual stage out of range";
                        return false;
                    }
                    output.stages[index] = parsed;
                }
            }
            storage.champions.push_back(value);
        }
        return true;
    }

    bool_t ParseChampionAssets(
        const json& root,
        RuntimeVisualStorage& storage,
        std::string& outError)
    {
        if (!root.contains("models") || !root["models"].is_array() || root["models"].size() != 17u ||
            !root.contains("ui") || !root["ui"].is_array() || root["ui"].size() != 17u)
        {
            outError = "ChampionAssetVisualDefs model/ui coverage must be 17";
            return false;
        }
        storage.ownedModels.clear();
        storage.ownedModels.reserve(17u);
        for (const json& entry : root["models"])
        {
            static constexpr const char* kStrings[] =
            {
                "key", "champion", "displayName", "animPrefix", "idleAnimation",
                "runAnimation", "basicAttackAnimation", "mesh", "shader", "defaultTexture",
            };
            bool_t valid = entry.is_object();
            for (const char* key : kStrings)
                valid = valid && entry.contains(key) && entry[key].is_string();
            valid = valid && entry.contains("basicAttackRange") && entry["basicAttackRange"].is_number() &&
                entry.contains("spawnPosition") && entry["spawnPosition"].is_array() && entry["spawnPosition"].size() == 3u &&
                entry.contains("spawnScale") && entry["spawnScale"].is_number() &&
                entry.contains("textureSlots") && entry["textureSlots"].is_array() && entry["textureSlots"].size() == 8u;
            if (!valid)
            {
                outError = "invalid champion model entry";
                return false;
            }
            OwnedModel owned{};
            owned.value.key = Fnv1a32(entry["key"].get<std::string>());
            const DefinitionKey championKey = Fnv1a32("champion." + ToLowerAscii(entry["champion"].get<std::string>()));
            owned.value.champion = ClientData::ResolveChampionFromDefinitionKey(championKey);
            if (owned.value.champion == eChampion::END)
            {
                outError = "unknown champion model identity";
                return false;
            }
            owned.displayName = entry["displayName"].get<std::string>();
            owned.animPrefix = entry["animPrefix"].get<std::string>();
            owned.idleAnimation = entry["idleAnimation"].get<std::string>();
            owned.runAnimation = entry["runAnimation"].get<std::string>();
            owned.basicAttackAnimation = entry["basicAttackAnimation"].get<std::string>();
            owned.mesh = entry["mesh"].get<std::string>();
            owned.shader = WidenAscii(entry["shader"].get<std::string>());
            owned.defaultTexture = WidenAscii(entry["defaultTexture"].get<std::string>());
            owned.value.basicAttackRange = entry["basicAttackRange"].get<f32_t>();
            owned.value.spawnPositionX = entry["spawnPosition"][0].get<f32_t>();
            owned.value.spawnPositionY = entry["spawnPosition"][1].get<f32_t>();
            owned.value.spawnPositionZ = entry["spawnPosition"][2].get<f32_t>();
            owned.value.spawnScale = entry["spawnScale"].get<f32_t>();
            for (u32_t index = 0u; index < 8u; ++index)
            {
                if (!entry["textureSlots"][index].is_string())
                {
                    outError = "champion model texture slot must be a string";
                    return false;
                }
                owned.textureSlots[index] = WidenAscii(entry["textureSlots"][index].get<std::string>());
            }
            storage.ownedModels.push_back(std::move(owned));
        }
        storage.models.resize(storage.ownedModels.size());
        for (std::size_t index = 0u; index < storage.ownedModels.size(); ++index)
        {
            OwnedModel& owned = storage.ownedModels[index];
            owned.value.displayName = owned.displayName.c_str();
            owned.value.animPrefix = owned.animPrefix.c_str();
            owned.value.idleAnimation = owned.idleAnimation.c_str();
            owned.value.runAnimation = owned.runAnimation.c_str();
            owned.value.basicAttackAnimation = owned.basicAttackAnimation.c_str();
            owned.value.mesh.resourceRelativePath = owned.mesh.c_str();
            owned.value.shader.runtimePath = owned.shader.c_str();
            owned.value.defaultTexture.resourceRelativePath = owned.defaultTexture.c_str();
            for (u32_t slot = 0u; slot < 8u; ++slot)
                owned.value.textureSlots[slot].resourceRelativePath = owned.textureSlots[slot].c_str();
            storage.models[index] = owned.value;
        }
        storage.modelPack.models = storage.models.data();
        storage.modelPack.modelCount = static_cast<u32_t>(storage.models.size());

        storage.ownedUi.clear();
        storage.ownedUi.reserve(17u);
        for (const json& entry : root["ui"])
        {
            if (!entry.is_object() || !entry.contains("key") || !entry["key"].is_string() ||
                !entry.contains("champion") || !entry["champion"].is_string() ||
                !entry.contains("loadscreen") || !entry["loadscreen"].is_string() ||
                !entry.contains("portrait") || !entry["portrait"].is_string())
            {
                outError = "invalid champion UI entry";
                return false;
            }
            OwnedUi owned{};
            owned.value.key = Fnv1a32(entry["key"].get<std::string>());
            const DefinitionKey championKey = Fnv1a32("champion." + ToLowerAscii(entry["champion"].get<std::string>()));
            owned.value.champion = ClientData::ResolveChampionFromDefinitionKey(championKey);
            owned.loadscreen = WidenAscii(entry["loadscreen"].get<std::string>());
            owned.portrait = WidenAscii(entry["portrait"].get<std::string>());
            if (owned.value.champion == eChampion::END)
            {
                outError = "unknown champion UI identity";
                return false;
            }
            storage.ownedUi.push_back(std::move(owned));
        }
        storage.ui.resize(storage.ownedUi.size());
        for (std::size_t index = 0u; index < storage.ownedUi.size(); ++index)
        {
            OwnedUi& owned = storage.ownedUi[index];
            owned.value.loadscreen.resourceRelativePath = owned.loadscreen.c_str();
            owned.value.portrait.resourceRelativePath = owned.portrait.c_str();
            storage.ui[index] = owned.value;
        }
        return true;
    }

    bool_t ValidateObjectVisuals(const json& root, std::string& outError)
    {
        static constexpr const char* kArrays[] =
        {
            "structures", "jungles", "minions", "fxMeshPreloads",
        };
        for (const char* key : kArrays)
        {
            if (!root.contains(key) || !root[key].is_array())
            {
                outError = std::string("ObjectVisualDefs missing array: ") + key;
                return false;
            }
            for (const json& entry : root[key])
            {
                if (!entry.is_object() || !entry.contains("key") || !entry["key"].is_string())
                {
                    outError = std::string("ObjectVisualDefs invalid entry: ") + key;
                    return false;
                }
            }
        }
        if (!root.contains("ambientProps") || !root["ambientProps"].is_object() ||
            !root.contains("mapRuntime") || !root["mapRuntime"].is_object())
        {
            outError = "ObjectVisualDefs ambientProps/mapRuntime missing";
            return false;
        }
        return true;
    }
}

namespace ClientData
{
    const ChampionVisualDefinition* FindRuntimeChampionVisualDefinition(eChampion champion)
    {
        const RuntimeVisualStorage* storage = g_active.load(std::memory_order_acquire);
        if (!storage)
            return nullptr;
        for (const ChampionVisualDefinition& value : storage->champions)
        {
            if (value.legacyChampion == champion)
                return &value;
        }
        return nullptr;
    }

    const ChampionVisualDefinition* FindRuntimeChampionVisualDefinition(DefinitionKey key)
    {
        const RuntimeVisualStorage* storage = g_active.load(std::memory_order_acquire);
        if (!storage)
            return nullptr;
        for (const ChampionVisualDefinition& value : storage->champions)
        {
            if (value.key == key)
                return &value;
        }
        return nullptr;
    }

    const ChampionModelVisualPack* GetRuntimeChampionModelVisualPack()
    {
        const RuntimeVisualStorage* storage = g_active.load(std::memory_order_acquire);
        return storage ? &storage->modelPack : nullptr;
    }

    const ChampionModelVisualDefinition* FindRuntimeChampionModelVisualDefinition(eChampion champion)
    {
        const RuntimeVisualStorage* storage = g_active.load(std::memory_order_acquire);
        if (!storage)
            return nullptr;
        for (const ChampionModelVisualDefinition& value : storage->models)
        {
            if (value.champion == champion)
                return &value;
        }
        return nullptr;
    }

    const ChampionUiVisualDefinition* FindRuntimeChampionUiVisualDefinition(eChampion champion)
    {
        const RuntimeVisualStorage* storage = g_active.load(std::memory_order_acquire);
        if (!storage)
            return nullptr;
        for (const ChampionUiVisualDefinition& value : storage->ui)
        {
            if (value.champion == champion)
                return &value;
        }
        return nullptr;
    }

    bool_t TryReloadRuntimeVisualDefinitions(std::string& outError)
    {
#if !defined(_DEBUG)
        outError = "debug-only";
        return false;
#else
        std::lock_guard<std::mutex> lock(ReloadMutex());
        json championVisuals;
        json objectVisuals;
        json championAssets;
        if (!ReadJson(L"Data\\LoL\\ClientPublic\\Visual\\ChampionVisualDefs.json", championVisuals, outError) ||
            !ReadJson(L"Data\\LoL\\ClientPublic\\Visual\\ObjectVisualDefs.json", objectVisuals, outError) ||
            !ReadJson(L"Data\\LoL\\ClientPublic\\Visual\\ChampionAssetVisualDefs.json", championAssets, outError))
        {
            return false;
        }
        auto storage = std::make_unique<RuntimeVisualStorage>();
        if (!ParseChampionVisuals(championVisuals, *storage, outError) ||
            !ValidateObjectVisuals(objectVisuals, outError) ||
            !ParseChampionAssets(championAssets, *storage, outError))
        {
            return false;
        }
        storage->objectVisualDocument = std::move(objectVisuals);
        g_active.store(storage.get(), std::memory_order_release);
        Generations().push_back(std::move(storage));
        g_revision.fetch_add(1u, std::memory_order_relaxed);
        return true;
#endif
    }

    void ClearRuntimeVisualDefinitions()
    {
        g_active.store(nullptr, std::memory_order_release);
    }

    u32_t GetRuntimeVisualDefinitionRevision()
    {
        return g_revision.load(std::memory_order_relaxed);
    }
}
