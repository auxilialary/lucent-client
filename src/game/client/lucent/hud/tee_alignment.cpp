#include "tee_alignment.h"

#include "visuals.h"

#include <base/str.h>

#include <engine/shared/config.h>

#include <game/client/components/chat.h>
#include <game/client/components/hud.h>
#include <game/client/components/scoreboard.h>
#include <game/client/gameclient.h>

void CHud::RenderLucentAlignment()
{
	using namespace LucentHud;
	using namespace LucentHud::Alignment;
	auto *pGame = GameClient();
	const int Self = pGame->m_Snap.m_LocalClientId;
	if(Client()->State() != IClient::STATE_ONLINE || pGame->m_Snap.m_SpecInfo.m_Active ||
		Self < 0 || Self >= MAX_CLIENTS || Self != pGame->m_aLocalIds[g_Config.m_ClDummy] || !pGame->m_Snap.m_pLocalCharacter ||
		!pGame->m_Snap.m_aCharacters[Self].m_Active || pGame->m_Scoreboard.IsActive())
	{
		m_LucentAlignment = {};
		return;
	}
	if(m_LucentAlignment.m_Controlled != Self)
	{
		m_LucentAlignment = {};
		m_LucentAlignment.m_Controlled = Self;
	}
	// Deliberately use the same snapshot pair and fraction for EVERY tee.
	// RenderPos can contain predicted local positions and unpredicted remote positions.
	CTarget aTargets[MAX_CLIENTS] = {};
	const float Intra = std::clamp(Client()->IntraGameTick(g_Config.m_ClDummy), 0.f, 1.f);
	const int Tick = Client()->GameTick(g_Config.m_ClDummy);
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		const auto &Character = pGame->m_Snap.m_aCharacters[i];
		// m_Active requires both current and previous character snapshots (also on 0.7).
		if(!Character.m_Active || !pGame->m_aClients[i].m_Active)
			continue;
		auto &Target = aTargets[i];
		Target.m_Id = i;
		Target.m_Valid = true;
		Target.m_X = mix((float)Character.m_Prev.m_X, (float)Character.m_Cur.m_X, Intra);
		Target.m_Y = mix((float)Character.m_Prev.m_Y, (float)Character.m_Cur.m_Y, Intra);
		// Only confirm states supplied by the server throughout the interpolated interval.
		const auto *pPrev = Character.m_pPrevExtendedData;
		const auto &Cur = Character.m_ExtendedData;
		Target.m_Frozen = Frozen(Character.m_HasExtendedData, Cur.m_FreezeEnd, Cur.m_Flags, Tick, CHARACTERFLAG_MOVEMENTS_DISABLED) &&
				  pPrev && Frozen(true, pPrev->m_FreezeEnd, pPrev->m_Flags, Client()->PrevGameTick(g_Config.m_ClDummy), CHARACTERFLAG_MOVEMENTS_DISABLED);
	}
	const int Dummy = Client()->DummyConnected() ? pGame->m_aLocalIds[!g_Config.m_ClDummy] : -1;
	const bool aEnabled[2] = {g_Config.m_LucentTeeAlignmentDummy != 0, g_Config.m_LucentTeeAlignmentPlayers != 0};
	const CLayout Layout(m_Width, m_Height, g_Config.m_LucentHudScale);
	const float Dt = Client()->RenderFrameTime();
	const float Alpha = Opacity(g_Config.m_LucentHudOpacity);
	const float Height = Layout.Unit(CapsuleHeight), Gap = Layout.Unit(StackGap);
	const float SlidePixels = 5.f * m_Height / std::max(1, Graphics()->ScreenHeight());
	const float FontSize = TargetFontSize(Layout.Unit(LabelSize), Graphics()->ScreenHeight(), m_Height);
	const float MaxWidth = std::min(Layout.Unit(112), m_Width - Layout.Unit(12));
	const float FixedWidth = Layout.Unit(30); // padding, avatar circle, text gap, reserved snowflake
	int aIds[2];
	bool aPresent[2] = {};
	float aaWidths[2][2] = {};
	float GroupWidth = 0;
	for(int Mode = 0; Mode < 2; ++Mode)
	{
		auto &State = m_LucentAlignment.m_aModes[Mode];
		aIds[Mode] = aEnabled[Mode] ? Select(aTargets, MAX_CLIENTS, Self, Dummy, Mode == 0, State.Target()) : -1;
		// Install the new crossfade layer without advancing its reveal yet.
		State.Update(aIds[Mode], aIds[Mode] >= 0 && aTargets[aIds[Mode]].m_Frozen, 0);
		aPresent[Mode] = aIds[Mode] >= 0;
		for(int LayerIndex = 0; LayerIndex < 2; ++LayerIndex)
		{
			const auto &Layer = State.m_aLayers[LayerIndex];
			if(Layer.m_Id < 0)
				continue;
			aPresent[Mode] |= Layer.m_Alpha > 0;
			auto &Visual = m_aAlignmentVisual[Mode * 2 + LayerIndex];
			// Capture outgoing identity/skin for the crossfade; refresh the active target.
			if(Visual.m_Id != Layer.m_Id || (LayerIndex == State.m_Current && aIds[Mode] >= 0))
			{
				Visual.m_Id = Layer.m_Id;
				Visual.m_Tee = pGame->m_aClients[Layer.m_Id].m_RenderInfo;
				str_copy(Visual.m_aName, Mode == 0 ? (Layer.m_Id == pGame->m_aLocalIds[0] ? "Main" : "Dummy") : pGame->m_aClients[Layer.m_Id].m_aName);
			}
			char aLabel[128];
			const auto Flags = TextRender()->GetRenderFlags();
			TextRender()->SetRenderFlags(TEXT_RENDER_FLAG_ONE_TIME_USE | TEXT_RENDER_FLAG_KERNING);
			const float TextWidth = FitLabel(aLabel, Visual.m_aName, MaxWidth - FixedWidth, [&](const char *pText) {
				return TextRender()->TextWidth(FontSize, pText, -1, m_Width);
			});
			TextRender()->SetRenderFlags(Flags);
			PrepareAlignmentText(Mode * 2 + LayerIndex, aLabel, FontSize);
			aaWidths[Mode][LayerIndex] = std::clamp(TextWidth + FixedWidth, Layout.Unit(40), MaxWidth);
			GroupWidth = std::max(GroupWidth, aaWidths[Mode][LayerIndex]);
		}
	}
	const int Count = (int)aPresent[0] + (int)aPresent[1];
	if(!Count)
	{
		for(auto &Row : m_LucentAlignment.m_aRows)
			Row = {};
		return;
	}
	// Chat does not publish its drawn line bounds. Reserve its source-defined envelope
	// (including the input region) rather than touching or changing the chat renderer.
	const float ChatTop = pGame->m_Chat.IsExpanded() ? 50.f : 200.f;
	const CRect ChatBounds = g_Config.m_ClShowChat || pGame->m_Chat.IsActive() ?
					 CRect{0, ChatTop, pGame->m_Chat.IsActive() ? m_Width : 5.f + g_Config.m_ClChatWidth, m_Height - ChatTop} :
					 CRect{};
	const CRect aObstacles[] = {m_LucentHotbarBounds, m_LucentStatusBounds, m_LucentCoordinatesBounds, ChatBounds};
	const float Bottom = BottomAbove(aObstacles, 4, m_Width, m_Height, GroupWidth, Gap + SlidePixels);
	const bool ObstacleRose = m_LucentAlignment.m_AnchorBottom >= 0 && Bottom < m_LucentAlignment.m_AnchorBottom;
	m_LucentAlignment.m_AnchorBottom = Bottom;
	for(int Mode = 0; Mode < 2; ++Mode)
	{
		auto &Row = m_LucentAlignment.m_aRows[Mode];
		if(!aPresent[Mode])
		{
			Row = {};
			continue;
		}
		const float TargetY = Bottom - Height - (Mode == 0 && aPresent[1] ? Height + Gap : 0);
		Row.Update(TargetY, Dt);
		// Immediate safety clamp when an external obstacle grows/moves upward.
		if(ObstacleRose && Row.m_Y > TargetY)
		{
			Row.m_Y = Row.m_From = TargetY;
			Row.m_Time = 0;
		}
		Row.m_Y = std::min(Row.m_Y, Bottom - Height);
	}
	for(int Mode = 0; Mode < 2; ++Mode)
	{
		auto &State = m_LucentAlignment.m_aModes[Mode];
		// Reserve room by moving the existing upper row before revealing a new lower
		// row. This avoids overlapping capsules during a one-to-two item transition.
		const bool Ready = !aPresent[0] || !aPresent[1] || Mode == 0 ||
				   m_LucentAlignment.m_aRows[0].m_Y + Height + Gap <= m_LucentAlignment.m_aRows[1].m_Y + .001f;
		State.Update(aIds[Mode], aIds[Mode] >= 0 && aTargets[aIds[Mode]].m_Frozen, Ready ? Dt : 0);
	}
	for(int Mode = 0; Mode < 2; ++Mode)
	{
		if(!aPresent[Mode])
			continue;
		const auto &State = m_LucentAlignment.m_aModes[Mode];
		for(int LayerIndex = 0; LayerIndex < 2; ++LayerIndex)
		{
			const auto &Layer = State.m_aLayers[LayerIndex];
			if(Layer.m_Id < 0 || Layer.m_Alpha <= 0)
				continue;
			const float Slide = SlidePixels * std::pow(1.f - Layer.m_Reveal, 3.f);
			const float Width = aaWidths[Mode][LayerIndex];
			float Y = m_LucentAlignment.m_aRows[Mode].m_Y + Slide;
			if(Mode == 0 && (m_LucentAlignment.m_aModes[1].m_aLayers[0].m_Alpha > 0 || m_LucentAlignment.m_aModes[1].m_aLayers[1].m_Alpha > 0))
				Y = std::min(Y, m_LucentAlignment.m_aRows[1].m_Y - Height - Layout.Unit(.25f));
			const CRect Rect{(m_Width - Width) / 2, Y, Width, Height};
			// If an expanded/full-width chat leaves no space beneath the timer, suppress
			// presentation rather than overlap existing HUD. Detection keeps updating.
			if(Rect.y < Layout.Unit(6) || (m_LucentTimerBounds.w > 0 && Rect.y < m_LucentTimerBounds.y + m_LucentTimerBounds.h + Gap))
				continue;
			const float A = Alpha * Layer.m_Alpha;
			Panel(Graphics(), Rect, A, Layout.m_Scale);
			const float CircleSize = Layout.Unit(14), Padding = Layout.Unit(2);
			const vec2 TeeCenter(Rect.x + Padding + CircleSize / 2, Rect.y + Height / 2);
			Graphics()->DrawRect(TeeCenter.x - CircleSize / 2, TeeCenter.y - CircleSize / 2, CircleSize, CircleSize, Accent(LucentStyle::Style::Hover * A), IGraphics::CORNER_ALL, CircleSize / 2);
			CTeeRenderInfo Tee = m_aAlignmentVisual[Mode * 2 + LayerIndex].m_Tee;
			Tee.m_Size = Layout.Unit(13);
			RenderTools()->RenderIdleTeeAtCenter(&Tee, TeeCenter, EMOTE_NORMAL, vec2(1, 0), A);
			DrawAlignmentText(Mode * 2 + LayerIndex, Rect.x + Layout.Unit(19), Rect.y + Height / 2, A);
			if(Layer.m_Freeze > .001f)
			{
				IGraphics::CLineItem aLines[9];
				const vec2 Center(Rect.x + Width - Layout.Unit(6), Rect.y + Height / 2);
				for(int i = 0; i < 3; ++i)
				{
					const vec2 Dir = direction(i * pi / 3), Offset = Dir * Layout.Unit(2.2f);
					aLines[i] = IGraphics::CLineItem(Center.x - Offset.x, Center.y - Offset.y, Center.x + Offset.x, Center.y + Offset.y);
				}
				for(int i = 0; i < 6; ++i)
				{
					const vec2 Dir = direction(i * pi / 3), Tip = Center + Dir * Layout.Unit(1.8f);
					const vec2 End = Tip - direction(i * pi / 3 + pi / 4) * Layout.Unit(.9f);
					aLines[3 + i] = IGraphics::CLineItem(Tip.x, Tip.y, End.x, End.y);
				}
				Graphics()->TextureClear();
				Graphics()->LinesBegin();
				Graphics()->SetColor(FrozenAccent(A * Layer.m_Freeze));
				Graphics()->LinesDraw(aLines, 9);
				Graphics()->LinesEnd();
			}
		}
	}
	Graphics()->SetColor(1, 1, 1, 1);
}

float CHud::PrepareAlignmentText(int Slot, const char *pText, float Size)
{
	auto &Text = m_aLucentText[LUCENT_ALIGNMENT_0 + Slot];
	const int PixelHeight = Graphics()->ScreenHeight();
	if(Text.m_Index.Valid() && (Text.m_Size != Size || Text.m_PixelHeight != PixelHeight))
		TextRender()->DeleteTextContainer(Text.m_Index);
	if(!Text.m_Index.Valid() || Text.m_Size != Size || str_comp(Text.m_aText, pText) != 0)
	{
		CTextCursor Cursor;
		Cursor.m_FontSize = Size;
		Cursor.m_LineWidth = m_Width;
		const auto Flags = TextRender()->GetRenderFlags();
		const auto Color = TextRender()->GetTextColor();
		TextRender()->SetRenderFlags(TEXT_RENDER_FLAG_ONE_TIME_USE | TEXT_RENDER_FLAG_KERNING);
		TextRender()->TextColor(1, 1, 1, 1);
		if(Text.m_Index.Valid())
			TextRender()->RecreateTextContainerSoft(Text.m_Index, &Cursor, pText);
		else
			TextRender()->CreateTextContainer(Text.m_Index, &Cursor, pText);
		TextRender()->SetRenderFlags(Flags);
		TextRender()->TextColor(Color);
		str_copy(Text.m_aText, pText);
		Text.m_Size = Size;
		Text.m_PixelHeight = PixelHeight;
	}
	return Text.m_Index.Valid() ? TextRender()->GetBoundingBoxTextContainer(Text.m_Index).m_W : 0;
}

void CHud::DrawAlignmentText(int Slot, float X, float CenterY, float Alpha)
{
	const auto &Text = m_aLucentText[LUCENT_ALIGNMENT_0 + Slot];
	if(!Text.m_Index.Valid())
		return;
	// Native renderer aligns translated containers to physical pixels. Glyphs are
	// rasterized at the target size; no shadow copy, outline, or container scaling.
	const auto Bounds = TextRender()->GetBoundingBoxTextContainer(Text.m_Index);
	TextRender()->RenderTextContainer(Text.m_Index, LucentHud::Text(Alpha), ColorRGBA(0, 0, 0, 0), X, CenterY - Bounds.m_H / 2);
}
