#include "AssetFormat/Anim/WAnimLoader.h"
#include "AssetFormat/Common/BinaryReader.h"
#include "AssetFormat/Common/Hash.h"
#include "AssetFormat/Common/WintersFileHeader.h"
#include "Resource/Animation.h"
#include "Resource/Skeleton.h"
#include <cstring>
#include <exception>
#include <vector>

namespace Winters::Asset
{
    static constexpr uint32_t MAX_ANIM_CHANNELS = 1024;
    static constexpr uint32_t MAX_ANIM_KEYS = 1'000'000;

    static std::string NarrowAscii(const std::wstring& s)
    {
        std::string out;
        out.reserve(s.size());
        for (wchar_t ch : s)
            out.push_back((ch >= 0 && ch <= 127) ? static_cast<char>(ch) : '_');
        return out;
    }

    static bool ValidateMeta(const AnimMetaHeader& h)
    {
        if (std::memcmp(h.magic, WANIM_MAGIC, 4) != 0) return false;
        if (h.channel_count > MAX_ANIM_CHANNELS) return false;
        if (h.total_key_count > MAX_ANIM_KEYS) return false;
        if (h.ticks_per_second <= 0.f) return false;
        return true;
    }

    template<typename T>
    static bool SpanFits(size_t offset, uint32_t count, size_t blockBytes)
    {
        const size_t bytes = static_cast<size_t>(count) * sizeof(T);
        return offset <= blockBytes && bytes <= blockBytes - offset;
    }

    static bool ResolveBoneByHash(const Engine::CSkeleton* pSkeleton,
        uint64_t hash,
        std::string& outName,
        int32_t& outIndex)
    {
        if (!pSkeleton) return false;
        const uint32_t boneCount = pSkeleton->GetBoneCount();
        for (uint32_t i = 0; i < boneCount; ++i)
        {
            const auto& bone = pSkeleton->GetBone(i);
            if (FNV1a(bone.strName.c_str()) == hash)
            {
                outName = bone.strName;
                outIndex = static_cast<int32_t>(i);
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<Engine::CAnimation> CWAnimLoader::LoadAsAnimation(
        const wchar_t* path,
        uint64_t expectedSkelHash,
        const Engine::CSkeleton* pSkeleton,
        const std::wstring& nameForDebug)
    {
        std::vector<uint8_t> file;
        if (!CBinaryReader::LoadFileToMemory(path, file))
            return nullptr;

        try
        {
            CBinaryReader r(file.data(), file.size());
            const auto fh = r.Read<WintersFileHeader>();
            if (std::memcmp(fh.magic, WINTERS_MAGIC, 4) != 0) return nullptr;
            if (fh.version_major != 1) return nullptr;
            if (fh.flags != WF_NONE) return nullptr;
            if (fh.content_size > r.Remaining()) return nullptr;

            const uint8_t* payload = r.Peek();
            CBinaryReader pr(payload, fh.content_size);

            const AnimMetaHeader hdr = pr.Read<AnimMetaHeader>();
            if (!ValidateMeta(hdr)) return nullptr;

            std::vector<AnimChannel> channels(hdr.channel_count);
            if (!channels.empty())
                pr.ReadBytes(channels.data(), channels.size() * sizeof(AnimChannel));

            const size_t eventBytes = static_cast<size_t>(hdr.event_count) * sizeof(AnimEvent);
            if (pr.Remaining() < eventBytes + sizeof(WAnimTrailer)) return nullptr;

            const size_t keyBlockSize = pr.Remaining() - eventBytes - sizeof(WAnimTrailer);
            std::vector<uint8_t> keyBlock(keyBlockSize);
            if (keyBlockSize > 0)
                pr.ReadBytes(keyBlock.data(), keyBlock.size());

            if (eventBytes > 0)
                pr.Skip(eventBytes);

            const WAnimTrailer trailer = pr.Read<WAnimTrailer>();
            if (trailer.skel_hash != expectedSkelHash)
                return nullptr;

            auto anim = Engine::CAnimation::Create(
                NarrowAscii(nameForDebug),
                static_cast<f64_t>(hdr.duration_ticks),
                static_cast<f64_t>(hdr.ticks_per_second));
            if (!anim) return nullptr;

            for (const AnimChannel& ch : channels)
            {
                std::string boneName;
                int32_t boneIndex = -1;
                if (!ResolveBoneByHash(pSkeleton, ch.bone_name_hash, boneName, boneIndex))
                    continue;

                if (!SpanFits<VectorKey>(ch.pos_offset, ch.pos_key_count, keyBlockSize)) return nullptr;
                if (!SpanFits<QuatKey>(ch.rot_offset, ch.rot_key_count, keyBlockSize)) return nullptr;
                if (!SpanFits<VectorKey>(ch.scl_offset, ch.scl_key_count, keyBlockSize)) return nullptr;

                Engine::BoneChannel bc;
                bc.strBoneName = boneName;
                bc.iBoneIndex = boneIndex;

                const auto* posKeys = reinterpret_cast<const VectorKey*>(keyBlock.data() + ch.pos_offset);
                for (uint32_t k = 0; k < ch.pos_key_count; ++k)
                    bc.vecPositionKeys.push_back({
                        static_cast<f64_t>(posKeys[k].time_ticks),
                        { posKeys[k].x, posKeys[k].y, posKeys[k].z } });

                const auto* rotKeys = reinterpret_cast<const QuatKey*>(keyBlock.data() + ch.rot_offset);
                for (uint32_t k = 0; k < ch.rot_key_count; ++k)
                    bc.vecRotationKeys.push_back({
                        static_cast<f64_t>(rotKeys[k].time_ticks),
                        { rotKeys[k].x, rotKeys[k].y, rotKeys[k].z, rotKeys[k].w } });

                const auto* sclKeys = reinterpret_cast<const VectorKey*>(keyBlock.data() + ch.scl_offset);
                for (uint32_t k = 0; k < ch.scl_key_count; ++k)
                    bc.vecScaleKeys.push_back({
                        static_cast<f64_t>(sclKeys[k].time_ticks),
                        { sclKeys[k].x, sclKeys[k].y, sclKeys[k].z } });

                anim->AddChannel(bc);
            }

            if (pSkeleton)
                anim->ResolveBoneIndices(pSkeleton);
            return anim;
        }
        catch (const std::exception&)
        {
            return nullptr;
        }
    }
}
