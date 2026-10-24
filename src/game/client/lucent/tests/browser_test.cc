#include <game/client/lucent/browser_state.h>
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
	CHECK(CLucentBrowserState::Descriptor("DDNet RUS - Brutal") == "Brutal");
	CHECK(CLucentBrowserState::Descriptor("DDNet GER - Moderate") == "Moderate");
	CHECK(CLucentBrowserState::Descriptor("DDNet RUS - Custom") == "DDNet RUS - Custom");
	CHECK(CLucentBrowserState::Descriptor("DDNet RUS - Brutal #2") == "DDNet RUS - Brutal #2");
	CHECK(CLucentBrowserState::Descriptor("Other - Insane") == "Other - Insane");
	CHECK(CLucentBrowserState::Descriptor("DDNet invalid - Solo") == "DDNet invalid - Solo");
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
	State.Toggle(P::FILTERS);
	State.Update(.016f, true);
	CHECK(State.m_Open > 0 && State.m_Open < 1);
	CHECK(State.PanelWidth(600) < 258);
	for(int i = 0; i < 100; ++i)
		State.Update(.016f, true);
	CHECK(State.m_Open == 1);
	for(float Width : {320.f, 600.f, 1066.f, 2400.f})
		CHECK(State.PanelWidth(Width) <= Width * .43f);
	State.Escape();
	State.Update(.016f, true);
	const float Closing = State.m_Open;
	CHECK(State.VisiblePanel() == P::FILTERS);
	State.Toggle(P::FILTERS);
	State.Update(.016f, true);
	CHECK(State.m_Open > Closing);
	State.Update(.016f, false);
	State.Toggle(P::PLAYERS);
	CHECK(State.m_Open == 1);
	CHECK(State.Escape());
	State.Update(.016f, false);
	CHECK(State.m_Open == 0);
	CHECK(State.VisiblePanel() == P::NONE);
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
			CHECK(Size.second * Scale * Frame <= 600 * .68f + .01f);
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
