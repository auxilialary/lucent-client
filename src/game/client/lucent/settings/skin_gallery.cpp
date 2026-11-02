#include "skin_gallery.h"

#include <engine/font_icons.h>
#include <engine/keys.h>

#include <game/client/lineinput.h>
#include <game/client/lucent/ui.h>
#include <game/client/lucent/ui/selection_corners.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>
namespace LucentSettings
{
	void RenderSkinGallery(CUi *pUi, ITextRender *pTextRender, CUIRect View, int Count, bool ScrollToSelected, const FSkinItem &GetItem, const FSkinPaint &Paint, const FSkinAction &Select, const FSkinAction &Favorite, bool KeyboardFocus)
	{
		static CListBox s_List;
		const CUIRect Clip = CSkinGalleryGeometry::Clip(View, pUi->PixelSize());
		if(Clip.w <= 0 || Clip.h <= 0)
			return;
		pUi->ClipEnable(&Clip);
		int Selected = -1;
		for(int i = 0; i < Count; ++i)
			if(GetItem(i).m_Selected)
				Selected = i;
		if(ScrollToSelected && Selected >= 0)
			s_List.ScrollToSelected();
		s_List.SetScrollbarWidth(8);
		s_List.SetScrollbarMargin(1);
		s_List.SetColors(LucentUi::Accent(0), LucentUi::Accent(0));
		s_List.SetActive(pUi->Enabled() && !CLineInput::GetActiveInput() && KeyboardFocus);
		s_List.DoStart(CSkinGalleryGeometry::RowHeight, Count, CSkinGalleryGeometry::Columns(View.w), 2, Selected, &View, false);
		int FavoriteIndex = -1;
		if(pUi->Enabled() && !CLineInput::GetActiveInput() && KeyboardFocus && Selected >= 0 && pUi->Input()->KeyPress(KEY_SPACE))
			FavoriteIndex = Selected;
		for(int i = 0; i < Count; ++i)
		{
			const auto Entry = GetItem(i);
			const auto Item = s_List.DoNextItem(Entry.m_pId, Entry.m_Selected, LucentUi::RADIUS);
			const float Hot = LucentUi::Highlight(Entry.m_pId, pUi->MouseHovered(&Item.m_Rect));
			const float Active = LucentUi::Highlight(Entry.m_pId, Entry.m_Selected, LucentUi::EChannel::ACTIVE);
			if(!Item.m_Visible)
				continue;
			LucentUi::DrawSelectionBackground(pUi->Graphics(),Item.m_Rect,pUi->PixelSize(),LucentUi::Accent(LucentStyle::Style::Hover * Hot + LucentStyle::Style::SurfaceGlow * Active));
			LucentUi::DrawSelectionCorners(pUi->Graphics(),Item.m_Rect,pUi->PixelSize(),Active);
			CUIRect Content, Star, Name, Preview;
			Item.m_Rect.Margin(5, &Content);
			Content.HSplitBottom(18, &Preview, &Name);
			Preview.VSplitRight(std::min(22.f, Preview.w * .25f), &Preview, &Star);
			Star.HSplitTop(22, &Star, nullptr);
			Paint(i, Preview);
			const auto Previous = pTextRender->GetTextColor();
			SLabelProperties Props;
			Props.m_MaxWidth = Name.w;
			Props.m_EllipsisAtEnd = true;
			Props.m_EnableWidthCheck = false;
			pTextRender->TextColor(LucentStyle::Style::Text);
			pUi->DoLabel(&Name, Entry.m_pName, 11, TEXTALIGN_MC, Props);
			pTextRender->TextColor(Previous);
			const CUIRect Hit = CSkinGalleryGeometry::Intersect(Item.m_Rect, Clip);
			if(pUi->MouseHovered(&Hit))
				pUi->DoToolTip(Entry.m_pId, &Hit, Entry.m_pName);
			const bool Focus = Entry.m_Selected && KeyboardFocus;
			const float Visibility = LucentUi::Highlight(Entry.m_pFavoriteId, Entry.m_Favorite || pUi->MouseHovered(&Hit) || Focus, LucentUi::EChannel::FOCUS);
			const bool Pressed = pUi->CheckActiveItem(Entry.m_pFavoriteId);
			if(Visibility > .001f)
			{
				pTextRender->SetFontPreset(EFontPreset::ICON_FONT);
				pTextRender->TextColor((Entry.m_Favorite || Pressed ? LucentStyle::Style::Accent : LucentStyle::Style::MutedText).WithAlpha(Visibility));
				pUi->DoLabel(&Star, FontIcon::STAR, 12, TEXTALIGN_MC);
				pTextRender->SetFontPreset(EFontPreset::DEFAULT_FONT);
				pTextRender->TextColor(Previous);
			}
			if(pUi->DoButtonLogic(Entry.m_pFavoriteId, 0, &Star, BUTTONFLAG_LEFT))
				FavoriteIndex = i;
			const CUIRect StarHit = CSkinGalleryGeometry::Intersect(Star, Clip);
			if(pUi->MouseHovered(&StarHit))
				pUi->DoToolTip(Entry.m_pFavoriteId, &StarHit, Localize("Favorite (Space)"));
		}
		const int NewSelected = s_List.DoEnd();
		pUi->ClipDisable();
		// Do not let the list's surrounding row turn a star click into a skin selection.
		if(FavoriteIndex >= 0)
			Favorite(FavoriteIndex);
		else if(pUi->Enabled() && NewSelected >= 0 && NewSelected < Count && NewSelected != Selected)
			Select(NewSelected);
		if(!Count)
			pUi->DoLabel(&View, Localize("No skins match your filter criteria"), 11, TEXTALIGN_MC);
	}
}
