// Standalone: compile with lucent_menu_interaction.cpp and the src include directory.
#include "../lucent_menu_interaction.h"
#include "../server_controls.h"

#include <cstdio>
#include <cstdlib>

#define CHECK(Expression) \
	do \
	{ \
		if(!(Expression)) \
		{ \
			std::fprintf(stderr, "line %d: %s\n", __LINE__, #Expression); \
			std::abort(); \
		} \
	} while(false)

int main()
{
	for(int Fps : {30, 60, 144, 240})
	{
		CLucentServerControls Controls;
		Controls.Update(0, true, false, true, true);
		CHECK(!Controls.IsOpen() && !Controls.Interactive());
		Controls.Open();
		for(int Frame = 0; Frame < Fps; ++Frame)
			Controls.Update(1.0f / Fps, true, true, true, true);
		CHECK(Controls.Interactive() && Controls.Progress() == 1);
		Controls.Close(); // Escape, outside click, Join, or More.
		Controls.Update(0.05f, true, true, true, true);
		CHECK(!Controls.Interactive() && Controls.Progress() < 1 && Controls.Progress() > 0);
		const float Closing = Controls.Progress();
		Controls.Toggle();
		Controls.Update(0, true, true, true, true);
		CHECK(Controls.Progress() == Closing);
		Controls.Update(1, true, true, true, true);
		CHECK(Controls.Progress() == 1);
		Controls.Update(0.01f, true, true, false, true); // Parent collapse clears intent.
		CHECK(!Controls.IsOpen() && !Controls.Interactive());
		Controls.Update(1, true, true, true, true);
		CHECK(!Controls.IsOpen() && Controls.Progress() == 0);
		Controls.Open();
		Controls.Update(0, false, true, true, true);
		CHECK(Controls.Interactive() && Controls.Progress() == 1);
		Controls.Update(0, true, true, true, false); // Focus / console.
		CHECK(!Controls.IsOpen() && Controls.Progress() == 0);
		Controls.Update(0, false, true, true, true);
		CHECK(!Controls.IsOpen());
		Controls.Open();
		Controls.Update(0, false, true, true, true);
		Controls.Update(0, true, false, true, true); // Stop or process exit.
		CHECK(!Controls.IsOpen() && Controls.Progress() == 0);
	}
	// Dispatch uses both process and asynchronous startup state, including
	// repeated activations while Android is still starting its service.
	CHECK(CLucentServerControls::CanStart(false, false));
	CHECK(!CLucentServerControls::CanStart(false, true));
	CHECK(!CLucentServerControls::CanStart(true, false));
	CHECK(!CLucentServerControls::CanStart(true, true));
	for(float Scale : {0.6f, 1.0f, 1.8f})
	{
		const CUIRect Chip = {670 * Scale, 180 * Scale, 164 * Scale, 34 * Scale};
		const CUIRect Group = {660 * Scale, 180 * Scale, 180 * Scale, 146 * Scale};
		const CUIRect Panel = CLucentServerControls::Bounds(Chip, Group);
		for(float Progress : {0.0f, 0.2f, 0.5f, 1.0f})
		{
			const CUIRect Join = CLucentServerControls::Row(Panel, 0, Progress);
			const CUIRect Stop = CLucentServerControls::Row(Panel, 1, Progress);
			CHECK(Join.w == Chip.w && Stop.w == Chip.w);
			CHECK(Join.y > Group.y + Group.h);
			CHECK(Stop.y > Join.y + Join.h);
			CHECK(Join.x > 600 * Scale); // Clear of the logo and central More menu.
			CHECK(CLucentMenuInteraction::Contains(CLucentServerControls::HitArea(Panel), vec2(Join.x, Join.y)));
			CHECK(CLucentMenuInteraction::Contains(CLucentServerControls::HitArea(Panel), vec2(Stop.x + Stop.w, Stop.y + Stop.h)));
		}
	}
	std::puts("Server controls lifecycle, reversal, geometry and duplicate-start guards passed");
	CLucentSecondaryNavigation Secondary;
	Secondary.Update(0, true, true, true, true);
	Secondary.Toggle();
	Secondary.Update(0.08f, true, true, true, true);
	CHECK(Secondary.Progress(0) > Secondary.Progress(1));
	CHECK(Secondary.Progress(2) == 0);
	const float Before = Secondary.Progress(0);
	Secondary.Toggle();
	Secondary.Update(0, true, true, true, true);
	CHECK(Secondary.Progress(0) == Before);
	Secondary.Update(0.02f, true, true, true, true);
	CHECK(Secondary.Progress(0) < Before);
	Secondary.Toggle();
	Secondary.Update(0.32f, true, true, true, true);
	CHECK(Secondary.Progress(4) == 1);
	Secondary.Update(0.04f, true, false, true, true);
	CHECK(!Secondary.Interactive(0));
	CHECK(Secondary.Progress(4) < 1);
	Secondary.Update(0.04f, true, true, true, true);
	CHECK(Secondary.IsOpen());
	CHECK(Secondary.Progress(4) == 1);
	Secondary.Update(0, true, false, false, true);
	Secondary.Update(0, true, true, true, true);
	CHECK(!Secondary.IsOpen());
	Secondary.Toggle();
	Secondary.Update(0, false, true, true, true);
	CHECK(Secondary.Progress(4) == 1);
	Secondary.Update(0, true, true, true, false);
	CHECK(!Secondary.IsOpen());
	CHECK(Secondary.Progress(0) == 0);
	Secondary.Update(0, false, true, true, true);
	CHECK(!Secondary.IsOpen());
	Secondary.Toggle();
	Secondary.Update(0, false, true, true, true);
	Secondary.Close();
	Secondary.Update(0, false, true, true, true);
	CHECK(Secondary.Progress(0) == 0);
	// Frame-rate-independent ordering, full reverse, and zero-time reversals.
	for(int Fps : {60, 144, 240})
	{
		Secondary.Reset();
		Secondary.Update(0, true, true, true, true);
		Secondary.Toggle();
		for(int Frame = 0; Frame < Fps; ++Frame)
		{
			Secondary.Update(1.0f / Fps, true, true, true, true);
			for(int Row = 1; Row < 5; ++Row)
				CHECK(Secondary.Progress(Row - 1) >= Secondary.Progress(Row));
		}
		for(int Row = 0; Row < 5; ++Row)
			CHECK(Secondary.Progress(Row) == 1);
		Secondary.Close();
		for(int Frame = 0; Frame < Fps; ++Frame)
			Secondary.Update(1.0f / Fps, true, true, true, true);
		for(int Row = 0; Row < 5; ++Row)
		{
			CHECK(Secondary.Progress(Row) == 0);
			CHECK(!Secondary.Interactive(Row));
		}
	}
	std::puts("Secondary opening, closing, reversal, parent synchronization, focus loss and disabled animations passed");
	using I = CLucentMenuInteraction;
	CHECK(I::InTriangle(vec2(1, 1), vec2(0, 0), vec2(4, 0), vec2(0, 4)));
	CHECK(I::InTriangle(vec2(2, 2), vec2(0, 0), vec2(4, 0), vec2(0, 4)));
	CHECK(!I::InTriangle(vec2(3, 3), vec2(0, 0), vec2(4, 0), vec2(0, 4)));
	CHECK(!I::InTriangle(vec2(0, 0), vec2(0, 0), vec2(0, 0), vec2(0, 0)));
	const CUIRect Logo = {400, 250, 200, 100};
	const CUIRect Left = {180, 200, 150, 200};
	const CUIRect Right = {670, 200, 150, 200};
	for(int Side : {-1, 1})
	{
		const auto Point = [Side](float X, float Y) { return vec2(500 + Side * X, Y); };
		I Menu;
		CHECK(!Menu.Update(0, vec2(0, 0), Logo, Left, Right, false, false));
		CHECK(Menu.Update(0.01, Point(95, 300), Logo, Left, Right, false, false));
		for(int Step = 0; Step <= 60; ++Step)
			CHECK(Menu.Update(0.02 + Step * 0.02, Point(101 + Step, 300 + Step * 0.3f), Logo, Left, Right, false, false));
		CHECK(Menu.Update(1.25, Point(180, 320), Logo, Left, Right, true, false));
		CHECK(Menu.Update(1.3, Point(200, 380), Logo, Left, Right, true, false));
		CHECK(Menu.Update(1.4, vec2(500, 500), Logo, Left, Right, false, false));
		CHECK(!Menu.Update(1.66, vec2(500, 500), Logo, Left, Right, false, false));
		Menu.Reset();
		CHECK(Menu.Update(0, Point(95, 300), Logo, Left, Right, false, false));
		CHECK(Menu.Update(0.005, Point(130, 280), Logo, Left, Right, false, false));
		CHECK(Menu.Update(0.01, Point(180, 250), Logo, Left, Right, true, false));
		Menu.Close();
		CHECK(!Menu.Update(0.02, Point(180, 250), Logo, Left, Right, true, false));
		// Slow diagonal return from every row of either button group.
		for(float Y : {210.0f, 300.0f, 390.0f})
		{
			Menu.Reset();
			CHECK(Menu.Update(0, Point(95, 300), Logo, Left, Right, false, false));
			CHECK(Menu.Update(0.01, Point(180, Y), Logo, Left, Right, true, false));
			for(int Step = 0; Step <= 60; ++Step)
				CHECK(Menu.Update(0.02 + Step * 0.02, Point(169 - Step * 1.1f, Y + (300 - Y) * (11 + Step * 1.1f) / 80), Logo, Left, Right, false, false));
			CHECK(Menu.Update(1.25, Point(95, 300), Logo, Left, Right, false, false));
		}
		Menu.Reset();
		CHECK(Menu.Update(0, Point(99, 300), Logo, Left, Right, false, false));
		CHECK(Menu.Update(0.01, Point(110, 300), Logo, Left, Right, false, false));
		CHECK(Menu.Update(0.02, Point(105, 300), Logo, Left, Right, false, false));
		CHECK(Menu.Update(0.33, Point(105, 300), Logo, Left, Right, false, false));
		CHECK(!Menu.Update(0.59, Point(105, 300), Logo, Left, Right, false, false));
		Menu.Reset();
		Menu.Update(0, Point(99, 300), Logo, Left, Right, false, false);
		Menu.Update(0.01, Point(110, 300), Logo, Left, Right, false, false);
		CHECK(Menu.Update(0.32, Point(110, 300), Logo, Left, Right, false, false));
		CHECK(!Menu.Update(0.58, Point(110, 300), Logo, Left, Right, false, false));
		Menu.Reset();
		Menu.Update(0, Point(99, 300), Logo, Left, Right, false, false);
		for(int Step = 1; Step < 200; ++Step)
			Menu.Update(Step * 0.02, Point(101 + Step * 0.1f, 300), Logo, Left, Right, false, false);
		CHECK(!Menu.IsOpen());
	}
	for(float Scale : {0.6f, 1.0f, 1.8f})
	{
		const auto Rect = [Scale](CUIRect R) { return CUIRect{R.x * Scale, R.y * Scale, R.w * Scale, R.h * Scale}; };
		I Scaled;
		CHECK(Scaled.Update(0, vec2(500, 300) * Scale, Rect(Logo), Rect(Left), Rect(Right), false, false));
		CHECK(Scaled.Update(0.01, vec2(700, 220) * Scale, Rect(Logo), Rect(Left), Rect(Right), true, false));
		CHECK(Scaled.Update(0.02, vec2(640, 245) * Scale, Rect(Logo), Rect(Left), Rect(Right), false, false));
		CHECK(Scaled.Update(0.22, vec2(595, 270) * Scale, Rect(Logo), Rect(Left), Rect(Right), false, false));
		// A miss followed by re-entry cancels pending closure.
		CHECK(Scaled.Update(0.23, vec2(500, 500) * Scale, Rect(Logo), Rect(Left), Rect(Right), false, false));
		CHECK(Scaled.Update(0.45, vec2(500, 300) * Scale, Rect(Logo), Rect(Left), Rect(Right), false, false));
		CHECK(Scaled.Update(1, vec2(500, 300) * Scale, Rect(Logo), Rect(Left), Rect(Right), false, false));
	}
	I Resume;
	CHECK(Resume.Update(0, vec2(500, 300), Logo, Left, Right, false, false));
	CHECK(Resume.Update(0.01, vec2(500, 500), Logo, Left, Right, false, false));
	CHECK(!Resume.Update(0.27, vec2(500, 500), Logo, Left, Right, false, false));
	CHECK(Resume.Update(0.28, vec2(700, 300), Logo, Left, Right, true, false, true));
	Resume.Close();
	CHECK(!Resume.Update(0.29, vec2(700, 300), Logo, Left, Right, true, false, true));
	I Resized;
	CHECK(Resized.Update(0, vec2(500, 300), Logo, Left, Right, false, false));
	CHECK(Resized.Update(0.01, vec2(640, 300), Logo, Left, Right, false, false));
	const CUIRect SmallLogo = {200, 125, 100, 50}, SmallLeft = {90, 100, 75, 100}, SmallRight = {335, 100, 75, 100};
	CHECK(Resized.Update(0.1, vec2(320, 150), SmallLogo, SmallLeft, SmallRight, false, false));
	CHECK(Resized.Update(0.3, vec2(295, 150), SmallLogo, SmallLeft, SmallRight, false, false));
	CHECK(Resized.Update(0.6, vec2(295, 150), SmallLogo, SmallLeft, SmallRight, false, false));
	I Menu;
	CHECK(Menu.Update(0, vec2(0, 0), Logo, Left, Right, false, true));
	Menu.Close();
	CHECK(!Menu.Update(0.01, vec2(500, 300), Logo, Left, Right, false, false));
	Menu.Update(0.02, vec2(0, 0), Logo, Left, Right, false, false);
	CHECK(Menu.Update(0.03, vec2(500, 300), Logo, Left, Right, false, false));
	std::puts("Lucent geometry and state tests passed (both sides, slow/fast, reversal, dwell, deadline, keyboard, Escape)");
}
