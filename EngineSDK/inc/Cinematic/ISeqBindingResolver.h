#pragma once

#include "Cinematic/CSequenceAsset.h"

#include <string>

class CCamera;
class ModelRenderer;

class WINTERS_ENGINE ISeqBindingResolver
{
public:
    virtual ~ISeqBindingResolver() = default;

    virtual CCamera* ResolveCamera(const std::string& strBinding) { (void)strBinding; return nullptr; }
    virtual ModelRenderer* ResolveModel(const std::string& strBinding) { (void)strBinding; return nullptr; }

    virtual bool_t ResolveCameraProjection(
        const std::string& strBinding,
        f32_t& fOutAspect,
        f32_t& fOutNearZ,
        f32_t& fOutFarZ)
    {
        (void)strBinding;
        (void)fOutAspect;
        (void)fOutNearZ;
        (void)fOutFarZ;
        return false;
    }

    virtual void TriggerFx(const std::string& strBinding, const SeqFxKey& key)
    {
        (void)strBinding;
        (void)key;
    }

    virtual void TriggerAudio(const std::string& strBinding, const SeqAudioKey& key)
    {
        (void)strBinding;
        (void)key;
    }

    virtual void SetVisibility(const std::string& strBinding, bool_t bVisible)
    {
        (void)strBinding;
        (void)bVisible;
    }

    virtual void SetTimeDilation(f32_t fScale)
    {
        (void)fScale;
    }
};
