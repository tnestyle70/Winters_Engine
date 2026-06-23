#include "FX/Graph/FxGraph.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <utility>

namespace
{
    struct JsonValue
    {
        enum class eType
        {
            Null,
            Bool,
            Number,
            String,
            Array,
            Object,
        };

        eType eValueType = eType::Null;
        bool_t bValue = false;
        f64_t dValue = 0.0;
        std::string strValue;
        std::vector<JsonValue> arrValue;
        std::map<std::string, JsonValue> objValue;
    };

    class JsonParser
    {
    public:
        explicit JsonParser(const std::string& strText)
            : m_strText(strText)
        {
        }

        bool_t Parse(JsonValue& outValue)
        {
            SkipWhitespace();
            if (!ParseValue(outValue))
                return false;
            SkipWhitespace();
            return m_iPos == m_strText.size();
        }

    private:
        bool_t ParseValue(JsonValue& outValue)
        {
            SkipWhitespace();
            if (m_iPos >= m_strText.size())
                return false;

            const char ch = m_strText[m_iPos];
            if (ch == '{')
                return ParseObject(outValue);
            if (ch == '[')
                return ParseArray(outValue);
            if (ch == '"')
                return ParseStringValue(outValue);
            if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0)
                return ParseNumber(outValue);
            if (ConsumeLiteral("true"))
            {
                outValue = {};
                outValue.eValueType = JsonValue::eType::Bool;
                outValue.bValue = true;
                return true;
            }
            if (ConsumeLiteral("false"))
            {
                outValue = {};
                outValue.eValueType = JsonValue::eType::Bool;
                outValue.bValue = false;
                return true;
            }
            if (ConsumeLiteral("null"))
            {
                outValue = {};
                return true;
            }

            return false;
        }

        bool_t ParseObject(JsonValue& outValue)
        {
            if (!ConsumeChar('{'))
                return false;

            outValue = {};
            outValue.eValueType = JsonValue::eType::Object;
            SkipWhitespace();
            if (ConsumeChar('}'))
                return true;

            for (;;)
            {
                std::string strKey;
                if (!ParseString(strKey))
                    return false;
                if (!ConsumeChar(':'))
                    return false;

                JsonValue value;
                if (!ParseValue(value))
                    return false;
                outValue.objValue[strKey] = std::move(value);

                if (ConsumeChar('}'))
                    return true;
                if (!ConsumeChar(','))
                    return false;
            }
        }

        bool_t ParseArray(JsonValue& outValue)
        {
            if (!ConsumeChar('['))
                return false;

            outValue = {};
            outValue.eValueType = JsonValue::eType::Array;
            SkipWhitespace();
            if (ConsumeChar(']'))
                return true;

            for (;;)
            {
                JsonValue value;
                if (!ParseValue(value))
                    return false;
                outValue.arrValue.push_back(std::move(value));

                if (ConsumeChar(']'))
                    return true;
                if (!ConsumeChar(','))
                    return false;
            }
        }

        bool_t ParseStringValue(JsonValue& outValue)
        {
            std::string str;
            if (!ParseString(str))
                return false;

            outValue = {};
            outValue.eValueType = JsonValue::eType::String;
            outValue.strValue = std::move(str);
            return true;
        }

        bool_t ParseString(std::string& outString)
        {
            SkipWhitespace();
            if (!ConsumeChar('"'))
                return false;

            outString.clear();
            while (m_iPos < m_strText.size())
            {
                const char ch = m_strText[m_iPos++];
                if (ch == '"')
                    return true;
                if (ch != '\\')
                {
                    outString.push_back(ch);
                    continue;
                }

                if (m_iPos >= m_strText.size())
                    return false;

                const char esc = m_strText[m_iPos++];
                switch (esc)
                {
                case '"': outString.push_back('"'); break;
                case '\\': outString.push_back('\\'); break;
                case '/': outString.push_back('/'); break;
                case 'b': outString.push_back('\b'); break;
                case 'f': outString.push_back('\f'); break;
                case 'n': outString.push_back('\n'); break;
                case 'r': outString.push_back('\r'); break;
                case 't': outString.push_back('\t'); break;
                case 'u':
                    if (m_iPos + 4 > m_strText.size())
                        return false;
                    m_iPos += 4;
                    outString.push_back('?');
                    break;
                default:
                    return false;
                }
            }

            return false;
        }

        bool_t ParseNumber(JsonValue& outValue)
        {
            SkipWhitespace();
            const size_t iStart = m_iPos;

            if (m_iPos < m_strText.size() && m_strText[m_iPos] == '-')
                ++m_iPos;
            while (m_iPos < m_strText.size() &&
                std::isdigit(static_cast<unsigned char>(m_strText[m_iPos])) != 0)
            {
                ++m_iPos;
            }
            if (m_iPos < m_strText.size() && m_strText[m_iPos] == '.')
            {
                ++m_iPos;
                while (m_iPos < m_strText.size() &&
                    std::isdigit(static_cast<unsigned char>(m_strText[m_iPos])) != 0)
                {
                    ++m_iPos;
                }
            }
            if (m_iPos < m_strText.size() &&
                (m_strText[m_iPos] == 'e' || m_strText[m_iPos] == 'E'))
            {
                ++m_iPos;
                if (m_iPos < m_strText.size() &&
                    (m_strText[m_iPos] == '+' || m_strText[m_iPos] == '-'))
                {
                    ++m_iPos;
                }
                while (m_iPos < m_strText.size() &&
                    std::isdigit(static_cast<unsigned char>(m_strText[m_iPos])) != 0)
                {
                    ++m_iPos;
                }
            }

            if (iStart == m_iPos)
                return false;

            char* pEnd = nullptr;
            const std::string strNumber = m_strText.substr(iStart, m_iPos - iStart);
            const f64_t dValue = std::strtod(strNumber.c_str(), &pEnd);
            if (!pEnd || *pEnd != '\0')
                return false;

            outValue = {};
            outValue.eValueType = JsonValue::eType::Number;
            outValue.dValue = dValue;
            return true;
        }

        bool_t ConsumeLiteral(const char* pLiteral)
        {
            SkipWhitespace();
            const size_t iStart = m_iPos;
            while (*pLiteral)
            {
                if (m_iPos >= m_strText.size() || m_strText[m_iPos] != *pLiteral)
                {
                    m_iPos = iStart;
                    return false;
                }
                ++m_iPos;
                ++pLiteral;
            }
            return true;
        }

        bool_t ConsumeChar(char ch)
        {
            SkipWhitespace();
            if (m_iPos >= m_strText.size() || m_strText[m_iPos] != ch)
                return false;
            ++m_iPos;
            return true;
        }

        void SkipWhitespace()
        {
            while (m_iPos < m_strText.size() &&
                std::isspace(static_cast<unsigned char>(m_strText[m_iPos])) != 0)
            {
                ++m_iPos;
            }
        }

    private:
        const std::string& m_strText;
        size_t m_iPos = 0;
    };

    std::string ReadTextFile(const std::string& strPath)
    {
        std::ifstream file(strPath, std::ios::binary);
        if (!file)
            return {};

        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    void SetError(std::string* pOutError, const char* pError)
    {
        if (pOutError)
            *pOutError = pError;
    }

    const JsonValue* FindMember(const JsonValue& obj, const char* pName)
    {
        if (obj.eValueType != JsonValue::eType::Object)
            return nullptr;

        const auto it = obj.objValue.find(pName);
        return it != obj.objValue.end() ? &it->second : nullptr;
    }

    std::string GetString(const JsonValue& obj, const char* pName, const std::string& strFallback = {})
    {
        const JsonValue* pValue = FindMember(obj, pName);
        if (!pValue || pValue->eValueType != JsonValue::eType::String)
            return strFallback;
        return pValue->strValue;
    }

    f64_t GetNumber(const JsonValue& obj, const char* pName, f64_t dFallback = 0.0)
    {
        const JsonValue* pValue = FindMember(obj, pName);
        if (!pValue || pValue->eValueType != JsonValue::eType::Number)
            return dFallback;
        return pValue->dValue;
    }

    std::string NormalizeToken(std::string str)
    {
        std::transform(str.begin(), str.end(), str.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        str.erase(std::remove(str.begin(), str.end(), '_'), str.end());
        str.erase(std::remove(str.begin(), str.end(), '-'), str.end());
        return str;
    }

    bool_t TryParseNodeType(const std::string& strValue, eFxNodeType& outType)
    {
        const std::string token = NormalizeToken(strValue);
        if (token == "spawnburst") { outType = eFxNodeType::SpawnBurst; return true; }
        if (token == "spawnrate") { outType = eFxNodeType::SpawnRate; return true; }
        if (token == "initposition") { outType = eFxNodeType::InitPosition; return true; }
        if (token == "initvelocity") { outType = eFxNodeType::InitVelocity; return true; }
        if (token == "initlifetime") { outType = eFxNodeType::InitLifetime; return true; }
        if (token == "initcolor") { outType = eFxNodeType::InitColor; return true; }
        if (token == "age") { outType = eFxNodeType::Age; return true; }
        if (token == "gravity") { outType = eFxNodeType::Gravity; return true; }
        if (token == "drag") { outType = eFxNodeType::Drag; return true; }
        if (token == "sizeoverlife") { outType = eFxNodeType::SizeOverLife; return true; }
        if (token == "coloroverlife") { outType = eFxNodeType::ColorOverLife; return true; }
        if (token == "billboardrenderer" || token == "billboard") { outType = eFxNodeType::BillboardRenderer; return true; }
        if (token == "meshrenderer" || token == "mesh" || token == "meshparticle") { outType = eFxNodeType::MeshRenderer; return true; }
        if (token == "ribbonrenderer" || token == "ribbon") { outType = eFxNodeType::RibbonRenderer; return true; }
        return false;
    }

    const char* NodeTypeToString(eFxNodeType eType)
    {
        switch (eType)
        {
        case eFxNodeType::SpawnBurst: return "SpawnBurst";
        case eFxNodeType::SpawnRate: return "SpawnRate";
        case eFxNodeType::InitPosition: return "InitPosition";
        case eFxNodeType::InitVelocity: return "InitVelocity";
        case eFxNodeType::InitLifetime: return "InitLifetime";
        case eFxNodeType::InitColor: return "InitColor";
        case eFxNodeType::Age: return "Age";
        case eFxNodeType::Gravity: return "Gravity";
        case eFxNodeType::Drag: return "Drag";
        case eFxNodeType::SizeOverLife: return "SizeOverLife";
        case eFxNodeType::ColorOverLife: return "ColorOverLife";
        case eFxNodeType::BillboardRenderer: return "BillboardRenderer";
        case eFxNodeType::MeshRenderer: return "MeshRenderer";
        case eFxNodeType::RibbonRenderer: return "RibbonRenderer";
        default: return "SpawnBurst";
        }
    }

    eFxRenderType ParseRenderType(const std::string& strValue)
    {
        const std::string token = NormalizeToken(strValue);
        if (token == "ribbon") return eFxRenderType::Ribbon;
        if (token == "beam") return eFxRenderType::Beam;
        if (token == "grounddecal") return eFxRenderType::GroundDecal;
        if (token == "meshparticle" || token == "mesh") return eFxRenderType::MeshParticle;
        if (token == "shockwavering") return eFxRenderType::ShockwaveRing;
        return eFxRenderType::Billboard;
    }

    const char* RenderTypeToString(eFxRenderType eType)
    {
        switch (eType)
        {
        case eFxRenderType::Ribbon: return "Ribbon";
        case eFxRenderType::Beam: return "Beam";
        case eFxRenderType::GroundDecal: return "GroundDecal";
        case eFxRenderType::MeshParticle: return "MeshParticle";
        case eFxRenderType::ShockwaveRing: return "ShockwaveRing";
        case eFxRenderType::Billboard:
        default:
            return "Billboard";
        }
    }

    eFxParamScope ParseParamScope(const std::string& strValue)
    {
        const std::string token = NormalizeToken(strValue);
        if (token == "system") return eFxParamScope::System;
        if (token == "emitter") return eFxParamScope::Emitter;
        if (token == "particle") return eFxParamScope::Particle;
        if (token == "engine") return eFxParamScope::Engine;
        return eFxParamScope::User;
    }

    eFxParamScope InferParamScopeFromName(const std::string& strName)
    {
        const size_t dot = strName.find('.');
        if (dot == std::string::npos)
            return eFxParamScope::User;
        return ParseParamScope(strName.substr(0, dot));
    }

    const char* ParamScopeToString(eFxParamScope eScope)
    {
        switch (eScope)
        {
        case eFxParamScope::System: return "System";
        case eFxParamScope::Emitter: return "Emitter";
        case eFxParamScope::Particle: return "Particle";
        case eFxParamScope::Engine: return "Engine";
        case eFxParamScope::User:
        default:
            return "User";
        }
    }

    eFxGraphParamType ParseParamType(const std::string& strValue)
    {
        const std::string token = NormalizeToken(strValue);
        if (token == "int" || token == "i32") return eFxGraphParamType::Int;
        if (token == "uint" || token == "u32") return eFxGraphParamType::UInt;
        if (token == "bool") return eFxGraphParamType::Bool;
        if (token == "vec3") return eFxGraphParamType::Vec3;
        if (token == "vec4" || token == "color") return eFxGraphParamType::Vec4;
        if (token == "string") return eFxGraphParamType::String;
        return eFxGraphParamType::Float;
    }

    const char* ParamTypeToString(eFxGraphParamType eType)
    {
        switch (eType)
        {
        case eFxGraphParamType::Int: return "Int";
        case eFxGraphParamType::UInt: return "UInt";
        case eFxGraphParamType::Bool: return "Bool";
        case eFxGraphParamType::Vec3: return "Vec3";
        case eFxGraphParamType::Vec4: return "Vec4";
        case eFxGraphParamType::String: return "String";
        case eFxGraphParamType::Float:
        default:
            return "Float";
        }
    }

    eFxGraphParamType InferParamType(const FxGraphParamValue& value)
    {
        if (std::holds_alternative<i32_t>(value)) return eFxGraphParamType::Int;
        if (std::holds_alternative<u32_t>(value)) return eFxGraphParamType::UInt;
        if (std::holds_alternative<bool_t>(value)) return eFxGraphParamType::Bool;
        if (std::holds_alternative<Vec3>(value)) return eFxGraphParamType::Vec3;
        if (std::holds_alternative<Vec4>(value)) return eFxGraphParamType::Vec4;
        if (std::holds_alternative<std::string>(value)) return eFxGraphParamType::String;
        return eFxGraphParamType::Float;
    }

    bool_t ReadVec3Value(const JsonValue& value, Vec3& outVec)
    {
        if (value.eValueType != JsonValue::eType::Array || value.arrValue.size() < 3)
            return false;

        const JsonValue& x = value.arrValue[0];
        const JsonValue& y = value.arrValue[1];
        const JsonValue& z = value.arrValue[2];
        if (x.eValueType != JsonValue::eType::Number ||
            y.eValueType != JsonValue::eType::Number ||
            z.eValueType != JsonValue::eType::Number)
        {
            return false;
        }

        outVec = Vec3{
            static_cast<f32_t>(x.dValue),
            static_cast<f32_t>(y.dValue),
            static_cast<f32_t>(z.dValue)
        };
        return true;
    }

    bool_t ReadVec4Value(const JsonValue& value, Vec4& outVec)
    {
        if (value.eValueType != JsonValue::eType::Array || value.arrValue.size() < 4)
            return false;

        const JsonValue& x = value.arrValue[0];
        const JsonValue& y = value.arrValue[1];
        const JsonValue& z = value.arrValue[2];
        const JsonValue& w = value.arrValue[3];
        if (x.eValueType != JsonValue::eType::Number ||
            y.eValueType != JsonValue::eType::Number ||
            z.eValueType != JsonValue::eType::Number ||
            w.eValueType != JsonValue::eType::Number)
        {
            return false;
        }

        outVec = Vec4{
            static_cast<f32_t>(x.dValue),
            static_cast<f32_t>(y.dValue),
            static_cast<f32_t>(z.dValue),
            static_cast<f32_t>(w.dValue)
        };
        return true;
    }

    FxGraphParamValue DefaultParamValue(eFxGraphParamType eType)
    {
        switch (eType)
        {
        case eFxGraphParamType::Int: return i32_t{ 0 };
        case eFxGraphParamType::UInt: return u32_t{ 0 };
        case eFxGraphParamType::Bool: return bool_t{ false };
        case eFxGraphParamType::Vec3: return Vec3{ 0.f, 0.f, 0.f };
        case eFxGraphParamType::Vec4: return Vec4{ 0.f, 0.f, 0.f, 0.f };
        case eFxGraphParamType::String: return std::string{};
        case eFxGraphParamType::Float:
        default:
            return f32_t{ 0.f };
        }
    }

    FxGraphParamValue ParseParamValue(const JsonValue& value, eFxGraphParamType eExpectedType)
    {
        switch (eExpectedType)
        {
        case eFxGraphParamType::Int:
            if (value.eValueType == JsonValue::eType::Number)
                return static_cast<i32_t>(value.dValue);
            break;
        case eFxGraphParamType::UInt:
            if (value.eValueType == JsonValue::eType::Number && value.dValue >= 0.0)
                return static_cast<u32_t>(value.dValue);
            break;
        case eFxGraphParamType::Bool:
            if (value.eValueType == JsonValue::eType::Bool)
                return value.bValue;
            break;
        case eFxGraphParamType::Vec3:
        {
            Vec3 vec{};
            if (ReadVec3Value(value, vec))
                return vec;
            break;
        }
        case eFxGraphParamType::Vec4:
        {
            Vec4 vec{};
            if (ReadVec4Value(value, vec))
                return vec;
            break;
        }
        case eFxGraphParamType::String:
            if (value.eValueType == JsonValue::eType::String)
                return value.strValue;
            break;
        case eFxGraphParamType::Float:
        default:
            if (value.eValueType == JsonValue::eType::Number)
                return static_cast<f32_t>(value.dValue);
            break;
        }

        return DefaultParamValue(eExpectedType);
    }

    FxGraphParamValue ParseNodeParamValue(const JsonValue& value)
    {
        if (value.eValueType == JsonValue::eType::String)
            return value.strValue;
        if (value.eValueType == JsonValue::eType::Bool)
            return value.bValue;
        if (value.eValueType == JsonValue::eType::Number)
            return static_cast<f32_t>(value.dValue);

        Vec4 vec4{};
        if (ReadVec4Value(value, vec4))
            return vec4;

        Vec3 vec3{};
        if (ReadVec3Value(value, vec3))
            return vec3;

        return std::string{};
    }

    bool_t TryParseCurve(const JsonValue& value, std::vector<FxGraphCurveKey>& outCurve)
    {
        if (value.eValueType != JsonValue::eType::Array)
            return false;

        std::vector<FxGraphCurveKey> parsed;
        for (const JsonValue& keyValue : value.arrValue)
        {
            if (keyValue.eValueType != JsonValue::eType::Array || keyValue.arrValue.size() < 2)
                return false;

            const JsonValue& timeValue = keyValue.arrValue[0];
            if (timeValue.eValueType != JsonValue::eType::Number)
                return false;

            Vec4 v{};
            const JsonValue& valuePart = keyValue.arrValue[1];
            if (valuePart.eValueType == JsonValue::eType::Number)
            {
                const f32_t f = static_cast<f32_t>(valuePart.dValue);
                v = Vec4{ f, f, f, f };
            }
            else if (!ReadVec4Value(valuePart, v))
            {
                return false;
            }

            FxGraphCurveKey key{};
            key.fTime = static_cast<f32_t>(timeValue.dValue);
            key.vValue = v;
            parsed.push_back(key);
        }

        outCurve = std::move(parsed);
        return true;
    }

    void WriteEscapedString(std::ostream& os, const std::string& str)
    {
        os << '"';
        for (char ch : str)
        {
            switch (ch)
            {
            case '"': os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\n': os << "\\n"; break;
            case '\r': os << "\\r"; break;
            case '\t': os << "\\t"; break;
            default: os << ch; break;
            }
        }
        os << '"';
    }

    void WriteVec3(std::ostream& os, const Vec3& v)
    {
        os << '[' << v.x << ", " << v.y << ", " << v.z << ']';
    }

    void WriteVec4(std::ostream& os, const Vec4& v)
    {
        os << '[' << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ']';
    }

    void WriteParamValue(std::ostream& os, const FxGraphParamValue& value)
    {
        if (const f32_t* pFloat = std::get_if<f32_t>(&value))
            os << *pFloat;
        else if (const i32_t* pInt = std::get_if<i32_t>(&value))
            os << *pInt;
        else if (const u32_t* pUInt = std::get_if<u32_t>(&value))
            os << *pUInt;
        else if (const bool_t* pBool = std::get_if<bool_t>(&value))
            os << (*pBool ? "true" : "false");
        else if (const Vec3* pVec3 = std::get_if<Vec3>(&value))
            WriteVec3(os, *pVec3);
        else if (const Vec4* pVec4 = std::get_if<Vec4>(&value))
            WriteVec4(os, *pVec4);
        else if (const std::string* pString = std::get_if<std::string>(&value))
            WriteEscapedString(os, *pString);
        else
            os << "0";
    }

    void WriteCurve(std::ostream& os, const std::vector<FxGraphCurveKey>& curve)
    {
        os << '[';
        for (size_t i = 0; i < curve.size(); ++i)
        {
            if (i > 0)
                os << ", ";
            os << '[' << curve[i].fTime << ", ";
            WriteVec4(os, curve[i].vValue);
            os << ']';
        }
        os << ']';
    }

    void WriteNodeParams(std::ostream& os, const FxGraphNode& node)
    {
        std::vector<std::string> keys;
        keys.reserve(node.params.size());
        for (const auto& pair : node.params)
            keys.push_back(pair.first);
        std::sort(keys.begin(), keys.end());

        os << "{";
        bool_t bWroteAny = false;
        for (const std::string& key : keys)
        {
            if (key == "curve" && !node.curve.empty())
                continue;

            if (bWroteAny)
                os << ", ";
            WriteEscapedString(os, key);
            os << ": ";
            WriteParamValue(os, node.params.at(key));
            bWroteAny = true;
        }

        if (!node.curve.empty())
        {
            if (bWroteAny)
                os << ", ";
            WriteEscapedString(os, "curve");
            os << ": ";
            WriteCurve(os, node.curve);
        }
        os << "}";
    }

    void WriteGraphBlock(std::ostream& os, const CFxGraph& graph, i32_t iBaseIndent)
    {
        const std::string indent(iBaseIndent, ' ');
        const std::string child = indent + "  ";
        const std::string grand = child + "  ";
        const std::string great = grand + "  ";

        os << indent << "{\n";
        os << child << "\"userParams\": [\n";
        for (size_t i = 0; i < graph.userParams.size(); ++i)
        {
            const FxGraphUserParam& param = graph.userParams[i];
            const eFxGraphParamType eType = param.type == InferParamType(param.value) ? param.type : InferParamType(param.value);
            os << grand << "{ \"name\": ";
            WriteEscapedString(os, param.name);
            os << ", \"scope\": ";
            WriteEscapedString(os, ParamScopeToString(param.scope));
            os << ", \"type\": ";
            WriteEscapedString(os, ParamTypeToString(eType));
            os << ", \"value\": ";
            WriteParamValue(os, param.value);
            os << " }";
            if (i + 1 < graph.userParams.size())
                os << ',';
            os << "\n";
        }
        os << child << "],\n";

        os << child << "\"emitterGraphs\": [\n";
        for (size_t iEmitter = 0; iEmitter < graph.emitterGraphs.size(); ++iEmitter)
        {
            const FxEmitterGraph& emitter = graph.emitterGraphs[iEmitter];
            os << grand << "{\n";
            os << great << "\"name\": ";
            WriteEscapedString(os, emitter.strName);
            os << ",\n";
            os << great << "\"renderType\": ";
            WriteEscapedString(os, RenderTypeToString(emitter.renderType));
            os << ",\n";

            os << great << "\"nodes\": [\n";
            for (size_t iNode = 0; iNode < emitter.nodes.size(); ++iNode)
            {
                const FxGraphNode& node = emitter.nodes[iNode];
                os << great << "  { \"id\": " << node.id << ", \"type\": ";
                WriteEscapedString(os, NodeTypeToString(node.type));
                os << ", \"x\": " << node.x << ", \"y\": " << node.y << ", \"params\": ";
                WriteNodeParams(os, node);
                os << " }";
                if (iNode + 1 < emitter.nodes.size())
                    os << ',';
                os << "\n";
            }
            os << great << "],\n";

            os << great << "\"edges\": [\n";
            for (size_t iEdge = 0; iEdge < emitter.edges.size(); ++iEdge)
            {
                const FxGraphEdge& edge = emitter.edges[iEdge];
                os << great << "  { \"from\": " << edge.from << ", \"to\": " << edge.to << " }";
                if (iEdge + 1 < emitter.edges.size())
                    os << ',';
                os << "\n";
            }
            os << great << "]\n";
            os << grand << "}";
            if (iEmitter + 1 < graph.emitterGraphs.size())
                os << ',';
            os << "\n";
        }
        os << child << "]\n";
        os << indent << "}";
    }

    void ParseUserParams(const JsonValue& graphRoot, CFxGraph& outGraph)
    {
        const JsonValue* pParams = FindMember(graphRoot, "userParams");
        if (!pParams || pParams->eValueType != JsonValue::eType::Array)
            return;

        for (const JsonValue& value : pParams->arrValue)
        {
            if (value.eValueType != JsonValue::eType::Object)
                continue;

            FxGraphUserParam param{};
            param.name = GetString(value, "name");
            if (param.name.empty())
                continue;

            const std::string strType = GetString(value, "type", "Float");
            param.type = ParseParamType(strType);

            const std::string strScope = GetString(value, "scope");
            param.scope = strScope.empty() ? InferParamScopeFromName(param.name) : ParseParamScope(strScope);

            if (const JsonValue* pParamValue = FindMember(value, "value"))
                param.value = ParseParamValue(*pParamValue, param.type);
            else
                param.value = DefaultParamValue(param.type);

            outGraph.userParams.push_back(std::move(param));
        }
    }

    void ParseNodes(const JsonValue& emitterValue, FxEmitterGraph& outEmitter)
    {
        const JsonValue* pNodes = FindMember(emitterValue, "nodes");
        if (!pNodes || pNodes->eValueType != JsonValue::eType::Array)
            return;

        for (const JsonValue& nodeValue : pNodes->arrValue)
        {
            if (nodeValue.eValueType != JsonValue::eType::Object)
                continue;

            eFxNodeType eType = eFxNodeType::SpawnBurst;
            if (!TryParseNodeType(GetString(nodeValue, "type"), eType))
                continue;

            FxGraphNode node{};
            node.id = static_cast<u32_t>(GetNumber(nodeValue, "id", 0.0));
            node.type = eType;
            node.x = static_cast<f32_t>(GetNumber(nodeValue, "x", 0.0));
            node.y = static_cast<f32_t>(GetNumber(nodeValue, "y", 0.0));

            const JsonValue* pParams = FindMember(nodeValue, "params");
            if (pParams && pParams->eValueType == JsonValue::eType::Object)
            {
                for (const auto& pair : pParams->objValue)
                {
                    if (pair.first == "curve" && TryParseCurve(pair.second, node.curve))
                        continue;
                    node.params[pair.first] = ParseNodeParamValue(pair.second);
                }
            }

            const JsonValue* pCurve = FindMember(nodeValue, "curve");
            if (pCurve)
                TryParseCurve(*pCurve, node.curve);

            outEmitter.nodes.push_back(std::move(node));
        }
    }

    void ParseEdges(const JsonValue& emitterValue, FxEmitterGraph& outEmitter)
    {
        const JsonValue* pEdges = FindMember(emitterValue, "edges");
        if (!pEdges || pEdges->eValueType != JsonValue::eType::Array)
            return;

        for (const JsonValue& edgeValue : pEdges->arrValue)
        {
            if (edgeValue.eValueType != JsonValue::eType::Object)
                continue;

            FxGraphEdge edge{};
            edge.from = static_cast<u32_t>(GetNumber(edgeValue, "from", 0.0));
            edge.to = static_cast<u32_t>(GetNumber(edgeValue, "to", 0.0));
            outEmitter.edges.push_back(edge);
        }
    }

    void ParseEmitterGraphs(const JsonValue& graphRoot, CFxGraph& outGraph)
    {
        const JsonValue* pEmitters = FindMember(graphRoot, "emitterGraphs");
        if (!pEmitters || pEmitters->eValueType != JsonValue::eType::Array)
            return;

        for (const JsonValue& emitterValue : pEmitters->arrValue)
        {
            if (emitterValue.eValueType != JsonValue::eType::Object)
                continue;

            FxEmitterGraph emitter{};
            emitter.strName = GetString(emitterValue, "name");
            emitter.renderType = ParseRenderType(GetString(emitterValue, "renderType", "Billboard"));
            ParseNodes(emitterValue, emitter);
            ParseEdges(emitterValue, emitter);
            outGraph.emitterGraphs.push_back(std::move(emitter));
        }
    }

    const JsonValue* SelectGraphRoot(const JsonValue& root)
    {
        if (const JsonValue* pGraph = FindMember(root, "graph"))
        {
            if (pGraph->eValueType == JsonValue::eType::Object)
                return pGraph;
        }

        if (FindMember(root, "emitterGraphs") || FindMember(root, "userParams"))
            return &root;

        return nullptr;
    }
}

bool_t CFxGraph::LoadFromJson(
    const std::string& strPath,
    CFxGraph& outGraph,
    std::string* pOutError)
{
    const std::string strJson = ReadTextFile(strPath);
    if (strJson.empty())
    {
        SetError(pOutError, "empty_or_missing_fx_graph_json");
        return false;
    }

    JsonValue root;
    JsonParser parser(strJson);
    if (!parser.Parse(root) || root.eValueType != JsonValue::eType::Object)
    {
        SetError(pOutError, "invalid_fx_graph_json");
        return false;
    }

    CFxGraph parsed;
    if (const JsonValue* pGraphRoot = SelectGraphRoot(root))
    {
        ParseUserParams(*pGraphRoot, parsed);
        ParseEmitterGraphs(*pGraphRoot, parsed);
    }

    outGraph = std::move(parsed);
    if (pOutError)
        pOutError->clear();
    return true;
}

bool_t CFxGraph::SaveToJson(
    const std::string& strPath,
    std::string* pOutError) const
{
    std::ofstream file(strPath, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        SetError(pOutError, "cannot_open_fx_graph_json_for_write");
        return false;
    }

    file << std::fixed << std::setprecision(6);
    file << "{\n";
    file << "  \"schema\": \"WintersWfx\",\n";
    file << "  \"version\": 2,\n";
    file << "  \"emitters\": [],\n";
    file << "  \"graph\": ";
    WriteGraphBlock(file, *this, 2);
    file << "\n";
    file << "}\n";

    if (!file)
    {
        SetError(pOutError, "failed_writing_fx_graph_json");
        return false;
    }

    if (pOutError)
        pOutError->clear();
    return true;
}
