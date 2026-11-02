#include <engine/image.h>

#include <game/client/lucent/settings/skin_gallery.h>
#include <game/client/lucent/settings/skins.h>
#include <game/client/lucent/skins/texture_quality.h>
#include <game/client/lucent/ui/color_picker.h>
#include <game/client/lucent/ui/selection_corners.h>
#include <game/client/lucent/ui/slider_geometry.h>
#include <game/client/ui.h>

#include <gtest/gtest.h>

#include <cstring>

TEST(LucentSkinManager, LimitsKeepAtlasGridAndNeverUpscale)
{
	for(int Limit : LucentSkins::TextureLimits)
		for(const auto Size : {LucentSkins::CTextureSize{256, 128}, {16384, 8192}, {1024, 16384}, {520, 260}})
		{
			const auto Reduced = LucentSkins::LimitedSize(Size.m_Width, Size.m_Height, Limit, 8, 4);
			EXPECT_GT(Reduced.m_Width, 0u);
			EXPECT_GT(Reduced.m_Height, 0u);
			EXPECT_LE(Reduced.m_Width, Size.m_Width);
			EXPECT_LE(Reduced.m_Height, Size.m_Height);
			EXPECT_LE(Reduced.m_Width, size_t(Limit));
			EXPECT_LE(Reduced.m_Height, size_t(Limit));
			EXPECT_EQ(Reduced.m_Width % 8, 0u);
			EXPECT_EQ(Reduced.m_Height % 4, 0u);
		}
	EXPECT_EQ(LucentSkins::LimitedSize(256, 128, 8192, 8, 4).m_Width, 256u);
	EXPECT_EQ(LucentSkins::LimitedSize(0, 128, 1024, 8, 4).m_Width, 0u);
	EXPECT_EQ(LucentSkins::LimitedSize(128, 0, 1024, 8, 4).m_Height, 0u);
	EXPECT_EQ(LucentSkins::LimitedSize(1, 16384, 128, 8, 4).m_Width, 0u);
}
TEST(LucentSkinManager, ReductionPreservesVisibleColorWithoutTransparentHalos)
{
	CImageInfo Image;
	Image.m_Width = 256;
	Image.m_Height = 128;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.Allocate();
	for(size_t Y = 0; Y < Image.m_Height; ++Y)
		for(size_t X = 0; X < Image.m_Width; ++X)
		{
			auto *P = Image.m_pData + (Y * Image.m_Width + X) * 4;
			const bool Visible = (X % 2) == 0;
			P[0] = Visible ? 255 : 0;
			P[1] = 0;
			P[2] = Visible ? 0 : 255;
			P[3] = Visible ? 255 : 0;
		}
	ASSERT_TRUE(LucentSkins::LimitTexture(Image, 128, 8, 4));
	ASSERT_EQ(Image.m_Width, 128u);
	ASSERT_EQ(Image.m_Height, 64u);
	for(size_t P = 0; P < Image.DataSize(); P += 4)
	{
		EXPECT_EQ(Image.m_pData[P], 255);
		EXPECT_EQ(Image.m_pData[P + 1], 0);
		EXPECT_EQ(Image.m_pData[P + 2], 0);
		EXPECT_EQ(Image.m_pData[P + 3], 127);
	}
	Image.Free();
	EXPECT_EQ(Image.m_pData, nullptr);
}
TEST(LucentSkinManager, UnchangedAndRejectedImagesRetainOwnership)
{
	CImageInfo Image;
	Image.m_Width = 128;
	Image.m_Height = 64;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.AllocateFillZero();
	auto *Pixels = Image.m_pData;
	EXPECT_TRUE(LucentSkins::LimitTexture(Image, 8192, 8, 4));
	EXPECT_EQ(Pixels, Image.m_pData);
	EXPECT_FALSE(LucentSkins::LimitTexture(Image, 128, 0, 4));
	EXPECT_EQ(Pixels, Image.m_pData);
	Image.Free();
	EXPECT_FALSE(LucentSkins::LimitTexture(Image, 128, 8, 4));
	CImageInfo Rgb;
	Rgb.m_Width = 256;
	Rgb.m_Height = 128;
	Rgb.m_Format = CImageInfo::FORMAT_RGB;
	Rgb.AllocateFillZero();
	Pixels = Rgb.m_pData;
	EXPECT_FALSE(LucentSkins::LimitTexture(Rgb, 128, 8, 4));
	EXPECT_EQ(Rgb.m_pData, Pixels);
}
TEST(LucentSkinManager, ResponsiveGalleryAndProfileRegionsStayWithinParent)
{
	for(float Scale : {.75f, 1.f, 1.5f, 2.f})
		for(float Height : {0.f, 1.f, 80.f, 600.f})
			for(float Width : {1.f, 320.f, 700.f, 1000.f, 1600.f})
			{
				const CUIRect View{11, 19, Width / Scale, Height / Scale};
				const auto L = LucentSettings::CSkinManagerLayout::Calculate(View);
				EXPECT_GE(L.m_Profile.w, 0);
				EXPECT_GE(L.m_Profile.h, 0);
				EXPECT_LE(L.m_Profile.y + L.m_Profile.h, View.y + View.h + .001f);
				EXPECT_LE(L.m_Gallery.y + L.m_Gallery.h, View.y + View.h + .001f);
				EXPECT_GE(L.m_Gallery.w, 0);
				EXPECT_GE(L.m_Gallery.h, 0);
				EXPECT_GE(L.m_Profile.x, View.x);
				EXPECT_GE(L.m_Gallery.x, View.x);
				EXPECT_LE(L.m_Profile.x + L.m_Profile.w, View.x + View.w + .001f);
				EXPECT_LE(L.m_Gallery.x + L.m_Gallery.w, View.x + View.w + .001f);
				if(L.m_TwoColumns)
					EXPECT_GE(L.m_Gallery.x, L.m_Profile.x + L.m_Profile.w);
				else
					EXPECT_GE(L.m_Gallery.y, L.m_Profile.y + L.m_Profile.h);
				EXPECT_GE(LucentSettings::CSkinGalleryGeometry::Columns(L.m_Gallery.w), 1);
			}
}

TEST(LucentSkinManager, FullyTransparentTextureRemainsTransparent)
{
	CImageInfo Image;
	Image.m_Width = 256;
	Image.m_Height = 128;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.AllocateFillZero();
	ASSERT_TRUE(LucentSkins::LimitTexture(Image, 128, 8, 4));
	for(size_t i = 0; i < Image.DataSize(); ++i)
		EXPECT_EQ(Image.m_pData[i], 0);
}

TEST(LucentSkinManager, HueRingIsHollowAndMouseAnglesRoundTrip)
{
	for(float Pixel : {.25f, .5f, 1.f, 2.f, 4.f})
		for(float Size : {2.f, 40.f, 120.f, 200.f})
		{
			const CUIRect Rect{13.25f, 22.5f, Size, Size * .9f};
			const auto G = LucentUi::CHueRingGeometry::Calculate(Rect, Pixel);
			EXPECT_FALSE(G.Contains(G.m_Center));
			EXPECT_FLOAT_EQ(G.HueAt(G.m_Center, .37f), .37f);
			EXPECT_GE(G.m_Inner, 0);
			EXPECT_LE(G.m_Inner, G.m_Outer);
			if(G.m_Outer <= 0)
				continue;
			for(int H = 0; H < 255; ++H)
			{
				const float Hue = H / 255.f;
				const auto P = G.Indicator(Hue);
				EXPECT_TRUE(G.Contains(P));
				EXPECT_GE(P.x, Rect.x);
				EXPECT_LE(P.x, Rect.x + Rect.w);
				EXPECT_GE(P.y, Rect.y);
				EXPECT_LE(P.y, Rect.y + Rect.h);
				const float Found = G.HueAt(P, Hue);
				EXPECT_NEAR(std::min(std::abs(Found - Hue), 1 - std::abs(Found - Hue)), 0, .00001f);
				ColorHSLA Color(Hue, .6f, .4f);
				Color.h = Found;
				EXPECT_EQ((Color.Pack(false) >> 16) & 255u, unsigned(H));
			}
		}
}
TEST(LucentSkinManager, NativeHslPackingPreservesEveryChannelAndDarkestLight)
{
	for(bool Alpha : {false, true})
		for(float Darkest : {ColorHSLA::DARKEST_LGT, ColorHSLA::DARKEST_LGT7})
			for(unsigned Byte = 0; Byte < 256; ++Byte)
			{
				const unsigned Packed = (Alpha ? (Byte << 24) : 0) | (Byte << 16) | ((255 - Byte) << 8) | ((Byte * 71) % 256);
				const ColorHSLA Color(Packed, Alpha);
				EXPECT_EQ(Color.Pack(Alpha), Packed);
				EXPECT_EQ(Color.UnclampLighting(Darkest).Pack(Darkest, Alpha), Packed);
				EXPECT_GE(Color.UnclampLighting(Darkest).l, Darkest);
				EXPECT_LE(Color.UnclampLighting(Darkest).l, 1);
			}
}
TEST(LucentSkinManager, GalleryFooterAndClippedHitsRemainDisjointWithoutQuantizingScroll)
{
	for(float Pixel : {.5f, 1.f, 1.5f, 2.f})
		for(float Height : {0.f, 40.f, 180.f, 331.5f, 540.f})
		{
			CUIRect View{7.3f, 13.7f, 620, Height};
			const CUIRect Parent = View;
			const CUIRect Footer = LucentSettings::CSkinGalleryGeometry::SplitFooter(View);
			const CUIRect Clip = LucentSettings::CSkinGalleryGeometry::Clip(View, Pixel);
			EXPECT_LE(Clip.y + Clip.h, Footer.y + .001f);
			EXPECT_GE(Clip.y, View.y);
			EXPECT_LE(Footer.y + Footer.h, Parent.y + Parent.h + .001f);
			for(int Count : {1, 3, 4, 47, 100})
			{
				const int Columns = LucentSettings::CSkinGalleryGeometry::Columns(View.w);
				const int Rows = (Count + Columns - 1) / Columns;
				const float Content = Rows * LucentSettings::CSkinGalleryGeometry::RowHeight;
				const float MaxScroll = std::max(0.f, Content - View.h);
				const CUIRect Last{View.x, View.y + Content - MaxScroll - 80, View.w, 80};
				if(View.h >= 80)
					EXPECT_NEAR(LucentSettings::CSkinGalleryGeometry::Intersect(Last, View).h, 80, .001f);
				for(float Scroll : {0.f, 13.375f, 83.125f, MaxScroll})
				{
					const CUIRect Card{View.x, View.y - Scroll, 150, 80};
					const auto Hit = LucentSettings::CSkinGalleryGeometry::Intersect(Card, Clip);
					EXPECT_LE(Hit.h, 80);
					if(Hit.h > 0)
						EXPECT_LE(Hit.y + Hit.h, Footer.y + .001f);
					EXPECT_FLOAT_EQ(Card.y, View.y - Scroll);
				}
			}
		}
}
TEST(LucentSkinManager, AdvancedFieldsUseBoundedParentRegions)
{
	for(float Width : {320.f, 700.f, 1000.f, 2400.f})
		for(float Height : {180.f, 300.f, 600.f})
		{
			const CUIRect View{9, 17, Width, Height};
			const auto L = LucentSettings::CSkinManagerLayout::Advanced(View);
			EXPECT_GE(L.m_Profile.x, View.x);
			EXPECT_LE(L.m_Profile.x + L.m_Profile.w, View.x + View.w);
			if(L.m_TwoColumns)
			{
				EXPECT_LE(L.m_Gallery.w, L.m_Profile.w * 1.5f + .001f);
				EXPECT_LE(L.m_Gallery.x + L.m_Gallery.w, View.x + View.w);
			}
		}
}

TEST(LucentSkinManager, PrefixChoicesKeepUnknownSavedValuesDistinct)
{
	EXPECT_EQ(LucentSettings::SkinPrefixIndex(""), 0);
	EXPECT_EQ(LucentSettings::SkinPrefixIndex("kitty"), 1);
	EXPECT_EQ(LucentSettings::SkinPrefixIndex("santa"), 2);
	for(const char *pUnknown : {"custom", "kitty_", "SANTA", "my_saved_prefix"})
		EXPECT_EQ(LucentSettings::SkinPrefixIndex(pUnknown), -1);
}

TEST(LucentSkinManager, PickerKeyboardCyclesFocusAndStepsNativeBytes)
{
	for(bool Alpha : {false, true})
	{
		LucentUi::CColorPickerState State;
		const int Count = Alpha ? 4 : 3;
		for(int Index = 1; Index <= Count; ++Index)
		{
			State.MoveFocus(Alpha, false);
			EXPECT_EQ(State.m_Focus, Index % Count);
		}
		State.MoveFocus(Alpha, true);
		EXPECT_EQ(State.m_Focus, Count - 1);
		for(int Byte = 0; Byte <= 255; ++Byte)
		{
			EXPECT_EQ(round_to_int(State.Step(Byte / 255.f, 1) * 255), std::min(255, Byte + 1));
			EXPECT_EQ(round_to_int(State.Step(Byte / 255.f, -1) * 255), std::max(0, Byte - 1));
		}
	}
}

TEST(LucentSkinManager, SelectionCornersStayInsideControlsAndRemainDisconnected)
{
	for(float Pixel : {.25f, .5f, 1.f, 2.f, 4.f})
		for(const auto Size : {vec2(1, 1), vec2(16, 8), vec2(34, 34), vec2(75, 34), vec2(160, 80), vec2(600, 120)})
			for(float Progress : {0.f, .25f, .7f, 1.f})
			{
				const CUIRect Rect{17, 29, Size.x, Size.y};
				const auto G = LucentUi::CSelectionCornersGeometry::Calculate(Rect, Pixel, Progress);
				const float Edge = (G.m_Thickness + G.m_Feather) / 2;
				EXPECT_GT(G.m_Thickness, 0);
				EXPECT_GT(G.m_Feather, 0);
				for(int Corner = 0; Corner < 4; ++Corner)
				{
					const auto &Path = G.m_Corners[Corner];
					ASSERT_GE(Path.m_Count, 7);
					ASSERT_LE(Path.m_Count, 35);
					for(int I = 0; I < Path.m_Count; ++I)
					{
						const auto P = Path.m_Points[I], N = Path.m_Normals[I];
						EXPECT_TRUE(std::isfinite(P.x) && std::isfinite(P.y));
						EXPECT_NEAR(length(N), 1, .00001f);
						EXPECT_GE(P.x - Edge, Rect.x - .00001f);
						EXPECT_LE(P.x + Edge, Rect.x + Rect.w + .00001f);
						EXPECT_GE(P.y - Edge, Rect.y - .00001f);
						EXPECT_LE(P.y + Edge, Rect.y + Rect.h + .00001f);
						if(Corner == 0 || Corner == 3)
							EXPECT_LT(P.x + Edge, Rect.Center().x);
						else
							EXPECT_GT(P.x - Edge, Rect.Center().x);
						if(Corner < 2)
							EXPECT_LT(P.y + Edge, Rect.Center().y);
						else
							EXPECT_GT(P.y - Edge, Rect.Center().y);
					}
				}
			}
	EXPECT_EQ(LucentUi::CSelectionCornersGeometry::Calculate({0, 0, 0, 30}, 1, 1).m_Corners[0].m_Count, 0);
}
TEST(LucentSkinManager, SelectionCornersRevealSegmentsWithoutMovingAnchors)
{
	const CUIRect Rect{5, 13, 160, 80};
	const auto Start = LucentUi::CSelectionCornersGeometry::Calculate(Rect, 1, 0);
	const auto End = LucentUi::CSelectionCornersGeometry::Calculate(Rect, 1, 1);
	EXPECT_LT(Start.m_Corners[0].m_Points[0].x, End.m_Corners[0].m_Points[0].x);
	EXPECT_FLOAT_EQ(Start.m_Corners[0].m_Points[0].y, End.m_Corners[0].m_Points[0].y);
	// Stroke geometry stays clipped by the existing list viewport at arbitrary scroll offsets.
	for(float Scroll : {0.f, 13.375f, 79.5f, 150.f})
	{
		const CUIRect Clip{5, 13, 160, 80}, Card{5, 13 - Scroll, 160, 80};
		const auto G = LucentUi::CSelectionCornersGeometry::Calculate(Card, 1, 1);
		const auto Hit = LucentSettings::CSkinGalleryGeometry::Intersect(Card, Clip);
		EXPECT_LE(Hit.h, 80);
		EXPECT_NEAR(G.m_Corners[0].m_Points[0].y, End.m_Corners[0].m_Points[0].y - Scroll, .0001f);
	}
}
TEST(LucentSkinManager, SelectionCornersMotionReversesAndSupportsIndependentProfiles)
{
	using LucentStyle::Motion::CTransition;
	const float Duration = LucentStyle::Motion::Selection;
	CTransition Motion;
	EXPECT_EQ(Motion.Update(true, 0, true, Duration), 0);
	const float Growing = Motion.Update(true, .08, true, Duration);
	EXPECT_GT(Growing, 0);
	EXPECT_LT(Growing, 1);
	EXPECT_FLOAT_EQ(Motion.Update(false, .08, true, Duration), Growing);
	const float Shrinking = Motion.Update(false, .1, true, Duration);
	EXPECT_LT(Shrinking, Growing);
	EXPECT_FLOAT_EQ(Motion.Update(true, .1, true, Duration), Shrinking);
	for(int I = 0; I < 100; ++I)
	{
		const float Value = Motion.Update(I % 2, .101 + I * .001, true, Duration);
		EXPECT_GE(Value, 0);
		EXPECT_LE(Value, 1);
	}
	EXPECT_EQ(Motion.Update(true, .3, false, Duration), 1);
	EXPECT_EQ(Motion.Update(false, .3, false, Duration), 0);
	for(int Fps : {60, 144, 240})
	{
		CTransition Timed;
		Timed.Update(true, 0, true, Duration);
		for(int Frame = 1; float(Frame) / Fps < .11f; ++Frame)
			Timed.Update(true, double(Frame) / Fps, true, Duration);
		EXPECT_NEAR(Timed.Update(true, .11, true, Duration), LucentStyle::Motion::CubicOut(.11f / Duration), .00001f);
	}
	std::array<CTransition, 2> Profiles;
	Profiles[0].Update(true, 0, false, Duration);
	EXPECT_EQ(Profiles[0].m_Value, 1);
	EXPECT_EQ(Profiles[1].m_Value, 0);
	// Unknown prefix cancels presentation immediately; no false option is selected.
	EXPECT_EQ(Profiles[0].Update(false, .1, false, Duration), 0);
}
TEST(LucentSkinManager, LoadedSkinLimitHasExactLinearNativeRoundTrip)
{
	CLinearScrollbarScale Scale;
	for(int Value = 256; Value <= 8192; ++Value)
	{
		const float Position = Scale.ToRelative(Value, 256, 8192);
		EXPECT_EQ(Scale.ToAbsolute(Position, 256, 8192), Value);
		EXPECT_GE(Position, 0);
		EXPECT_LE(Position, 1);
	}
	EXPECT_FLOAT_EQ(Scale.ToRelative(512, 256, 8192), 256.f / 7936);
	EXPECT_EQ(Scale.ToAbsolute(.5f, 256, 8192), 4224);
	for(float Width : {160.f, 320.f, 480.f})
		for(int Value : {256, 512, 1024, 4224, 8192})
		{
			const float Position = Scale.ToRelative(Value, 256, 8192);
			const CUIRect Rail{17, 23, Width, 24};
			const CUIRect Handle{Rail.x, Rail.y, 33, Rail.h};
			const auto G = LucentUi::CSliderGeometry::Calculate(Rail, Handle, Position, 1, true);
			EXPECT_NEAR(G.m_Fill.w / G.m_Track.w, Position, .00001f);
			const float Radius = G.m_Thumb.w / 2;
			EXPECT_NEAR(G.m_Thumb.x + Radius, std::clamp(Rail.x + Width * Position, Rail.x + Radius, Rail.x + Width - Radius), .001f);
		}
}

TEST(LucentSkinManager, SelectionBackgroundLeavesCornerAxesEmpty)
{
	for(float Pixel : {.25f, .5f, 1.f, 2.f, 4.f})
		for(const auto Size : {vec2(1, 1), vec2(16, 8), vec2(34, 34), vec2(75, 34), vec2(160, 80), vec2(600, 120)})
		{
			const CUIRect Rect{17, 29, Size.x, Size.y};
			const auto Fill = LucentUi::CSelectionBackgroundGeometry::Calculate(Rect, Pixel);
			const auto Corners = LucentUi::CSelectionCornersGeometry::Calculate(Rect, Pixel, 1);
			const float Top = Corners.m_Corners[0].m_Points[0].y;
			const auto &TL = Corners.m_Corners[0];
			const float CornerLeft = TL.m_Points[TL.m_Count - 1].x;
			for(size_t Index = 0; Index < Fill.m_Outer.size(); ++Index)
			{
				const auto Outer = Fill.m_Outer[Index], Inner = Fill.m_Inner[Index];
				EXPECT_GT(Outer.x, CornerLeft);
				EXPECT_LT(Outer.x, Rect.x + Rect.w - (CornerLeft - Rect.x));
				EXPECT_GT(Outer.y, Top);
				EXPECT_LT(Outer.y, Rect.y + Rect.h - (Top - Rect.y));
				EXPECT_GE(Inner.x, Rect.x);
				EXPECT_LE(Inner.x, Rect.x + Rect.w);
				EXPECT_GE(Inner.y, Rect.y);
				EXPECT_LE(Inner.y, Rect.y + Rect.h);
				EXPECT_GE(distance(Outer, Rect.Center()) + .00001f, distance(Inner, Rect.Center()));
			}
		}
}
