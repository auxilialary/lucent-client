#include "ping_relay.h"

#include "ping_websocket.h"

#include <base/time.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
namespace LucentPing
{
	namespace
	{
		double Now() { return double(time_get()) / time_freq(); }
		std::string Hex(const std::array<uint8_t, 32> &Bytes)
		{
			const char *pHex = "0123456789abcdef";
			std::string S;
			for(auto B : Bytes)
			{
				S += pHex[B >> 4];
				S += pHex[B & 15];
			}
			return S;
		}
	}
	class CRelayTransport::CImpl
	{
	public:
		mutable std::mutex m_Mutex;
		std::condition_variable m_Changed;
		bool m_Stop = false, m_Enabled = false, m_AllowLoopbackTest;
		uint64_t m_Generation = 0;
		std::string m_Endpoint;
		std::optional<CContext> m_Context;
		CRelayStatus m_Status;
		std::deque<CRelayFrame> m_Send;
		std::deque<CReceivedPing> m_Receive;
		double m_SendTokens = 4, m_SendTime = 0;
		std::thread m_Thread;
		explicit CImpl(bool Allow) : m_AllowLoopbackTest(Allow), m_Thread([this] { Run(); }) {}
		~CImpl()
		{
			{
				std::lock_guard Lock(m_Mutex);
				m_Stop = true;
				++m_Generation;
			}
			m_Changed.notify_all();
			m_Thread.join();
		}
		bool Current(uint64_t Generation) const { return !m_Stop && Generation == m_Generation; }
		void Run()
		{
			std::unique_lock Lock(m_Mutex);
			while(!m_Stop)
			{
				m_Changed.wait(Lock, [this] { return m_Stop || (m_Enabled && m_Context && m_Context->Valid() && ValidRelayEndpoint(m_Endpoint, m_AllowLoopbackTest)); });
				if(m_Stop)
					break;
				const uint64_t Generation = m_Generation;
				const auto Context = *m_Context;
				const auto Url = m_Endpoint + "?server=" + Hex(Context.m_Server) + "&map=" + Hex(Context.m_Map);
				unsigned Attempt = 0;
				while(Current(Generation))
				{
					m_Status.m_State = ETransportState::Connecting;
					m_Status.m_Participants = 0;
					m_Status.m_Detail = "Connecting to relay";
					Lock.unlock();
					auto Socket = CreatePingWebSocket();
					const bool Connected = Socket->Connect(Url);
					Lock.lock();
					const double Start = Now();
					double LastReceive = Start, LastKeepalive = Start, InTime = Start, InTokens = 8;
					std::unordered_map<uint64_t, uint64_t> LastIds;
					bool Alive = Connected;
					while(Current(Generation) && Alive)
					{
						if(!m_Send.empty() && m_Status.m_State == ETransportState::Connected)
						{
							const auto Bytes = EncodeRelay(m_Send.front());
							m_Send.pop_front();
							Lock.unlock();
							const bool Sent = Socket->Send(Bytes.data(), Bytes.size(), false);
							Lock.lock();
							if(!Current(Generation))
								break;
							if(!Sent)
							{
								Alive = false;
								break;
							}
							++m_Status.m_Sent;
						}
						for(int i = 0; i < 64 && Current(Generation); ++i)
						{
							CWebSocketMessage Message;
							Lock.unlock();
							const int Result = Socket->Poll(Message);
							Lock.lock();
							if(!Current(Generation))
								break;
							if(Result < 0)
							{
								Alive = false;
								break;
							}
							if(!Result)
								break;
							const double Received = Now();
							if(Message.m_Text)
							{
								if(Message.m_Data != std::vector<uint8_t>({'l', 'p', '2'}))
								{
									Alive = false;
									break;
								}
								LastReceive = Received;
								continue;
							}
							CRelayFrame Frame;
							if(!DecodeRelay(Message.m_Data.data(), Message.m_Data.size(), Frame))
							{
								++m_Status.m_Dropped;
								Alive = false;
								break;
							}
							LastReceive = Received;
							if(Frame.m_Type == 2)
							{
								m_Status.m_Participants = unsigned(Frame.m_Id);
								m_Status.m_State = ETransportState::Connected;
								m_Status.m_Detail = "Relay connected (group claims are not identity)";
								continue;
							}
							if(!Frame.m_Sender)
							{
								++m_Status.m_Dropped;
								continue;
							}
							auto It = LastIds.find(Frame.m_Sender);
							if(It != LastIds.end() && Frame.m_Id <= It->second)
							{
								++m_Status.m_Dropped;
								continue;
							}
							if(LastIds.size() >= 128 && It == LastIds.end())
								LastIds.erase(LastIds.begin());
							LastIds[Frame.m_Sender] = Frame.m_Id;
							InTokens = std::min(8.0, InTokens + std::max(0.0, Received - InTime) * 8);
							InTime = Received;
							if(InTokens < 1 || m_Receive.size() >= 8)
							{
								++m_Status.m_Dropped;
								continue;
							}
							--InTokens;
							m_Receive.push_back({Frame.m_Id, Frame.m_Position, Received, Frame.m_SenderClientId});
							++m_Status.m_Received;
						}
						if(!Current(Generation) || !Alive)
							break;
						const double Time = Now();
						if(Time - LastReceive > 45 || (m_Status.m_State != ETransportState::Connected && Time - Start > 5))
						{
							Alive = false;
							break;
						}
						if(Time - LastKeepalive >= 15)
						{
							Lock.unlock();
							const bool Sent = Socket->Send(reinterpret_cast<const uint8_t *>("lp2"), 3, true);
							Lock.lock();
							LastKeepalive = Time;
							if(!Sent)
								Alive = false;
						}
						m_Changed.wait_for(Lock, std::chrono::milliseconds(20), [&] { return !Current(Generation); });
					}
					Lock.unlock();
					Socket.reset();
					Lock.lock();
					if(!Current(Generation))
						break;
					m_Send.clear();
					m_Receive.clear();
					m_Status.m_Participants = 0;
					m_Status.m_State = ETransportState::Error;
					++m_Status.m_Reconnects;
					m_Status.m_Detail = "Relay v2 failed/closed; retrying (service must support /v2; check TLS/rate limits)";
					if(Connected && Now() - Start >= 10)
						Attempt = 0;
					const double Delay = RelayReconnectDelay(Attempt++);
					m_Changed.wait_for(Lock, std::chrono::duration<double>(Delay), [&] { return !Current(Generation); });
				}
			}
		}
	};
	CRelayTransport::CRelayTransport(bool AllowLoopbackTest) : m_pImpl(std::make_unique<CImpl>(AllowLoopbackTest)) {}
	CRelayTransport::~CRelayTransport() = default;
	void CRelayTransport::Configure(bool Enabled, const std::string &Endpoint, const std::optional<CContext> &Context)
	{
		auto &I = *m_pImpl;
		std::lock_guard Lock(I.m_Mutex);
		if(I.m_Enabled == Enabled && I.m_Endpoint == Endpoint && I.m_Context == Context)
			return;
		I.m_Enabled = Enabled;
		I.m_Endpoint = Endpoint;
		I.m_Context = Context;
		++I.m_Generation;
		I.m_Send.clear();
		I.m_Receive.clear();
		I.m_SendTokens = 4;
		I.m_SendTime = Now();
		I.m_Status.m_Participants = 0;
		I.m_Status.m_State = ETransportState::Disconnected;
		I.m_Status.m_Detail = !Enabled ? "Disabled" : !Context || !Context->Valid() ? "Waiting for online game/map SHA256" :
											      "Waiting for relay connection";
		if(Enabled && !ValidRelayEndpoint(Endpoint, I.m_AllowLoopbackTest))
		{
			I.m_Status.m_State = ETransportState::Error;
			I.m_Status.m_Detail = "Set cl_lucent_ping_relay_url to the deployed wss://host/v2 endpoint";
		}
		I.m_Changed.notify_all();
	}
	bool CRelayTransport::SendPing(uint64_t Id, vec2 Position, int SenderClientId)
	{
		auto &I = *m_pImpl;
		std::lock_guard Lock(I.m_Mutex);
		if(!I.m_Enabled || I.m_Status.m_State != ETransportState::Connected || !Id || !ValidSenderClientId(SenderClientId) || !ValidPosition(Position) || I.m_Send.size() >= 8)
			return false;
		const double Time = Now();
		I.m_SendTokens = std::min(4.0, I.m_SendTokens + std::max(0.0, Time - I.m_SendTime) * 4);
		I.m_SendTime = Time;
		if(I.m_SendTokens < 1)
			return false;
		--I.m_SendTokens;
		I.m_Send.push_back({1, 0, Id, Position, SenderClientId});
		I.m_Changed.notify_all();
		return true;
	}
	bool CRelayTransport::Receive(CReceivedPing &Ping)
	{
		auto &I = *m_pImpl;
		std::lock_guard Lock(I.m_Mutex);
		if(I.m_Receive.empty())
			return false;
		Ping = I.m_Receive.front();
		I.m_Receive.pop_front();
		return true;
	}
	CRelayStatus CRelayTransport::Status() const
	{
		std::lock_guard Lock(m_pImpl->m_Mutex);
		return m_pImpl->m_Status;
	}
}
