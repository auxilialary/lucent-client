#include "ping_websocket.h"

#include <base/detect.h>

#include <algorithm>
#include <array>
#include <deque>
#include <mutex>
#include <thread>
#if defined(CONF_FAMILY_WINDOWS)
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
#if LIBCURL_VERSION_NUM >= 0x075600
#include <curl/websockets.h>
#endif
#endif
namespace LucentPing
{
	namespace
	{
#if defined(CONF_FAMILY_WINDOWS)
		class CPlatformWebSocket final : public IWebSocket
		{
			HINTERNET m_Session = nullptr, m_Connection = nullptr, m_Request = nullptr, m_Socket = nullptr;
			std::thread m_Reader;
			std::mutex m_Mutex;
			std::deque<CWebSocketMessage> m_Messages;
			bool m_Failed = false;
			void Read()
			{
				CWebSocketMessage Message;
				for(;;)
				{
					std::array<uint8_t, 33> Buffer{};
					DWORD Read = 0;
					WINHTTP_WEB_SOCKET_BUFFER_TYPE Type;
					if(WinHttpWebSocketReceive(m_Socket, Buffer.data(), DWORD(Buffer.size()), &Read, &Type) != NO_ERROR || Type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
						break;
					if(Message.m_Data.size() + Read > 32)
						break;
					const bool Text = Type == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE || Type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
					if(!Message.m_Data.empty() && Text != Message.m_Text)
						break;
					Message.m_Text = Text;
					Message.m_Data.insert(Message.m_Data.end(), Buffer.begin(), Buffer.begin() + Read);
					if(Type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE || Type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)
					{
						std::lock_guard Lock(m_Mutex);
						if(m_Messages.size() >= 64)
							break;
						m_Messages.push_back(std::move(Message));
						Message = {};
					}
				}
				std::lock_guard Lock(m_Mutex);
				m_Failed = true;
			}

		public:
			~CPlatformWebSocket() override
			{
				// Closing the handle cancels the reader; never called on the game/render thread.
				if(m_Socket)
				{
					WinHttpWebSocketShutdown(m_Socket, 1000, nullptr, 0);
					WinHttpCloseHandle(m_Socket);
				}
				if(m_Reader.joinable())
					m_Reader.join();
				if(m_Request)
					WinHttpCloseHandle(m_Request);
				if(m_Connection)
					WinHttpCloseHandle(m_Connection);
				if(m_Session)
					WinHttpCloseHandle(m_Session);
			}
			bool Connect(const std::string &Url) override
			{
				std::string Http = Url;
				const bool Secure = Http.starts_with("wss://");
				Http.replace(0, Secure ? 3 : 2, Secure ? "https" : "http");
				std::wstring Wide(Http.begin(), Http.end());
				URL_COMPONENTS Parts{};
				Parts.dwStructSize = sizeof(Parts);
				Parts.dwHostNameLength = Parts.dwUrlPathLength = Parts.dwExtraInfoLength = DWORD(-1);
				if(!WinHttpCrackUrl(Wide.c_str(), DWORD(Wide.size()), 0, &Parts))
					return false;
				std::wstring Host(Parts.lpszHostName, Parts.dwHostNameLength), Path(Parts.lpszUrlPath, Parts.dwUrlPathLength);
				if(Parts.dwExtraInfoLength)
					Path.append(Parts.lpszExtraInfo, Parts.dwExtraInfoLength);
				m_Session = WinHttpOpen(L"LucentMapPing/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
				if(!m_Session)
					return false;
				if(!WinHttpSetTimeouts(m_Session, 2000, 2000, 2000, 2000))
					return false;
				DWORD Tls = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
				if(!WinHttpSetOption(m_Session, WINHTTP_OPTION_SECURE_PROTOCOLS, &Tls, sizeof(Tls)))
					return false;
				m_Connection = WinHttpConnect(m_Session, Host.c_str(), Parts.nPort, 0);
				if(!m_Connection)
					return false;
				m_Request = WinHttpOpenRequest(m_Connection, L"GET", Path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, Secure ? WINHTTP_FLAG_SECURE : 0);
				if(!m_Request)
					return false;
				DWORD Policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
				if(!WinHttpSetOption(m_Request, WINHTTP_OPTION_REDIRECT_POLICY, &Policy, sizeof(Policy)) || !WinHttpSetOption(m_Request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0))
					return false;
				// Keep default certificate-chain/hostname verification; never set SECURITY_FLAG_IGNORE_*.
				if(!WinHttpSendRequest(m_Request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(m_Request, nullptr))
					return false;
				DWORD Status = 0, Length = sizeof(Status);
				if(!WinHttpQueryHeaders(m_Request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &Status, &Length, WINHTTP_NO_HEADER_INDEX) || Status != 101)
					return false;
				m_Socket = WinHttpWebSocketCompleteUpgrade(m_Request, 0);
				if(!m_Socket)
					return false;
				m_Reader = std::thread([this] { Read(); });
				return true;
			}
			bool Send(const uint8_t *pData, size_t Size, bool Text) override
			{
				return WinHttpWebSocketSend(m_Socket, Text ? WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE : WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, const_cast<uint8_t *>(pData), DWORD(Size)) == NO_ERROR;
			}
			int Poll(CWebSocketMessage &Message) override
			{
				std::lock_guard Lock(m_Mutex);
				if(!m_Messages.empty())
				{
					Message = std::move(m_Messages.front());
					m_Messages.pop_front();
					return 1;
				}
				return m_Failed ? -1 : 0;
			}
		};
#else
		class CPlatformWebSocket final : public IWebSocket
		{
			CURL *m_pCurl = nullptr;
			std::vector<uint8_t> m_Partial;

		public:
			~CPlatformWebSocket() override
			{
				if(m_pCurl)
					curl_easy_cleanup(m_pCurl);
			}
			bool Connect(const std::string &Url) override
			{
#if LIBCURL_VERSION_NUM >= 0x075600
				const auto *pVersion = curl_version_info(CURLVERSION_NOW);
				bool Supported = false;
				for(auto p = pVersion->protocols; *p; ++p)
					if(std::string(*p) == "wss")
						Supported = true;
				if(!Supported)
					return false;
				m_pCurl = curl_easy_init();
				if(!m_pCurl)
					return false;
				curl_easy_setopt(m_pCurl, CURLOPT_URL, Url.c_str());
				curl_easy_setopt(m_pCurl, CURLOPT_CONNECT_ONLY, 2L);
				curl_easy_setopt(m_pCurl, CURLOPT_SSL_VERIFYPEER, 1L);
				curl_easy_setopt(m_pCurl, CURLOPT_SSL_VERIFYHOST, 2L);
				curl_easy_setopt(m_pCurl, CURLOPT_FOLLOWLOCATION, 0L);
				curl_easy_setopt(m_pCurl, CURLOPT_NOSIGNAL, 1L);
				curl_easy_setopt(m_pCurl, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
				curl_easy_setopt(m_pCurl, CURLOPT_TIMEOUT_MS, 3000L);
				return curl_easy_perform(m_pCurl) == CURLE_OK;
#else
				return false;
#endif
			}
			bool Send(const uint8_t *pData, size_t Size, bool Text) override
			{
#if LIBCURL_VERSION_NUM >= 0x075600
				size_t Sent = 0; // Small fixed frames. Any partial/would-block send reconnects; never buffers unbounded data.
				return curl_ws_send(m_pCurl, pData, Size, &Sent, 0, Text ? CURLWS_TEXT : CURLWS_BINARY) == CURLE_OK && Sent == Size;
#else
				return false;
#endif
			}
			int Poll(CWebSocketMessage &Message) override
			{
#if LIBCURL_VERSION_NUM >= 0x075600
				std::array<uint8_t, 33> Buffer{};
				size_t Received = 0;
				const curl_ws_frame *pMeta = nullptr;
				auto Result = curl_ws_recv(m_pCurl, Buffer.data(), Buffer.size(), &Received, &pMeta);
				if(Result == CURLE_AGAIN)
					return 0;
				if(Result != CURLE_OK || !pMeta || pMeta->flags & CURLWS_CLOSE)
					return -1;
				if(pMeta->flags & (CURLWS_PING | CURLWS_PONG))
					return 0;
				if(m_Partial.size() + Received > 32)
					return -1;
				m_Partial.insert(m_Partial.end(), Buffer.begin(), Buffer.begin() + Received);
				if(pMeta->bytesleft || pMeta->flags & CURLWS_CONT)
					return 0;
				Message.m_Text = (pMeta->flags & CURLWS_TEXT) != 0;
				Message.m_Data = std::move(m_Partial);
				m_Partial.clear();
				return 1;
#else
				return -1;
#endif
			}
		};
#endif
	}
	std::unique_ptr<IWebSocket> CreatePingWebSocket() { return std::make_unique<CPlatformWebSocket>(); }
}
