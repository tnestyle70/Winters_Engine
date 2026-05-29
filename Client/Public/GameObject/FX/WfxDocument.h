#pragma once

#include "Defines.h"
#include "FX/FxAsset.h"

#include <string>

namespace WfxTool
{
    class CWfxDocument final
    {
    public:
        bool_t LoadFromFile(const wstring_t& strPath);
        bool_t SaveToFile(const wstring_t& strPath) const;
        bool_t Save() const;

        void SetAsset(FxAsset asset, const wstring_t& strPath);

        bool_t IsLoaded() const { return m_bLoaded; }
        FxAsset& GetAsset() { return m_Asset; }
        const FxAsset& GetAsset() const { return m_Asset; }
        const wstring_t& GetPath() const { return m_strPath; }
        const std::string& GetLastError() const { return m_strLastError; }

    private:
        mutable std::string m_strLastError;
        wstring_t m_strPath;
        FxAsset m_Asset{};
        bool_t m_bLoaded = false;
    };
}
