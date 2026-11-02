#ifndef GAME_CLIENT_LUCENT_GAME_PING_WEBSOCKET_H
#define GAME_CLIENT_LUCENT_GAME_PING_WEBSOCKET_H
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace LucentPing
{
	struct CWebSocketMessage
	{
		bool m_Text = false;
		std::vector<uint8_t> m_Data;
	};
	// Used exclusively by the relay worker thread. Implementations use verified platform/library TLS.
	class IWebSocket
	{
	public:
		virtual ~IWebSocket() = default;
		virtual bool Connect(const std::string &Url) = 0;
		virtual bool Send(const uint8_t *pData, size_t Size, bool Text) = 0;
		// 1 = message, 0 = no data, -1 = closed/error. Max accepted message is 32 bytes.
		virtual int Poll(CWebSocketMessage &Message) = 0;
	};
	std::unique_ptr<IWebSocket> CreatePingWebSocket();
}
#endif
