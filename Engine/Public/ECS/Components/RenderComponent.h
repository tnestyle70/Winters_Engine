#pragma once

#include "WintersTypes.h"

class ModelRenderer;

// Client-side render bridge attached to ECS entities.
// The renderer pointer is a non-owning view; the owning container lives in the
// scene or client-side spawn service and is intentionally not replicated.
struct RenderComponent
{
    ModelRenderer* pRenderer = nullptr;
    bool_t bVisible = true;
    bool_t bAnimated = true;
    bool_t bSceneManaged = false;
};
