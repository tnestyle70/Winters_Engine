#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"

// Resolves a content file path without relying on the process current directory.
// Tries, in order:
//   1) <directory of WintersGame.exe> + relativePath
//   2) <current working directory> + relativePath
//   3) GetFullPathNameW(relativePath) when it refers to an existing file
// On success, fills outFullPath with a path suitable for D3DCompileFromFile.
WINTERS_ENGINE bool WintersResolveContentPath(const wchar_t* relativePath, wchar_t* outFullPath, uint32_t outCapacityChars);
