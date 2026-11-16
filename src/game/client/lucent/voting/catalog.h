#ifndef GAME_CLIENT_LUCENT_VOTING_CATALOG_H
#define GAME_CLIENT_LUCENT_VOTING_CATALOG_H

#include <base/str.h>
#include <game/voting.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace LucentVoting
{
	inline std::string_view Trim(const char *pText)
	{
		const char *pStart = str_utf8_skip_whitespaces(pText);
		const char *pEnd = pStart;
		for(const char *pRead = pStart; *pRead;)
		{
			if(!str_utf8_isspace(str_utf8_decode(&pRead)))
				pEnd = pRead;
		}
		return {pStart, static_cast<size_t>(pEnd - pStart)};
	}

	inline std::string_view SectionName(const char *pDescription)
	{
		std::string_view Text = Trim(pDescription);
		for(const std::string_view Rule : {"---", "───"})
		{
			if(Text.size() <= 2 * Rule.size() || !Text.starts_with(Rule) || !Text.ends_with(Rule))
				continue;
			Text.remove_prefix(Rule.size());
			Text.remove_suffix(Rule.size());
			const std::string_view Stroke = Rule.substr(0, Rule.size() / 3);
			while(Text.starts_with(Stroke))
				Text.remove_prefix(Stroke.size());
			while(Text.ends_with(Stroke))
				Text.remove_suffix(Stroke.size());
			char aName[VOTE_DESC_LENGTH] = {};
			if(Text.size() >= sizeof(aName))
				return {};
			Text.copy(aName, Text.size());
			const auto Name = Trim(aName);
			if(!Name.empty())
				return Text.substr(Name.data() - aName, Name.size());
			return {};
		}
		return {};
	}

	struct CMapLabel
	{
		std::string_view m_Title, m_Author, m_Rating;
	};

	inline CMapLabel ParseMapLabel(std::string_view Text)
	{
		const size_t Separator = Text.rfind(" | ");
		if(Separator == std::string_view::npos || Separator == 0)
			return {};
		const auto Stars = Text.substr(Separator + 3);
		if(Stars.size() != 7 || Stars[0] < '0' || Stars[0] > '5' || Stars.substr(1) != "/5 ★")
			return {};
		const auto Title = Text.substr(0, Separator);
		constexpr std::string_view By = " by ";
		const size_t Author = Title.find(By);
		if(Author > 0 && Author != std::string_view::npos && Author == Title.rfind(By) && Author + By.size() < Title.size())
			return {Title.substr(0, Author), Title.substr(Author + By.size()), Stars};
		return {Title, {}, Stars};
	}

	struct CMapDetails
	{
		std::string_view m_Finishes, m_Time;
	};

	inline CMapDetails ParseMapDetails(const char *pDescription)
	{
		auto Text = Trim(pDescription);
		constexpr std::string_view FlagMark = " ⚑", Prefix = " | ", Suffix = " ◷";
		const size_t Flag = Text.find(FlagMark);
		if(Flag == 0 || Flag == std::string_view::npos || Text.substr(0, Flag).find_first_not_of("0123456789") != std::string_view::npos)
			return {};
		const auto Finishes = Text.substr(0, Flag + FlagMark.size());
		Text.remove_prefix(Finishes.size());
		if(Text.empty())
			return {Finishes, {}};
		if(!Text.starts_with(Prefix) || !Text.ends_with(Suffix) || Text.size() <= Prefix.size() + Suffix.size())
			return {};
		const auto Time = Text.substr(Prefix.size(), Text.size() - Prefix.size() - Suffix.size());
		if(Time.find(':') == std::string_view::npos || Time.find_first_not_of("0123456789:.") != std::string_view::npos)
			return {};
		return {Finishes, Text.substr(Prefix.size())};
	}

	struct CMapType
	{
		const char *m_pName = nullptr;
		bool m_Active = false;
	};

	inline CMapType ParseMapType(const char *pDescription)
	{
		auto Text = Trim(pDescription);
		constexpr std::string_view Active = "☒ ", Inactive = "☐ ";
		const bool IsActive = Text.starts_with(Active);
		if(!IsActive && !Text.starts_with(Inactive))
			return {};
		Text.remove_prefix(IsActive ? Active.size() : Inactive.size());
		constexpr std::string_view SmallCapsSuffix = " Mᴀᴘs", PlainSuffix = " Maps";
		if(Text.ends_with(SmallCapsSuffix))
			Text.remove_suffix(SmallCapsSuffix.size());
		else if(Text.ends_with(PlainSuffix))
			Text.remove_suffix(PlainSuffix.size());
		else
			return {};
		constexpr struct
		{
			std::string_view m_Label;
			const char *m_pName;
		} aTypes[] = {
			{"Nᴏᴠɪᴄᴇ", "Novice"}, {"Mᴏᴅᴇʀᴀᴛᴇ", "Moderate"}, {"Bʀᴜᴛᴀʟ", "Brutal"},
			{"Iɴsᴀɴᴇ", "Insane"}, {"Dᴜᴍᴍʏ", "Dummy"}, {"DDᴍᴀX", "DDmaX"},
			{"DDᴍᴀX.Eᴀsʏ", "DDmaX.Easy"}, {"DDᴍᴀX.Nᴇxᴛ", "DDmaX.Next"},
			{"DDᴍᴀX.Pʀᴏ", "DDmaX.Pro"}, {"DDᴍᴀX.Nᴜᴛ", "DDmaX.Nut"},
			{"Oʟᴅsᴄʜᴏᴏʟ", "Oldschool"}, {"Sᴏʟᴏ", "Solo"}, {"Rᴀᴄᴇ", "Race"},
			{"Fᴜɴ", "Fun"}, {"Eᴠᴇɴᴛ", "Event"},
		};
		for(const auto &Type : aTypes)
		{
			if(Text == Type.m_Label || Text == Type.m_pName)
				return {Type.m_pName, IsActive};
		}
		return {};
	}

	inline constexpr float SectionHeight = 21.f, BottomPadding = 8.f;
	inline constexpr int ServerOptionsSection = -2;

	struct CSidebarLayout
	{
		float m_Width = 0, m_Height = 0;
	};

	inline CSidebarLayout SidebarLayout(float Width, float Height, bool MapTab)
	{
		if(Width >= 460)
			return {std::clamp(Width * .18f, 115.f, 164.f), 0};
		return {0, MapTab ? std::clamp(Height * .32f, 0.f, 148.f) : 0.f};
	}

	inline float ListHeight(float RowHeight, int Rows, int Sections)
	{
		return RowHeight * Rows + SectionHeight * Sections + (Rows ? BottomPadding : 0);
	}

	struct CCategory
	{
		std::string m_Name;
		const CVoteOptionClient *m_pHeading = nullptr;
		int m_Count = 0;
	};

	struct COption
	{
		const CVoteOptionClient *m_pOption;
		int m_Category;
		const char *m_pDetails = "";
		bool m_Decoration = false;
		bool m_Command = false;
	};

	struct CCatalog
	{
		std::vector<CCategory> m_vCategories;
		std::vector<COption> m_vOptions;
		std::vector<int> m_vMapTypes;
		bool m_HasDecorations = false;

		void Rebuild(const CVoteOptionClient *pFirst)
		{
			m_vOptions.clear();
			m_vCategories.clear();
			m_vMapTypes.clear();
			m_vCategories.emplace_back(); // Options without a server heading.
			m_HasDecorations = false;
			bool TypeMenu = false;
			for(auto *pOption = pFirst; pOption && SectionName(pOption->m_aDescription).empty(); pOption = pOption->m_pNext)
			{
				if(ParseMapType(pOption->m_aDescription).m_Active)
				{
					TypeMenu = true;
					break;
				}
			}
			int Category = 0;
			for(auto *pOption = pFirst; pOption; pOption = pOption->m_pNext)
			{
				const auto Section = SectionName(pOption->m_aDescription);
				if(!Section.empty())
				{
					Category = m_vCategories.size();
					m_vCategories.push_back({std::string(Section), pOption});
					m_vOptions.push_back({pOption, Category, "", true});
					m_HasDecorations = true;
					continue;
				}

				auto &Option = m_vOptions.emplace_back(COption{pOption, Category});
				if(TypeMenu && Category == 0 && ParseMapType(pOption->m_aDescription).m_pName)
				{
					m_vMapTypes.push_back(m_vOptions.size() - 1);
					Option.m_Decoration = true;
					m_HasDecorations = true;
				}
				else if(Category == 0 && Trim(pOption->m_aDescription) == "Make sure no one is racing before voting!")
				{
					Option.m_Decoration = true;
					m_HasDecorations = true;
				}
				else if(Trim(pOption->m_aDescription).empty())
					Option.m_Decoration = true;
				else if(m_vOptions.size() > 1 && !ParseMapDetails(pOption->m_aDescription).m_Finishes.empty())
				{
					auto &Previous = m_vOptions[m_vOptions.size() - 2];
					if(Previous.m_Category == Category && !ParseMapLabel(Previous.m_pOption->m_aDescription).m_Rating.empty())
					{
						Previous.m_pDetails = pOption->m_aDescription;
						Option.m_Decoration = true;
						m_HasDecorations = true;
					}
				}
				// DDNet puts server actions before its map sections.
				Option.m_Command = TypeMenu && Category == 0 && !Option.m_Decoration &&
						   ParseMapLabel(pOption->m_aDescription).m_Rating.empty();
				if(!Option.m_Decoration && !Option.m_Command)
					++m_vCategories[Category].m_Count;
			}
		}

		int Section(int Index) const
		{
			const auto &Option = m_vOptions[Index];
			return Option.m_Command ? ServerOptionsSection : Option.m_Category;
		}

		bool Matches(int Index, const char *pSearch, bool Raw) const
		{
			const auto &Option = m_vOptions[Index];
			return (Raw || !m_HasDecorations || !Option.m_Decoration) &&
			       (!*pSearch || str_utf8_find_nocase(Option.m_pOption->m_aDescription, pSearch) ||
				       (!Raw && str_utf8_find_nocase(Option.m_pDetails, pSearch)));
		}

		std::vector<int> Filter(const char *pSearch, const std::vector<std::string> &vCategories, bool Raw) const
		{
			std::vector<bool> vIncluded(m_vCategories.size(), Raw || vCategories.empty());
			if(!Raw && !vCategories.empty())
			{
				for(size_t i = 0; i < m_vCategories.size(); ++i)
					vIncluded[i] = std::find(vCategories.begin(), vCategories.end(), m_vCategories[i].m_Name) != vCategories.end();
			}
			std::vector<int> vVisible;
			vVisible.reserve(m_vOptions.size());
			for(int i = 0; i < (int)m_vOptions.size(); ++i)
				if(Matches(i, pSearch, Raw) && (vIncluded[m_vOptions[i].m_Category] || m_vOptions[i].m_Command))
					vVisible.push_back(i);
			if(!Raw && !m_vMapTypes.empty())
				std::stable_partition(vVisible.begin(), vVisible.end(), [&](int Index) { return m_vOptions[Index].m_Command; });
			return vVisible;
		}

		int VisibleSelection(const std::vector<int> &vVisible, const CVoteOptionClient *pSelected, const char *pDescription) const
		{
			if(pSelected)
			{
				for(int i = 0; i < (int)vVisible.size(); ++i)
				{
					const auto *pOption = m_vOptions[vVisible[i]].m_pOption;
					if(pOption == pSelected && str_comp(pOption->m_aDescription, pDescription) == 0)
						return i;
				}
			}
			return -1;
		}
	};
}

#endif
