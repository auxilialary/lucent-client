#ifndef GAME_CLIENT_LUCENT_HUD_FINISH_SUMMARY_H
#define GAME_CLIENT_LUCENT_HUD_FINISH_SUMMARY_H

#include <game/client/lucent/scoreboard/points.h>

#include <charconv>
#include <cmath>

namespace LucentHud::Finish
{
	struct CRank
	{
		int m_Global = 0;
		int m_Regional = 0;
	};

	inline constexpr double DisplaySeconds = 5;

	struct CRankReply
	{
		CRank m_Rank;
		char m_aRegion[5] = {};
	};

	inline int TakeNumber(std::string_view &Text)
	{
		if(Text.empty())
			return -1;
		int Number;
		const auto [pEnd, Error] = std::from_chars(Text.data(), Text.data() + Text.size(), Number);
		if(Error != std::errc{} || Number < 0)
			return -1;
		Text.remove_prefix(pEnd - Text.data());
		return Number;
	}

	inline bool MatchesRankHeader(std::string_view Text, std::string_view Name, std::string_view Best)
	{
		if(!Text.starts_with(Name))
			return false;
		Text.remove_prefix(Name.size());
		constexpr std::string_view Separator = " - ", Percent = " - better than ";
		if(!Text.starts_with(Separator))
			return false;
		Text.remove_prefix(Separator.size());
		if(!Text.starts_with(Best))
			return false;
		Text.remove_prefix(Best.size());
		if(!Text.starts_with(Percent))
			return false;
		Text.remove_prefix(Percent.size());
		const int Better = TakeNumber(Text);
		return Better >= 0 && Better <= 100 && Text == "%";
	}

	inline std::optional<CRankReply> ParseRankReply(std::string_view Text)
	{
		constexpr std::string_view Prefix = "Global rank ";
		if(!Text.starts_with(Prefix))
			return std::nullopt;
		Text.remove_prefix(Prefix.size());
		CRankReply Reply;
		Reply.m_Rank.m_Global = TakeNumber(Text);
		if(Reply.m_Rank.m_Global <= 0)
			return std::nullopt;
		if(Text.empty())
			return Reply;
		if(!Text.starts_with(" - "))
			return std::nullopt;
		Text.remove_prefix(3);
		const auto Space = Text.find(' ');
		if(Space == 0 || Space >= sizeof(Reply.m_aRegion) ||
			Text.substr(0, Space).find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != std::string_view::npos)
			return std::nullopt;
		Text.copy(Reply.m_aRegion, Space);
		Text.remove_prefix(Space + 1);
		if(Text == "unranked")
			return Reply;
		if(!Text.starts_with("rank "))
			return std::nullopt;
		Text.remove_prefix(5);
		Reply.m_Rank.m_Regional = TakeNumber(Text);
		if(Reply.m_Rank.m_Regional <= 0 || !Text.empty())
			return std::nullopt;
		return Reply;
	}

	inline double RankDisplayUntil(CRank Previous, CRank Current, double Now, double VisibleUntil)
	{
		const bool Changed = (Current.m_Global > 0 && Current.m_Global != Previous.m_Global) ||
				     (Current.m_Regional > 0 && Current.m_Regional != Previous.m_Regional);
		return Changed ? std::max(VisibleUntil, Now + DisplaySeconds) : VisibleUntil;
	}

	struct CProfile
	{
		int m_Points = LucentScoreboard::UnknownPoints;
		int m_MapPoints = -1;
		int m_Rank = 0;
		double m_BestSeconds = 0;
	};

	struct CMap
	{
		int m_Points = -1;
		int m_Rank = 0;
	};

	inline std::string_view JsonString(const json_value &Value)
	{
		return Value.type == json_string ? std::string_view(Value.u.string.ptr, Value.u.string.length) : std::string_view{};
	}

	inline int JsonNumber(const json_value &Value, int Fallback = -1)
	{
		return Value.type == json_integer && Value.u.integer >= 0 && Value.u.integer <= INT_MAX ? int(Value.u.integer) : Fallback;
	}

	inline CProfile ParseProfile(const json_value &Root, std::string_view Player, const char *pMap)
	{
		if(Root.type != json_object)
			return {};
		if(Root.u.object.length == 0)
			return {0};
		if(JsonString(Root["player"]) != Player)
			return {};
		CProfile Result;
		Result.m_Points = LucentScoreboard::ParsePoints(Root, Player);
		const auto &Types = Root["types"];
		if(Types.type != json_object)
			return Result;
		for(const auto &Type : Types.u.object)
		{
			const auto &Map = (*Type.value)["maps"][pMap];
			if(Map.type != json_object)
				continue;
			Result.m_MapPoints = JsonNumber(Map["points"]);
			Result.m_Rank = JsonNumber(Map["rank"], 0);
			const auto &Time = Map["time"];
			const double Seconds = Time.type == json_double ? Time.u.dbl : Time.type == json_integer ? double(Time.u.integer) : 0.0;
			if(std::isfinite(Seconds) && Seconds > 0)
				Result.m_BestSeconds = Seconds;
			break;
		}
		return Result;
	}

	inline CMap ParseMap(const json_value &Root, std::string_view Player, std::string_view Map, std::string_view Region)
	{
		if(JsonString(Root["name"]) != Map)
			return {};
		CMap Result;
		Result.m_Points = JsonNumber(Root["points"]);
		const auto &Ranks = Root["ranks"];
		if(Ranks.type != json_array)
			return Result;
		for(const auto *pRank : Ranks.u.array)
		{
			if(JsonString((*pRank)["player"]) == Player && (Region.empty() || JsonString((*pRank)["country"]).starts_with(Region)))
			{
				Result.m_Rank = JsonNumber((*pRank)["rank"], 0);
				break;
			}
		}
		return Result;
	}

	// The game server only confirms eligibility. Both point values come from DDNet.org.
	inline int ConfirmedAward(std::string_view Text)
	{
		constexpr std::string_view Prefix = "You earned ";
		if(!Text.starts_with(Prefix))
			return 0;
		Text.remove_prefix(Prefix.size());
		const int Points = TakeNumber(Text);
		if(Points <= 0)
			return 0;
		return Text == (Points == 1 ? " point for finishing this map!" : " points for finishing this map!") ? Points : 0;
	}
}

#endif
