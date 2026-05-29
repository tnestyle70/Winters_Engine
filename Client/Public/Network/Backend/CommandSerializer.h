#pragma once

#include "WintersTypes.h"

#include <vector>

class CCommandSerializer
{
public:
    CCommandSerializer() = default;

    const std::vector<u8_t>& LastPayload() const { return m_LastPayload; }

private:
    std::vector<u8_t> m_LastPayload;
};
