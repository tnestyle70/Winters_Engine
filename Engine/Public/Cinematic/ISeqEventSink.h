#pragma once

#include "WintersAPI.h"

#include <string>

class WINTERS_ENGINE ISeqEventSink
{
public:
    virtual ~ISeqEventSink() = default;

    virtual void PushCandidate(const std::string& strEvent, const std::string& strPayload)
    {
        (void)strEvent;
        (void)strPayload;
    }
};
