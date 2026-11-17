#ifndef GAME_CLIENT_LUCENT_GAME_CHAT_POLICY_H
#define GAME_CLIENT_LUCENT_GAME_CHAT_POLICY_H

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace LucentChat
{
	enum class ECategory
	{
		PLAYER,
		SYSTEM,
		WELCOME,
		RACE,
		CONNECTION
	};
	struct CMessageRule
	{
		ECategory m_Category;
		std::string_view m_Fragment;
	};
	// The filter and its context menu share this exhaustive list of recognized patterns.
	inline constexpr CMessageRule Rules[] = {
		{ECategory::CONNECTION, "entered and joined"},
		{ECategory::CONNECTION, "has left the game"},
		{ECategory::CONNECTION, "joined the game"},
		{ECategory::CONNECTION, "changed their name to"},
		{ECategory::CONNECTION, "has joined the"},
		{ECategory::CONNECTION, "has left the team"},
		{ECategory::RACE, "good luck"},
		{ECategory::RACE, "enjoy your race"},
		{ECategory::RACE, "current race time"},
		{ECategory::RACE, "you have been racing for"},
		{ECategory::WELCOME, "welcome"},
		{ECategory::WELCOME, "this server runs"},
		{ECategory::WELCOME, "please visit"},
		{ECategory::WELCOME, "type /help"},
		{ECategory::WELCOME, "use /help"},
	};
	inline ECategory Classify(int ClientId, std::string_view Message)
	{
		if(ClientId >= 0)
			return ECategory::PLAYER;
		std::string Text(Message);
		std::transform(Text.begin(), Text.end(), Text.begin(), [](unsigned char C) { return (char)std::tolower(C); });
		for(const auto &Rule : Rules)
			if(Text.find(Rule.m_Fragment) != std::string::npos)
				return Rule.m_Category;
		return ECategory::SYSTEM;
	}
	inline float Opacity(double Age)
	{
		const float T = (float)std::clamp(std::min(Age / .25, (4.8 - Age) / .4), 0.0, 1.0);
		return T * T * (3 - 2 * T);
	}
}
#endif
