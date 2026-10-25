#include <engine/textrender.h>

#include <game/client/lucent/browser_layout.h>
#include <game/client/lucent/browser_state.h>
#include <game/client/lucent/lucent_text_motion.h>
#include <game/client/lucent/preview_policy.h>

#include <cstdio>
#include <cstdlib>
#define CHECK(x) \
	do \
	{ \
		if(!(x)) \
		{ \
			std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); \
			std::exit(1); \
		} \
	} while(0)
int main()
{
	// Visual motion follows actual glyph geometry, with bounded trails at every FPS.
	for(int Fps : {60, 144, 240})
	{
		CLucentTextMotion Caret;
		Caret.Update(vec2(10, 20), 12, 0, true, false);
		Caret.Update(vec2(18, 20), 12, .01, true, false);
		for(int Frame = 1; Frame <= Fps; ++Frame)
		{
			const double Now = .01 + double(Frame) / Fps;
			const vec2 Visual = Caret.Position(Now);
			CHECK(Visual.x >= 10 && Visual.x <= 18 && Visual.y == 20);
			CHECK(distance(Visual, Caret.Tail(Now, 12)) <= 12 * LucentStyle::Motion::TextTrailLength);
			if(Now >= .22)
				CHECK(Visual == vec2(18, 20) && Caret.TrailAlpha(Now) == 0);
		}
		Caret.Update(vec2(25, 20), 12, 2, true, false);
		const vec2 Retarget = Caret.Position(2.04);
		Caret.Update(vec2(30, 20), 12, 2.04, true, false);
		CHECK(distance(Caret.Position(2.04), Retarget) < .001f);
		Caret.Update(vec2(300, 20), 12, 2.05, true, false); // Home/End or paste
		CHECK(Caret.Position(2.05) == vec2(300, 20));
		CHECK(Caret.Tail(2.05, 12) == vec2(300, 20));
		CHECK(Caret.CaretAlpha(2.05) == 0 && Caret.CaretAlpha(3) == 1);
		Caret.Update(vec2(10, 32), 12, 2.06, true, false); // wrap/newline
		CHECK(Caret.Position(2.06) == vec2(10, 32));
		Caret.Update(vec2(16, 32), 12, 2.07, false, false);
		CHECK(Caret.Position(2.07) == vec2(16, 32) && Caret.TrailAlpha(2.07) == 0);
		Caret.Update(vec2(22, 32), 12, 2.08, true, true); // scroll/IME/scale
		CHECK(Caret.Position(2.08) == vec2(22, 32));
		Caret.Reset();
		Caret.Update(vec2(100, 50), 12, 2.09, true, false);
		CHECK(Caret.Position(2.09) == vec2(100, 50));
		CHECK(Caret.Position(30) == vec2(100, 50) && Caret.TrailAlpha(30) == 0);
	}
	for(float Width : {320.f, 600.f, 1000.f})
	{
		const auto Closed = CLucentBrowserFilterLayout::Measure(Width, 0, 0, true, false, false, false);
		const auto Open = CLucentBrowserFilterLayout::Measure(Width, 0, 0, true, false, false, false, 1);
		CHECK(Open.m_ContentHeight == Closed.m_ContentHeight + 30);
	}

	// A closing Filters workspace outlives the shorter generic overlay animation.
	{
		CLucentBrowserState Filters;
		Filters.Toggle(CLucentBrowserState::EPanel::FILTERS);
		Filters.Update(0, false);
		Filters.Escape();
		for(int i = 0; i < 4; ++i)
			Filters.Update(.1f, true);
		CHECK(Filters.m_Expansion == 0 && Filters.FiltersWorkspace());
		CHECK(Filters.m_Overlay.m_Visible == CLucentBrowserState::EOverlay::FILTERS);
		Filters.Toggle(CLucentBrowserState::EPanel::FILTERS);
		Filters.Update(.01f, true);
		CHECK(Filters.FiltersWorkspace());
		Filters.Update(0, false);
		CHECK(Filters.FiltersOpacity() == 1);
	}

	// The same timestamps must produce the same activation at 60/144/240 FPS.
	for(int Fps : {60, 144, 240})
	{
		LucentStyle::Motion::CTransition Filled, Focus;
		LucentStyle::Motion::CClock Clock;
		Clock.BeginFrame(100, true);
		Filled.Update(true, 100, true);
		Focus.Update(true, 100, true);
		for(int Frame = 1; Frame <= Fps / 6; ++Frame)
		{
			const double Now = 100 + double(Frame) / Fps;
			Clock.BeginFrame(Now, true);
			Filled.Update(true, Now, true);
		}
		const double Now = 100 + 1.0 / 6;
		CHECK(std::abs(Filled.Update(true, Now, true) - LucentStyle::Motion::CubicOut(float((1.0 / 6) / LucentStyle::Motion::Hover))) < .0001f);
		const float Before = Filled.m_Value;
		CHECK(Filled.Update(false, Now, true) == Before);
		CHECK(Filled.Update(true, Now, true) == Before);
		CHECK(Filled.Update(true, Now + .2, true) == 1);
		// Losing focus leaves a populated field active; clearing it reverses smoothly.
		CHECK(Focus.Update(true, Now + .2, true) == 1);
		CHECK(Focus.Update(false, Now + .2, true) == 1);
		CHECK(Focus.Update(false, Now + .4, true) == 0);
		CHECK(Filled.m_Value == 1);
		CHECK(Filled.Update(false, Now + .2, true) == 1);
		CHECK(Filled.Update(false, Now + .4, true) == 0);
		CHECK(Filled.Update(true, Now + .4, false) == 1);
		CHECK(Filled.Update(true, Now + .401, true) == 1);
		CHECK(Filled.Update(false, Now + .402, false) == 0);
		CHECK(std::abs(Clock.GradientTime() - double(Fps / 6) / Fps) < .000001);
		const double Phase = Clock.m_Time;
		Clock.BeginFrame(101, false);
		CHECK(Clock.GradientTime() == 0);
		Clock.BeginFrame(102, true);
		CHECK(Clock.GradientTime() == Phase);
	}
	// Exact logo stops, periodicity, reverse travel, and intensity.
	const auto White = LucentStyle::Gradient::Sample(-.25f, 0);
	const auto Rose = LucentStyle::Gradient::Sample(0, 0);
	const auto Pink = LucentStyle::Gradient::Sample(.25f, 0);
	CHECK(std::abs(White.g - 1) < .00001f);
	CHECK(std::abs(Rose.g - 180.f / 255) < .00001f);
	CHECK(std::abs(Pink.g - 161.f / 255) < .00001f);
	CHECK(std::abs(LucentStyle::Gradient::Sample(.12f, 1).g - LucentStyle::Gradient::Sample(.12f, 7).g) < .00001f);
	CHECK(LucentStyle::Gradient::Sample(.2f, 1, {6, 1, 0}).g == 1);
	CHECK(std::abs(LucentStyle::Gradient::Sample(.2f, 1, {6, -1, 1}).g - LucentStyle::Gradient::Sample(.2f, -1).g) < .00001f);

	CHECK(LucentStyle::Gradient::TextSample(0, 100, 0, 1, White) == White);
	CHECK(LucentStyle::Gradient::TextSample(100, 100, 0, 1, White) == Pink);

	// Exercise the color transform used by both renderer atlas passes. A glyph's
	// left/right vertices differ in one frame; translating the container cannot
	// change the local gradient, and alpha remains compatible with color splits.
	for(int Fps : {60, 144, 240})
	{
		LucentStyle::Motion::CClock Clock;
		Clock.BeginFrame(0, true);
		for(int Frame = 1; Frame <= Fps; ++Frame)
			Clock.BeginFrame(double(Frame) / Fps, true);
		const STextVertexColor::FSampler Sampler = [&](float X, ColorRGBA Base) {
			return LucentStyle::Gradient::TextSample(X, 100, Clock.GradientTime(), 1, Base);
		};
		const ColorRGBA Base(.8f, .8f, .8f, .6f), Tint(1, 1, 1, .5f);
		const auto Left = STextVertexColor::Apply(Sampler, 12, 12, Base, Tint);
		const auto Right = STextVertexColor::Apply(Sampler, 62, 12, Base, Tint);
		CHECK(std::abs(Left.g - Right.g) > .05f);
		CHECK(std::abs(Left.a - .3f) < .00001f);
		CHECK(Left == STextVertexColor::Apply(Sampler, 812, 812, Base, Tint));
		CHECK(STextVertexColor::Apply({}, 12, 12, Base, Tint) == ColorRGBA(.8f, .8f, .8f, .3f));
		CHECK(LucentStyle::Gradient::TextSample(12, 100, 1, 0, Base) == Base);
		Clock.BeginFrame(2, false);
		const auto Static = STextVertexColor::Apply(Sampler, 62, 12, Base, Tint);
		Clock.BeginFrame(30, false);
		CHECK(Static == STextVertexColor::Apply(Sampler, 62, 12, Base, Tint));
		CHECK(std::abs(STextVertexColor::Apply(Sampler, 12, 12, Base, Tint).g - Static.g) > .05f);
	}

	// Supporting artwork must leave contextual text below it at every viewport.
	for(float Width : {800.f, 1098.67f, 1280.f, 1706.67f})
	{
		const CUIRect Screen = {0, 0, Width, 600};
		for(float Players : {0.f, 1.f})
		{
			const auto Context = CLucentBrowserSceneLayout::Measure(Screen, 1, Players);
			CHECK(Context.m_Artwork.y + Context.m_Artwork.h <= Context.m_TitleY + .01f);
			CHECK(Context.m_Artwork.x + Context.m_Artwork.w <= Width);
			const auto Preview = CLucentBrowserSceneLayout::Measure(Screen, 0, Players);
			CHECK(Preview.m_Artwork.w > Context.m_Artwork.w * 2);
		}
	}
	CLucentBrowserState Retarget;
	Retarget.m_Scene.m_Target = CLucentBrowserState::EScene::PLAYERS;
	Retarget.Update(.1f, true);
	const float Position = Retarget.ScenePosition();
	Retarget.m_Scene.m_Target = CLucentBrowserState::EScene::PREVIEW;
	Retarget.Update(0, true);
	CHECK(Retarget.ScenePosition() == Position);
	Retarget.Update(0, false);
	CHECK(Retarget.ScenePosition() == 0 && Retarget.PlayersPosition() == 0);
	CHECK(CLucentBrowserState::Descriptor("DDNet RUS - Brutal") == "Brutal");
	CHECK(CLucentBrowserState::Descriptor("DDNet GER - Moderate") == "Moderate");
	CHECK(CLucentBrowserState::Descriptor("DDNet RUS - Custom") == "DDNet RUS - Custom");
	CHECK(CLucentBrowserState::Descriptor("DDNet RUS - Brutal #2") == "DDNet RUS - Brutal #2");
	CHECK(CLucentBrowserState::Descriptor("Other - Insane") == "Other - Insane");
	CHECK(CLucentBrowserState::Descriptor("DDNet invalid - Solo") == "DDNet invalid - Solo");
	CHECK(CLucentBrowserState::Descriptor("DDNet RUS - Brutal #2", "Brutal") == "Brutal");
	CHECK(CLucentBrowserState::Descriptor("Unusual server", "None") == "Unusual server");
	// The actual filter measurement must include every row when overflowing,
	// and shrink again when changing grid tabs or disabling rank indication.
	const auto Countries = CLucentBrowserFilterLayout::Measure(1000, 101, 3, true, true, true, true);
	CHECK(Countries.m_Wide && Countries.m_GridHeight == 21 * 22);
	CHECK(Countries.m_ContentHeight >= Countries.m_GridHeight + 59);
	const auto Types = CLucentBrowserFilterLayout::Measure(1000, 101, 3, false, true, true, true);
	CHECK(Types.m_GridHeight == 17 && Types.m_ContentHeight == 231.f);
	const auto Narrow = CLucentBrowserFilterLayout::Measure(600, 8, 4, true, true, true, true);
	CHECK(!Narrow.m_Wide && Narrow.m_GridHeight == 44);
	CHECK(Narrow.m_ContentHeight > Types.m_ContentHeight);
	CHECK(CLucentBrowserFilterLayout::Measure(600, 8, 4, true, true, true, false).m_ContentHeight == Narrow.m_ContentHeight - 21 + 28);
	CHECK(CLucentBrowserFilterLayout::Measure(600, 8, 4, true, false, false, false).m_GridHeight == 0);
	// Quick controls wrap into bounded rows; narrow workspaces include every group.
	CHECK(CLucentBrowserFilterLayout::QuickColumns(820) == 3);
	CHECK(CLucentBrowserFilterLayout::QuickColumns(420) == 2);
	CHECK(CLucentBrowserFilterLayout::QuickColumns(300) == 1);
	CHECK(CLucentBrowserFilterLayout::QuickHeight(300) > CLucentBrowserFilterLayout::QuickHeight(420));
	for(float Width : {300.f, 420.f, 600.f, 631.f, 632.f, 720.f, 820.f, 1098.67f})
	{
		const auto Layout = CLucentBrowserFilterLayout::Measure(Width, 101, 15, true, true, true, true);
		CHECK(Layout.m_ContentHeight >= Layout.m_GridHeight + CLucentBrowserFilterLayout::QuickHeight(CLucentBrowserFilterLayout::WorkspaceWidth(Width) - 12));
		CHECK(CLucentBrowserFilterLayout::QuickColumns(Width) >= 1);
	}
	// Production column geometry at the requested 1648x900 viewport (600 UI height).
	for(float ScreenWidth : {632.f, 720.f, 1648.f * 600 / 900, 1706.f})
	{
		const float Width = CLucentBrowserFilterLayout::WorkspaceWidth(ScreenWidth);
		const CUIRect Workspace = {(ScreenWidth - Width) / 2, 0, Width - 12, 600};
		const auto Left = CLucentBrowserFilterLayout::Column(Workspace, 0);
		const auto Right = CLucentBrowserFilterLayout::Column(Workspace, 1);
		CHECK(Left.w == Right.w && Left.w >= 290);
		CHECK(Left.x + Left.w < Right.x);
		CHECK(std::abs(Right.x + Right.w - (Workspace.x + Workspace.w)) < .001f);
		CHECK(Left.w < Workspace.w * .5f);
	}
	CHECK(!CLucentBrowserFilterLayout::Measure(631, 0, 0, true, false, false, false).m_Wide);
	CHECK(CLucentBrowserFilterLayout::Measure(632, 0, 0, true, false, false, false).m_Wide);

	CLucentCountryPickerState Picker;
	CHECK(Picker.Reconcile(200, 127, false) && Picker.m_Cursor == 127);
	CHECK(!Picker.CanCommit(200, false, false));
	CHECK(Picker.CanCommit(200, true, false));
	CHECK(Picker.Reconcile(3, -1, true) && Picker.m_Cursor == 0);
	Picker.m_Cursor = 2;
	CHECK(!Picker.Reconcile(3, -1, false) && Picker.m_Cursor == 2);
	CHECK(Picker.CanCommit(3, false, true));
	CHECK(Picker.Reconcile(0, -1, true) && Picker.m_Cursor == -1);
	CHECK(!Picker.CanCommit(0, true, true));
	CHECK(Picker.Reconcile(200, 127, true) && Picker.m_Cursor == 127);
	Picker.m_Cursor = 250;
	CHECK(!Picker.CanCommit(200, true, false));
	Picker = {};
	CHECK(Picker.Reconcile(200, 3, false) && Picker.m_Cursor == 3);
	CLucentBrowserState Tooltip;
	CHECK(Tooltip.TooltipsAllowed());
	Tooltip.Toggle(CLucentBrowserState::EPanel::DIRECT_CONNECT);
	CHECK(!Tooltip.TooltipsAllowed());
	Tooltip.Update(0, false);
	Tooltip.Escape();
	CHECK(!Tooltip.TooltipsAllowed()); // Suppress during the entire closing reveal.
	Tooltip.Update(.1f, true);
	CHECK(!Tooltip.TooltipsAllowed());
	Tooltip.Update(0, false);
	CHECK(Tooltip.TooltipsAllowed());
	CLucentBrowserState Context;
	CHECK(Context.PrimarySelection("browser:8303") == nullptr);
	Context.Toggle(CLucentBrowserState::EPanel::FRIENDS);
	CHECK(std::string(Context.PrimarySelection("browser:8303")).empty());
	Context.Toggle(CLucentBrowserState::EPanel::COMMUNITIES);
	CHECK(std::string(Context.PrimarySelection("friend:8303")).empty());
	Context.Servers();
	CHECK(std::string(Context.PrimarySelection("")).compare("browser:8303") == 0);
	Context.Toggle(CLucentBrowserState::EPanel::FILTERS);
	CHECK(Context.FiltersWorkspace());
	for(int i = 0; i < 32; ++i)
	{
		Context.Update(1.f / 60, true);
		CHECK(Context.BrowserOpacity() == 0 || Context.FiltersOpacity() == 0);
	}
	CHECK(Context.BrowserOpacity() == 0 && Context.FiltersOpacity() == 1);
	Context.Escape();
	for(int i = 0; i < 32; ++i)
	{
		Context.Update(1.f / 60, true);
		CHECK(Context.BrowserOpacity() == 0 || Context.FiltersOpacity() == 0);
	}
	CHECK(!Context.FiltersWorkspace() && Context.BrowserOpacity() == 1);
	Context.Toggle(CLucentBrowserState::EPanel::DIRECT_CONNECT);
	Context.Update(0, false);
	Context.Toggle(CLucentBrowserState::EPanel::FILTERS);
	Context.Update(0, true);
	CHECK(Context.BrowserOpacity() == 1 && Context.FiltersOpacity() == 0);
	Context.Update(.1f, true);
	const float BrowserBeforeReverse = Context.BrowserOpacity();
	Context.Toggle(CLucentBrowserState::EPanel::DIRECT_CONNECT);
	Context.Update(0, true);
	CHECK(Context.BrowserOpacity() == BrowserBeforeReverse);
	Context.Update(0, false);
	CHECK(!Context.FiltersWorkspace());
	CLucentBrowserState State;
	using P = CLucentBrowserState::EPanel;
	CHECK(!State.Escape());
	State.Tab(true);
	CHECK(State.m_Focus == 6);
	State.Tab(false);
	CHECK(State.m_Focus == 0);
	for(int i = 0; i < 7; ++i)
		State.Tab(false);
	CHECK(State.m_Focus == 0);
	using Primary = CLucentBrowserState::EPrimary;
	using Scene = CLucentBrowserState::EScene;
	using Overlay = CLucentBrowserState::EOverlay;
	State.Toggle(P::FRIENDS);
	State.Toggle(P::DETAILS);
	State.Toggle(P::FILTERS);
	State.Update(0, false);
	CHECK(State.m_Primary.m_Visible == Primary::FRIENDS);
	CHECK(State.m_Scene.m_Visible == Scene::DETAILS);
	CHECK(State.m_Overlay.m_Visible == Overlay::FILTERS);
	CHECK(State.Escape());
	CHECK(State.m_Overlay.m_Target == Overlay::NONE && State.m_Scene.m_Target == Scene::DETAILS);
	CHECK(State.Escape());
	CHECK(State.m_Scene.m_Target == Scene::PREVIEW && State.m_Primary.m_Target == Primary::FRIENDS);
	CHECK(State.Escape());
	CHECK(!State.Escape());
	for(int Fps : {30, 60, 144, 240})
	{
		CLucentBrowserState Motion;
		Motion.Toggle(P::FRIENDS);
		Motion.Toggle(P::DETAILS);
		Motion.Toggle(P::FILTERS);
		for(int i = 0; i < Fps; ++i)
			Motion.Update(1.f / Fps, true);
		CHECK(Motion.m_Primary.m_Visible == Primary::FRIENDS && Motion.m_Primary.m_Alpha == 1);
		CHECK(Motion.m_Scene.m_Visible == Scene::DETAILS && Motion.m_Scene.m_Alpha == 1);
		CHECK(Motion.Expansion() == 1);
		Motion.Toggle(P::PLAYERS);
		Motion.Update(.05f, true);
		const float Alpha = Motion.m_Scene.m_Alpha;
		Motion.Toggle(P::DETAILS);
		Motion.Update(0, true);
		CHECK(Motion.m_Scene.m_Alpha == Alpha && Motion.m_Scene.m_Visible == Scene::DETAILS);
		Motion.Escape();
		Motion.Update(.05f, true);
		const float Height = Motion.Expansion();
		Motion.Toggle(P::FILTERS);
		Motion.Update(0, true);
		CHECK(Motion.Expansion() == Height);
		Motion.Toggle(P::DIRECT_CONNECT);
		Motion.Update(0, false);
		CHECK(Motion.m_Overlay.m_Visible == Overlay::DIRECT_CONNECT && Motion.Expansion() == 1);
		Motion.Servers();
		Motion.Update(0, false);
		CHECK(Motion.m_Primary.m_Visible == Primary::SERVERS && Motion.Expansion() == 0);
		CHECK(Motion.m_Scene.m_Target == Scene::DETAILS);
	}
	CLucentBrowserState Closing;
	Closing.Toggle(P::FILTERS);
	Closing.Update(0, false);
	Closing.Escape();
	Closing.Update(.1f, true);
	CHECK(Closing.m_Overlay.m_Visible == Overlay::FILTERS && Closing.Expansion() > 0);
	for(int i = 0; i < 4; ++i)
		Closing.Update(.1f, true);
	CHECK(Closing.m_Overlay.m_Visible == Overlay::NONE && Closing.Expansion() == 0);
	// Retargeting preserves geometry, including reversal and changed UI scale.
	CLucentBrowserIndicator Indicator;
	Indicator.Update(10, 50, 0, true);
	Indicator.Update(100, 90, .1f, true);
	const float Mid = Indicator.X();
	CHECK(Mid > 10 && Mid < 100);
	Indicator.Update(20, 40, 0, true);
	CHECK(Indicator.X() == Mid);
	Indicator.Update(20, 40, .22f, true);
	CHECK(Indicator.X() == 20 && Indicator.Width() == 40);
	Indicator.Update(80, 120, 0, false);
	CHECK(Indicator.X() == 80 && Indicator.Width() == 120);
	State.Select("one:8303", true);
	CHECK(State.m_SelectionPulse == 1);
	State.Update(.1f, true);
	const float Pulse = State.m_SelectionPulse;
	State.Select("one:8303", true);
	CHECK(State.m_SelectionPulse == Pulse);
	State.Select("two:8303", true);
	CHECK(State.m_SelectionPulse == 1);
	State.Select("two:8303", false);
	CHECK(State.m_SelectionPulse == 0);
	State.Toggle(P::COMMUNITIES);
	State.Update(0, false);
	CHECK(State.m_Primary.m_Visible == Primary::COMMUNITIES);
	State.Toggle(P::FILTERS);
	State.Update(0, false);
	CHECK(State.m_Primary.m_Visible == Primary::COMMUNITIES);
	State.Servers();
	State.Update(0, false);
	CHECK(State.m_Primary.m_Visible == Primary::SERVERS && State.m_Overlay.m_Visible == Overlay::NONE);
	for(int Fps : {60, 144, 240})
	{
		CLucentBrowserIndicator Nav;
		Nav.Update(0, 30, 0, true);
		for(int Frame = 0; Frame < int(std::ceil(.22f * Fps)); ++Frame)
			Nav.Update(100, 80, 1.f / Fps, true);
		CHECK(std::abs(Nav.X() - 100) < .001f && std::abs(Nav.Width() - 80) < .001f);
	}
	CLucentBrowserContent<std::string> Title;
	Title.m_Target = "first map";
	Title.Update(.21f, true, .42f);
	CHECK(Title.m_Visible == "first map" && Title.m_Alpha == 0);
	Title.Update(.21f, true, .42f);
	CHECK(Title.Opacity() == 1);
	Title.m_Target = "second map";
	Title.Update(.05f, true, .42f);
	const float TitleAlpha = Title.Opacity();
	Title.m_Target = "first map";
	Title.Update(0, true, .42f);
	CHECK(Title.Opacity() == TitleAlpha);
	Title.m_Target = "latest map";
	Title.Update(0, false, .42f);
	CHECK(Title.m_Visible == "latest map" && Title.Opacity() == 1);
	for(int Fps : {30, 60, 144, 240})
	{
		CLucentBrowserState Selection;
		Selection.Select("one:8303", true);
		for(int i = 0; i < Fps; ++i)
			Selection.Update(1.f / Fps, true);
		CHECK(Selection.m_SelectionPulse == 0);
		Selection.Select("two:8303", true);
		Selection.Update(.1f, true);
		Selection.Select("three:8303", true);
		CHECK(Selection.m_SelectionPulse == 1);
		Selection.Select("", true);
		CHECK(Selection.m_SelectionPulse == 0);
	}
	CLucentBrowserState SharedMap;
	SharedMap.Select("one:8303", true, "map|crc|size");
	CHECK(SharedMap.m_SelectionPulse == 1);
	SharedMap.Update(.1f, true);
	SharedMap.Select("two:8303", true, "map|crc|size");
	CHECK(SharedMap.m_SelectionPulse == 1);
	SharedMap.Select("three:8303", true, "map|new-crc|size");
	CHECK(SharedMap.m_SelectionPulse == 1);
	int Sort = 3, Order = 0;
	State.Sort(3, Sort, Order, true);
	CHECK(Order == 1);
	State.Sort(3, Sort, Order, true);
	CHECK(Order == 2);
	State.Sort(3, Sort, Order, true);
	CHECK(Order == 0);
	State.Sort(5, Sort, Order, false);
	CHECK(Sort == 5 && Order == 0);
	// Two independent network identities may host the same map. Sorting and
	// filtering the view must never silently select a different server.
	std::array<const char *, 3> Addresses = {"host:8303", "host:8304", "other:8303"};
	const auto FindAddress = [&](const char *pAddress) { return State.FindSelection(int(Addresses.size()), pAddress, [&](int i) { return Addresses[i]; }); };
	CHECK(FindAddress("host:8304") == 1);
	std::swap(Addresses[1], Addresses[2]);
	CHECK(FindAddress("host:8304") == 2);
	Addresses[2] = "filtered-out";
	CHECK(FindAddress("host:8304") == -1);
	CHECK(State.FindSelection(0, "host:8304", [&](int i) { return Addresses[i]; }) == -1);
	CHECK(FindAddress("manual-direct-address:8303") == -1);
	CLucentPreviewSelection Preview;
	CHECK(Preview.Select("Kobra 4|a|1", 1));
	CHECK(!Preview.Ready(1.1));
	CHECK(Preview.Ready(1.23));
	const auto First = Preview.Generation();
	Preview.Start();
	CHECK(!Preview.Ready(2));
	CHECK(!Preview.Select("Kobra 4|a|1", 2));
	CHECK(Preview.Accept(First));
	CHECK(Preview.Select("Kobra 4|b|1", 2));
	CHECK(!Preview.Accept(First));
	for(int i = 0; i < 100; ++i)
	{
		Preview.Select(std::to_string(i), 3 + i * .01);
		CHECK(!Preview.Ready(3 + i * .01));
	}
	CHECK(Preview.Select("", 6));
	CHECK(!Preview.Ready(7));
	for(const auto &Size : {std::pair{1920u, 1080u}, std::pair{512u, 2048u}, std::pair{4096u, 8u}})
	{
		const float Scale = LucentPreview::DecodeScale(Size.first, Size.second);
		CHECK(int(Size.first * Scale) * int(Size.second * Scale) <= 57600);
		CHECK(std::max(Size.first, Size.second) * Scale <= 640.01f);
		for(float ScreenWidth : {480.f, 1098.f, 1706.f})
		{
			const float Frame = LucentPreview::FrameScale(Size.first * Scale, Size.second * Scale, ScreenWidth, 600);
			CHECK(Size.first * Scale * Frame <= ScreenWidth * .49f + .01f);
			CHECK(Size.second * Scale * Frame <= 600 * .46f + .01f);
			CHECK(Frame <= 1.6f);
		}
	}
	for(float Mask : {0.f, .1f, .5f, .85f})
		for(float T : {0.f, .1f, .5f, .9f, 1.f})
		{
			const float Old = LucentPreview::FadeAlpha(Mask, T, true, true);
			const float New = LucentPreview::FadeAlpha(Mask, T, false, true);
			CHECK(std::abs(New + Old * (1 - New) - Mask) < .00001f);
			CHECK(std::abs(LucentPreview::FadeAlpha(Mask, T, true, false) - Mask * (1 - T)) < .00001f);
		}

	unsigned char aPng[33] = {137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 1, 64, 0, 0, 0, 180};
	uint32_t W, H;
	CHECK(LucentPreview::PngDimensions(aPng, sizeof(aPng), W, H));
	CHECK(W == 320 && H == 180);
	CHECK(!LucentPreview::PngDimensions(aPng, 32, W, H));
	CHECK(!LucentPreview::PngDimensions(aPng, 5 * 1024 * 1024, W, H));
	aPng[16] = 255;
	CHECK(!LucentPreview::PngDimensions(aPng, sizeof(aPng), W, H));
	aPng[16] = 0;
	aPng[0] = 0;
	CHECK(!LucentPreview::PngDimensions(aPng, sizeof(aPng), W, H));
	CHECK(LucentPreview::ThumbnailUrl("https://ddnet.org/ranks/maps/Kobra_4.png"));
	CHECK(LucentPreview::ThumbnailUrl("https://ddnet.org/ranks/maps/%E2%98%83.png"));
	CHECK(!LucentPreview::ThumbnailUrl("https://ddnet.org.evil/ranks/maps/map.png"));
	CHECK(!LucentPreview::ThumbnailUrl("https://ddnet.org/ranks/maps/../map.png"));
	CHECK(!LucentPreview::ThumbnailUrl("http://ddnet.org/ranks/maps/map.png"));
	LucentPreview::CFailures Failures;
	Failures.Add("missing", 10);
	CHECK(Failures.Contains("missing", 69));
	CHECK(!Failures.Contains("missing", 70));
	CHECK(!Failures.Contains("other", 11));
	for(int i = 0; i < 32; ++i)
		Failures.Add(std::to_string(i), 20 + i);
	CHECK(!Failures.Contains("missing", 55));
	CHECK(Failures.Contains("31", 55));
	struct Texture
	{
		bool m_Valid = false;
		bool IsValid() const { return m_Valid; }
	};
	struct Entry
	{
		std::string m_Key;
		Texture m_Texture;
		double m_Used = 0;
	};
	std::array<Entry, 3> Cache = {{{"a", {true}, 1}, {"b", {true}, 2}, {"c", {false}, 0}}};
	CHECK(LucentPreview::Find(Cache, "a") == 0);
	CHECK(LucentPreview::Find(Cache, "c") == -1);
	CHECK(LucentPreview::EvictionSlot(Cache, 0, 1) == 2);
	Cache[2] = {"new", {true}, 3};
	CHECK(LucentPreview::Find(Cache, "new") == 2);
	CHECK(LucentPreview::EvictionSlot(Cache, 2, -1) == 0);
	std::puts("Lucent browser state checks passed");
}
