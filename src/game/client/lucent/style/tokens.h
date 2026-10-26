#ifndef GAME_CLIENT_LUCENT_STYLE_TOKENS_H
#define GAME_CLIENT_LUCENT_STYLE_TOKENS_H

#include <base/color.h>

namespace LucentStyle
{
	namespace Console
	{
		constexpr float PillRadius = 2, PillPadding = 3, Gap = 6, MetadataSize = 8;
		constexpr float CopySize = 16, CopyFade = .12f;
		constexpr float CopyGap = 11;
		constexpr double CopiedDuration = 1.05;
		constexpr float HistoryPadding = 4, FilterGap = 8, FilterPadding = 6;
		constexpr float PopoverPadding = 8, PopoverRow = 28, PopoverWidth = 210, LevelsWidth = 270, PopoverHeader = 18;
		constexpr float PopoverRadius = 9, PopoverMotion = .16f, PopoverTravel = 4, PopoverIndicator = 4;
		inline const ColorRGBA PopoverSurface(.025f, .021f, .035f, .94f);
		constexpr float MorePopoverOpacity = 1.f;
		constexpr float PopoverHintHeight = 40;
		constexpr float FilterHeight = 26, FilterFontSize = 9, FilterFullWidth = 620, FilterMinWidth = 72;
		constexpr float FilterTransition = .18f, FilterInactiveAlpha = .48f;
		constexpr float SeveritySize = 3;
		inline const ColorRGBA SeverityColors[] = {
			ColorRGBA(.92f, .48f, .51f, 1), ColorRGBA(.88f, .71f, .43f, 1), ColorRGBA(.67f, .79f, .73f, 1),
			ColorRGBA(.55f, .71f, .85f, 1), ColorRGBA(.76f, .63f, .84f, 1), ColorRGBA(.62f, .60f, .65f, 1)};
		constexpr float MetadataWidthFraction = .27f, CopyIdleAlpha = .65f;
		inline const ColorRGBA Timestamp(.19f, .18f, .22f, .65f);
		inline const ColorRGBA MetadataText(.78f, .76f, .82f, 1);
		inline const ColorRGBA Graphics(.35f, .29f, .46f, .75f);
		inline const ColorRGBA Network(.22f, .35f, .43f, .75f);
		inline const ColorRGBA Chat(.27f, .39f, .34f, .75f);
		inline const ColorRGBA Server(.43f, .32f, .25f, .75f);
		inline const ColorRGBA Browser(.29f, .34f, .47f, .75f);
		inline const ColorRGBA Other(.37f, .28f, .35f, .75f);
		inline const ColorRGBA Client(.38f, .31f, .43f, .85f);
		inline const ColorRGBA Assets(.30f, .39f, .34f, .85f);
		inline const ColorRGBA Record(.41f, .30f, .39f, .85f);
		inline const ColorRGBA Editor(.38f, .37f, .27f, .85f);
	}
	namespace Style
	{
		inline const ColorRGBA Accent(1, .82f, .91f, 1);
		inline const ColorRGBA Surface(.025f, .021f, .035f, .98f);
		inline const ColorRGBA MenuBackground(.018f, .015f, .027f, 1);
		inline const ColorRGBA BrowserBackground(.010f, .009f, .014f, 1);
		inline const ColorRGBA Text(.88f, .87f, .92f, 1);
		inline const ColorRGBA PrimaryText(.96f, .85f, .92f, 1);
		inline const ColorRGBA MutedText(.64f, .60f, .67f, 1);
		inline const ColorRGBA NavigationText(.66f, .63f, .69f, 1);
		inline const ColorRGBA NavigationHover(.91f, .83f, .88f, 1);
		inline const ColorRGBA MenuHover(1, .82f, .9f, 1);
		constexpr float Radius = 3, Hairline = .5f, ControlSize = 14, LabelGap = 7;
		constexpr float BorderIdle = .12f, BorderActive = .40f, FilledBorder = .42f, FocusBorder = .62f;
		inline const ColorRGBA BrowserHint(.77f, .71f, .76f, 1);
		inline const ColorRGBA SecondaryText(.63f, .62f, .69f, 1);
		inline const ColorRGBA DetailText(.72f, .68f, .74f, 1);
		inline const ColorRGBA UnavailableText(.64f, .58f, .64f, 1);
		inline const ColorRGBA ServerSelection(.19f, .12f, .16f, .65f);
		inline const ColorRGBA ServerText(.86f, .84f, .88f, 1);
		inline const ColorRGBA ServerHover(.98f, .96f, .98f, 1);
		inline const ColorRGBA MetadataText(.58f, .57f, .64f, 1);
		inline const ColorRGBA MetadataHover(.72f, .69f, .76f, 1);
		inline const ColorRGBA UtilitySurface(0, 0, 0, 1);
		inline const ColorRGBA ErrorText(1, .4f, .4f, 1);
		inline const ColorRGBA FavoriteText(.95f, .79f, .87f, 1);
		constexpr float InputHover = .08f, InputFocus = .12f, SurfaceGlow = .07f, TextHover = .25f;
		constexpr float CheckAlpha = .85f, CheckInset = 4, CheckRadius = 1, MixedHeight = 1.5f;
		constexpr float ButtonUnderline = .7f, PressedUnderline = .95f, PressedText = .8f;
		constexpr float SceneBase = .25f, SceneHover = .75f, ScenePressed = .10f, SceneIdle = .025f;
		constexpr float Disabled = .35f, Selected = .13f, Hover = .045f, Pressed = .07f;
	}
	namespace Checkbox
	{
		constexpr float Size = 14, Border = 1.1f, DotSize = 7, LeftInset = 5;
		constexpr float LabelOffset = LeftInset + Size + Style::LabelGap;
		constexpr float TextIdle = .80f, TextHover = .95f, TextChecked = 1.f;
		constexpr float FillDuration = .20f;
		constexpr float RingIntensity = .85f, GradientTilt = .2f;
		constexpr double Period = 5;
		inline const ColorRGBA BorderIdle(.22f, .20f, .25f, 1);
		inline const ColorRGBA BorderHover(.48f, .35f, .43f, 1);
	}

	namespace Motion
	{
		constexpr float Hover = .18f, Focus = .18f, Selection = .22f, Branch = .28f;
		constexpr double Radial = .88;
		constexpr float MenuResponse = 12;
		constexpr float SecondaryReveal = .32f, SecondaryStagger = .04f, SecondaryItem = .16f;
		constexpr float MenuReveal = .35f, MenuDelay = .065f, MenuStagger = .02f, MenuItem = .245f;
		constexpr float MapTitle = .42f;
		constexpr float TextAppear = .12f, TextTrail = .22f, TextTrailIntensity = .28f;
		constexpr float TextTrailGlyphs = 1.5f, TextSpeedLow = 25.f, TextSpeedHigh = 240.f;
		constexpr float TextSpeedResponse = .055f, TextIdleInterval = .3f, TextMinEventInterval = .001f;
		constexpr float TextTrailIdleIntensity = .4f;
		constexpr float TextHeadWidth = .065f, TextHeadMinWidth = .7f;
		constexpr float TextJumpLimit = 3.f;
		constexpr float FilterInput = .27f, Filters = .48f, FilterRise = 12.f;
		constexpr float FilterNavTravel = 60, FilterFooterTravel = 50;
		constexpr float FilterQuickDelay = .08f, FilterBodyDelay = .16f;
		constexpr float Primary = .30f, Scene = .36f, Overlay = .32f, Expansion = .38f, Players = .40f;
	}
}
#endif
