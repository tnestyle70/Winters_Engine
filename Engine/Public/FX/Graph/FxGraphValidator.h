#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/Graph/FxGraph.h"

#include <string>
#include <vector>

struct FxValidationIssue
{
    u32_t nodeId = 0;
    std::string message;
    bool_t bError = true;
};

struct FxValidationResult
{
    bool_t bValid = false;
    std::vector<FxValidationIssue> issues;
    std::vector<u32_t> topoOrder;
};

class CFxGraphValidator
{
public:
    WINTERS_ENGINE static FxValidationResult Validate(const FxEmitterGraph& emitter);
};
