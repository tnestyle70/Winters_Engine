#pragma once
#include "AssetFormat/Anim/WSkelFormat.h"
#include <vector>

namespace Winters::Asset
{
    struct WSkelLoaded
    {
        SkelMetaHeader header{};
        std::vector<uint8_t> rawPayload;
        const BoneNode* bones = nullptr;
        const GlobalRootMatrix* globalRoot = nullptr;
        const SocketEntry* sockets = nullptr;
        uint64_t skelHash = 0;
    };

    class WINTERS_ENGINE CWSkelLoader
    {
    public:
        static bool Load(const wchar_t* path, WSkelLoaded& out);
    };
}
