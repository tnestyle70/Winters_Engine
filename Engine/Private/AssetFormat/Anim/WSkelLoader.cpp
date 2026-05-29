#include "AssetFormat/Anim/WSkelLoader.h"
#include "AssetFormat/Common/BinaryReader.h"
#include "AssetFormat/Common/Hash.h"
#include "AssetFormat/Common/WintersFileHeader.h"
#include <cstring>
#include <exception>

namespace Winters::Asset
{
    static constexpr uint32_t MAX_SKEL_BONES = 512;
    static constexpr uint32_t MAX_SKEL_SOCKETS = 256;

    static bool ValidateHeader(const SkelMetaHeader& h)
    {
        if (std::memcmp(h.magic, WSKEL_MAGIC, 4) != 0) return false;
        if (h.bone_count == 0 || h.bone_count > MAX_SKEL_BONES) return false;
        if (h.socket_count > MAX_SKEL_SOCKETS) return false;
        return true;
    }

    bool CWSkelLoader::Load(const wchar_t* path, WSkelLoaded& out)
    {
        out = {};

        std::vector<uint8_t> file;
        if (!CBinaryReader::LoadFileToMemory(path, file))
            return false;

        try
        {
            CBinaryReader r(file.data(), file.size());

            const auto fh = r.Read<WintersFileHeader>();
            if (std::memcmp(fh.magic, WINTERS_MAGIC, 4) != 0) return false;
            if (fh.version_major != 1) return false;
            if (fh.flags != WF_NONE) return false;
            if (fh.content_size > r.Remaining()) return false;

            const uint8_t* payload = r.Peek();
            out.rawPayload.assign(payload, payload + fh.content_size);

            CBinaryReader pr(out.rawPayload.data(), out.rawPayload.size());
            out.header = pr.Read<SkelMetaHeader>();
            if (!ValidateHeader(out.header)) return false;

            const size_t bonesBytes = static_cast<size_t>(out.header.bone_count) * sizeof(BoneNode);
            const size_t socketsBytes = static_cast<size_t>(out.header.socket_count) * sizeof(SocketEntry);
            const size_t required = sizeof(SkelMetaHeader) + bonesBytes + sizeof(GlobalRootMatrix) + socketsBytes;
            if (required > out.rawPayload.size()) return false;

            out.bones = reinterpret_cast<const BoneNode*>(out.rawPayload.data() + sizeof(SkelMetaHeader));
            out.globalRoot = reinterpret_cast<const GlobalRootMatrix*>(
                reinterpret_cast<const uint8_t*>(out.bones) + bonesBytes);
            out.sockets = (out.header.socket_count > 0)
                ? reinterpret_cast<const SocketEntry*>(
                    reinterpret_cast<const uint8_t*>(out.globalRoot) + sizeof(GlobalRootMatrix))
                : nullptr;

            uint64_t hash = 0xcbf29ce484222325ull;
            for (uint32_t i = 0; i < out.header.bone_count; ++i)
            {
                if (out.bones[i].parent_index >= static_cast<int32_t>(out.header.bone_count))
                    return false;
                hash ^= out.bones[i].name_hash;
                hash *= 0x100000001b3ull;
            }
            out.skelHash = hash;
            return true;
        }
        catch (const std::exception&)
        {
            out = {};
            return false;
        }
    }
}
