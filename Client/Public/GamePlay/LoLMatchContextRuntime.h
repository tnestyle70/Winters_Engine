#pragma once

#include "Shared/GameSim/Definitions/LoLMatchContext.h"

namespace Client
{
    class CLoLMatchContextRuntime final
    {
    public:
        static CLoLMatchContextRuntime& Instance();

        MatchContext& Context() { return m_Context; }
        const MatchContext& Context() const { return m_Context; }
        void Reset() { m_Context = MatchContext{}; }

    private:
        CLoLMatchContextRuntime() = default;

        MatchContext m_Context{};
    };
}
