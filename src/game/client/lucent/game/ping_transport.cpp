#include "ping_transport.h"

#include <base/secure.h>

#include <algorithm>
#include <deque>
#include <limits>
namespace LucentPing
{
	namespace
	{
		struct CBucket
		{
			double m_Tokens = 4, m_Last = 0;
			void Reset(double Now)
			{
				m_Tokens = 4;
				m_Last = Now;
			}
			bool Take(double Now)
			{
				m_Tokens = std::min(4.0, m_Tokens + std::max(0.0, Now - m_Last) * 4);
				m_Last = Now;
				if(m_Tokens < 1)
					return false;
				--m_Tokens;
				return true;
			}
		};
		class CDirectPingTransport final : public IPingTransport
		{
			NETSOCKET m_Socket = nullptr;
			CTransportStatus m_Status;
			CContext m_Context;
			CSecret m_Token{}, m_Key{}, m_Nonce{};
			bool m_Host = false;
			uint64_t m_SendSequence = 0, m_RecvSequence = 0, m_LastEvent = 0;
			double m_Started = 0, m_LastReceive = 0, m_LastSend = 0, m_LastHello = 0;
			CBucket m_Incoming, m_Outgoing, m_Replies;
			std::deque<CReceivedPing> m_Received;
			void Close(ETransportState State, const char *pDetail)
			{
				if(m_Socket)
					net_udp_close(m_Socket);
				m_Socket = nullptr;
				m_Token.fill(0);
				m_Key.fill(0);
				m_Nonce.fill(0);
				m_Received.clear();
				m_Status.m_State = State;
				m_Status.m_pDetail = pDetail;
			}
			bool Open(NETADDR Bind, const CContext &Context, double Now)
			{
				Leave();
				m_Status = {};
				m_Context = Context;
				if(!Context.Valid())
				{
					Close(ETransportState::Error, "Missing game server/map SHA256");
					return false;
				}
				if(Bind.type != NETTYPE_IPV4 && Bind.type != NETTYPE_IPV6 && Bind.type != (NETTYPE_IPV4 | NETTYPE_IPV6))
				{
					Close(ETransportState::Error, "Unsupported address family");
					return false;
				}
				m_Socket = net_udp_create(Bind);
				if(!m_Socket)
				{
					Close(ETransportState::Error, "Cannot open UDP socket");
					return false;
				}
				// net_udp_create already sets non-blocking mode; verify it explicitly.
				if(net_set_non_blocking(m_Socket) != 0)
				{
					Close(ETransportState::Error, "Cannot set non-blocking UDP");
					return false;
				}
				m_SendSequence = m_RecvSequence = m_LastEvent = 0;
				m_Host = false;
				m_Started = m_LastReceive = m_LastSend = Now;
				m_LastHello = Now - 1;
				m_Incoming.Reset(Now);
				m_Outgoing.Reset(Now);
				m_Replies.Reset(Now);
				return true;
			}
			bool Send(EPacketType Type, const NETADDR &Peer, const CSecret &Auth, const CSecret &Nonce, double Now, uint64_t Id = 0, vec2 Position = vec2(0, 0), int SenderClientId = -1)
			{
				if(!m_Socket || m_SendSequence == std::numeric_limits<uint64_t>::max())
					return false;
				CPacket Packet;
				Packet.m_Type = Type;
				Packet.m_Auth = Auth;
				Packet.m_Nonce = Nonce;
				Packet.m_Context = m_Context;
				Packet.m_Sequence = ++m_SendSequence;
				Packet.m_EventId = Id;
				Packet.m_Position = Position;
				Packet.m_SenderClientId = SenderClientId;
				const auto Wire = Encode(Packet);
				if(net_udp_send(m_Socket, &Peer, Wire.data(), int(Wire.size())) != int(Wire.size()))
					return false;
				m_LastSend = Now;
				return true;
			}
			void Handle(const NETADDR &From, const uint8_t *pData, int Size, double Now)
			{
				CPacket P;
				const auto Result = Decode(pData, size_t(Size), P);
				if(Result == EDecode::Invalid)
				{
					++m_Status.m_Dropped;
					return;
				}
				const bool SamePeer = net_addr_comp(&From, &m_Status.m_Peer) == 0;
				if(m_Status.m_State == ETransportState::Listening && P.m_Type == EPacketType::Hello)
				{
					if(!m_Replies.Take(Now))
					{
						++m_Status.m_Dropped;
						return;
					}
					if(Result != EDecode::Ok || P.m_Context != m_Context || !SecretEqual(P.m_Auth, m_Token))
					{
						Send(EPacketType::Reject, From, {}, P.m_Nonce, Now);
						++m_Status.m_Dropped;
						return;
					}
					m_Status.m_Peer = From;
					m_Nonce = P.m_Nonce;
					secure_random_fill(m_Key.data(), unsigned(m_Key.size()));
					m_RecvSequence = P.m_Sequence;
					m_LastReceive = Now;
					m_Status.m_State = ETransportState::Connected;
					m_Status.m_pDetail = "LAN UDP peer connected";
					Send(EPacketType::Accept, From, m_Key, m_Nonce, Now);
					return;
				}
				if(!SamePeer || Result != EDecode::Ok || !SecretEqual(P.m_Nonce, m_Nonce))
				{
					++m_Status.m_Dropped;
					return;
				}
				if(m_Status.m_State == ETransportState::Connecting)
				{
					if(P.m_Type == EPacketType::Reject)
					{
						Close(ETransportState::Error, "Handshake rejected (token/version/game context)");
						return;
					}
					if(P.m_Type != EPacketType::Accept || P.m_Context != m_Context || P.m_Auth == CSecret{})
					{
						++m_Status.m_Dropped;
						return;
					}
					m_Key = P.m_Auth;
					m_RecvSequence = P.m_Sequence;
					m_LastReceive = Now;
					m_Status.m_State = ETransportState::Connected;
					m_Status.m_pDetail = "LAN UDP peer connected";
					return;
				}
				if(m_Status.m_State != ETransportState::Connected)
					return;
				if(P.m_Context != m_Context)
				{
					++m_Status.m_Dropped;
					return;
				}
				// Repeat ACCEPT when the original was lost. HELLO replay never extends peer liveness.
				if(m_Host && P.m_Type == EPacketType::Hello && SecretEqual(P.m_Auth, m_Token))
				{
					if(m_Replies.Take(Now))
						Send(EPacketType::Accept, From, m_Key, m_Nonce, Now);
					return;
				}
				if(!SecretEqual(P.m_Auth, m_Key) || P.m_Sequence <= m_RecvSequence ||
					(P.m_Type != EPacketType::Ping && P.m_Type != EPacketType::Heartbeat && P.m_Type != EPacketType::Disconnect))
				{
					++m_Status.m_Dropped;
					return;
				}
				m_RecvSequence = P.m_Sequence;
				if(P.m_Type == EPacketType::Disconnect)
				{
					Close(ETransportState::Disconnected, "Peer left");
					return;
				}
				if(P.m_Type == EPacketType::Ping)
				{
					if(P.m_EventId <= m_LastEvent)
					{
						++m_Status.m_Dropped;
						return;
					}
					m_LastEvent = P.m_EventId;
					if(!m_Incoming.Take(Now) || m_Received.size() >= 8)
					{
						++m_Status.m_Dropped;
						return;
					}
					m_Received.push_back({P.m_EventId, P.m_Position, Now, P.m_SenderClientId});
					++m_Status.m_ReceivedPings;
				}
				m_LastReceive = Now;
			}

		public:
			~CDirectPingTransport() override { Leave(); }
			bool Host(NETADDR Bind, const CContext &Context, double Now) override
			{
				if(!Bind.port)
				{
					Leave();
					Close(ETransportState::Error, "Host port must be 1..65535");
					return false;
				}
				if(!Open(Bind, Context, Now))
					return false;
				m_Host = true;
				secure_random_fill(m_Token.data(), unsigned(m_Token.size()));
				m_Status.m_ListenPort = Bind.port;
				m_Status.m_State = ETransportState::Listening;
				m_Status.m_pDetail = "Waiting for one LAN peer";
				return true;
			}
			bool Join(NETADDR Peer, const CSecret &Token, const CContext &Context, double Now) override
			{
				if(!Peer.port || (Peer.type != NETTYPE_IPV4 && Peer.type != NETTYPE_IPV6))
				{
					Leave();
					Close(ETransportState::Error, "Expected numeric IPv4/IPv6 address:port");
					return false;
				}
				NETADDR Bind{};
				Bind.type = Peer.type;
				if(!Open(Bind, Context, Now))
					return false;
				m_Status.m_Peer = Peer;
				m_Token = Token;
				secure_random_fill(m_Nonce.data(), unsigned(m_Nonce.size()));
				m_Status.m_State = ETransportState::Connecting;
				m_Status.m_pDetail = "Waiting for ACCEPT";
				Send(EPacketType::Hello, Peer, m_Token, m_Nonce, Now);
				m_LastHello = Now;
				return true;
			}
			void Leave() override
			{
				if(m_Socket && m_Status.m_State == ETransportState::Connected)
					Send(EPacketType::Disconnect, m_Status.m_Peer, m_Key, m_Nonce, m_LastSend);
				Close(ETransportState::Disconnected, "Closed");
			}
			void Update(const CContext *pContext, double Now) override
			{
				if(!m_Socket)
					return;
				if(!pContext || !pContext->Valid() || *pContext != m_Context)
				{
					Leave();
					m_Status.m_pDetail = "Game session ended or changed";
					return;
				}
				// Bounded work and bounded queues even under an arbitrary UDP flood.
				for(int i = 0; i < 64 && m_Socket; ++i)
				{
					NETADDR From{};
					unsigned char *pData = nullptr;
					const int Size = net_udp_recv(m_Socket, &From, &pData);
					if(Size < 0)
						break;
					if(Size == 0)
						continue;
					if(Size != int(PacketSize))
					{
						++m_Status.m_Dropped;
						continue;
					}
					Handle(From, pData, Size, Now);
				}
				if(!m_Socket)
					return;
				if(net_udp_is_broken(m_Socket))
				{
					Close(ETransportState::Error, "UDP socket failed");
					return;
				}
				if(m_Status.m_State == ETransportState::Connecting)
				{
					if(Now - m_Started >= 5)
					{
						Close(ETransportState::Error, "Connection timeout");
						return;
					}
					if(Now - m_LastHello >= .5)
					{
						Send(EPacketType::Hello, m_Status.m_Peer, m_Token, m_Nonce, Now);
						m_LastHello = Now;
					}
				}
				if(m_Status.m_State == ETransportState::Connected)
				{
					if(Now - m_LastReceive >= 8)
					{
						Close(ETransportState::Error, "Peer heartbeat timeout");
						return;
					}
					if(Now - m_LastSend >= 1)
						Send(EPacketType::Heartbeat, m_Status.m_Peer, m_Key, m_Nonce, Now);
				}
			}
			bool SendPing(uint64_t Id, vec2 Position, double Now, int SenderClientId) override
			{
				if(m_Status.m_State != ETransportState::Connected || !Id || !ValidSenderClientId(SenderClientId) || !ValidPosition(Position) || !m_Outgoing.Take(Now))
					return false;
				if(!Send(EPacketType::Ping, m_Status.m_Peer, m_Key, m_Nonce, Now, Id, Position, SenderClientId))
					return false;
				++m_Status.m_SentPings;
				return true;
			}
			bool Receive(CReceivedPing &Ping) override
			{
				if(m_Received.empty())
					return false;
				Ping = m_Received.front();
				m_Received.pop_front();
				return true;
			}
			CTransportStatus Status() const override { return m_Status; }
			CSecret HostToken() const override { return m_Host && m_Status.m_State == ETransportState::Listening ? m_Token : CSecret{}; }
		};
	}
	const char *TransportStateName(ETransportState State)
	{
		switch(State)
		{
		case ETransportState::Disconnected: return "Disconnected";
		case ETransportState::Listening: return "Listening";
		case ETransportState::Connecting: return "Connecting";
		case ETransportState::Connected: return "Connected";
		case ETransportState::Error: return "Error";
		}
		return "Error";
	}
	std::unique_ptr<IPingTransport> CreateDirectPingTransport() { return std::make_unique<CDirectPingTransport>(); }
}
