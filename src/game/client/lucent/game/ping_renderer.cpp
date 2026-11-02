#include "ping_renderer.h"

#include "ping_geometry.h"

#include <base/str.h>

#include <engine/graphics.h>
#include <engine/textrender.h>

#include <game/client/lucent/motion/transition.h>

#include <cmath>
namespace LucentPing
{
	void Render(IGraphics *pGraphics, ITextRender *pText, CRenderTools *pRenderTools, const CSenderAppearances &Senders, const CState &State, double Now, vec2 WorldMin, vec2 WorldSize, vec2 DistanceOrigin, bool Animate)
	{
		const auto PreviousScreen = pGraphics->GetScreen();
		// Physical viewport aspect, independent of UI scale; logical height keeps markers compact.
		const vec2 View(600.f * pGraphics->ScreenAspect(), 600.f);
		pGraphics->MapScreen(CScreenRect(vec2(0, 0), View));
		pGraphics->TextureClear();
		std::vector<CIndicatorLayout> Placed;
		size_t EventIndex = 0;
		for(const auto &Event : State.Events())
		{
			pGraphics->TextureClear();
			const auto Projection = Project(Event.m_Position, WorldMin, WorldSize, View, 48.f);
			const auto &Sender = Senders[EventIndex++];
			const bool Identity = Event.m_Remote && Sender.m_Available;
			char aDistance[32];
			str_format(aDistance, sizeof(aDistance), "%.1f", DistanceTiles(DistanceOrigin, Event.m_Position));
			const float NameWidth = Identity ? std::min(pText->TextWidth(9.5f, Sender.m_Name.c_str()), std::max(1.f, std::min(96.f, View.x - 48))) : 0;
			const float DistanceWidth = Projection.m_Offscreen ? pText->TextWidth(10, aDistance) : 0;
			const vec2 LabelSize = SenderLabelSize(NameWidth, DistanceWidth, View, Identity);
			vec2 Pos = Projection.m_Position;
			vec2 Label = DistanceLabelPosition(Pos, View, LabelSize);
			if(Projection.m_Offscreen)
			{
				const auto Layout = PlaceIndicator(Pos, View, LabelSize, Placed);
				Pos = Layout.m_Marker;
				Label = Layout.m_Label;
				Placed.push_back(Layout);
			}
			const float Alpha = Event.Alpha(Now);
			const float Appear = Animate ? LucentStyle::Motion::CubicOut(float((Now - Event.m_Created) / .22)) : 1.f;
			const auto Ring = [&](float Radius, float Opacity) {
				// A feathered annulus with round silhouette, rendered as a triangle strip.
				pGraphics->QuadsBegin();
				for(int i = 0; i < 64; ++i)
				{
					const float A = i * 2.f * pi / 64, B = (i + 1) * 2.f * pi / 64;
					const vec2 U(std::cos(A), std::sin(A)), V(std::cos(B), std::sin(B));
					for(int Band = 0; Band < 3; ++Band)
					{
						const float Inner = Radius - 1.2f + Band * .8f, Outer = Inner + .8f;
						const float Low = Band == 0 ? 0.f : Opacity, High = Band == 2 ? 0.f : Opacity;
						const auto Color = LucentStyle::Style::Accent;
						pGraphics->SetColor4(Color.WithMultipliedAlpha(Low), Color.WithMultipliedAlpha(High), Color.WithMultipliedAlpha(Low), Color.WithMultipliedAlpha(High));
						IGraphics::CFreeformItem Quad(Pos.x + U.x * Inner, Pos.y + U.y * Inner, Pos.x + U.x * Outer, Pos.y + U.y * Outer, Pos.x + V.x * Inner, Pos.y + V.y * Inner, Pos.x + V.x * Outer, Pos.y + V.y * Outer);
						pGraphics->QuadsDrawFreeform(&Quad, 1);
					}
				}
				pGraphics->QuadsEnd();
			};
			Ring(6.f * (.7f + .3f * Appear), Alpha * Appear);
			const float Pulse = float((Now - Event.m_Created) / .65);
			if(Animate && Pulse < 1)
				Ring(7 + 13 * Pulse, Alpha * (1 - Pulse) * .55f);
			pGraphics->QuadsBegin();
			pGraphics->SetColor(LucentStyle::Style::Accent.WithMultipliedAlpha(Alpha * Appear));
			pGraphics->DrawCircle(Pos.x, Pos.y, 1.8f, 16);
			pGraphics->QuadsEnd();
			if(Projection.m_Offscreen)
			{
				const vec2 D = Projection.m_Direction, N(-D.y, D.x);
				const vec2 Tip = Pos + D * 14.f, Back = Pos + D * 9.f;
				pGraphics->LinesBegin();
				pGraphics->SetColor(LucentStyle::Style::Accent.WithMultipliedAlpha(Alpha));
				IGraphics::CLineItem Lines[] = {{Tip.x, Tip.y, Back.x + N.x * 4, Back.y + N.y * 4}, {Tip.x, Tip.y, Back.x - N.x * 4, Back.y - N.y * 4}};
				pGraphics->LinesDraw(Lines, 2);
				pGraphics->LinesEnd();
			}
			if(Identity || Projection.m_Offscreen)
			{
				const float Fade = Alpha * Appear;
				pGraphics->TextureClear();
				pGraphics->DrawRect(Label.x, Label.y, LabelSize.x, LabelSize.y, LucentStyle::Style::Surface.WithMultipliedAlpha(Fade * .85f), IGraphics::CORNER_ALL, 3);
				const auto PreviousTextColor = pText->GetTextColor();
				pText->TextColor(LucentStyle::Style::Text.WithMultipliedAlpha(Fade));
				if(Identity)
				{
					auto Tee = Sender.m_Tee;
					Tee.m_Size = 22;
					pRenderTools->RenderIdleTeeAtCenter(&Tee, Label + vec2(15, 16), EMOTE_NORMAL, vec2(1, 0), Fade);
					CTextCursor Text;
					Text.SetPosition(vec2(Label.x + 31, Label.y + (Projection.m_Offscreen ? 4 : 11)));
					Text.m_FontSize = 9.5f;
					Text.m_Flags = TEXTFLAG_RENDER | TEXTFLAG_DISALLOW_NEWLINE | TEXTFLAG_ELLIPSIS_AT_END;
					Text.m_LineWidth = LabelSize.x - 35;
					Text.m_MaxLines = 1;
					pText->TextEx(&Text, Sender.m_Name.c_str());
					if(Projection.m_Offscreen)
						pText->Text(Label.x + 31, Label.y + 19, 10, aDistance);
				}
				else
					pText->Text(Label.x + 4, Label.y + 2, 10, aDistance);
				pText->TextColor(PreviousTextColor);
			}
		}
		pGraphics->SetColor(1, 1, 1, 1);
		pGraphics->MapScreen(PreviousScreen);
	}
}
