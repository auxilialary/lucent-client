// Standalone: compile with lucent_menu_interaction.cpp and the src include directory.
#include <game/client/lucent/menu/interaction.h>
#include <game/client/lucent/menu/server_controls.h>

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
		CHECK(!Controls.IsOpen() && Controls.Interactive());
		Controls.Open(); // Entering never needs a running process.
		Controls.Update(0, true, true, true);
		CHECK(Controls.IsOpen() && !Controls.Interactive());
		for(int Frame = 0; Frame < Fps; ++Frame)
			Controls.Update(1.0f / Fps, true, true, true);
		CHECK(Controls.Progress() == 1 && Controls.Interactive());
		Controls.Close(); // Back / Escape does not own the process.
		CHECK(!Controls.Interactive());
		Controls.Update(0.05f, true, true, true);
		const float Before = Controls.Progress();
		CHECK(Before > 0 && Before < 1);
		Controls.Open();
		Controls.Update(0, true, true, true);
		CHECK(Controls.Progress() == Before);
		Controls.Update(1, true, true, true);
		Controls.Update(0, true, false, true); // Entire parent has disappeared.
		CHECK(!Controls.IsOpen() && Controls.Progress() == 0);
		Controls.Open();
		Controls.Update(0, false, true, true);
		CHECK(Controls.Progress() == 1 && Controls.Interactive());
		Controls.Close();
		Controls.Update(0, false, true, true);
		CHECK(Controls.Progress() == 0 && Controls.Interactive());
		Controls.Open();
		Controls.Update(0, true, true, false);
		CHECK(!Controls.IsOpen() && Controls.Progress() == 0);
	}
	for(int Step = 1; Step < 28; ++Step)
	{
		CLucentServerControls Controls;
		Controls.Open();
		Controls.Update(Step * 0.01f, true, true, true);
		const float Before = Controls.Progress();
		Controls.Close();
		Controls.Update(0, true, true, true);
		CHECK(Controls.Progress() == Before && !Controls.Interactive());
		Controls.Update(0.005f, true, true, true);
		CHECK(Controls.Progress() < Before);
	}
	CHECK(CLucentServerControls::CanStart(false, false));
	CHECK(!CLucentServerControls::CanStart(false, true));
	CHECK(!CLucentServerControls::CanStart(true, false));
	CHECK(!CLucentServerControls::CanStart(true, true));
	using EAction = CLucentServerControls::EAction;
	CHECK(CLucentServerControls::Action(0, false, false) == EAction::START);
	CHECK(CLucentServerControls::Action(1, false, false) == EAction::NONE);
	CHECK(CLucentServerControls::Action(0, true, false) == EAction::CONNECT);
	CHECK(CLucentServerControls::Action(1, true, false) == EAction::STOP);
	CHECK(CLucentServerControls::Action(0, false, true) == EAction::NONE);
	for(bool Running : {false, true})
		for(bool Starting : {false, true})
		{
			CHECK(CLucentServerControls::Action(2, Running, Starting) == EAction::BACK);
			CHECK(CLucentServerControls::CanDispatch(EAction::BACK, Running, Starting));
			CHECK(CLucentServerControls::CanDispatch(EAction::START, Running, Starting) == (!Running && !Starting));
			CHECK(CLucentServerControls::CanDispatch(EAction::CONNECT, Running, Starting) == Running);
			CHECK(CLucentServerControls::CanDispatch(EAction::STOP, Running, Starting) == Running);
			CHECK(!CLucentServerControls::CanDispatch(EAction::NONE, Running, Starting));
		}
	// Failure/exit cannot redirect Connect or Stop into Start; retry remains available.
	CHECK(!CLucentServerControls::CanDispatch(CLucentServerControls::Action(0, true, false), false, false));
	CHECK(CLucentServerControls::CanDispatch(CLucentServerControls::Action(0, false, false), false, false));
	CLucentServerControls Lifecycle;
	Lifecycle.Open();
	Lifecycle.Update(0, false, true, true);
	// Failed start, successful retry, process exit and re-entry read fresh state.
	CHECK(Lifecycle.IsOpen() && CLucentServerControls::Action(0, false, false) == EAction::START);
	CHECK(Lifecycle.IsOpen() && CLucentServerControls::Action(0, true, false) == EAction::CONNECT);
	CHECK(Lifecycle.IsOpen() && CLucentServerControls::Action(0, false, false) == EAction::START);
	Lifecycle.Update(0.05f, true, true, true); // Parent still visible while retracting.
	CHECK(Lifecycle.IsOpen());
	Lifecycle.Update(0, false, false, true);
	CHECK(!Lifecycle.IsOpen());
	Lifecycle.Open();
	Lifecycle.Update(0, false, true, true);
	CHECK(Lifecycle.IsOpen() && CLucentServerControls::Action(1, true, false) == EAction::STOP);
	std::puts("Branch reversal, parent reset, instant mode and duplicate-start guards passed");
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
