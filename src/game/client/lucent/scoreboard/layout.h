#ifndef GAME_CLIENT_LUCENT_SCOREBOARD_LAYOUT_H
#define GAME_CLIENT_LUCENT_SCOREBOARD_LAYOUT_H
#include <engine/textrender.h>

#include <game/client/lucent/hud/layout.h>
#include <game/client/lucent/motion/transition.h>
#include <game/client/lucent/style/tokens.h>

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
		float m_Name = 0, m_IdentityWidth = 0;
		float m_Country = 0, m_CountryWidth = 0, m_Points = 0, m_PointsWidth = 0, m_Ping = 0, m_PingWidth = 0;
		static float ChromeWidth(float Score, float Points, float Ping, float Team = 0)
		{ return Team + Score + Points + Ping + 18 + 22 + 2 * 7 + 4 * 4 + (Score > 0 ? 4 : 0); }
		static float MinimumWidth(float Score, float Points, float Ping, float Team = 0)
		{ return ChromeWidth(Score, Points, Ping, Team) + 112; }
		static float PreferredWidth(float Score, float Points, float Ping, float Name, float Clan, float Team = 0)
		{ return ChromeWidth(Score, Points, Ping, Team) + std::max(112.f, Name + Clan); }
		static CFields Calculate(float Width, float Font, float Score, float Points, float Ping, float /*Name*/, float /*Clan*/, float Team = 0)
		{
			CFields F;
			const float Scale = Font / 12, Pad = 7 * Scale, Gap = 4 * Scale;
			F.m_Score = Pad + Team * Scale;
			F.m_ScoreWidth = Score * Scale;
			F.m_Tee = F.m_Score + F.m_ScoreWidth + (Score > 0 ? Gap : 0);
			F.m_TeeWidth = 18 * Scale;
			F.m_Name = F.m_Tee + F.m_TeeWidth + Gap;
			F.m_PingWidth = Ping * Scale;
			F.m_Ping = Width - Pad - F.m_PingWidth;
			F.m_PointsWidth = Points * Scale;
			F.m_Points = F.m_Ping - Gap - F.m_PointsWidth;
			F.m_CountryWidth = 22 * Scale;
			F.m_Country = F.m_Points - Gap - F.m_CountryWidth;
			F.m_IdentityWidth = std::max(0.f, F.m_Country - Gap - F.m_Name);
			return F;
		}
	};
	// One row-local identity block; numeric and country anchors remain owned by CFields.
	struct CIdentity
	{
		float m_NameWidth = 0, m_ReadyWidth = 0, m_ClanWidth = 0, m_Padding = 0, m_Gap = 0;
		float m_ClanFont = 0, m_Height = 0;
		static float ClanDemand(float Measured) { return Measured * LucentStyle::Scoreboard::ClanFontScale + 2 * LucentStyle::Scoreboard::ClanPadding; }
		static CIdentity Calculate(float Width, float Font, float Row, float Name, float Clan, bool HasClan, float Ready)
		{
			const float Scale = Font / 12;
			return CalculateMeasured(Width, Font, Row, Name * Scale, Clan * LucentStyle::Scoreboard::ClanFontScale * Scale, HasClan, Ready);
		}
		static CIdentity CalculateMeasured(float Width, float Font, float Row, float Name, float Clan, bool HasClan, float Ready, float Padding = -1)
		{
			CIdentity I;
			Width = std::max(0.f, Width);
			const float Scale = Font / 12;
			I.m_ClanFont = Font * LucentStyle::Scoreboard::ClanFontScale;
			I.m_Height = std::min(Row, Font * 1.25f);
			I.m_Padding = std::min(Padding >= 0 ? Padding : LucentStyle::Scoreboard::ClanPadding * Scale, Width * .04f);
			I.m_Gap = HasClan ? std::min(LucentStyle::Scoreboard::ClanGap * Scale, Width * .04f) : 0;
			I.m_ReadyWidth = std::clamp(Ready, 0.f, std::max(0.f, Width - I.m_Gap));
			const float Available = std::max(0.f, Width - I.m_Gap - I.m_ReadyWidth);
			const float NameDemand = std::max(0.f, Name);
			const float ClanWidth = HasClan ? Clan + 2 * (Padding >= 0 ? Padding : LucentStyle::Scoreboard::ClanPadding * Scale) : 0;
			// Preserve short clans in full. Under pressure, reserve a readable nickname
			// first, then let it reclaim everything a short clan does not need.
			const float NamePriority = std::min(NameDemand, std::max(72 * Scale, Available * .65f));
			I.m_ClanWidth = std::min(ClanWidth, std::max(0.f, Available - NamePriority));
			I.m_NameWidth = std::min(NameDemand, Available - I.m_ClanWidth);
			I.m_Padding = HasClan ? std::min(I.m_Padding, I.m_ClanWidth / 2) : 0;
			return I;
		}
		float ClanOffset(float RenderedName) const { return std::clamp(RenderedName, 0.f, m_NameWidth) + m_ReadyWidth + m_Gap; }
		float ClanTextWidth() const { return std::max(.01f, m_ClanWidth - 2 * m_Padding); }
	};
	inline float CommonFont(float Row, float NarrowestColumn, float RequiredWidth)
	{ return std::max(.1f, std::min({12.f, Row * .65f, 12.f * NarrowestColumn / std::max(1.f, RequiredWidth)})); }
	inline void SingleLine(char *pText)
	{
		for(; *pText; ++pText)
			if(static_cast<unsigned char>(*pText) < 32)
				*pText = ' ';
	}
	struct CRange
	{ int m_Start = 0, m_End = 0; };
	inline constexpr float GroupHeaderRows = 1.05f, GroupPaddingRows = .15f;
	struct CGroupRows
	{
		struct CGroup
		{
			int m_Team = 0, m_Start = 0, m_End = 0;
			float m_Top = 0, m_Bottom = 0;
		};
		std::array<float, 128> m_RowY{};
		std::array<CGroup, 128> m_Groups{};
		int m_Count = 0;
		float m_Units = 0;
		static CGroupRows Calculate(const std::array<int, 128> &Teams, CRange Range)
		{
			CGroupRows G;
			for(int I = Range.m_Start; I < Range.m_End;)
			{
				const int Start = I, Team = Teams[I];
				int End = I + 1;
				if(Team > 0)
					while(End < Range.m_End && Teams[End] == Team)
						++End;
				const float Top = G.m_Units;
				if(Team > 0)
					G.m_Units += GroupPaddingRows + GroupHeaderRows;
				for(; I < End; ++I)
					G.m_RowY[I] = G.m_Units++;
				if(Team > 0)
				{
					G.m_Units += GroupPaddingRows;
					G.m_Groups[G.m_Count++] = {Team, Start, End, Top, G.m_Units};
				}
			}
			return G;
		}
	};
	// Complete measured identities in the exact alive/dead/team row order.
	// A column shares its gutter, but name and clan remain a single row demand.
	struct CContentWidths
	{
		std::array<std::array<float, 128>, 2> m_Identity{}, m_Gutter{};
		float m_Chrome = 0;
		bool m_Independent = false;
		std::array<std::array<int, 128>, 2> m_Team{};
		std::array<std::array<float, 128>, 2> m_Score{}, m_GroupTitle{};
		float m_Points = 0, m_Ping = 0;
		std::array<CRange, 4> Partition(int Group, int Count, int Columns) const
		{
			std::array<CRange, 4> Result{};
			if(!Count || !Columns)
				return Result;
			Columns = std::min(Columns, Count);
			if(!m_Independent)
			{
				for(int C = 0; C < Columns; ++C)
					Result[C] = {std::min(Count, C * ((Count + Columns - 1) / Columns)), std::min(Count, (C + 1) * ((Count + Columns - 1) / Columns))};
				return Result;
			}
			// Twentieths of a row make the exact shared header/padding cost integral.
			constexpr int Unit = 20;
			const int Overhead = int(std::round((GroupHeaderRows + 2 * GroupPaddingRows) * Unit));
			std::array<int, 129> Starts{};
			for(int I = 0; I < Count; ++I)
				Starts[I + 1] = Starts[I] + (m_Team[Group][I] > 0 && (I == 0 || m_Team[Group][I - 1] != m_Team[Group][I]));
			const auto Height = [&](int Start, int End) {
				const int Continued = Start > 0 && m_Team[Group][Start] > 0 && m_Team[Group][Start - 1] == m_Team[Group][Start];
				return (End - Start) * Unit + (Starts[End] - Starts[Start] + Continued) * Overhead;
			};
			constexpr int Infinity = 1000000;
			std::array<std::array<int, 129>, 5> Maximum{};
			for(auto &Row : Maximum)
				Row.fill(Infinity);
			Maximum[0][0] = 0;
			for(int C = 1; C <= Columns; ++C)
				for(int End = C; End <= Count; ++End)
					for(int Start = C - 1; Start < End; ++Start)
						Maximum[C][End] = std::min(Maximum[C][End], std::max(Maximum[C - 1][Start], Height(Start, End)));
			// Within the minimax ceiling maximize the shortest panel, reducing the
			// final height spread before preferring intact teams and stable ties.
			std::array<std::array<int, 129>, 5> Minimum{};
			for(auto &Row : Minimum)
				Row.fill(-1);
			Minimum[0][0] = Infinity;
			for(int C = 1; C <= Columns; ++C)
				for(int End = C; End <= Count; ++End)
					for(int Start = C - 1; Start < End; ++Start)
						if(Height(Start, End) <= Maximum[Columns][Count] && Minimum[C - 1][Start] >= 0)
							Minimum[C][End] = std::max(Minimum[C][End], std::min(Minimum[C - 1][Start], Height(Start, End)));
			struct CCost
			{
				int m_Splits = 1000000, m_Previous = -1;
				int64_t m_Squares = INT64_MAX;
			};
			std::array<std::array<CCost, 129>, 5> Cost{};
			Cost[0][0] = {0, 0, 0};
			for(int C = 1; C <= Columns; ++C)
				for(int End = C; End <= Count; ++End)
					for(int Start = C - 1; Start < End; ++Start)
					{
						const int H = Height(Start, End);
						if(H < Minimum[Columns][Count] || H > Maximum[Columns][Count] || Cost[C - 1][Start].m_Previous < 0)
							continue;
						const int Split = Start > 0 && m_Team[Group][Start] > 0 && m_Team[Group][Start - 1] == m_Team[Group][Start];
						const int Splits = Cost[C - 1][Start].m_Splits + Split;
						const int64_t Squares = Cost[C - 1][Start].m_Squares + int64_t(H) * H;
						auto &Best = Cost[C][End];
						if(Splits < Best.m_Splits || (Splits == Best.m_Splits && Squares < Best.m_Squares))
							Best = {Splits, Start, Squares};
					}
			for(int C = Columns, End = Count; C > 0; --C)
			{
				const int Start = Cost[C][End].m_Previous;
				Result[C - 1] = {Start, End};
				End = Start;
			}
			return Result;
		}
		CRange Range(int Group, int Count, int Column, int Columns) const { return Partition(Group, Count, Columns)[Column]; }

		float ScoreWidth(int Group, CRange Range) const
		{
			float Width = 0;
			for(int I = Range.m_Start; I < Range.m_End; ++I)
				Width = std::max(Width, m_Score[Group][I]);
			return Width;
		}
		float ColumnPreferred(int Group, CRange Range) const
		{
			float Identity = 0, Title = 0;
			for(int I = Range.m_Start; I < Range.m_End; ++I)
			{
				Identity = std::max(Identity, m_Identity[Group][I]);
				Title = std::max(Title, m_GroupTitle[Group][I]);
			}
			return std::max(Title + 14, CFields::ChromeWidth(ScoreWidth(Group, Range), m_Points, m_Ping) + std::max(72.f, Identity));
		}
		bool operator==(const CContentWidths &) const = default;
		float Preferred(int Red, int Blue, int RedColumns, int BlueColumns) const
		{
			float Required = m_Chrome + 112;
			for(int Group = 0; Group < (BlueColumns ? 2 : 1); ++Group)
			{
				const int Count = Group ? Blue : Red, Columns = Group ? BlueColumns : RedColumns;
				const int PerColumn = (Count + Columns - 1) / Columns;
				for(int Column = 0; Column < Columns; ++Column)
				{
					float Identity = 112, Gutter = 0;
					for(int I = Column * PerColumn; I < std::min(Count, (Column + 1) * PerColumn); ++I)
					{
						Identity = std::max(Identity, m_Identity[Group][I]);
						Gutter = std::max(Gutter, m_Gutter[Group][I]);
					}
					Required = std::max(Required, m_Chrome + Identity + Gutter);
				}
			}
			return Required;
		}
	};
	// UI coordinates (600 high). A bounded search maximizes legibility before density.
	struct CLayout
	{
		int m_RedColumns = 1, m_BlueColumns = 0;
		int m_Columns = 1, m_Rows = 0, m_SpectatorColumns = 1, m_SpectatorRows = 0;
		float m_Width = 0, m_Height = 0, m_Row = 22, m_SpectatorHeight = 0, m_SpectatorRow = 14, m_SpectatorWidth = 0;
		float m_ColumnChrome = ColumnHeadlines + ColumnBottom;
		float m_Gap = ColumnGap;
		std::array<float, MaxColumns> m_Widths{}, m_Scores{}, m_Units{};
		std::array<CRange, MaxColumns> m_Ranges{};
		float ColumnWidth(int Column = 0) const { return m_Widths[Column] > 0 ? m_Widths[Column] : (m_Width - m_Gap * (m_Columns - 1)) / m_Columns; }
		float ColumnX(int Column) const
		{
			float X = 0;
			for(int I = 0; I < Column; ++I)
				X += ColumnWidth(I) + m_Gap;
			return X;
		}
	};
	inline CLayout Calculate(float Width, float Height, int Red, int Blue, bool Teams, int Spectators, float ContentWidth = 440, float MinimumWidth = 400, const CContentWidths *pContent = nullptr)
	{
		Width = std::max(120.f, Width - 32);
		Height = std::max(120.f, Height - 120);
		Red = std::clamp(Red, 0, 128);
		Blue = Teams ? std::clamp(Blue, 0, 128 - Red) : 0;
		Spectators = std::clamp(Spectators, 0, 128 - Red - Blue);
		CLayout Best;
		float BestQuality = -1;
		const bool Independent = pContent && pContent->m_Independent;
		const int FirstRed = Independent && Red == 0 && Blue > 0 ? 0 : 1;
		const int LastRed = Independent && Red == 0 ? FirstRed : 4;
		const int FirstBlue = Teams && (!Independent || Blue > 0) ? 1 : 0;
		const int LastBlue = FirstBlue ? 4 : 0;
		for(int RedColumns = FirstRed; RedColumns <= LastRed; ++RedColumns)
			for(int BlueColumns = FirstBlue; BlueColumns <= LastBlue; ++BlueColumns)
			{
				if(RedColumns > std::max(1, Red) || BlueColumns > std::max(1, Blue))
					continue;
				CLayout L;
				L.m_Gap = std::max(Red, Blue) > 64 ? 6.f : ColumnGap;
				L.m_RedColumns = RedColumns;
				L.m_BlueColumns = BlueColumns;
				L.m_Columns = RedColumns + BlueColumns;
				L.m_Rows = std::max(RedColumns ? (Red + RedColumns - 1) / RedColumns : 0, BlueColumns ? (Blue + BlueColumns - 1) / BlueColumns : 0);
				const float Preferred = pContent && !Independent ? pContent->Preferred(Red, Blue, RedColumns, BlueColumns) : ContentWidth;
				std::array<float, MaxColumns> Minimums{};
				float TotalMinimum = 0;
				const auto RedRanges = pContent ? pContent->Partition(0, Red, RedColumns) : std::array<CRange, 4>{};
				const auto BlueRanges = pContent && BlueColumns ? pContent->Partition(1, Blue, BlueColumns) : std::array<CRange, 4>{};
				float MaxUnits = Independent ? 0.f : float(L.m_Rows);
				L.m_Width = 0;
				for(int Index = 0; Index < L.m_Columns; ++Index)
				{
					const int Group = Index >= RedColumns ? 1 : 0, Columns = Group ? BlueColumns : RedColumns;
					const int Count = Group ? Blue : Red, Column = Index - (Group ? RedColumns : 0);
					L.m_Ranges[Index] = pContent ? (Group ? BlueRanges[Column] : RedRanges[Column]) : CRange{std::min(Count, Column * ((Count + Columns - 1) / Columns)), std::min(Count, (Column + 1) * ((Count + Columns - 1) / Columns))};
					L.m_Units[Index] = Independent ? CGroupRows::Calculate(pContent->m_Team[Group], L.m_Ranges[Index]).m_Units : float(L.m_Ranges[Index].m_End - L.m_Ranges[Index].m_Start);
					MaxUnits = std::max(MaxUnits, L.m_Units[Index]);
					L.m_Scores[Index] = Independent ? pContent->ScoreWidth(Group, L.m_Ranges[Index]) : 0;
					L.m_Widths[Index] = Independent ? pContent->ColumnPreferred(Group, L.m_Ranges[Index]) : std::max(Preferred, MinimumWidth);
					Minimums[Index] = Independent ? CFields::ChromeWidth(L.m_Scores[Index], pContent->m_Points, pContent->m_Ping) + 72 : MinimumWidth;
					TotalMinimum += Minimums[Index];
					L.m_Width += L.m_Widths[Index];
				}
				const float Available = std::max(1.f, Width - (L.m_Columns - 1) * L.m_Gap);
				if(L.m_Width > Available)
				{
					const float Extra = std::max(0.f, Available - TotalMinimum), Demand = std::max(1.f, L.m_Width - TotalMinimum);
					for(int Index = 0; Index < L.m_Columns; ++Index)
						L.m_Widths[Index] = TotalMinimum >= Available ? Minimums[Index] * Available / TotalMinimum : Minimums[Index] + (L.m_Widths[Index] - Minimums[Index]) * Extra / Demand;
					L.m_Width = Available;
				}
				L.m_Width += (L.m_Columns - 1) * L.m_Gap;
				L.m_ColumnChrome = ColumnHeadlines + ColumnBottom + (Teams ? TeamTitleHeight : 0);
				const float SpectatorBudget = std::min(Height * (Red + Blue ? .45f : .85f), Height - HeaderHeight - HeaderGap - L.m_ColumnChrome - MaxUnits);
				const int ComfortableRows = std::max(1, int((SpectatorBudget - 28) / 14));
				const int SpectatorColumns = (Spectators + ComfortableRows - 1) / ComfortableRows;
				// Spectator density must not widen unrelated player columns or create
				// empty player panels. Its separate floating surface can be wider.
				L.m_SpectatorWidth = Spectators ? std::min(Width, std::max(L.m_Width, 16.f + 150 * SpectatorColumns)) : 0;
				L.m_SpectatorColumns = std::max(1, int((L.m_SpectatorWidth - 16) / 150));
				L.m_SpectatorRows = (Spectators + L.m_SpectatorColumns - 1) / L.m_SpectatorColumns;
				L.m_SpectatorRow = L.m_SpectatorRows ? std::min(14.f, std::max(1.f, SpectatorBudget - 28) / L.m_SpectatorRows) : 14.f;
				L.m_SpectatorHeight = Spectators ? 28.f + L.m_SpectatorRows * L.m_SpectatorRow : 0;
				L.m_Row = std::clamp((Height - HeaderHeight - HeaderGap - L.m_ColumnChrome - L.m_SpectatorHeight) / std::max(1.f, MaxUnits), 1.f, 22.f);
				L.m_Height = HeaderHeight + ((L.m_Rows || Teams) ? HeaderGap + L.m_ColumnChrome + MaxUnits * L.m_Row : 0);
				// Width and row density both constrain the shared row renderer.
				float BodyQuality = 1;
				for(int Index = 0; Index < L.m_Columns; ++Index)
					BodyQuality = std::min(BodyQuality, CommonFont(L.m_Row, L.ColumnWidth(Index), Independent ? CFields::ChromeWidth(L.m_Scores[Index], pContent->m_Points, pContent->m_Ping) + 72 : MinimumWidth) / 12);
				const float Quality = std::min(BodyQuality, Spectators ? L.m_SpectatorRow / 14 : 1.f) -
						      .008f * L.m_Columns + .01f * std::min(1.f, L.ColumnWidth() / std::max(1.f, Preferred));
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
		return {Left + Layout.ColumnX(Column), Top + HeaderHeight + HeaderGap,
			Layout.ColumnWidth(Column), Layout.m_ColumnChrome + (Layout.m_Units[Column] > 0 ? Layout.m_Units[Column] : float(Rows)) * Layout.m_Row};
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
		CContentWidths m_Content;
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
				m_Layout = Calculate(Input.m_Width, Input.m_Height, Input.m_Red, Input.m_Blue, Input.m_Teams, Input.m_Spectators, Input.m_ContentWidth, Input.m_MinimumWidth, Input.m_Content.m_Chrome > 0 ? &Input.m_Content : nullptr);
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
