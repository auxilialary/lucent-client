// Bindwheel console compatibility follows the Tater/Entity Client interface.
// Lucent owns its input state, persistence, two-page model and rendering.
#include "component.h"

#include <base/log.h>

#include <engine/client.h>
#include <engine/config.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/lucent/localization.h>
#include <game/client/lucent/motion/clock.h>
#include <game/client/lucent/style/gradient.h>
#include <game/client/render.h>

using namespace LucentBindWheel;

bool CLucentBindWheel::Allowed() const
{
	return Client()->State() == IClient::STATE_ONLINE && Kernel()->RequestInterface<IEngineGraphics>()->WindowActive() &&
	       !GameClient()->m_Menus.IsActive() && !GameClient()->m_Chat.IsActive() &&
	       !GameClient()->m_GameConsole.IsActive() && !GameClient()->m_Emoticon.IsActive() &&
	       !GameClient()->m_Scoreboard.IsActive() && !GameClient()->m_Spectator.IsActive();
}

void CLucentBindWheel::UpdateShift()
{
	m_Session.Shift(m_LeftShift || m_RightShift);
}

void CLucentBindWheel::ConOpen(IConsole::IResult *pResult, void *pUser)
{
	auto &Self = *static_cast<CLucentBindWheel *>(pUser);
	if(pResult->GetInteger(0))
	{
		// CBinds repeats +commands while held. Cancellation must survive repeat strokes.
		if(!Self.m_Session.Press(Self.GameClient()->m_Binds.m_HandlingNewBind))
			return;
		if(!Self.Allowed())
		{
			Self.OnRelease();
			return;
		}
		if(!Self.IsActive())
		{
			Self.m_LeftShift = Self.Input()->KeyIsPressed(KEY_LSHIFT);
			Self.m_RightShift = Self.Input()->KeyIsPressed(KEY_RSHIFT);
			Self.m_Session.Open(Self.m_LeftShift || Self.m_RightShift, g_Config.m_ClLucentBindwheelResetMouse != 0);
			Self.m_Touch = {};
		}
	}
	else
	{
		Self.m_Session.ReleaseStroke();
		Self.Execute(true);
	}
}

void CLucentBindWheel::Execute(bool Close)
{
	if(!Allowed())
	{
		OnRelease();
		return;
	}
	// Uses event-updated Shift state, including press+release between two frames.
	UpdateShift();
	const std::string Command = m_Session.Execute(m_Pages, 68.f * g_Config.m_ClLucentBindwheelScale / 100.f, Close);
	if(!Command.empty())
		Console()->ExecuteLine(Command.c_str(), IConsole::CLIENT_ID_UNSPECIFIED);
}

void CLucentBindWheel::ConHover(IConsole::IResult *pResult, void *pUser)
{
	if(pResult->GetInteger(0))
		static_cast<CLucentBindWheel *>(pUser)->Execute(false);
}

bool CLucentBindWheel::Set(int PageIndex, int Index, const CBind &Bind)
{
	if(PageIndex < 0 || PageIndex > 1 || Index < 0 || Index > int(m_Pages[PageIndex].size()) || Index >= MaxBinds || !Valid(Bind))
		return false;
	if(Index < int(m_Pages[PageIndex].size()) && m_Pages[PageIndex][Index] == Bind)
		return true;
	m_History.Remember(m_Pages);
	OnRelease();
	auto &Page = m_Pages[PageIndex];
	if(Index == int(Page.size()))
		Page.push_back(Bind);
	else
		Page[Index] = Bind;
	return true;
}

void CLucentBindWheel::Remove(int PageIndex, int Index)
{
	if(PageIndex < 0 || PageIndex > 1 || Index < 0 || Index >= int(m_Pages[PageIndex].size()))
		return;
	m_History.Remember(m_Pages);
	OnRelease();
	m_Pages[PageIndex].erase(m_Pages[PageIndex].begin() + Index);
}

void CLucentBindWheel::Move(int PageIndex, int Index, int Direction)
{
	if(Direction == 0 || PageIndex < 0 || PageIndex > 1 || Index < 0 || Index >= int(m_Pages[PageIndex].size()) || Index + Direction < 0 || Index + Direction >= int(m_Pages[PageIndex].size()))
		return;
	m_History.Remember(m_Pages);
	OnRelease();
	std::swap(m_Pages[PageIndex][Index], m_Pages[PageIndex][Index + Direction]);
}

void CLucentBindWheel::Reorder(int PageIndex, int From, int To)
{
	if(PageIndex < 0 || PageIndex > 1 || From < 0 || To < 0 || From == To || From >= int(m_Pages[PageIndex].size()) || To >= int(m_Pages[PageIndex].size()))
		return;
	m_History.Remember(m_Pages);
	OnRelease();
	LucentBindWheel::Reorder(m_Pages[PageIndex], From, To);
}

bool CLucentBindWheel::Transfer(int FromPage, int From, int ToPage, int To)
{
	if(!CanTransfer(m_Pages, FromPage, From, ToPage, To))
		return false;
	m_History.Remember(m_Pages);
	OnRelease();
	return LucentBindWheel::Transfer(m_Pages, FromPage, From, ToPage, To);
}

bool CLucentBindWheel::Undo()
{
	if(!m_History.Undo(m_Pages))
		return false;
	OnRelease();
	return true;
}

void CLucentBindWheel::ConAdd(IConsole::IResult *pResult, void *pUser)
{
	auto &Self = *static_cast<CLucentBindWheel *>(pUser);
	if(!Self.Set(0, int(Self.Page(0).size()), {pResult->GetString(0), pResult->GetString(1)}))
		Self.Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "bindwheel", "Invalid entry or page is full (64 entries maximum)");
}

void CLucentBindWheel::ConPageAdd(IConsole::IResult *pResult, void *pUser)
{
	auto &Self = *static_cast<CLucentBindWheel *>(pUser);
	const int Page = pResult->GetInteger(0);
	if(Page < 0 || Page > 1 || !Self.Set(Page, int(Self.Page(Page).size()), {pResult->GetString(1), pResult->GetString(2)}))
		Self.Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "bindwheel", "Invalid page/entry or page is full");
}

void CLucentBindWheel::ConLegacy(IConsole::IResult *pResult, void *pUser)
{
	auto &Self = *static_cast<CLucentBindWheel *>(pUser);
	const int Index = pResult->GetInteger(0);
	const CBind Bind{pResult->GetString(1), pResult->GetString(2)};
	if(Index < 0 || Index >= MaxBinds || !Valid(Bind))
		return;
	Self.m_History.Remember(Self.m_Pages);
	Self.OnRelease();
	if(int(Self.m_Pages[0].size()) <= Index)
		Self.m_Pages[0].resize(Index + 1, {"*", ""});
	Self.m_Pages[0][Index] = Bind;
}

void CLucentBindWheel::ConRemove(IConsole::IResult *pResult, void *pUser)
{
	auto &Self = *static_cast<CLucentBindWheel *>(pUser);
	const auto &Page = Self.Page(0);
	auto It = std::find(Page.begin(), Page.end(), CBind{pResult->GetString(0), pResult->GetString(1)});
	if(It != Page.end())
		Self.Remove(0, int(It - Page.begin()));
}

void CLucentBindWheel::ConClear(IConsole::IResult *pResult, void *pUser)
{
	auto &Self = *static_cast<CLucentBindWheel *>(pUser);
	const int Page = pResult->NumArguments() ? pResult->GetInteger(0) : 0;
	if(Page < 0 || Page > 1)
		return;
	if(Self.m_Pages[Page].empty())
		return;
	Self.m_History.Remember(Self.m_Pages);
	Self.OnRelease();
	Self.m_Pages[Page].clear();
}

void CLucentBindWheel::Save(IConfigManager *pConfig, void *pUser)
{
	const auto &Self = *static_cast<CLucentBindWheel *>(pUser);
	for(int Page = 0; Page < 2; ++Page)
		for(const auto &Bind : Self.Page(Page))
		{
			const auto Line = SaveLine(Page, Bind);
			pConfig->WriteLine(Line.c_str());
		}
}

void CLucentBindWheel::OnConsoleInit()
{
	ConfigManager()->RegisterCallback(Save, this);
	Console()->Register("+bindwheel", "", CFGFLAG_CLIENT, ConOpen, this, "Hold to open bindwheel; hold Shift for page 2");
	Console()->Register("+bindwheel_execute_hover", "", CFGFLAG_CLIENT, ConHover, this, "Execute hovered action once per wheel opening");
	Console()->Register("bindwheel", "i[index] s[name] r[command]", CFGFLAG_CLIENT, ConLegacy, this, "Legacy indexed bindwheel entry");
	Console()->Register("add_bindwheel", "s[name] r[command]", CFGFLAG_CLIENT, ConAdd, this, "Add action to page 1");
	Console()->Register("lucent_add_bindwheel", "i[page] s[name] r[command]", CFGFLAG_CLIENT, ConPageAdd, this, "Add action to page 0 or 1 (Shift)");
	Console()->Register("remove_bindwheel", "s[name] r[command]", CFGFLAG_CLIENT, ConRemove, this, "Remove matching action from page 1");
	Console()->Register("delete_all_bindwheel_binds", "?i[page]", CFGFLAG_CLIENT, ConClear, this, "Clear page 0 (default) or 1 (Shift)");
}

void CLucentBindWheel::RefreshSources()
{
	m_Sources = Discover(Storage());
	m_Status = m_Sources.empty() ? "No other-client wheels found. Enter a config file path below." : "Select a source and review its actions before importing.";
}

bool CLucentBindWheel::Import(const CSource &Source, int PageIndex, bool Settings, bool Keys)
{
	if(PageIndex < 0 || PageIndex > 1 || Source.m_Data.m_Binds.empty() || Source.m_Data.m_Errors || Source.m_Data.m_Binds.size() > MaxBinds)
		return false;
	for(const auto &Bind : Source.m_Data.m_Binds)
		if(!Valid(Bind))
			return false;
	if(m_Pages[PageIndex] != Source.m_Data.m_Binds)
		m_History.Remember(m_Pages);
	OnRelease();
	m_Pages[PageIndex] = Source.m_Data.m_Binds;
	if(Settings)
	{
		if(Source.m_Data.m_ResetMouse >= 0)
			g_Config.m_ClLucentBindwheelResetMouse = Source.m_Data.m_ResetMouse;
		if(Source.m_Data.m_Animation >= 0)
			g_Config.m_ClLucentBindwheelAnimation = Source.m_Data.m_Animation;
		if(Source.m_Data.m_Scale >= 0)
			g_Config.m_ClLucentBindwheelScale = Source.m_Data.m_Scale;
	}
	int KeysAdded = 0;
	if(Keys)
		for(const auto &KeyName : Source.m_Data.m_Keys)
		{
			const auto Slot = GameClient()->m_Binds.GetBindSlot(KeyName.c_str());
			if(Slot.m_Key <= KEY_UNKNOWN || Slot.m_Key >= KEY_LAST || Slot.m_Key == KEY_LSHIFT || Slot.m_Key == KEY_RSHIFT)
				continue;
			if(!GameClient()->m_Binds.Get(Slot)[0])
			{
				GameClient()->m_Binds.Bind(Slot.m_Key, "+bindwheel", true, Slot.m_ModifierMask);
				++KeysAdded;
			}
		}
	g_Config.m_ClLucentBindwheelMigrated = 1;
	int Review = 0;
	for(const auto &Bind : Page(PageIndex))
		if(!Bind.m_Command.empty() && !Console()->LineIsValid(Bind.m_Command.c_str()))
			++Review;
	char aStatus[512];
	str_format(aStatus, sizeof(aStatus), LCNTLocalize("Imported %d actions; %d free keys bound; %d commands need review."), int(Page(PageIndex).size()), KeysAdded, Review);
	m_Status = aStatus;
	log_info("bindwheel", "%s Source: %s", m_Status.c_str(), Source.m_Path.c_str());
	return true;
}

void CLucentBindWheel::OnInit()
{
	RefreshSources();
	if(!g_Config.m_ClLucentBindwheelMigrated && Page(0).empty() && Page(1).empty())
		for(const auto &Source : m_Sources)
			if(Import(Source, 0, true, true))
				break;
	m_History.Clear();
	// An intentional empty wheel must remain empty on later starts.
	g_Config.m_ClLucentBindwheelMigrated = 1;
}

void CLucentBindWheel::OnReset()
{
	OnRelease();
	m_Session.m_Cursor = vec2(0, 0);
	m_Reveal = {};
	m_Flip = {};
	m_Touch = {};
	m_DrawSelected = -1;
	m_DrawPage = 0;
}

void CLucentBindWheel::OnRelease() { m_Session.Cancel(); }
void CLucentBindWheel::OnStateChange(int NewState, int OldState) { OnReset(); }

bool CLucentBindWheel::OnCursorMove(float X, float Y, IInput::ECursorType Type)
{
	if(!IsActive())
		return false;
	if(!Allowed())
	{
		OnRelease();
		return false;
	}
	Ui()->ConvertMouseMove(&X, &Y, Type);
	m_Session.m_Cursor += vec2(X, Y);
	const float Limit = 166.f * g_Config.m_ClLucentBindwheelScale / 100.f;
	if(length(m_Session.m_Cursor) > Limit)
		m_Session.m_Cursor = normalize(m_Session.m_Cursor) * Limit;
	return true;
}

bool CLucentBindWheel::OnInput(const IInput::CEvent &Event)
{
	if(!IsActive())
		return false;
	if(!Allowed())
	{
		OnRelease();
		return false;
	}
	if(Event.m_Key == KEY_ESCAPE && (Event.m_Flags & IInput::FLAG_PRESS))
	{
		OnRelease();
		return true;
	}
	if(Event.m_Key == KEY_LSHIFT || Event.m_Key == KEY_RSHIFT)
	{
		bool &Held = Event.m_Key == KEY_LSHIFT ? m_LeftShift : m_RightShift;
		if(Event.m_Flags & IInput::FLAG_PRESS)
			Held = true;
		if(Event.m_Flags & IInput::FLAG_RELEASE)
			Held = false;
		UpdateShift();
		// Release must reach CBinds to stop actions already held on Shift before opening.
		// Its modifier-release path preserves the wheel's own opening key.
		return (Event.m_Flags & IInput::FLAG_RELEASE) == 0;
	}
	return false;
}

void CLucentBindWheel::OnRender()
{
	if(IsActive() && !Allowed())
		OnRelease();
	const auto &Clock = LucentStyle::Motion::Clock();
	const bool Animate = Clock.m_Animate && g_Config.m_ClLucentBindwheelAnimation > 0;
	const float Duration = std::max(.001f, g_Config.m_ClLucentBindwheelAnimation / 1000.f);
	const float Reveal = m_Reveal.Update(IsActive(), Clock.m_Now, Animate, IsActive() ? Duration : Duration * .6f);
	if(Client()->State() != IClient::STATE_ONLINE || Reveal <= .001f)
		return;
	if(IsActive())
	{
		const bool WasTouch = m_Touch.m_AnyPressed;
		Ui()->UpdateTouchState(m_Touch);
		if(m_Touch.m_AnyPressed)
		{
			m_Session.m_Cursor = (m_Touch.m_PrimaryPosition - vec2(.5f, .5f)) * Ui()->Screen()->Size();
			if(length(m_Session.m_Cursor) > 190.f * g_Config.m_ClLucentBindwheelScale / 100.f)
				OnRelease();
		}
		else if(WasTouch)
			Execute(true);
		m_DrawPage = m_Session.m_Page;
		m_DrawSelected = Select(m_Session.m_Cursor, int(Page(m_DrawPage).size()), 68.f * g_Config.m_ClLucentBindwheelScale / 100.f);
	}
	const auto Flip = m_Flip.Update(m_Session.m_Page, Clock.m_Now, Animate);
	const int DrawPage = Flip.m_Page;
	const float Width = Flip.m_Width;
	const float Scale = g_Config.m_ClLucentBindwheelScale / 100.f * (.96f + .04f * Reveal);
	const vec2 Center = Ui()->Screen()->Center();
	const CUIRect Rect{Center.x - 190 * Scale, Center.y - 190 * Scale, 380 * Scale, 380 * Scale};
	Ui()->MapScreen();
	Draw(Rect, DrawPage, DrawPage == m_DrawPage ? m_DrawSelected : -1, Reveal, Width);
	if(IsActive())
		RenderTools()->RenderCursor(Center + m_Session.m_Cursor, 20.f);
}

void CLucentBindWheel::Draw(CUIRect Rect, int PageIndex, int Selected, float Opacity, float WidthScale, bool Preview, int Hovered)
{
	const auto &Binds = Page(PageIndex);
	const vec2 Center = Rect.Center();
	const float R = std::min(Rect.w, Rect.h) / 2, Unit = R / 190.f;
	const float MeshOpacity = Opacity * CUIRect::DrawOpacity();
	const auto Point = [&](float Angle, float Radius) { const vec2 P = direction(Angle) * Radius; return Center + vec2(P.x * WidthScale, P.y); };
	// Match Lucent's ribbon coverage: a one-framebuffer-pixel fringe,
	// including radial edges. Tessellation keeps chord error below 1/8 pixel.
	const float Pixel = Graphics()->GetScreen().Size().y / Graphics()->ScreenSize().y;
	const auto Arc = [&](float Begin, float EndAngle, float Inner, float Outer, ColorRGBA Color) {
		const bool Closed = EndAngle - Begin >= 2 * pi - .001f;
		const float Fringe = Pixel / std::max(.025f, WidthScale);
		const float Half = std::min((Outer - Inner) / 2, Fringe / 2);
		const float Coverage = std::min(1.f, (Outer - Inner) / Fringe);
		const float Radii[] = {std::max(0.f, Inner - Fringe / 2), Inner + Half, Outer - Half, Outer + Fringe / 2};
		const float Alphas[] = {Inner <= 0 ? Coverage : 0, Coverage, Coverage, 0};
		const float AngularFringe = Closed ? 0 : std::min((EndAngle - Begin) / 4, Fringe / std::max(1.f, Outer));
		const float Start = Begin + AngularFringe / 2, End = EndAngle - AngularFringe / 2;
		const int Steps = std::max(2, int(std::ceil((End - Start) * std::sqrt(Outer / Pixel))));
		const auto Strip = [&](float A, float B, float AlphaA, float AlphaB) {
			for(int j = 0; j < 3; ++j)
			{
				Graphics()->SetColor4(Color.WithMultipliedAlpha(MeshOpacity * Alphas[j] * AlphaA),
					Color.WithMultipliedAlpha(MeshOpacity * Alphas[j + 1] * AlphaA),
					Color.WithMultipliedAlpha(MeshOpacity * Alphas[j] * AlphaB),
					Color.WithMultipliedAlpha(MeshOpacity * Alphas[j + 1] * AlphaB));
				const IGraphics::CFreeformItem Quad(Point(A, Radii[j]), Point(A, Radii[j + 1]), Point(B, Radii[j]), Point(B, Radii[j + 1]));
				Graphics()->QuadsDrawFreeform(&Quad, 1);
			}
		};
		if(!Closed)
			Strip(Begin - AngularFringe / 2, Start, 0, 1);
		for(int i = 0; i < Steps; ++i)
			Strip(Start + (End - Start) * i / Steps, Start + (End - Start) * (i + 1) / Steps, 1, 1);
		if(!Closed)
			Strip(End, EndAngle + AngularFringe / 2, 1, 0);
	};
	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	const float Step = 2 * pi / std::max(1, int(Binds.size()));
	for(int i = 0; i < int(Binds.size()); ++i)
	{
		const float A = Step * (i - .5f) + .014f, B = Step * (i + .5f) - .014f;
		const bool Hot = i == Selected || i == Hovered;
		const float Nudge = i == Selected ? 3 * Unit : Hot ? 1.5f * Unit :
								     0;
		Arc(A, B, 80 * Unit + Nudge, R - 7 * Unit + Nudge, LucentStyle::Gradient::Blend(LucentStyle::Style::Surface, LucentStyle::Style::Accent, .065f).WithAlpha(.97f));
		Arc(A, B, R - 7 * Unit + Nudge, R - 6.3f * Unit + Nudge, LucentStyle::Style::Accent.WithAlpha(.16f));
		if(Hot)
		{
			Arc(A, B, 80 * Unit + Nudge, R - 7 * Unit + Nudge, LucentStyle::Style::Accent.WithAlpha(i == Selected ? .14f : .07f));
			Arc(A, B, R - 6 * Unit + Nudge, R - 4 * Unit + Nudge, LucentStyle::Gradient::Sample(float(i) / std::max(1, int(Binds.size())), LucentStyle::Motion::Clock().GradientTime()));
		}
	}
	Arc(0, 2 * pi, 73 * Unit, 74 * Unit, LucentStyle::Style::Accent.WithAlpha(.2f));
	Arc(0, 2 * pi, 0, 73 * Unit, LucentStyle::Style::Surface.WithAlpha(.98f));
	Graphics()->QuadsEnd();
	const auto Previous = TextRender()->GetTextColor();
	const auto Label = [&](CUIRect Box, const char *pText, float Size, ColorRGBA Color) {
		TextRender()->TextColor(Color.WithMultipliedAlpha(Opacity));
		SLabelProperties Props;
		Props.m_MaxWidth = Box.w;
		Props.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&Box, pText, Size, TEXTALIGN_MC, Props);
	};
	// Keep text readable rather than mirroring it on the rear face of the flip.
	if(WidthScale > .35f)
	{
		for(int i = 0; i < int(Binds.size()); ++i)
		{
			const vec2 P = Point(Step * i, 130 * Unit);
			const float W = std::min(96.f, 650.f / std::max(1, int(Binds.size()))) * Unit * WidthScale;
			const auto Name = Binds.size() > 16 ? std::to_string(i + 1) : Binds[i].m_Name;
			Label({P.x - W / 2, P.y - 12 * Unit, W, 24 * Unit}, Name.empty() ? "*" : Name.c_str(), 11 * Unit, i == Selected || i == Hovered ? LucentStyle::Style::Accent : LucentStyle::Style::Text);
		}
		const float W = 132 * Unit * WidthScale;
		if(Hovered >= 0 && Hovered < int(Binds.size()))
			Selected = Hovered;
		const bool HasSelection = Selected >= 0 && Selected < int(Binds.size());
		Label({Center.x - W / 2, Center.y - 30 * Unit, W, 20 * Unit}, HasSelection ? Binds[Selected].m_Name.c_str() : LCNTLocalize(Binds.empty() ? "Empty wheel" : "Choose an action"), 12 * Unit, LucentStyle::Style::PrimaryText);
		Label({Center.x - W / 2, Center.y - 7 * Unit, W, 18 * Unit}, HasSelection ? Binds[Selected].m_Command.c_str() : "",
			9 * Unit, LucentStyle::Style::MutedText);
		Label({Center.x - W / 2, Center.y + 21 * Unit, W, 18 * Unit}, Preview ? (PageIndex == 0 ? "1 / 2" : "2 / 2") : LCNTLocalize(PageIndex == 0 ? "1 / 2  ·  Hold Shift" : "2 / 2  ·  Release Shift"), 9 * Unit, LucentStyle::Style::Accent);
	}
	TextRender()->TextColor(Previous);
}
