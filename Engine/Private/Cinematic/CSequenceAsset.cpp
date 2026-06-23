#include "Cinematic/CSequenceAsset.h"

#include <algorithm>
#include <cctype>
#include <cmath>
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
                outValue.eValueType = JsonValue::eType::Bool;
                outValue.bValue = true;
                return true;
            }
            if (ConsumeLiteral("false"))
            {
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

    bool_t GetBool(const JsonValue& obj, const char* pName, bool_t bFallback = false)
    {
        const JsonValue* pValue = FindMember(obj, pName);
        if (!pValue || pValue->eValueType != JsonValue::eType::Bool)
            return bFallback;
        return pValue->bValue;
    }

    bool_t ReadVec3(const JsonValue& obj, const char* pName, Vec3& outVec)
    {
        const JsonValue* pValue = FindMember(obj, pName);
        if (!pValue || pValue->eValueType != JsonValue::eType::Array ||
            pValue->arrValue.size() < 3)
        {
            return false;
        }

        const JsonValue& x = pValue->arrValue[0];
        const JsonValue& y = pValue->arrValue[1];
        const JsonValue& z = pValue->arrValue[2];
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

    std::string ToLower(std::string str)
    {
        std::transform(str.begin(), str.end(), str.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return str;
    }

    bool_t TryParseTrackType(const std::string& strType, eSeqTrackType& outType)
    {
        const std::string strLower = ToLower(strType);
        if (strLower == "camera") { outType = eSeqTrackType::Camera; return true; }
        if (strLower == "anim") { outType = eSeqTrackType::Anim; return true; }
        if (strLower == "fx") { outType = eSeqTrackType::Fx; return true; }
        if (strLower == "audio") { outType = eSeqTrackType::Audio; return true; }
        if (strLower == "event") { outType = eSeqTrackType::Event; return true; }
        if (strLower == "visibility") { outType = eSeqTrackType::Visibility; return true; }
        if (strLower == "timedilation") { outType = eSeqTrackType::TimeDilation; return true; }
        return false;
    }

    eSeqInterp ParseInterp(const std::string& strInterp)
    {
        const std::string strLower = ToLower(strInterp);
        if (strLower == "constant")
            return eSeqInterp::Constant;
        if (strLower == "cubic")
            return eSeqInterp::Cubic;
        return eSeqInterp::Linear;
    }

    const char* TrackTypeToString(eSeqTrackType eType)
    {
        switch (eType)
        {
        case eSeqTrackType::Camera: return "Camera";
        case eSeqTrackType::Anim: return "Anim";
        case eSeqTrackType::Fx: return "Fx";
        case eSeqTrackType::Audio: return "Audio";
        case eSeqTrackType::Event: return "Event";
        case eSeqTrackType::Visibility: return "Visibility";
        case eSeqTrackType::TimeDilation: return "TimeDilation";
        default: return "Unknown";
        }
    }

    const char* InterpToString(eSeqInterp eInterp)
    {
        switch (eInterp)
        {
        case eSeqInterp::Constant: return "constant";
        case eSeqInterp::Cubic: return "cubic";
        case eSeqInterp::Linear:
        default:
            return "linear";
        }
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

    template <typename T>
    void SortByTime(std::vector<T>& keys)
    {
        std::sort(keys.begin(), keys.end(),
            [](const T& lhs, const T& rhs)
            {
                return lhs.dTimeSec < rhs.dTimeSec;
            });
    }

    template <typename T>
    bool_t IsKeyTimeSorted(const std::vector<T>& keys)
    {
        for (size_t i = 1; i < keys.size(); ++i)
        {
            if (keys[i].dTimeSec < keys[i - 1].dTimeSec)
                return false;
        }
        return true;
    }

    template <typename T>
    void AppendUnsortedKeyDiagnostic(
        const std::vector<T>& keys,
        const std::string& strTrackLabel,
        const char* pKeyName,
        std::vector<std::string>& outErrors)
    {
        if (!IsKeyTimeSorted(keys))
            outErrors.push_back(strTrackLabel + ": " + pKeyName + " are not sorted by time");
    }

    bool_t IsKnownTrackType(eSeqTrackType eType)
    {
        switch (eType)
        {
        case eSeqTrackType::Camera:
        case eSeqTrackType::Anim:
        case eSeqTrackType::Fx:
        case eSeqTrackType::Audio:
        case eSeqTrackType::Event:
        case eSeqTrackType::Visibility:
        case eSeqTrackType::TimeDilation:
            return true;
        default:
            return false;
        }
    }

    bool_t TrackRequiresBinding(eSeqTrackType eType)
    {
        switch (eType)
        {
        case eSeqTrackType::Camera:
        case eSeqTrackType::Anim:
        case eSeqTrackType::Fx:
        case eSeqTrackType::Visibility:
            return true;
        default:
            return false;
        }
    }

    std::string MakeTrackLabel(size_t iTrack, const SeqTrack& track)
    {
        std::ostringstream ss;
        ss << "track[" << iTrack << "]";
        if (!track.strName.empty())
            ss << " '" << track.strName << "'";
        ss << " (" << TrackTypeToString(track.eType) << ")";
        return ss.str();
    }

    void AppendTrackOrderDiagnostics(
        const SeqTrack& track,
        size_t iTrack,
        std::vector<std::string>& outErrors)
    {
        const std::string strTrackLabel = MakeTrackLabel(iTrack, track);
        AppendUnsortedKeyDiagnostic(track.cameraKeys, strTrackLabel, "camera keys", outErrors);
        AppendUnsortedKeyDiagnostic(track.animKeys, strTrackLabel, "anim keys", outErrors);
        AppendUnsortedKeyDiagnostic(track.fxKeys, strTrackLabel, "fx keys", outErrors);
        AppendUnsortedKeyDiagnostic(track.audioKeys, strTrackLabel, "audio keys", outErrors);
        AppendUnsortedKeyDiagnostic(track.eventKeys, strTrackLabel, "event keys", outErrors);
        AppendUnsortedKeyDiagnostic(track.visibilityKeys, strTrackLabel, "visibility keys", outErrors);
        AppendUnsortedKeyDiagnostic(track.timeDilationKeys, strTrackLabel, "time dilation keys", outErrors);
    }

    size_t GetTrackKeyCount(const SeqTrack& track)
    {
        switch (track.eType)
        {
        case eSeqTrackType::Camera: return track.cameraKeys.size();
        case eSeqTrackType::Anim: return track.animKeys.size();
        case eSeqTrackType::Fx: return track.fxKeys.size();
        case eSeqTrackType::Audio: return track.audioKeys.size();
        case eSeqTrackType::Event: return track.eventKeys.size();
        case eSeqTrackType::Visibility: return track.visibilityKeys.size();
        case eSeqTrackType::TimeDilation: return track.timeDilationKeys.size();
        default: return 0;
        }
    }

    bool_t IsFinite(f64_t dValue)
    {
        return std::isfinite(dValue) ? true : false;
    }

    bool_t IsFinite(f32_t fValue)
    {
        return std::isfinite(static_cast<f64_t>(fValue)) ? true : false;
    }

    template <typename T>
    void AppendKeyTimeDiagnostics(
        const std::vector<T>& keys,
        const std::string& strTrackLabel,
        const char* pKeyName,
        f64_t dDurationSec,
        std::vector<std::string>& outErrors)
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            const f64_t dTimeSec = keys[i].dTimeSec;
            if (!IsFinite(dTimeSec))
            {
                outErrors.push_back(strTrackLabel + ": " + pKeyName + "[" + std::to_string(i) + "] has non-finite time");
                continue;
            }
            if (dTimeSec < 0.0)
                outErrors.push_back(strTrackLabel + ": " + pKeyName + "[" + std::to_string(i) + "] has negative time");
            if (dDurationSec > 0.0 && dTimeSec > dDurationSec)
                outErrors.push_back(strTrackLabel + ": " + pKeyName + "[" + std::to_string(i) + "] exceeds durationSec");
            if (i > 0 && dTimeSec == keys[i - 1].dTimeSec)
                outErrors.push_back(strTrackLabel + ": " + pKeyName + "[" + std::to_string(i) + "] duplicates previous key time");
        }

        AppendUnsortedKeyDiagnostic(keys, strTrackLabel, pKeyName, outErrors);
    }
}

bool_t CSequenceAsset::LoadFromJson(const std::string& strPath, CSequenceAsset& outAsset)
{
    std::ifstream file(strPath, std::ios::binary);
    if (!file)
        return false;

    std::ostringstream ss;
    ss << file.rdbuf();

    JsonValue root;
    JsonParser parser(ss.str());
    if (!parser.Parse(root) || root.eValueType != JsonValue::eType::Object)
        return false;

    CSequenceAsset parsed;
    parsed.strName = GetString(root, "name");
    parsed.dDurationSec = GetNumber(root, "durationSec", 0.0);
    parsed.iDisplayRate = static_cast<u32_t>(GetNumber(root, "displayRate", 60.0));
    parsed.bLoop = GetBool(root, "loop", false);

    const JsonValue* pTracks = FindMember(root, "tracks");
    if (pTracks && pTracks->eValueType == JsonValue::eType::Array)
    {
        for (const JsonValue& trackValue : pTracks->arrValue)
        {
            if (trackValue.eValueType != JsonValue::eType::Object)
                continue;

            eSeqTrackType eType = eSeqTrackType::Camera;
            const std::string strTrackType = GetString(trackValue, "type");
            if (!TryParseTrackType(strTrackType, eType))
            {
                parsed.m_arrLoadValidationErrors.push_back(
                    "track[" + std::to_string(parsed.tracks.size()) +
                    "]: unknown track type '" + strTrackType + "'");
                continue;
            }

            SeqTrack track;
            track.eType = eType;
            track.strName = GetString(trackValue, "name");
            track.strBinding = GetString(trackValue, "binding");

            const JsonValue* pKeys = FindMember(trackValue, "keys");
            if (pKeys && pKeys->eValueType == JsonValue::eType::Array)
            {
                for (const JsonValue& keyValue : pKeys->arrValue)
                {
                    if (keyValue.eValueType != JsonValue::eType::Object)
                        continue;

                    switch (eType)
                    {
                    case eSeqTrackType::Camera:
                    {
                        SeqCameraKey key;
                        key.dTimeSec = GetNumber(keyValue, "time", 0.0);
                        ReadVec3(keyValue, "pos", key.vPos);
                        ReadVec3(keyValue, "rotEuler", key.vRotEulerDeg);
                        key.fFovDeg = static_cast<f32_t>(GetNumber(keyValue, "fov", 60.0));
                        key.eInterp = ParseInterp(GetString(keyValue, "interp", "linear"));
                        key.bCut = GetBool(keyValue, "cut", false);
                        track.cameraKeys.push_back(std::move(key));
                        break;
                    }
                    case eSeqTrackType::Anim:
                    {
                        SeqAnimKey key;
                        key.dTimeSec = GetNumber(keyValue, "time", 0.0);
                        key.strAnim = GetString(keyValue, "anim");
                        key.bLoop = GetBool(keyValue, "loop", false);
                        key.bReverse = GetBool(keyValue, "reverse", false);
                        key.fSpeed = static_cast<f32_t>(GetNumber(keyValue, "speed", 1.0));
                        track.animKeys.push_back(std::move(key));
                        break;
                    }
                    case eSeqTrackType::Fx:
                    {
                        SeqFxKey key;
                        key.dTimeSec = GetNumber(keyValue, "time", 0.0);
                        key.strWfx = GetString(keyValue, "wfx");
                        key.strAnchor = GetString(keyValue, "anchor");
                        key.bOneShot = GetBool(keyValue, "oneShot", true);
                        track.fxKeys.push_back(std::move(key));
                        break;
                    }
                    case eSeqTrackType::Audio:
                    {
                        SeqAudioKey key;
                        key.dTimeSec = GetNumber(keyValue, "time", 0.0);
                        key.strSound = GetString(keyValue, "sound");
                        key.strChannel = GetString(keyValue, "channel", "Effect");
                        key.fVolume = static_cast<f32_t>(GetNumber(keyValue, "volume", 1.0));
                        track.audioKeys.push_back(std::move(key));
                        break;
                    }
                    case eSeqTrackType::Event:
                    {
                        SeqEventKey key;
                        key.dTimeSec = GetNumber(keyValue, "time", 0.0);
                        key.strEvent = GetString(keyValue, "event");
                        key.strPayload = GetString(keyValue, "payload");
                        track.eventKeys.push_back(std::move(key));
                        break;
                    }
                    case eSeqTrackType::Visibility:
                    {
                        SeqVisibilityKey key;
                        key.dTimeSec = GetNumber(keyValue, "time", 0.0);
                        key.bVisible = GetBool(keyValue, "visible", true);
                        track.visibilityKeys.push_back(std::move(key));
                        break;
                    }
                    case eSeqTrackType::TimeDilation:
                    {
                        SeqTimeDilationKey key;
                        key.dTimeSec = GetNumber(keyValue, "time", 0.0);
                        key.fScale = static_cast<f32_t>(GetNumber(keyValue, "scale", 1.0));
                        key.eInterp = ParseInterp(GetString(keyValue, "interp", "linear"));
                        track.timeDilationKeys.push_back(std::move(key));
                        break;
                    }
                    default:
                        break;
                    }
                }
            }

            AppendTrackOrderDiagnostics(track, parsed.tracks.size(), parsed.m_arrLoadValidationErrors);
            parsed.tracks.push_back(std::move(track));
        }
    }

    parsed.SortKeys();
    outAsset = std::move(parsed);
    return true;
}

bool_t CSequenceAsset::SaveToJson(const std::string& strPath) const
{
    std::ofstream file(strPath, std::ios::binary | std::ios::trunc);
    if (!file)
        return false;

    file << std::fixed << std::setprecision(6);
    file << "{\n";
    file << "  \"format\": \"wseq\",\n";
    file << "  \"version\": 1,\n";
    file << "  \"name\": ";
    WriteEscapedString(file, strName);
    file << ",\n";
    file << "  \"durationSec\": " << dDurationSec << ",\n";
    file << "  \"displayRate\": " << iDisplayRate << ",\n";
    file << "  \"loop\": " << (bLoop ? "true" : "false") << ",\n";
    file << "  \"tracks\": [\n";

    for (size_t iTrack = 0; iTrack < tracks.size(); ++iTrack)
    {
        const SeqTrack& track = tracks[iTrack];
        file << "    {\n";
        file << "      \"type\": ";
        WriteEscapedString(file, TrackTypeToString(track.eType));
        file << ",\n";
        file << "      \"name\": ";
        WriteEscapedString(file, track.strName);
        file << ",\n";
        file << "      \"binding\": ";
        WriteEscapedString(file, track.strBinding);
        file << ",\n";
        file << "      \"keys\": [\n";

        auto writeKeyComma = [&file](bool_t bNeedsComma)
        {
            if (bNeedsComma)
                file << ",\n";
        };

        switch (track.eType)
        {
        case eSeqTrackType::Camera:
            for (size_t i = 0; i < track.cameraKeys.size(); ++i)
            {
                const SeqCameraKey& key = track.cameraKeys[i];
                writeKeyComma(i > 0);
                file << "        { \"time\": " << key.dTimeSec << ", \"pos\": ";
                WriteVec3(file, key.vPos);
                file << ", \"rotEuler\": ";
                WriteVec3(file, key.vRotEulerDeg);
                file << ", \"fov\": " << key.fFovDeg << ", \"interp\": ";
                WriteEscapedString(file, InterpToString(key.eInterp));
                file << ", \"cut\": " << (key.bCut ? "true" : "false") << " }";
            }
            break;
        case eSeqTrackType::Anim:
            for (size_t i = 0; i < track.animKeys.size(); ++i)
            {
                const SeqAnimKey& key = track.animKeys[i];
                writeKeyComma(i > 0);
                file << "        { \"time\": " << key.dTimeSec << ", \"anim\": ";
                WriteEscapedString(file, key.strAnim);
                file << ", \"loop\": " << (key.bLoop ? "true" : "false");
                file << ", \"reverse\": " << (key.bReverse ? "true" : "false");
                file << ", \"speed\": " << key.fSpeed << " }";
            }
            break;
        case eSeqTrackType::Fx:
            for (size_t i = 0; i < track.fxKeys.size(); ++i)
            {
                const SeqFxKey& key = track.fxKeys[i];
                writeKeyComma(i > 0);
                file << "        { \"time\": " << key.dTimeSec << ", \"wfx\": ";
                WriteEscapedString(file, key.strWfx);
                file << ", \"anchor\": ";
                WriteEscapedString(file, key.strAnchor);
                file << ", \"oneShot\": " << (key.bOneShot ? "true" : "false") << " }";
            }
            break;
        case eSeqTrackType::Audio:
            for (size_t i = 0; i < track.audioKeys.size(); ++i)
            {
                const SeqAudioKey& key = track.audioKeys[i];
                writeKeyComma(i > 0);
                file << "        { \"time\": " << key.dTimeSec << ", \"sound\": ";
                WriteEscapedString(file, key.strSound);
                file << ", \"channel\": ";
                WriteEscapedString(file, key.strChannel);
                file << ", \"volume\": " << key.fVolume << " }";
            }
            break;
        case eSeqTrackType::Event:
            for (size_t i = 0; i < track.eventKeys.size(); ++i)
            {
                const SeqEventKey& key = track.eventKeys[i];
                writeKeyComma(i > 0);
                file << "        { \"time\": " << key.dTimeSec << ", \"event\": ";
                WriteEscapedString(file, key.strEvent);
                file << ", \"payload\": ";
                WriteEscapedString(file, key.strPayload);
                file << " }";
            }
            break;
        case eSeqTrackType::Visibility:
            for (size_t i = 0; i < track.visibilityKeys.size(); ++i)
            {
                const SeqVisibilityKey& key = track.visibilityKeys[i];
                writeKeyComma(i > 0);
                file << "        { \"time\": " << key.dTimeSec;
                file << ", \"visible\": " << (key.bVisible ? "true" : "false") << " }";
            }
            break;
        case eSeqTrackType::TimeDilation:
            for (size_t i = 0; i < track.timeDilationKeys.size(); ++i)
            {
                const SeqTimeDilationKey& key = track.timeDilationKeys[i];
                writeKeyComma(i > 0);
                file << "        { \"time\": " << key.dTimeSec;
                file << ", \"scale\": " << key.fScale << ", \"interp\": ";
                WriteEscapedString(file, InterpToString(key.eInterp));
                file << " }";
            }
            break;
        default:
            break;
        }

        file << "\n";
        file << "      ]\n";
        file << "    }";
        if (iTrack + 1 < tracks.size())
            file << ',';
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";
    return true;
}

bool_t CSequenceAsset::Validate(std::vector<std::string>* pOutErrors) const
{
    std::vector<std::string> errors = GetValidationErrors();
    if (pOutErrors)
        *pOutErrors = errors;
    return errors.empty();
}

std::vector<std::string> CSequenceAsset::GetValidationErrors() const
{
    std::vector<std::string> errors = m_arrLoadValidationErrors;

    if (!IsFinite(dDurationSec) || dDurationSec < 0.0)
    {
        errors.push_back("durationSec must be finite and non-negative");
    }
    else if (!tracks.empty() && dDurationSec <= 0.0)
    {
        errors.push_back("durationSec must be greater than zero when tracks are present");
    }

    if (iDisplayRate == 0)
        errors.push_back("displayRate must be greater than zero");

    for (size_t iTrack = 0; iTrack < tracks.size(); ++iTrack)
    {
        const SeqTrack& track = tracks[iTrack];
        const std::string strTrackLabel = MakeTrackLabel(iTrack, track);

        if (!IsKnownTrackType(track.eType))
        {
            errors.push_back(strTrackLabel + ": unknown track type");
            continue;
        }

        if (TrackRequiresBinding(track.eType) && track.strBinding.empty())
            errors.push_back(strTrackLabel + ": binding is required");

        if (GetTrackKeyCount(track) == 0)
            errors.push_back(strTrackLabel + ": track has no keys");

        switch (track.eType)
        {
        case eSeqTrackType::Camera:
            AppendKeyTimeDiagnostics(track.cameraKeys, strTrackLabel, "camera key", dDurationSec, errors);
            for (size_t i = 0; i < track.cameraKeys.size(); ++i)
            {
                const SeqCameraKey& key = track.cameraKeys[i];
                if (!IsFinite(key.fFovDeg) || key.fFovDeg <= 0.f)
                    errors.push_back(strTrackLabel + ": camera key[" + std::to_string(i) + "] has invalid fov");
            }
            break;
        case eSeqTrackType::Anim:
            AppendKeyTimeDiagnostics(track.animKeys, strTrackLabel, "anim key", dDurationSec, errors);
            for (size_t i = 0; i < track.animKeys.size(); ++i)
            {
                const SeqAnimKey& key = track.animKeys[i];
                if (key.strAnim.empty())
                    errors.push_back(strTrackLabel + ": anim key[" + std::to_string(i) + "] has empty anim");
                if (!IsFinite(key.fSpeed) || key.fSpeed <= 0.f)
                    errors.push_back(strTrackLabel + ": anim key[" + std::to_string(i) + "] has invalid speed");
            }
            break;
        case eSeqTrackType::Fx:
            AppendKeyTimeDiagnostics(track.fxKeys, strTrackLabel, "fx key", dDurationSec, errors);
            for (size_t i = 0; i < track.fxKeys.size(); ++i)
            {
                if (track.fxKeys[i].strWfx.empty())
                    errors.push_back(strTrackLabel + ": fx key[" + std::to_string(i) + "] has empty wfx");
            }
            break;
        case eSeqTrackType::Audio:
            AppendKeyTimeDiagnostics(track.audioKeys, strTrackLabel, "audio key", dDurationSec, errors);
            for (size_t i = 0; i < track.audioKeys.size(); ++i)
            {
                const SeqAudioKey& key = track.audioKeys[i];
                if (key.strSound.empty())
                    errors.push_back(strTrackLabel + ": audio key[" + std::to_string(i) + "] has empty sound");
                if (key.strChannel.empty())
                    errors.push_back(strTrackLabel + ": audio key[" + std::to_string(i) + "] has empty channel");
                if (!IsFinite(key.fVolume) || key.fVolume < 0.f)
                    errors.push_back(strTrackLabel + ": audio key[" + std::to_string(i) + "] has invalid volume");
            }
            break;
        case eSeqTrackType::Event:
            AppendKeyTimeDiagnostics(track.eventKeys, strTrackLabel, "event key", dDurationSec, errors);
            for (size_t i = 0; i < track.eventKeys.size(); ++i)
            {
                if (track.eventKeys[i].strEvent.empty())
                    errors.push_back(strTrackLabel + ": event key[" + std::to_string(i) + "] has empty event");
            }
            break;
        case eSeqTrackType::Visibility:
            AppendKeyTimeDiagnostics(track.visibilityKeys, strTrackLabel, "visibility key", dDurationSec, errors);
            break;
        case eSeqTrackType::TimeDilation:
            AppendKeyTimeDiagnostics(track.timeDilationKeys, strTrackLabel, "time dilation key", dDurationSec, errors);
            for (size_t i = 0; i < track.timeDilationKeys.size(); ++i)
            {
                const SeqTimeDilationKey& key = track.timeDilationKeys[i];
                if (!IsFinite(key.fScale) || key.fScale <= 0.f)
                    errors.push_back(strTrackLabel + ": time dilation key[" + std::to_string(i) + "] has invalid scale");
            }
            break;
        default:
            break;
        }
    }

    return errors;
}

void CSequenceAsset::Clear()
{
    strName.clear();
    dDurationSec = 0.0;
    iDisplayRate = 60;
    bLoop = false;
    tracks.clear();
    m_arrLoadValidationErrors.clear();
}

void CSequenceAsset::SortKeys()
{
    for (SeqTrack& track : tracks)
    {
        SortByTime(track.cameraKeys);
        SortByTime(track.animKeys);
        SortByTime(track.fxKeys);
        SortByTime(track.audioKeys);
        SortByTime(track.eventKeys);
        SortByTime(track.visibilityKeys);
        SortByTime(track.timeDilationKeys);
    }
}
