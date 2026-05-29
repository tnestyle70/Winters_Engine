#include "AssetFormat/Anim/WAnimWriter.h"
#include "AssetFormat/Common/BinaryWriter.h"
#include "AssetFormat/Common/Hash.h"
#include <assimp/scene.h>
#include <cstring>
#include <vector>

namespace Winters::Asset
{
    template<typename T>
    static void AppendAs(std::vector<uint8_t>& buf, const T& value)
    {
        const auto* p = reinterpret_cast<const uint8_t*>(&value);
        buf.insert(buf.end(), p, p + sizeof(T));
    }

    static bool IsLoopAnim(const char* name)
    {
        if (!name) return false;
        return std::strstr(name, "idle") != nullptr ||
            std::strstr(name, "run") != nullptr ||
            std::strstr(name, "loop") != nullptr;
    }

    bool CWAnimWriter::WriteFromAssimp(const aiAnimation* pAnim,
        const aiScene*,
        uint64_t skelHash,
        const wchar_t* pOutPath)
    {
        if (!pAnim || !pOutPath) return false;

        const double ticksPerSecond = (pAnim->mTicksPerSecond > 0.0)
            ? pAnim->mTicksPerSecond
            : 24.0;

        AnimMetaHeader hdr{};
        std::memcpy(hdr.magic, WANIM_MAGIC, 4);
        hdr.channel_count = pAnim->mNumChannels;
        hdr.duration_ticks = static_cast<float>(pAnim->mDuration);
        hdr.ticks_per_second = static_cast<float>(ticksPerSecond);
        hdr.is_loop = IsLoopAnim(pAnim->mName.C_Str()) ? 1 : 0;

        std::vector<AnimChannel> channels;
        std::vector<uint8_t> keyBlock;
        channels.reserve(pAnim->mNumChannels);

        uint32_t totalKeys = 0;
        for (uint32_t c = 0; c < pAnim->mNumChannels; ++c)
        {
            const aiNodeAnim* node = pAnim->mChannels[c];
            AnimChannel ch{};
            ch.bone_name_hash = FNV1a(node->mNodeName.C_Str());
            ch.bone_index_cached = -1;

            ch.pos_offset = static_cast<uint32_t>(keyBlock.size());
            ch.pos_key_count = node->mNumPositionKeys;
            for (uint32_t k = 0; k < node->mNumPositionKeys; ++k)
            {
                const auto& pk = node->mPositionKeys[k];
                AppendAs(keyBlock, VectorKey{
                    static_cast<float>(pk.mTime),
                    pk.mValue.x, pk.mValue.y, pk.mValue.z });
            }

            ch.rot_offset = static_cast<uint32_t>(keyBlock.size());
            ch.rot_key_count = node->mNumRotationKeys;
            for (uint32_t k = 0; k < node->mNumRotationKeys; ++k)
            {
                const auto& rk = node->mRotationKeys[k];
                AppendAs(keyBlock, QuatKey{
                    static_cast<float>(rk.mTime),
                    rk.mValue.x, rk.mValue.y, rk.mValue.z, rk.mValue.w });
            }

            ch.scl_offset = static_cast<uint32_t>(keyBlock.size());
            ch.scl_key_count = node->mNumScalingKeys;
            for (uint32_t k = 0; k < node->mNumScalingKeys; ++k)
            {
                const auto& sk = node->mScalingKeys[k];
                AppendAs(keyBlock, VectorKey{
                    static_cast<float>(sk.mTime),
                    sk.mValue.x, sk.mValue.y, sk.mValue.z });
            }

            totalKeys += ch.pos_key_count + ch.rot_key_count + ch.scl_key_count;
            channels.push_back(ch);
        }

        hdr.total_key_count = totalKeys;
        hdr.event_count = 0;

        CBinaryWriter w;
        w.Write(hdr);
        for (const auto& ch : channels)
            w.Write(ch);
        if (!keyBlock.empty())
            w.WriteBytes(keyBlock.data(), keyBlock.size());
        w.Write(WAnimTrailer{ skelHash });
        return w.SaveToFile(pOutPath, WF_NONE);
    }
}
