#include "World/WorldCellDocument.h"

#include "WintersPaths.h"

#include <Windows.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace
{
	constexpr const char* kDefaultSchema = "winters.world.cell.v1";

	std::wstring Utf8ToWide(const std::string& strText)
	{
		if (strText.empty())
			return {};

		const int iLength = ::MultiByteToWideChar(
			CP_UTF8,
			0,
			strText.c_str(),
			-1,
			nullptr,
			0);
		if (iLength <= 0)
			return std::wstring(strText.begin(), strText.end());

		std::wstring strWide(static_cast<size_t>(iLength), L'\0');
		::MultiByteToWideChar(
			CP_UTF8,
			0,
			strText.c_str(),
			-1,
			strWide.data(),
			iLength);
		if (!strWide.empty() && strWide.back() == L'\0')
			strWide.pop_back();
		return strWide;
	}

	std::filesystem::path ResolvePathForRead(const std::string& strRelative)
	{
		const std::wstring strWide = Utf8ToWide(strRelative);
		wchar_t szResolved[MAX_PATH] = {};
		if (!strWide.empty() && WintersResolveContentPath(strWide.c_str(), szResolved, MAX_PATH))
			return std::filesystem::path(szResolved);

		std::filesystem::path path(strWide);
		if (path.is_absolute())
			return path;

		return std::filesystem::current_path() / path;
	}

	std::filesystem::path ResolvePathForWrite(const std::string& strRelative)
	{
		return ResolvePathForRead(strRelative);
	}

	void SkipWhitespace(const std::string& strText, size_t& i)
	{
		while (i < strText.size())
		{
			const char c = strText[i];
			if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
				break;
			++i;
		}
	}

	size_t FindValueStart(const std::string& strText, const char* pKey)
	{
		const std::string strNeedle = std::string("\"") + pKey + "\"";
		size_t iSearch = 0;
		while (true)
		{
			const size_t iKey = strText.find(strNeedle, iSearch);
			if (iKey == std::string::npos)
				return std::string::npos;

			size_t i = iKey + strNeedle.size();
			SkipWhitespace(strText, i);
			if (i < strText.size() && strText[i] == ':')
			{
				++i;
				SkipWhitespace(strText, i);
				return i;
			}

			iSearch = iKey + strNeedle.size();
		}
	}

	bool ParseStringAt(const std::string& strText, size_t& i, std::string& strOut)
	{
		SkipWhitespace(strText, i);
		if (i >= strText.size() || strText[i] != '"')
			return false;
		++i;

		std::string strValue;
		while (i < strText.size())
		{
			const char c = strText[i++];
			if (c == '"')
			{
				strOut = std::move(strValue);
				return true;
			}
			if (c != '\\')
			{
				strValue.push_back(c);
				continue;
			}
			if (i >= strText.size())
				return false;

			const char escaped = strText[i++];
			switch (escaped)
			{
			case '"': strValue.push_back('"'); break;
			case '\\': strValue.push_back('\\'); break;
			case '/': strValue.push_back('/'); break;
			case 'b': strValue.push_back('\b'); break;
			case 'f': strValue.push_back('\f'); break;
			case 'n': strValue.push_back('\n'); break;
			case 'r': strValue.push_back('\r'); break;
			case 't': strValue.push_back('\t'); break;
			case 'u':
				if (i + 4 <= strText.size())
					i += 4;
				strValue.push_back('?');
				break;
			default:
				strValue.push_back(escaped);
				break;
			}
		}

		return false;
	}

	bool TryExtractString(const std::string& strText, const char* pKey, std::string& strOut)
	{
		size_t i = FindValueStart(strText, pKey);
		if (i == std::string::npos)
			return false;
		return ParseStringAt(strText, i, strOut);
	}

	bool TryExtractBool(const std::string& strText, const char* pKey, bool& bOut)
	{
		size_t i = FindValueStart(strText, pKey);
		if (i == std::string::npos)
			return false;

		if (strText.compare(i, 4, "true") == 0)
		{
			bOut = true;
			return true;
		}
		if (strText.compare(i, 5, "false") == 0)
		{
			bOut = false;
			return true;
		}
		return false;
	}

	bool TryExtractFloat(const std::string& strText, const char* pKey, f32_t& fOut)
	{
		size_t i = FindValueStart(strText, pKey);
		if (i == std::string::npos)
			return false;

		errno = 0;
		char* pEnd = nullptr;
		const double dValue = std::strtod(strText.c_str() + i, &pEnd);
		if (pEnd == strText.c_str() + i || errno == ERANGE)
			return false;

		fOut = static_cast<f32_t>(dValue);
		return true;
	}

	bool TryExtractUInt(const std::string& strText, const char* pKey, u32_t& iOut)
	{
		size_t i = FindValueStart(strText, pKey);
		if (i == std::string::npos)
			return false;

		if (i < strText.size() && strText[i] == '-')
			return false;

		errno = 0;
		char* pEnd = nullptr;
		const unsigned long value = std::strtoul(strText.c_str() + i, &pEnd, 10);
		if (pEnd == strText.c_str() + i || errno == ERANGE)
			return false;

		iOut = static_cast<u32_t>(value);
		return true;
	}

	bool ParseVec3At(const std::string& strText, size_t& i, Vec3& vOut)
	{
		SkipWhitespace(strText, i);
		if (i >= strText.size() || strText[i] != '[')
			return false;
		++i;

		f32_t values[3] = {};
		for (int component = 0; component < 3; ++component)
		{
			SkipWhitespace(strText, i);
			errno = 0;
			char* pEnd = nullptr;
			const double dValue = std::strtod(strText.c_str() + i, &pEnd);
			if (pEnd == strText.c_str() + i || errno == ERANGE)
				return false;
			values[component] = static_cast<f32_t>(dValue);
			i = static_cast<size_t>(pEnd - strText.c_str());

			SkipWhitespace(strText, i);
			if (component < 2)
			{
				if (i >= strText.size() || strText[i] != ',')
					return false;
				++i;
			}
		}

		SkipWhitespace(strText, i);
		if (i >= strText.size() || strText[i] != ']')
			return false;
		++i;

		vOut = Vec3{ values[0], values[1], values[2] };
		return true;
	}

	bool TryExtractVec3(const std::string& strText, const char* pKey, Vec3& vOut)
	{
		size_t i = FindValueStart(strText, pKey);
		if (i == std::string::npos)
			return false;
		return ParseVec3At(strText, i, vOut);
	}

	bool ExtractArrayBody(const std::string& strText, const char* pKey, std::string& strOut)
	{
		size_t i = FindValueStart(strText, pKey);
		if (i == std::string::npos)
			return false;

		SkipWhitespace(strText, i);
		if (i >= strText.size() || strText[i] != '[')
			return false;

		const size_t iBodyStart = i + 1;
		int iDepth = 0;
		bool bInString = false;
		bool bEscaped = false;
		for (; i < strText.size(); ++i)
		{
			const char c = strText[i];
			if (bInString)
			{
				if (bEscaped)
				{
					bEscaped = false;
				}
				else if (c == '\\')
				{
					bEscaped = true;
				}
				else if (c == '"')
				{
					bInString = false;
				}
				continue;
			}

			if (c == '"')
			{
				bInString = true;
				continue;
			}
			if (c == '[')
			{
				++iDepth;
				continue;
			}
			if (c == ']')
			{
				--iDepth;
				if (iDepth == 0)
				{
					strOut = strText.substr(iBodyStart, i - iBodyStart);
					return true;
				}
			}
		}

		return false;
	}

	std::vector<std::string> ExtractObjectSlices(const std::string& strArrayBody)
	{
		std::vector<std::string> objects;
		bool bInString = false;
		bool bEscaped = false;
		int iDepth = 0;
		size_t iObjectStart = std::string::npos;

		for (size_t i = 0; i < strArrayBody.size(); ++i)
		{
			const char c = strArrayBody[i];
			if (bInString)
			{
				if (bEscaped)
					bEscaped = false;
				else if (c == '\\')
					bEscaped = true;
				else if (c == '"')
					bInString = false;
				continue;
			}

			if (c == '"')
			{
				bInString = true;
				continue;
			}
			if (c == '{')
			{
				if (iDepth == 0)
					iObjectStart = i;
				++iDepth;
				continue;
			}
			if (c == '}')
			{
				--iDepth;
				if (iDepth == 0 && iObjectStart != std::string::npos)
				{
					objects.push_back(strArrayBody.substr(iObjectStart, i - iObjectStart + 1));
					iObjectStart = std::string::npos;
				}
			}
		}

		return objects;
	}

	std::string EscapeJsonString(const std::string& strValue)
	{
		std::ostringstream out;
		for (const unsigned char c : strValue)
		{
			switch (c)
			{
			case '"': out << "\\\""; break;
			case '\\': out << "\\\\"; break;
			case '\b': out << "\\b"; break;
			case '\f': out << "\\f"; break;
			case '\n': out << "\\n"; break;
			case '\r': out << "\\r"; break;
			case '\t': out << "\\t"; break;
			default:
				if (c < 0x20)
				{
					out << "\\u"
						<< std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c)
						<< std::dec << std::setfill(' ');
				}
				else
				{
					out << static_cast<char>(c);
				}
				break;
			}
		}
		return out.str();
	}

	void WriteVec3(std::ostream& out, const Vec3& value)
	{
		out << '[' << value.x << ", " << value.y << ", " << value.z << ']';
	}
}

void CWorldCellDocument::Clear()
{
	m_schema = kDefaultSchema;
	m_cellId = "untitled";
	m_area = 0;
	m_blockX = 0;
	m_blockY = 0;
	m_variant = 0;
	m_cellSizeMeters = 64.f;
	m_origin = Vec3{ 0.f, 0.f, 0.f };
	m_dataLayer = "Base";
	m_placements.clear();
	m_references.clear();
	m_nextId = 1;
}

bool CWorldCellDocument::Load(const std::string& strJsonRelative)
{
	const std::filesystem::path path = ResolvePathForRead(strJsonRelative);
	std::ifstream file(path, std::ios::binary);
	if (!file)
		return false;

	const std::string strJson{
		std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>() };

	CWorldCellDocument parsed;
	parsed.Clear();

	TryExtractString(strJson, "schema", parsed.m_schema);
	TryExtractString(strJson, "cellId", parsed.m_cellId);
	TryExtractUInt(strJson, "area", parsed.m_area);
	TryExtractUInt(strJson, "blockX", parsed.m_blockX);
	TryExtractUInt(strJson, "blockY", parsed.m_blockY);
	TryExtractUInt(strJson, "variant", parsed.m_variant);
	TryExtractFloat(strJson, "cellSizeMeters", parsed.m_cellSizeMeters);
	TryExtractVec3(strJson, "origin", parsed.m_origin);
	TryExtractString(strJson, "dataLayer", parsed.m_dataLayer);

	std::string strPlacementsBody;
	if (ExtractArrayBody(strJson, "placements", strPlacementsBody))
	{
		for (const std::string& strObject : ExtractObjectSlices(strPlacementsBody))
		{
			WorldPlacement placement;
			TryExtractUInt(strObject, "id", placement.id);
			TryExtractString(strObject, "kind", placement.kind);
			TryExtractString(strObject, "name", placement.name);
			TryExtractString(strObject, "wmesh", placement.wmesh);
			TryExtractVec3(strObject, "position", placement.position);
			TryExtractVec3(strObject, "rotationDeg", placement.rotationDeg);
			TryExtractVec3(strObject, "scale", placement.scale);
			TryExtractBool(strObject, "animated", placement.animated);
			TryExtractBool(strObject, "transformResolved", placement.transformResolved);
			parsed.m_nextId = std::max(parsed.m_nextId, placement.id + 1);
			parsed.m_placements.push_back(std::move(placement));
		}
	}

	std::string strReferencesBody;
	if (ExtractArrayBody(strJson, "references", strReferencesBody))
	{
		for (const std::string& strObject : ExtractObjectSlices(strReferencesBody))
		{
			WorldReference reference;
			TryExtractString(strObject, "kind", reference.kind);
			TryExtractString(strObject, "model", reference.model);
			TryExtractString(strObject, "reason", reference.reason);
			parsed.m_references.push_back(std::move(reference));
		}
	}

	*this = std::move(parsed);
	return true;
}

bool CWorldCellDocument::Save(const std::string& strJsonRelative) const
{
	const std::filesystem::path path = ResolvePathForWrite(strJsonRelative);
	const std::filesystem::path parent = path.parent_path();
	if (!parent.empty())
	{
		std::error_code error;
		std::filesystem::create_directories(parent, error);
	}

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
		return false;

	file << std::fixed << std::setprecision(6);
	file << "{\n";
	file << "  \"schema\": \"" << EscapeJsonString(m_schema) << "\",\n";
	file << "  \"cellId\": \"" << EscapeJsonString(m_cellId) << "\",\n";
	file << "  \"area\": " << m_area << ",\n";
	file << "  \"blockX\": " << m_blockX << ",\n";
	file << "  \"blockY\": " << m_blockY << ",\n";
	file << "  \"variant\": " << m_variant << ",\n";
	file << "  \"cellSizeMeters\": " << m_cellSizeMeters << ",\n";
	file << "  \"origin\": ";
	WriteVec3(file, m_origin);
	file << ",\n";
	file << "  \"dataLayer\": \"" << EscapeJsonString(m_dataLayer) << "\",\n";
	file << "  \"placements\": [\n";
	for (size_t i = 0; i < m_placements.size(); ++i)
	{
		const WorldPlacement& placement = m_placements[i];
		file << "    {\n";
		file << "      \"id\": " << placement.id << ",\n";
		file << "      \"kind\": \"" << EscapeJsonString(placement.kind) << "\",\n";
		file << "      \"name\": \"" << EscapeJsonString(placement.name) << "\",\n";
		file << "      \"wmesh\": \"" << EscapeJsonString(placement.wmesh) << "\",\n";
		file << "      \"position\": ";
		WriteVec3(file, placement.position);
		file << ",\n";
		file << "      \"rotationDeg\": ";
		WriteVec3(file, placement.rotationDeg);
		file << ",\n";
		file << "      \"scale\": ";
		WriteVec3(file, placement.scale);
		file << ",\n";
		file << "      \"animated\": " << (placement.animated ? "true" : "false") << ",\n";
		file << "      \"transformResolved\": " << (placement.transformResolved ? "true" : "false") << "\n";
		file << "    }" << (i + 1 < m_placements.size() ? "," : "") << "\n";
	}
	file << "  ],\n";
	file << "  \"references\": [\n";
	for (size_t i = 0; i < m_references.size(); ++i)
	{
		const WorldReference& reference = m_references[i];
		file << "    {\n";
		file << "      \"kind\": \"" << EscapeJsonString(reference.kind) << "\",\n";
		file << "      \"model\": \"" << EscapeJsonString(reference.model) << "\",\n";
		file << "      \"reason\": \"" << EscapeJsonString(reference.reason) << "\"\n";
		file << "    }" << (i + 1 < m_references.size() ? "," : "") << "\n";
	}
	file << "  ]\n";
	file << "}\n";

	return true;
}

WorldPlacement* CWorldCellDocument::FindPlacement(u32_t id)
{
	for (WorldPlacement& placement : m_placements)
	{
		if (placement.id == id)
			return &placement;
	}
	return nullptr;
}

const WorldPlacement* CWorldCellDocument::FindPlacement(u32_t id) const
{
	for (const WorldPlacement& placement : m_placements)
	{
		if (placement.id == id)
			return &placement;
	}
	return nullptr;
}

u32_t CWorldCellDocument::AllocPlacementId()
{
	return m_nextId++;
}
