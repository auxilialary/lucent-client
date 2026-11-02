#ifndef GAME_CLIENT_LUCENT_SCOREBOARD_LAYOUT_H
#define GAME_CLIENT_LUCENT_SCOREBOARD_LAYOUT_H
#include <engine/textrender.h>

#include <game/client/lucent/hud/layout.h>
#include <game/client/lucent/motion/transition.h>

#include <algorithm>
#include <cmath>

namespace LucentScoreboard
{
	inline constexpr int SingleLineTextFlags = TEXTFLAG_RENDER | TEXTFLAG_DISALLOW_NEWLINE | TEXTFLAG_STOP_AT_END | TEXTFLAG_ELLIPSIS_AT_END;
	inline constexpr float HeaderHeight = 34.f, HeaderGap = 10.f, ColumnGap = 10.f;
	inline constexpr float HeaderPadding = 12.f, HeaderContentGap = 12.f, HeaderMaxWidth = 460.f;
	inline constexpr float TeamTitleHeight = 26.f, ColumnHeadlines = 22.f, ColumnBottom = 8.f;
	inline constexpr int MaxColumns = 8;

	// Numeric metrics use a 12-unit reference and scale uniformly for every column.
	struct CFields
	{
		float m_Score = 0, m_ScoreWidth = 0, m_Tee = 0, m_TeeWidth = 0;
		float m_Name = 0, m_NameWidth = 0, m_Clan = 0, m_ClanWidth = 0;
		float m_Country = 0, m_CountryWidth = 0, m_Points = 0, m_PointsWidth = 0, m_Ping = 0, m_PingWidth = 0;
		static float MinimumWidth(float Score, float Points, float Ping, float Team = 0)
		{ return Team + Score + Points + Ping + 18 + 22 + 72 + 40 + 2 * 7 + 6 * 4; }
		static float PreferredWidth(float Score, float Points, float Ping, float Name, float Clan, float Team = 0)
		{
			return MinimumWidth(Score, Points, Ping, Team) + std::max(0.f, Name + 16 - 72) + std::max(0.f, Clan + 4 - 40);
		}
		static CFields Calculate(float Width, float Font, float Score, float Points, float Ping, float Name, float Clan, float Team = 0)
		{
			CFields F;
			const float Scale = Font / 12, Pad = 7 * Scale, Gap = 4 * Scale;
			F.m_Score = Pad + Team * Scale;
			F.m_ScoreWidth = Score * Scale;
			F.m_Tee = F.m_Score + F.m_ScoreWidth + Gap;
			F.m_TeeWidth = 18 * Scale;
			F.m_Name = F.m_Tee + F.m_TeeWidth + Gap;
			F.m_PingWidth = Ping * Scale;
			F.m_Ping = Width - Pad - F.m_PingWidth;
			F.m_PointsWidth = Points * Scale;
			F.m_Points = F.m_Ping - Gap - F.m_PointsWidth;
			F.m_CountryWidth = 22 * Scale;
			F.m_Country = F.m_Points - Gap - F.m_CountryWidth;
			const float Middle = std::max(0.f, F.m_Country - Gap - F.m_Name - Gap);
			const float ExtraName = std::max(0.f, Name + 16 - 72), ExtraClan = std::max(0.f, Clan + 4 - 40);
			const float Base = std::min(1.f, Middle / std::max(.01f, 112 * Scale));
			const float Extra = std::max(0.f, Middle - 112 * Scale);
			const float Share = ExtraName + ExtraClan > 0 ? ExtraName / (ExtraName + ExtraClan) : 72.f / 112;
			F.m_NameWidth = 72 * Scale * Base + Extra * Share;
			F.m_Clan = F.m_Name + F.m_NameWidth + Gap;
			F.m_ClanWidth = Middle - F.m_NameWidth;
			return F;
		}
	};
	inline float CommonFont(float Row, float NarrowestColumn, float RequiredWidth)
	{ return std::max(.1f, std::min({12.f, Row * .65f, 12.f * NarrowestColumn / std::max(1.f, RequiredWidth)})); }
	inline void SingleLine(char *pText)
	{
		for(; *pText; ++pText)
			if(static_cast<unsigned char>(*pText) < 32)
				*pText = ' ';
	}
	// UI coordinates (600 high). A bounded search maximizes legibility before density.
	struct CLayout
	{
		int m_RedColumns = 1, m_BlueColumns = 0;
		int m_Columns = 1, m_Rows = 0, m_SpectatorColumns = 1, m_SpectatorRows = 0;
		float m_Width = 0, m_Height = 0, m_Row = 22, m_SpectatorHeight = 0, m_SpectatorRow = 14;
		float m_ColumnChrome = ColumnHeadlines + ColumnBottom;
		float m_Gap = ColumnGap;
		float ColumnWidth() const { return (m_Width - m_Gap * (m_Columns - 1)) / m_Columns; }
	};
	inline CLayout Calculate(float Width, float Height, int Red, int Blue, bool Teams, int Spectators, float ContentWidth = 440, float MinimumWidth = 400)
	{
		Width = std::max(120.f, Width - 32);
		Height = std::max(120.f, Height - 120);
		Red = std::clamp(Red, 0, 128);
		Blue = Teams ? std::clamp(Blue, 0, 128 - Red) : 0;
		Spectators = std::clamp(Spectators, 0, 128 - Red - Blue);
		CLayout Best;
		float BestQuality = -1;
		for(int RedColumns = 1; RedColumns <= 4; ++RedColumns)
			for(int BlueColumns = Teams ? 1 : 0; BlueColumns <= (Teams ? 4 : 0); ++BlueColumns)
			{
				if((Teams && Red == 0 && RedColumns > 1) || (Blue == 0 && BlueColumns > 1))
					continue;
				CLayout L;
				L.m_Gap = std::max(Red, Blue) > 64 ? 6.f : ColumnGap;
				L.m_RedColumns = RedColumns;
				L.m_BlueColumns = BlueColumns;
				L.m_Columns = RedColumns + BlueColumns;
				L.m_Rows = std::max((Red + RedColumns - 1) / RedColumns, BlueColumns ? (Blue + BlueColumns - 1) / BlueColumns : 0);
				L.m_Width = std::min(Width, L.m_Columns * std::max(ContentWidth, 400.f) + (L.m_Columns - 1) * L.m_Gap);
				L.m_ColumnChrome = ColumnHeadlines + ColumnBottom + (Teams ? TeamTitleHeight : 0);
				L.m_SpectatorColumns = std::max(1, (int)((L.m_Width - 16) / 150));
				L.m_SpectatorRows = (Spectators + L.m_SpectatorColumns - 1) / L.m_SpectatorColumns;
				const float SpectatorBudget = std::min(Height * (Red + Blue ? .45f : .85f), Height - HeaderHeight - HeaderGap - L.m_ColumnChrome - L.m_Rows);
				L.m_SpectatorRow = L.m_SpectatorRows ? std::min(14.f, std::max(1.f, SpectatorBudget - 28) / L.m_SpectatorRows) : 14.f;
				L.m_SpectatorHeight = Spectators ? 28.f + L.m_SpectatorRows * L.m_SpectatorRow : 0;
				L.m_Row = std::clamp((Height - HeaderHeight - HeaderGap - L.m_ColumnChrome - L.m_SpectatorHeight) / std::max(1, L.m_Rows), 1.f, 22.f);
				L.m_Height = HeaderHeight + ((L.m_Rows || Teams) ? HeaderGap + L.m_ColumnChrome + L.m_Rows * L.m_Row : 0);
				// Width and row density both constrain the shared row renderer.
				const float BodyQuality = CommonFont(L.m_Row, L.ColumnWidth(), MinimumWidth) / 12;
				const float Quality = std::min(BodyQuality, Spectators ? L.m_SpectatorRow / 14 : 1.f) -
						      .008f * L.m_Columns + .01f * std::min(1.f, L.ColumnWidth() / std::max(1.f, ContentWidth));
				if(Quality > BestQuality)
				{
					BestQuality = Quality;
					Best = L;
				}
			}
		return Best;
	}

	// Header depends on its own content and viewport, never on the player grid width/count.
	inline LucentHud::CRect HeaderBounds(float CenterX, float Top, float ViewportWidth, float MapWidth, float TimerWidth)
	{
		const float Limit = std::max(1.f, std::min(HeaderMaxWidth, ViewportWidth * .75f));
		const float Width = std::min(Limit, 2 * HeaderPadding + MapWidth + HeaderContentGap + TimerWidth);
		return {CenterX - Width / 2, Top, Width, HeaderHeight};
	}
	inline LucentHud::CRect ColumnBounds(const CLayout &Layout, float Left, float Top, int Column, int Rows)
	{
		return {Left + Column * (Layout.ColumnWidth() + Layout.m_Gap), Top + HeaderHeight + HeaderGap,
			Layout.ColumnWidth(), Layout.m_ColumnChrome + Rows * Layout.m_Row};
	}
	inline float SpectatorSpace(const CLayout &Layout, int Count)
	{
		return Count > 0 ? Layout.m_SpectatorHeight + ColumnGap : 0.f;
	}

	inline int RoundTime(int Tick, int Speed, int RoundStart, int TimeLimit, int Warmup, bool RaceTime, bool GameOver)
	{
		Speed = std::max(1, Speed);
		if(TimeLimit && Warmup <= 0)
			return GameOver ? 0 : std::max(0, TimeLimit * 60 - (Tick - RoundStart) / Speed);
		return std::max(0, RaceTime ? (Tick + Warmup) / Speed : (Tick - RoundStart) / Speed);
	}

	struct CRange
	{ int m_Start, m_End; };

	template<size_t N>
	float ColumnGutter(const std::array<float, N> &OrderedWidths, CRange Range)
	{
		float Width = 0;
		for(int I = std::max(0, Range.m_Start); I < std::min(int(N), Range.m_End); ++I)
			Width = std::max(Width, OrderedWidths[I]);
		return Width;
	}
	inline CRange ColumnRange(int Count, int Column, int Columns)
	{
		const int PerColumn = (Count + Columns - 1) / Columns;
		const int Start = std::min(Count, Column * PerColumn);
		return {Start, std::min(Count, Start + PerColumn)};
	}
	struct CInput
	{
		float m_Width = 0, m_Height = 0;
		int m_Red = 0, m_Blue = 0;
		bool m_Teams = false;
		int m_Spectators = 0;
		float m_ContentWidth = 400, m_MinimumWidth = 400;
		bool operator==(const CInput &) const = default;
	};
	struct CPlanner
	{
		CInput m_Input{};
		CLayout m_Layout{};
		bool m_Valid = false;
		const CLayout &Update(const CInput &Input)
		{
			if(!m_Valid || !(Input == m_Input))
			{
				m_Input = Input;
				m_Layout = Calculate(Input.m_Width, Input.m_Height, Input.m_Red, Input.m_Blue, Input.m_Teams, Input.m_Spectators, Input.m_ContentWidth, Input.m_MinimumWidth);
				m_Valid = true;
			}
			return m_Layout;
		}
	};
	inline float FitFont(float Font, float Measured, float Available)
	{
		return Font * std::min(1.f, std::max(0.f, Available) / std::max(1.f, Measured));
	}
	struct CExtent
	{
		float m_Value = 0, m_From = 0, m_Target = 0;
		double m_Start = -1;
		float Update(float Target, double Now)
		{
			if(m_Start < 0)
			{
				m_Value = m_From = m_Target = Target;
				m_Start = Now;
			}
			m_Value = LucentStyle::Motion::Lerp(m_From, m_Target, LucentStyle::Motion::CubicOut(float((Now - m_Start) / .16)));
			if(Target != m_Target)
			{
				m_From = m_Value;
				m_Target = Target;
				m_Start = Now;
			}
			return m_Value;
		}
	};
	struct CPanelMotion
	{
		CExtent m_X, m_Y, m_Width, m_Height;
		LucentStyle::Motion::CTransition m_Visibility;
		LucentHud::CRect Update(const LucentHud::CRect &Target, double Now, bool Visible)
		{
			m_Visibility.Update(Visible, Now, true, .12f);
			if(Visible)
				return {m_X.Update(Target.x, Now), m_Y.Update(Target.y, Now), m_Width.Update(Target.w, Now), m_Height.Update(Target.h, Now)};
			if(m_X.m_Start < 0)
				return {};
			// Continue the outgoing trajectory during fade-out so rapid reactivation
			// retargets the same visible geometry instead of a stale frozen sample.
			return {m_X.Update(m_X.m_Target, Now), m_Y.Update(m_Y.m_Target, Now),
				m_Width.Update(m_Width.m_Target, Now), m_Height.Update(m_Height.m_Target, Now)};
		}
	};

}
#endif
