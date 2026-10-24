// Standalone: compile with lucent_menu_interaction.cpp and the src include directory.
#include "../lucent_menu_interaction.h"

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
