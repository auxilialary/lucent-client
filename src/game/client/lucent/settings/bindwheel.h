#ifndef GAME_CLIENT_LUCENT_SETTINGS_BINDWHEEL_H
#define GAME_CLIENT_LUCENT_SETTINGS_BINDWHEEL_H
#include <game/client/ui_rect.h>

#include <algorithm>
class CGameClient;
class CUi;
class ITextRender;
namespace LucentSettings
{
	struct CBindWheelImportLayout
	{
		CUIRect m_Header, m_Target, m_Sources, m_Review, m_Options, m_Secondary, m_Notice, m_Footer;
		static CBindWheelImportLayout Calculate(CUIRect View, bool Options)
		{
			CBindWheelImportLayout L;
			const float Density = std::min(1.f, View.h / 400.f);
			View.HSplitTop(42 * Density, &L.m_Header, &View);
			View.HSplitTop(28 * Density, &L.m_Target, &View);
			View.HSplitTop(8 * Density, nullptr, &View);
			View.HSplitBottom(30 * Density, &View, &L.m_Footer);
			View.HSplitBottom(32 * Density, &View, &L.m_Notice);
			View.HSplitBottom(26 * Density, &View, &L.m_Secondary);
			View.HSplitBottom(Options ? 48 * Density : 0, &View, &L.m_Options);
			const float Gap = std::min(12.f, View.w * .025f);
			View.VSplitLeft((View.w - Gap) * .36f, &L.m_Sources, &L.m_Review);
			L.m_Review.VSplitLeft(Gap, nullptr, &L.m_Review);
			return L;
		}
	};
	struct CBindWheelLayout
	{
		CUIRect m_Title, m_Tabs, m_Editor, m_Preview, m_Controls, m_Footer;
		static CBindWheelLayout Calculate(CUIRect View)
		{
			CBindWheelLayout L;
			const float Density = std::min(1.f, View.h / 440.f);
			View.HSplitTop(36 * Density, &L.m_Title, &View);
			View.HSplitTop(26 * Density, &L.m_Tabs, &View);
			View.HSplitTop(10 * Density, nullptr, &View);
			View.HSplitBottom(24 * Density, &View, &L.m_Footer);
			CUIRect Right;
			const float Gap = std::min(24.f, View.w * .03f);
			View.VSplitLeft(std::max(0.f, (View.w - Gap) * .47f), &L.m_Editor, &Right);
			Right.VSplitLeft(Gap, nullptr, &Right);
			Right.HSplitBottom(std::min(130.f, Right.h * .42f), &L.m_Preview, &L.m_Controls);
			L.m_Preview.h = std::max(0.f, L.m_Preview.h - 10 * Density);
			return L;
		}
	};
	void RenderBindWheel(CGameClient *pGame, CUi *pUi, ITextRender *pTextRender, CUIRect View);
	void LeaveBindWheel(CUi *pUi);
}
#endif
