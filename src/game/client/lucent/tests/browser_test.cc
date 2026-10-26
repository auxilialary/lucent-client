#include <engine/textrender.h>

#include <game/client/lucent/browser_layout.h>
#include <game/client/lucent/browser_state.h>
#include <game/client/lucent/checkbox_geometry.h>
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
	// Optical placement follows font baseline/cap metrics, not row height.
	for(float Baseline : {11.25f, 13.f, 15.5f})
		for(float CapHeight : {6.f, 8.f, 9.25f})
		{
			const auto Center = LucentUi::CheckboxCenter(vec2(3, 7), Baseline, CapHeight);
			CHECK(Center.y == 7 + Baseline - CapHeight / 2);
			CHECK(Center.x == 15);
		}

	// Circle radii, area, winding and concentricity at fractional UI scales.
	for(float Scale : {.5f, .75f, 1.f, 1.25f, 2.f, 3.f})
	{
		using namespace LucentStyle;
		const vec2 Center(17.25f, 41.75f);
		for(float Radius : {Checkbox::Size * Scale / 2, (Checkbox::Size / 2 - Checkbox::Border) * Scale, Checkbox::DotSize * Scale / 2})
		{
			const auto Boundary = LucentUi::CheckboxBoundary(Center, Radius);
			float Area = 0;
			vec2 Mean(0, 0);
			for(size_t I = 0; I < Boundary.size(); ++I)
			{
				const vec2 A = Boundary[I] - Center, B = Boundary[(I + 1) % Boundary.size()] - Center;
				CHECK(std::abs(length(A) - Radius) < .0001f);
				CHECK(A.x * B.y - A.y * B.x > 0);
				Area += (A.x * B.y - A.y * B.x) / 2;
				Mean += A / float(Boundary.size());
			}
			CHECK(length(Mean) < .0001f);
			CHECK(std::abs(Area - pi * Radius * Radius) < .001f * pi * Radius * Radius);
		}
	}
	// Coverage stays inside actual 21 UI Filters rows and clear of the label.
	for(float PixelsPerUnit : {.5f, .75f, 1.f, 1.25f, 1.5f, 2.f, 3.f})
		for(float Progress : {0.f, .01f, .25f, .5f, .75f, 1.f})
		{
			using namespace LucentStyle;
			const vec2 Origin(17.25f, 41.75f);
			const vec2 Center = LucentUi::CheckboxCenter(Origin, 14.f, 8.f);
			const auto Coverage = LucentUi::CheckboxRingCoverage(1.f / PixelsPerUnit);
			const float Feather = .5f / PixelsPerUnit;
			const float Half = Checkbox::Size / 2;
			const auto Outer = LucentUi::CheckboxBoundary(Center, Half + Feather);
			for(const auto &Point : Outer)
			{
				CHECK(Point.x > Origin.x && Point.y > Origin.y && Point.y < Origin.y + 21);
				CHECK(Point.x < Origin.x + Checkbox::LabelOffset);
			}
			CHECK(std::abs(Center.x - (Origin.x + 12.f)) < .0001f);
			CHECK(std::abs(Center.y - (Origin.y + 10.f)) < .0001f);
			CHECK(Checkbox::LabelOffset - (Center.x - Origin.x + Half) == 7.f);
			const float DotRadius = Checkbox::DotSize / 2 * Progress;
			const float DotEdge = std::min(.5f / PixelsPerUnit, DotRadius * .9f);
			CHECK(DotRadius - DotEdge >= 0);
			CHECK(DotRadius + DotEdge < Half - Checkbox::Border);
			CHECK(Coverage.m_Radii[0] < Coverage.m_Radii[1]);
			CHECK(Coverage.m_Radii[1] <= Coverage.m_Radii[2]);
			CHECK(Coverage.m_Radii[2] < Coverage.m_Radii[3]);
			CHECK(std::abs((Coverage.m_Radii[0] + Coverage.m_Radii[3]) / 2 - (Half - Checkbox::Border / 2)) < .0001f);
			CHECK(Coverage.m_Peak > 0 && Coverage.m_Peak <= 1);
			if(PixelsPerUnit * Checkbox::Border < 1)
				CHECK(Coverage.m_Peak < 1);
		}

	{
		using namespace LucentStyle;
		const auto Idle = Style::Text.WithAlpha(Checkbox::TextIdle);
		const auto Inactive = Gradient::TextSample(0, 100, 2, 0, Idle);
		CHECK(Inactive.r == Idle.r && Inactive.a == .8f);
		const auto Left = Gradient::TextSample(0, 100, 0, 1, Idle);
		const auto Right = Gradient::TextSample(100, 100, 0, 1, Idle);
		CHECK(std::abs(Left.g - Right.g) > .2f);
		const auto Surface = Gradient::Sample(Gradient::ControlOrigin, 2, {Checkbox::Period});
		const auto Text = Gradient::TextSample(0, 100, 2, 1, Idle, {Checkbox::Period});
		CHECK(std::abs(Surface.g - Text.g) < .0001f);
	}

	for(int Fps : {60, 144, 240})
	{
		LucentStyle::Motion::CTransition Fill;
		Fill.Update(true, 0, true, LucentStyle::Checkbox::FillDuration);
		float Previous = 0;
		for(int Frame = 1; Frame <= Fps; ++Frame)
		{
			const float Value = Fill.Update(true, double(Frame) / Fps, true, LucentStyle::Checkbox::FillDuration);
			CHECK(Value >= Previous && Value <= 1);
			Previous = Value;
		}
		CHECK(Fill.m_Value == 1);
	}
	// Checkbox reversal preserves the visible value, including rapid retargets.
	{
		using namespace LucentStyle;
		Motion::CTransition Fill;
		CHECK(Fill.Update(false, 0, true) == 0);
		Fill.Update(true, 0, true, Checkbox::FillDuration);
		const float Partial = Fill.Update(true, .06, true, Checkbox::FillDuration);
		CHECK(Partial > 0 && Partial < 1);
		CHECK(Fill.Update(false, .06, true, Checkbox::FillDuration) == Partial);
		const float Falling = Fill.Update(false, .08, true, Checkbox::FillDuration);
		CHECK(Falling < Partial);
		CHECK(Fill.Update(true, .08, true, Checkbox::FillDuration) == Falling);
		CHECK(Fill.Update(true, 1, true, Checkbox::FillDuration) == 1);
		CHECK(Fill.Update(false, 1, false) == 0);
		CHECK(Fill.Update(true, 1, false) == 1);
		Motion::CClock Clock;
		Clock.BeginFrame(0, true);
		Clock.BeginFrame(.05, true);
		const double Phase = Clock.GradientTime();
		CHECK(Phase == .05);
		Clock.BeginFrame(2, false);
		CHECK(Clock.GradientTime() == 0);
		Clock.BeginFrame(4, true);
		CHECK(Clock.GradientTime() == Phase);
		const auto A = Gradient::Sample(-.25f, 0, {Checkbox::Period});
		const auto B = Gradient::Sample(.25f, 0, {Checkbox::Period});
		CHECK(std::abs(A.g - B.g) > .2f);
		const auto Loop = Gradient::Sample(-.25f, Checkbox::Period, {Checkbox::Period});
		CHECK(std::abs(A.g - Loop.g) < .0001f);
		CHECK(Checkbox::Size == 14 && Checkbox::DotSize == 7);
		CHECK(Checkbox::Border == 1.1f);
	}

	// Replay the same edit events at each refresh rate. Motion uses event time,
	// never frame count, and cannot accumulate lag during held Backspace.
	{
		CLucentTextMotion Slow;
		Slow.Update(vec2(100, 20), 12, 0, true, true);
		Slow.Update(vec2(105, 20), 12, .5, true, false);
		Slow.Update(vec2(119, 20), 12, 1, true, false);
		CHECK(Slow.Position(1) == vec2(119, 20));
		Slow.Update(vec2(133, 20), 12, 1.04, true, false);
		CHECK(Slow.Position(1.04).x == 133 && Slow.Tail(1.04, 12).x < 133);
		CHECK(133 - Slow.Tail(1.04, 12).x <= 14 * LucentStyle::Motion::TextTrailGlyphs);
		const auto BeforeScroll = Slow.Tail(1.05, 12);
		Slow.Translate(vec2(-20, 0));
		CHECK(distance(Slow.Tail(1.05, 12), BeforeScroll - vec2(20, 0)) < .001f);
		Slow.Update(vec2(10, 40), 12, 1.06, true, false);
		CHECK(Slow.Position(1.06) == vec2(10, 40) && Slow.TrailAlpha(1.06) == 0);
	}
	float Reference = 0;
	for(int Fps : {60, 144, 240})
	{
		CLucentTextMotion Caret;
		Caret.Update(vec2(100, 20), 12, 0, true, true);
		for(int Event = 1; Event <= 40; ++Event)
		{
			const double Now = Event * .04;
			const float X = Event <= 20 ? 100 + Event * 8 : 260 - (Event - 20) * 8;
			Caret.Update(vec2(X, 20), 12, Now, true, false);
			for(int Frame = 0; Frame < Fps * .04; ++Frame)
			{
				const double Time = Now + double(Frame) / Fps;
				Caret.Update(vec2(X, 20), 12, Time, true, false);
				const auto Head = Caret.Position(Time);
				CHECK(Head.x == X);
				CHECK(Event <= 20 ? Caret.Tail(Time, 12).x <= X : Caret.Tail(Time, 12).x >= X);
				CHECK(distance(Head, Caret.Tail(Time, 12)) <= 8 * LucentStyle::Motion::TextTrailGlyphs + .001f);
			}
			if(Event == 40)
			{
				CHECK(Caret.Position(Now).x == X && Caret.Tail(Now, 12).x > X);
				if(Fps == 60)
					Reference = Caret.Tail(Now, 12).x;
				CHECK(std::abs(Caret.Tail(Now, 12).x - Reference) < .001f);
			}
		}
		CHECK(Caret.Position(2) == vec2(100, 20) && Caret.TrailAlpha(2) == 0);
		Caret.Update(vec2(300, 20), 12, 2.1, true, true);
		CHECK(Caret.Position(2.1) == vec2(300, 20) && Caret.TrailAlpha(2.1) == 0);
		Caret.Update(vec2(308, 20), 12, 2.2, false, false);
		CHECK(Caret.Position(2.2) == vec2(308, 20) && Caret.TrailAlpha(2.2) == 0);
		Caret.Translate(vec2(-30, 0));
		CHECK(Caret.Position(2.2) == vec2(278, 20));
		Caret.Reset();
		Caret.Update(vec2(10, 32), 12, 3, true, false);
		CHECK(Caret.Position(3) == vec2(10, 32) && Caret.TrailAlpha(3) == 0);
	}

	// Held-key regression: no advancing/duplicate head and no age reset of
	// earlier letters, even when repeat events arrive between render frames.
	for(int Fps : {60, 144, 240})
	{
		CLucentTextMotion Motion;
		CLucentTextAppear Appear;
		Motion.Update(vec2(100, 20), 12, 0, true, true);
		for(int Repeat = 1; Repeat <= 120; ++Repeat)
		{
			const double Now = Repeat * .025;
			const float X = Repeat <= 60 ? 100 + Repeat * 8 : 580 - (Repeat - 60) * 8;
			Motion.Update(vec2(X, 20), 12, Now, true, false);
			if(Repeat <= 60)
				Appear.Replace(Repeat - 1, 0, 1, Now);
			else
				Appear.Replace(120 - Repeat, 1, 0, Now);
			for(int Frame = 0; Frame < Fps * .025; ++Frame)
			{
				const double Time = Now + double(Frame) / Fps;
				CHECK(Motion.Position(Time) == vec2(X, 20));
				const float Tail = Motion.Tail(Time, 12).x;
				CHECK(Repeat <= 60 ? Tail <= X : Tail >= X);
				CHECK(std::abs(Tail - X) <= 12.001f);
				if(Repeat > 7 && Repeat <= 60)
				{
					CHECK(Appear.Alpha(Repeat - 7, Time) == 1);
					CHECK(Appear.Alpha(Repeat - 1, Now) == 0);
				}
			}
		}
		CHECK(Motion.Tail(3.25, 12) == vec2(100, 20));
		CHECK(Motion.TrailAlpha(3.25) == 0);
	}

	// Fade only inserted decoded characters. A Cyrillic letter, emoji and a
	// combining mark each occupy one position here, independent of UTF-8 bytes.
	for(int Fps : {60, 144, 240})
	{
		CLucentTextAppear Appear;
		Appear.Replace(3, 0, 1, 1);
		CHECK(Appear.Alpha(2, 1) == 1 && Appear.Alpha(3, 1) == 0);
		float Previous = 0;
		for(int Frame = 0; Frame <= Fps; ++Frame)
		{
			const double Now = 1 + double(Frame) / Fps;
			const float Alpha = Appear.Alpha(3, Now);
			CHECK(Alpha >= Previous && Alpha <= 1);
			CHECK(Appear.Alpha(2, Now) == 1);
			Previous = Alpha;
		}
		CHECK(!Appear.Active(2));
		Appear.Replace(4, 0, 1, 2);
		Appear.Replace(5, 0, 1, 2.04);
		CHECK(Appear.Alpha(3, 2.04) == 1);
		CHECK(Appear.Alpha(4, 2.04) > Appear.Alpha(5, 2.04));
		// Insert inside a still-fading paste: its two surviving parts retain age.
		Appear.Reset();
		Appear.Replace(2, 0, 4, 3);
		Appear.Replace(4, 0, 1, 3.04);
		CHECK(Appear.Alpha(3, 3.04) == Appear.Alpha(5, 3.04));
		CHECK(Appear.Alpha(4, 3.04) == 0 && Appear.Alpha(7, 3.04) == 1);
		// Backspace removes immediately and preserves the surviving suffix age.
		Appear.Replace(3, 3, 0, 3.05);
		CHECK(Appear.Alpha(2, 3.05) == Appear.Alpha(3, 3.05));
		CHECK(Appear.Alpha(4, 3.05) == 1);
		// IME preedit is fully visible and shifts only display indices.
		CHECK(Appear.Alpha(3, 3.05, 3, 2) == 1);
		CHECK(Appear.Alpha(5, 3.05, 3, 2) == Appear.Alpha(3, 3.05));
		// Large paste/committed Unicode text is a single bounded range.
		Appear.Reset();
		Appear.Replace(7, 2, 100000, 4);
		CHECK(Appear.Alpha(6, 4) == 1 && Appear.Alpha(100006, 4) == 0);
		CHECK(Appear.Alpha(7, 4.06) > .49f && Appear.Alpha(7, 4.06) < .51f);
		CHECK(Appear.Alpha(100007, 4) == 1);
		Appear.Reset(); // Input switch, programmatic Set and disabled animations.
		CHECK(!Appear.Active(4) && Appear.Alpha(7, 4) == 1);
	}
	{
		// Exercise the renderer's glyph-alpha hook with the existing gradient.
		const float Alpha = .35f;
		const STextGlyphOpacity Opacity{&Alpha, [](int Index, const void *pContext) {
							return Index == 2 ? *static_cast<const float *>(pContext) : 1.f;
						}};
		const ColorRGBA Base(.9f, .8f, .7f, .6f);
		const auto Faded = Opacity.Apply(2, Base);
		CHECK(Faded.r == Base.r && Faded.g == Base.g && Faded.b == Base.b);
		CHECK(std::abs(Faded.a - .21f) < .00001f);
		CHECK(Opacity.Apply(1, Base) == Base && STextGlyphOpacity{}.Apply(2, Base) == Base);
		const auto Colored = LucentStyle::Gradient::TextSample(20, 100, 1, 1, Faded);
		CHECK(Colored.a == Faded.a && Colored.g != Faded.g);
	}
	{
		CLucentTextMotion Batch, Slow, Fast;
		for(auto *pMotion : {&Batch, &Slow, &Fast})
			pMotion->Update(vec2(100, 20), 12, 0, true, true);
		Batch.Update(vec2(140, 20), 12, .04, true, false, 5);
		CHECK(Batch.Position(.04).x == 140 && Batch.Tail(.04, 12).x >= 128);
		Slow.Update(vec2(108, 20), 12, .2, true, false);
		Fast.Update(vec2(108, 20), 12, .04, true, false);
		CHECK(Fast.Position(.04).x == 108 && Slow.Position(.2).x == 108);
		CHECK(Fast.TrailAlpha(.04) > Slow.TrailAlpha(.2));
		// Reversal immediately moves the ribbon behind the new direction.
		Fast.Update(vec2(100, 20), 12, .08, true, false);
		CHECK(Fast.Position(.08).x == 100 && Fast.Tail(.08, 12).x > 100);
		CHECK(Fast.Position(.30) == vec2(100, 20) && Fast.TrailAlpha(.30) == 0);
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
			CHECK(Preview.m_Artwork.w == Context.m_Artwork.w);
			CHECK(Preview.m_Artwork.h > Context.m_Artwork.h * 2);
			for(int Step = 0; Step <= 20; ++Step)
			{
				const auto Layout = CLucentBrowserSceneLayout::Measure(Screen, Step / 20.f, Players);
				CHECK(Layout.m_Artwork.x >= Screen.x + 18 + CLucentBrowserSceneLayout::ListWidth(Screen) + 12);
				CHECK(Layout.m_Artwork.y + Layout.m_Artwork.h + 12 <= Layout.m_TitleY + .01f);
				CHECK(Layout.m_Artwork.x + Layout.m_Artwork.w <= Width - 24 + .01f);
			}
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
	CHECK(Types.m_GridHeight == 17 && Types.m_ContentHeight > Types.m_GridHeight);
	const auto Narrow = CLucentBrowserFilterLayout::Measure(600, 8, 4, true, true, true, true);
	CHECK(!Narrow.m_Wide && Narrow.m_GridHeight == 44);
	CHECK(Narrow.m_ContentHeight > Narrow.m_GridHeight);
	CHECK(CLucentBrowserFilterLayout::Measure(600, 8, 4, true, true, true, false).m_ContentHeight == Narrow.m_ContentHeight);
	CHECK(CLucentBrowserFilterLayout::Measure(600, 8, 4, true, false, false, false).m_GridHeight == 0);
	// Shared geometry fits at desktop aspect ratios and at larger UI scales.
	CHECK(CLucentBrowserFilterLayout::QuickColumns(820) == 2);
	CHECK(CLucentBrowserFilterLayout::QuickColumns(420) == 2);
	CHECK(CLucentBrowserFilterLayout::QuickColumns(300) == 1);
	CHECK(CLucentBrowserFilterLayout::QuickColumns(420, 220) == 1);
	for(float Width : {280.f, 389.f, 390.f, 420.f, 619.f, 620.f, 720.f, 820.f, 1920.f * 600 / 1080, 2560.f * 600 / 1440, 1280.f * 600 / 720})
		for(float Minimum : {150.f, 185.f, 240.f})
			for(bool CountriesTab : {false, true})
				for(bool HasGrid : {false, true})
				{
					const float Workspace = CLucentBrowserFilterLayout::WorkspaceWidth(Width);
					const auto Layout = CLucentBrowserFilterLayout::Measure(Width, 101, 15, CountriesTab, HasGrid, true, true, Minimum);
					const float GroupWidth = Layout.m_Wide ? (Workspace - CLucentBrowserFilterLayout::ColumnGap(Workspace)) / 2 : Workspace;
					const float Left = CLucentBrowserFilterLayout::GroupHeight(GroupWidth, 4, Minimum);
					const float Right = CLucentBrowserFilterLayout::GroupHeight(GroupWidth, 5, Minimum);
					const float Groups = Layout.m_Wide ? std::max(Left, Right) : Left + 8 + Right;
					// Walk the same sections as rendering, through the bottom of the grid.
					const float Bottom = Groups + 26 + CLucentBrowserFilterLayout::InputHeight(Workspace) + 5 + 24.5f + CLucentBrowserFilterLayout::SectionDividerGap + CLucentBrowserFilterLayout::SectionBottomGap + 21 + (HasGrid ? 25 + CLucentBrowserFilterLayout::TabGridGap + Layout.m_GridHeight : 0);
					CHECK(std::abs(Layout.m_ContentHeight - Bottom - 13) < .001f);
					CUIRect Row = {18, 40, Workspace, CLucentBrowserFilterLayout::InputHeight(Workspace)};
					for(int i = 0; i < 3; ++i)
					{
						const auto Chip = CLucentBrowserFilterLayout::ChipBounds(Row, i);
						CHECK(Chip.x >= Row.x && Chip.x + Chip.w <= Row.x + Row.w + .001f);
						CHECK(Chip.y + Chip.h <= Row.y + Row.h);
						if(i > 0)
						{
							const auto Previous = CLucentBrowserFilterLayout::ChipBounds(Row, i - 1);
							CHECK(Chip.x >= Previous.x + Previous.w || Chip.y >= Previous.y + Previous.h);
						}
					}
				}
	for(float Workspace : {240.f, 420.f, 620.f, 820.f})
		for(bool CountriesTab : {false, true})
		{
			const float Width = CLucentBrowserFilterLayout::GridWidth(Workspace, 101, CountriesTab);
			CHECK(Width <= Workspace && Width <= 300);
			CHECK(Width / 5 >= 44); // Hit cells stay larger than the 32x16 flags.
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
	CHECK(!CLucentBrowserFilterLayout::Measure(619, 0, 0, true, false, false, false).m_Wide);
	CHECK(CLucentBrowserFilterLayout::Measure(620, 0, 0, true, false, false, false).m_Wide);

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
