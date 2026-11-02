#ifndef ENGINE_SHARED_FONT_MANAGER_H
#define ENGINE_SHARED_FONT_MANAGER_H

#include <base/str.h>

#include <string>

namespace LucentFonts
{
	inline constexpr const char *Directory = "lucent/fonts";
	inline constexpr unsigned MaxFileSize = 32 * 1024 * 1024;
	inline bool Supported(const char *pName)
	{
		const int Length = str_length(pName);
		return Length > 4 && (str_comp_nocase(pName + Length - 4, ".ttf") == 0 || str_comp_nocase(pName + Length - 4, ".otf") == 0);
	}
	inline bool ManagedFilename(const char *pName)
	{
		if(!Supported(pName) || pName[0] == '.' || str_length(pName) >= 256)
			return false;
		for(const char *p = pName; *p; ++p)
			if(*p == '/' || *p == '\\' || *p == ':' || (unsigned char)*p < 32)
				return false;
		return true;
	}
	// Presentation only: persisted filenames and content hashes remain untouched.
	inline std::string DisplayName(const char *pFilename)
	{
		std::string Name = pFilename;
		if(Supported(Name.c_str()))
			Name.resize(Name.size() - 4);
		while(true)
		{
			const auto Dash = Name.rfind('-');
			if(Dash == std::string::npos)
				break;
			const auto Length = Name.size() - Dash - 1;
			bool Hash = Length == 16 || Length == 64;
			for(size_t i = Dash + 1; Hash && i < Name.size(); ++i)
				Hash = (Name[i] >= '0' && Name[i] <= '9') || (Name[i] >= 'a' && Name[i] <= 'f') || (Name[i] >= 'A' && Name[i] <= 'F');
			if(!Hash)
				break;
			Name.resize(Dash);
		}
		const auto Upper = [](char C) { return C >= 'A' && C <= 'Z'; };
		const auto Lower = [](char C) { return C >= 'a' && C <= 'z'; };
		std::string Result;
		for(size_t Start = 0; Start < Name.size();)
		{
			const auto End = Name.find_first_of("-_ ", Start);
			const std::string Token = Name.substr(Start, End == std::string::npos ? End : End - Start);
			Start = End == std::string::npos ? Name.size() : End + 1;
			if(Token.empty())
				continue;
			bool PointSize = Token.size() > 2 && str_comp_nocase(Token.c_str() + Token.size() - 2, "pt") == 0;
			for(size_t i = 0; PointSize && i < Token.size() - 2; ++i)
				PointSize = Token[i] >= '0' && Token[i] <= '9';
			if(PointSize)
				continue;
			if(!Result.empty())
				Result += ' ';
			const char *pStyle = nullptr;
			for(const char *pCandidate : {"SemiBold", "ExtraBold", "ExtraLight", "DemiBold"})
				if(str_comp_nocase(Token.c_str(), pCandidate) == 0)
					pStyle = pCandidate;
			if(pStyle)
			{
				Result += pStyle;
				continue;
			}
			for(size_t i = 0; i < Token.size(); ++i)
			{
				if(i > 0 && Upper(Token[i]) && (Lower(Token[i - 1]) || (Upper(Token[i - 1]) && i + 1 < Token.size() && Lower(Token[i + 1]))))
					Result += ' ';
				Result += Token[i];
			}
		}
		return Result.empty() ? Name : Result;
	}

}
#endif
