#include <game/client/lucent/game/chat_policy.h>
#include <game/client/lucent/settings/workspace.h>

#include <cmath>
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
	using namespace LucentChat;
	using C = ECategory;
	for(const auto &Rule : Rules)
	{
		const std::string Message = "prefix " + std::string(Rule.m_Fragment) + " suffix";
		CHECK(Classify(-1, Message) == Rule.m_Category);
		CHECK(Classify(0, Message) == C::PLAYER);
		std::string Upper = Message;
		std::transform(Upper.begin(), Upper.end(), Upper.begin(), [](unsigned char Ch) { return (char)std::toupper(Ch); });
		CHECK(Classify(-1, Upper) == Rule.m_Category);
	}
	CHECK(Classify(-1, "Good luck in your race!") == C::RACE);
	CHECK(Classify(-1, "GOOD LUCK IN YOUR RACE!") == C::RACE);
	CHECK(Classify(-1, "You have been racing for 10 minutes") == C::RACE);
	CHECK(Classify(-1, "Welcome to DDNet!") == C::WELCOME);
	CHECK(Classify(-1, "Type /help for commands") == C::WELCOME);
	CHECK(Classify(-1, "'Welcome' entered and joined the game") == C::CONNECTION);
	CHECK(Classify(-1, "'Good luck' has left the game") == C::CONNECTION);
	CHECK(Classify(-1, "Team save failed") == C::SYSTEM);
	CHECK(Classify(-2, "Connected to server") == C::SYSTEM);
	CHECK(Classify(0, "Welcome! Good luck in your race!") == C::PLAYER);
	CHECK(Classify(63, "has left the game") == C::PLAYER);
	CHECK(Classify(-1, "") == C::SYSTEM);
	CHECK(Opacity(-1) == 0 && Opacity(0) == 0);
	CHECK(Opacity(.25) == 1 && Opacity(4.0) == 1);
	CHECK(Opacity(4.8) == 0 && Opacity(5) == 0);
	CHECK(std::abs(Opacity(.125) - .5f) < .001f);
	CHECK(std::abs(Opacity(4.6) - .5f) < .001f);
	for(double T = 0; T < 5; T += .01)
		CHECK(Opacity(T) >= 0 && Opacity(T) <= 1);
	int Workspace = LucentSettings::WorkspaceLucent, Page = LucentSettings::PAGE_CHAT, DDNet = 6;
	LucentSettings::NormalizeWorkspace(Workspace, Page, DDNet);
	CHECK(Page == LucentSettings::PAGE_CHAT && DDNet == 6);
	CHECK(LucentSettings::PAGE_BINDWHEEL == 5);
	std::puts("Chat classification, player-message preservation, fade timing and settings persistence passed");
}
