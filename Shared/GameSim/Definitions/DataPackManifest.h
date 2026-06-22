#pragma once

#include "WintersTypes.h"

enum class eDataPackVisibility : u8_t
{
    ServerPrivate = 0,
    ClientPublic = 1,
    SharedContract = 2,
    TestOnly = 3,
};

struct DataPackManifest
{
    u32_t uSchemaVersion = 1;
    u32_t uDataVersion = 1;
    u32_t uBuildHash = 0;
    u32_t uRulesetId = 0;
    eDataPackVisibility eVisibility = eDataPackVisibility::SharedContract;
};
