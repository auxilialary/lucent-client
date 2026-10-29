#ifndef GAME_CLIENT_LUCENT_SETTINGS_PLAYER_H
#define GAME_CLIENT_LUCENT_SETTINGS_PLAYER_H
#include "mode.h"

#include <array>
#include <functional>
#include <string>
class CUi;
class ITextRender;
class CCountryFlags;
namespace LucentSettings
{
	struct CPlayerProfile
	{
		char *m_pName = nullptr;
		size_t m_NameSize = 0;
		char *m_pClan = nullptr;
		size_t m_ClanSize = 0;
		int *m_pCountry = nullptr;
		const char *m_pEmptyName = nullptr;
		std::string m_Cooldown;
		const char *DisplayName() const { return *m_pName ? m_pName : m_pEmptyName; }
	};
	struct CPlayerBindings
	{
		std::array<CPlayerProfile, 2> m_Profiles;
		std::function<void(bool)> m_Changed;
		std::function<void(bool, CUIRect, float)> m_RenderTee;
		bool SelectCountry(bool Dummy, int Country) const
		{
			int *pCountry = m_Profiles[int(Dummy)].m_pCountry;
			if(*pCountry == Country)
				return false;
			*pCountry = Country;
			m_Changed(Dummy);
			return true;
		}
	};
	// One parent-derived form region owns both anchors for every profile field.
	struct CPlayerLayout
	{
		CUIRect m_PreviewHeading, m_PreviewRow;
		bool m_TwoColumns;
		CUIRect m_Form, m_NameLabel, m_Name, m_ClanLabel, m_Clan, m_CountryLabel, m_Country, m_Cooldown;
		float m_Height;
		static constexpr float FormMaxWidth = 420, FieldHeight = 28, FieldGap = 12;
		static CUIRect ContentRegion(CUIRect Column) { return SettingsContentRegion(Column); }
		static CUIRect FitFlag(CUIRect Region)
		{
			const float W = std::max(0.f, std::min(Region.w, Region.h * 2));
			return {Region.x + (Region.w - W) / 2, Region.y + (Region.h - W / 2) / 2, W, W / 2};
		}
		template<typename FMeasure>
		static CPlayerLayout Calculate(CUIRect View, FMeasure Measure)
		{
			CPlayerLayout R{};
			R.m_TwoColumns = View.w >= 760;
			const float FormWidth = std::min(FormMaxWidth, std::max(0.f, View.w) * (R.m_TwoColumns ? .44f : 1.f));
			View.VSplitLeft(FormWidth, &R.m_Form, nullptr);
			R.m_Form = ContentRegion(R.m_Form);
			float Y = View.y;
			const auto Take = [&](float H, float GapAfter = 4.f) {
				CUIRect Row{R.m_Form.x, Y, R.m_Form.w, H};
				Y += H + GapAfter;
				return Row;
			};
			const auto Label = [&](int Id) { return Take(std::max(18.f, Measure(Id, std::max(1.f, R.m_Form.w)) + 2)); };
			R.m_NameLabel = Label(0);
			R.m_Name = Take(FieldHeight, FieldGap);
			R.m_ClanLabel = Label(1);
			R.m_Clan = Take(FieldHeight, FieldGap);
			R.m_CountryLabel = Label(2);
			R.m_Country = Take(FieldHeight, FieldGap);
			R.m_Cooldown = Label(3);
			CUIRect Preview = View;
			if(R.m_TwoColumns)
			{
				View.VSplitLeft(FormWidth + 24, nullptr, &Preview);
				Preview.w = std::min(500.f, Preview.w);
			}
			else
			{
				Preview.y = Y + FieldGap;
				Preview.w = FormWidth;
			}
			Preview = ContentRegion(Preview);
			Preview.HSplitTop(std::max(18.f, Measure(4, std::max(1.f, Preview.w)) + 2), &R.m_PreviewHeading, &Preview);
			Preview.HSplitTop(4, nullptr, &Preview);
			Preview.HSplitTop(36, &R.m_PreviewRow, nullptr);
			R.m_Height = std::max(Y, R.m_PreviewRow.y + R.m_PreviewRow.h) - View.y + 6;
			return R;
		}
	};
	struct CPlayerCountryRowLayout
	{
		CUIRect m_Flag, m_Name, m_Arrow;
		static constexpr float FlagWidth = 28;
		static CPlayerCountryRowLayout Calculate(CUIRect Row)
		{
			CPlayerCountryRowLayout R{};
			CUIRect Content;
			Row.VMargin(std::min(LucentStyle::Style::LabelGap, std::max(0.f, Row.w) / 2), &Content);
			Content.VSplitRight(std::min(Row.h, Content.w * .2f), &Content, &R.m_Arrow);
			Content.VSplitLeft(std::min(FlagWidth, Content.w * .25f), &R.m_Flag, &Content);
			R.m_Flag = CPlayerLayout::FitFlag(R.m_Flag);
			Content.VSplitLeft(std::min(LucentStyle::Style::LabelGap, Content.w * .1f), nullptr, &R.m_Name);
			return R;
		}
	};

	struct CPlayerPreviewLayout
	{
		CUIRect m_Tee, m_Name, m_Clan, m_Flag;
		static CPlayerPreviewLayout Calculate(CUIRect Row)
		{
			CPlayerPreviewLayout R{};
			CUIRect Content = SettingsContentRegion(Row);
			Content.VSplitLeft(std::min(36.f, Content.w * .18f), &R.m_Tee, &Content);
			Content.VSplitRight(std::min(28.f, Content.w * .16f), &Content, &R.m_Flag);
			Content.VMargin(std::min(LucentStyle::Style::LabelGap, Content.w / 2), &Content);
			Content.VSplitLeft(Content.w * .62f, &R.m_Name, &R.m_Clan);
			R.m_Flag = CPlayerLayout::FitFlag(R.m_Flag);
			return R;
		}
	};
	struct CPlayerCountryListRowLayout
	{
		CUIRect m_Flag, m_Name, m_Code;
		static CPlayerCountryListRowLayout Calculate(CUIRect Row)
		{
			CPlayerCountryListRowLayout R{};
			CUIRect Content = SettingsContentRegion(Row);
			Content.VSplitLeft(std::min(28.f, Content.w * .18f), &R.m_Flag, &Content);
			Content.VSplitLeft(std::min(LucentStyle::Style::LabelGap, Content.w * .1f), nullptr, &Content);
			Content.VSplitRight(std::min(48.f, Content.w * .28f), &Content, &R.m_Code);
			Content.VSplitRight(std::min(LucentStyle::Style::LabelGap, Content.w * .1f), &R.m_Name, nullptr);
			R.m_Flag = CPlayerLayout::FitFlag(R.m_Flag);
			return R;
		}
	};
	struct CPlayerGalleryLayout
	{
		CUIRect m_Title, m_Search, m_Hint, m_Grid;
		int m_Columns;
		static constexpr float CellHeight = 28;
		static CUIRect Popup(CUIRect Screen)
		{
			const float W = std::min(640.f, std::max(1.f, Screen.w - 12));
			const float H = std::min(400.f, std::max(1.f, Screen.h - 12));
			return {Screen.x + (Screen.w - W) / 2, Screen.y + (Screen.h - H) / 2, W, H};
		}
		static CPlayerGalleryLayout Calculate(CUIRect View)
		{
			CPlayerGalleryLayout R{};
			View.w = std::max(0.f, View.w);
			View.h = std::max(0.f, View.h);
			View.HSplitTop(std::min(24.f, View.h), &R.m_Title, &View);
			View.HSplitTop(std::min(28.f, View.h), &R.m_Search, &View);
			View.HSplitTop(std::min(22.f, View.h), &R.m_Hint, &View);
			// Whole visible rows make initial selection/reflow land on an intact first row.
			View.h = std::floor(View.h / CellHeight) * CellHeight;
			R.m_Grid = View;
			R.m_Columns = View.w >= 460 ? 2 : 1;
			return R;
		}
	};

	// A resource-driven filter: the name table never restricts the available flags.
	template<typename FContains>
	bool PlayerCountryMatches(const char *pCode, const char *pName, const char *pLocalizedName, const char *pNumeric, const char *pQuery, FContains Contains)
	{
		return !*pQuery || Contains(pCode, pQuery) || Contains(pName, pQuery) || Contains(pLocalizedName, pQuery) || Contains(pNumeric, pQuery);
	}
	void RenderPlayer(CUi *pUi, ITextRender *pTextRender, CUIRect View, CCountryFlags *pFlags, bool &Dummy, const CPlayerBindings &Bindings, bool ConsoleActive);
	void LeavePlayer(CUi *pUi);
}
#endif
