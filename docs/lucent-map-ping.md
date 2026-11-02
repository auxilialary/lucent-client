# Local map pings: experimental direct UDP LAN MVP

Automatic opt-in relay is documented in [the relay service](../services/lucent-ping-relay/README.md).
This document describes the separate experimental host/join mode.

No socket exists before an explicit `lucent_ping_host` or `lucent_ping_join` command.
Both clients must already be online on the same DDNet server endpoint and loaded map
SHA256. Missing context is rejected. Map load, disconnect, reset, shutdown or a changed
server/map closes the ping transport; reconnect requires another explicit command.
The DDNet game socket, protocol, prediction and snapshots are not modified.

## Two clients

Connect both to the same DDNet server/map. In the first client's local console:

```
lucent_ping_host 18303
```

The default port is 18303. The host prints a fresh random 128-bit token as 32 hex
characters directly into the local console backlog (not through the logger).
Privately copy it to the second client's local console:

```
lucent_ping_join 127.0.0.1:18303 <32-hex-token>
lucent_ping_status
bind p lucent_ping
```

Use the host's LAN IP instead of 127.0.0.1 for another machine. IPv6 uses
`[address]:port`; only numeric addresses are accepted, avoiding blocking DNS.
The host also needs its own `bind p lucent_ping`. Commands are not sent to game chat.
Use `lucent_ping_status` on either side for state, last peer, counters and diagnostics;
it never prints credentials. `lucent_ping_leave` closes that side and notifies its peer.
Host/listener binds IPv4/IPv6; join uses a separate ephemeral UDP port. Two instances
on one PC need only the host to use 18303. A third participant cannot replace the peer.

## Limits and security boundary

This is plaintext UDP for loopback/LAN testing, **not a secure public Internet
transport**. The handshake token and connection bearer key can be observed on the
network; a token is not encryption or a MAC. Address pinning, random connection nonce,
sequence numbers and increasing event IDs reject unrelated sources, old sessions,
replays and duplicates, but do not protect against an on-path attacker. Keep tokens
out of config files, saved binds, game chat and shared console dumps. The program
keeps its console/history in memory; explicitly dumping the console can include
what the user typed and the displayed token. Tokens are not saved by the subsystem.

No signaling, ICE, STUN, TURN, UPnP, NAT traversal or relay is implemented. Host firewall
must permit the chosen UDP port. Across NAT a reachable address/port forwarding would
be required; CGNAT often prevents inbound connections. UDP delivery/order is not
guaranteed. Pings are intentionally not retransmitted; late/out-of-order events are
dropped. HELLO retries every 0.5 seconds, connect timeout is 5 seconds, heartbeat
interval is 1 second, and peer timeout is 8 seconds. After disconnect/error the
socket closes; hosting/joining again rotates credentials. Listening is idle until a
peer joins or the user leaves/game context ends.

Ping send/receive rate is 4 per second with a burst of 4; excess local pings still
render locally. HELLO responses have the same global bound. Each update consumes at
most 64 datagrams, receive queue is at most 8, and the shared local/remote manager
retains the newest 8 pings. Incoming lifetime starts at local monotonic receive time;
incoming events never call SendPing. All local and remote markers use the existing
renderer/geometry, 4-second lifetime, animations and fade without visual changes.

`IPingTransport` owns the narrow host/join/update/send/receive/status boundary for a
future secure ICE implementation. Direct UDP has no renderer or CGameClient access.

## Wire format (version 2)

All datagrams are exactly 128 bytes, no native struct layout or timestamps. Integers
and IEEE-754 float32 coordinate bits use big-endian byte order.

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | ASCII LPNG |
| 4 | 1 | version 2 |
| 5 | 1 | HELLO=1, ACCEPT=2, REJECT=3, PING=4, HEARTBEAT=5, DISCONNECT=6 |
| 6 | 2 | claimed DDNet Client ID + 1 for PING (1..128); zero for other types |
| 8 | 16 | HELLO token; ACCEPT/session packets use a fresh connection bearer key; REJECT zero |
| 24 | 16 | random join nonce, echoed in replies and session packets |
| 40 | 32 | SHA256 of canonical DDNet server numeric address including port |
| 72 | 32 | loaded map SHA256 |
| 104 | 8 | nonzero sender sequence, monotonically increasing |
| 112 | 8 | nonzero event ID for PING, zero otherwise |
| 120 | 4 | world X for PING, zero otherwise |
| 124 | 4 | world Y for PING, zero otherwise |

Coordinates must be finite and within +/-16777216 world units. Context, exact source
IP/port, nonce, connection key, sequence and event ID are checked before enqueueing.
Unrecognized versions/types, wrong length/sender fields and invalid numbers are
never added to the manager. Duplicate HELLO may resend a lost ACCEPT to the pinned
peer but never refreshes liveness or creates another connection.

## Automated checks

Build only through `scripts/build-lucent-vs.ps1` with `game-client`, `testrunner` and
available `lucent-*-test` targets in the existing saved x64-Release tree. Run
`testrunner.exe --gtest_filter=LucentPingTransport.*` for real separate UDP loopback
sockets with a deterministic monotonic test clock (no waiting several seconds for
timeouts), wire validation, compatibility/replay/source checks, flood/queue limits,
heartbeat, retry, disconnect/reconnect and context cleanup. This is not evidence of
a test between two physical computers or a GUI game session.

Sender identity uses protocol v2: PING carries a claimed DDNet Client ID (0..127),
never names/skins. Local snapshots supply the visible name and RenderTee info.
Absent players get no invented identity. Client ID is not authenticated by DDNet
or this transport. Upgrade both direct-UDP clients together; v1 handshakes are
rejected. For relay v2 the public Worker also needs manual deployment (see service README).
