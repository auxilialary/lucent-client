# Automatic Lucent map ping relay

Cloudflare Worker + Durable Object; no VPS, player accounts, session tokens or event
database. Public Lucent relay: `https://lucent-ping-relay.apolemia.workers.dev`.
The client defaults to `wss://lucent-ping-relay.apolemia.workers.dev/v2`, matching
Worker's `/v2` WebSocket route. `/health` is its HTTP diagnostic route.
The existing deployment was supplied by the operator; no deployment is performed here.

## Required public-service update for sender identity

**The Worker must be manually redeployed before v2 clients can exchange pings on the
public service. No automatic deployment was performed.** Existing v1 deployment
cannot accept the new sender field. The new Worker serves `/v2` with `LPR2`, places
rooms under `v2:<server>:<map>`, and rejects `/v1` with HTTP 426 and an upgrade message.
The client default now points to the same public hostname at `/v2`. Explicit `/v1`
overrides must be changed/reset after updating. Direct UDP version also becomes 2
(the datagram remains 128 bytes). No new DO migration is needed: the existing `v1`
migration tag names the namespace/class, not the ping protocol version.

Client ID is a **self-reported slot**, not an authenticated identity. Valid-slot
spoofing cannot be prevented by the relay; no identity-verification badge is shown.
All endpoint/map checks, connection sender IDs, replay/rate/size limits remain.

## Initial deployment (operator)

1. Install Node.js 22+; run `npm ci` and `npm test` in this directory.
2. Set a unique Worker `name` in `wrangler.toml`. Choose `CONNECT_RATE.namespace_id`
   unique to this service in your account (template: `1001`). Confirm account
   Workers/Durable Objects availability, quotas and billing before deploying.
3. Run `npx wrangler login` and `npx wrangler whoami` yourself to verify your account.
   No account ID, API token or secret is embedded in the repository.
4. Run `npm run deploy` yourself. Wrangler applies the `v1` SQLite DO class migration
   and prints the actual HTTPS hostname. No event tables or storage calls are used.
5. Use the real hostname as `wss://<deployed-host>/v2`. This is a placeholder, not a
   working service address. `/health` is available for diagnostics.
6. Ordinary Lucent clients already use the public endpoint by default and require no
   URL configuration. `cl_lucent_ping_relay_url` remains an optional override for
   operators/private deployments. Valid TLS is required; custom domains are optional.

No deployment, Cloudflare login or account change was performed during implementation.
`npm exec wrangler deploy -- --dry-run` validates packaging locally without publishing.

## Player commands

Connect to a DDNet server; no manual URL, IP, port or token is required:

```
lucent_ping_online 1
lucent_ping_status
bind p lucent_ping
```

Clients claiming the same DDNet endpoint and loaded map SHA256 automatically join
the same group. Status includes this client in the participant count. Players do
not exchange IPs, ports, nicknames or tokens. In this Lucent test build, online
mode defaults to enabled. `lucent_ping_online` is a standard `CFGFLAG_SAVE` config
variable: `0` disables and persists across normal restarts; `1` re-enables and persists.
The existing DDNet config manager saves it with other settings at normal shutdown.
If global settings saving is deliberately disabled, the normal DDNet rule applies.
Lifecycle resets, map changes and shutdown close the socket without overwriting the
preference. The endpoint remains a separate config variable; no Settings UI was added.

`lucent_ping_online 0` disables exchange, clears network queues and remote markers,
and preserves local markers. Map/server changes or disconnect invalidate the old
connection generation and queues. While online mode remains enabled, a new valid
game context automatically connects to its group. Old-generation messages are
ignored. Remote pings are never forwarded back. The existing manager, combined
8-marker cap, 4-second lifetime, fade, animations and off-screen distance are reused.
Remote markers additionally show a compact native RenderTee and width-limited name
from the current snapshot. Local markers have no identity label. Off-screen labels
stack along screen edges without rotating the original direction vector.

Experimental direct UDP commands remain documented in `../../docs/lucent-map-ping.md`.
Host/join explicitly disable relay; enabling relay closes UDP. No transport bridge.

## Architecture and protocol

Worker validates `/v2?server=<64 lowercase hex>&map=<64 lowercase hex>`, both hashes
nonzero. Server hash is SHA256 of canonical numeric DDNet address including port;
map hash comes from the loaded map. One DO per pair uses `ctx.acceptWebSocket` and
`webSocketMessage/Close/Error`. `serializeAttachment` retains random connection ID,
last event ID and rate counters across hibernation; no coordinates/history are
persisted. The `lp2` auto-response keepalive does not wake the JavaScript object.
Connections are identified by random IDs, never nicknames.
Ping v2 also carries the claimed DDNet Client ID, validated to 0..127. Receivers
resolve the current name, colors and 0.6/0.7 render info locally; no names/skins
are transmitted. Missing snapshot players keep a neutral marker without a name.
Dummy uses the controlled local slot; spectator uses its own ID, not the followed Tee.

Binary messages are exactly 32 bytes. Integers and IEEE float32 bits are big-endian:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | ASCII LPR2, protocol version 2 |
| 4 | 1 | Ping=1, participant count=2 |
| 5 | 1 | Reserved zero |
| 6 | 2 | DDNet Client ID + 1 (1..128) for ping; zero for participant count |
| 8 | 8 | Sender: client zero, relay replaces with random connection ID |
| 16 | 8 | Nonzero ping event ID; or participant count 1..64 |
| 24 | 4 | World X float32, zero for count |
| 28 | 4 | World Y float32, zero for count |

Coordinates must be finite within +/-16777216 world units. Event IDs must increase
per connection; duplicates/replays are dropped. Keepalive is exact text `lp2`, not
broadcast. Delivery has no history, offline queue, acknowledgement or retransmission
on reconnect. The relay sends only to other live connections in the group.

## TLS, threading and limits

Windows uses system WinHTTP WebSocket with TLS 1.2 and default certificate-chain and
hostname verification, with redirects disabled. All network I/O, DNS and cleanup run
off the game/render thread. Public methods only access bounded queues. A worker
owns connection/reconnect and a WinHTTP reader handles blocking receive; closing
the socket cancels the reader. Other platforms use libcurl WebSocket API and require
a build with WSS support; unsupported libraries fail closed with diagnostics. That
branch was not runtime-tested here. Bundled Windows curl lacks WS and is not used.
There is no custom TLS implementation or certificate bypass.

Backoff: 1, 2, 4, 8, 16, maximum 30 seconds; resets after a stable connection.
Heartbeat every 15 seconds, receive timeout 45 seconds. Disable/context changes cancel
reconnect and clear queues immediately; an in-flight OS connect finishes/cleans up on
the worker with short timeouts. No joins or socket I/O occur in frame callbacks.

- Edge admission: 60 connection attempts/minute per hashed IP, across groups.
- At most 64 participants/group and 4 connections/IP/group.
- Per connection: 4 ping attempts/second, burst 4; sustained flood closes socket.
- Group: budget 32 broadcasts/second across active connections.
- Native outbound: 4/second burst 4; inbound: 8/second burst 8, queues capped at 8.
- Native reader: at most 64 queued frames, maximum message 32 bytes.

Routing claims do **not** prove player identity or actual DDNet server membership.
Anyone knowing the hashes can join and send rate-limited false pings. TLS protects
the connection to the relay, not the authenticity of those claims or confidentiality
from its operator. The operator sees live coordinates and network addresses. No
nickname, skin, chat or other game traffic is sent. Edge limiting is approximate and per
Cloudflare location; distributed abuse is not eliminated. Operators should set
account quotas/alerts and appropriate edge protections. Players can disable exchange
with `lucent_ping_online 0`.

## Local checks

```
npm ci
npm test
```

Miniflare executes the actual Worker/DO with real local WebSockets: isolation of
servers/maps, two-way precision, no echo, duplicates, malformed/oversized frames,
flood/admission caps, hibernating keepalive, cleanup and fresh reconnect.

Native integration (from this directory on Windows):

```powershell
../../scripts/build-lucent-vs.ps1 -Targets game-client,testrunner,lucent-ping-test,lucent-browser-test,lucent-menu-test,lucent-radial-transition-test,lucent-settings-test,lucent-menu-effects-test
$env:LUCENT_PING_NATIVE_TEST = (Resolve-Path ../../out/build/x64-Release/testrunner.exe).Path
npm test
```

This additionally runs two real C++ relay transports through the local DO, forcibly
restarts the service to check automatic reconnect, changes context, checks cleanup,
and verifies rejection of a local self-signed TLS certificate without adding trust.
Plain WS loopback is enabled only by the native test constructor; the game requires
WSS. These checks do not imply a public deployment, two-PC test or GUI verification.

Primary API documentation:

- [Cloudflare Hibernation](https://developers.cloudflare.com/durable-objects/examples/websocket-hibernation-server/)
- [Rate Limiting binding](https://developers.cloudflare.com/workers/runtime-apis/bindings/rate-limit/)
- [WinHTTP WebSocket](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpwebsocketcompleteupgrade)
- [libcurl WebSocket](https://curl.se/libcurl/c/libcurl-ws.html)

## Public relay smoke test (explicit opt-in)

After the standard VS build, from the repository root:

```powershell
$env:LUCENT_PING_TEST_PUBLIC = "1"
./out/build/x64-Release/testrunner.exe --gtest_filter=LucentPingRelay.TwoNativeClientsPublicRelay
Remove-Item Env:LUCENT_PING_TEST_PUBLIC
```

This uses two production WSS transports with certificate verification and random
synthetic server/map hashes, isolated from real players. It checks handshake/member
counts, actual two-way coordinates, duplicate/no-echo behavior, map isolation,
rejoin and cleanup. Normal test runs skip this external-network test. It does not
launch the game or prove GUI behavior or connectivity from another computer.

## Sender-identity implementation files

- `src/game/client/lucent/game/ping.h`: event sender slot and unchanged local/remote lifetime/limit.
- `ping_identity.h`, `ping_sender.h/.cpp`: slot validation, controlled Player/Dummy selection and current-snapshot appearance lookup.
- `ping_component.cpp`: local sender selection, receive metadata and render inputs.
- `ping_protocol.h/.cpp`, `ping_transport.h/.cpp`: direct UDP v2 sender field and validation.
- `ping_relay.h/.cpp`, `ping_relay_protocol.cpp`, `ping_config_variables.h`: relay v2 metadata and endpoint.
- `ping_geometry.h/.cpp`, `ping_renderer.h/.cpp`: compact native Tee/name composition, width limits, safe bounds and edge stacking.
- `src/test/lucent_ping_relay_test.cpp`, `src/test/lucent_ping_transport_test.cpp`, `src/game/client/lucent/tests/ping_test.cc`: identity/protocol/appearance/layout coverage.
- `services/lucent-ping-relay/src/worker.js`, `test/relay.test.mjs`: v2 validation/routing and loopback tests.
- `CMakeLists.txt`, this README and `docs/lucent-map-ping.md`: build/test registration and protocol/deployment documentation.

GUI/game rendering and real DDNet 0.6/0.7 gameplay sessions were not launched. Automated
checks cover normalized snapshot-selection policy, copying both versions' render
metadata, protocol transport, layout geometry and clipping bounds, rather than pixel
appearance. Public v2 connectivity awaits the operator's manual Worker update.

## Persistent automatic connection

In the current test build no enable command is required after launch. With the saved
preference enabled, the existing component calls the same relay `Configure` path
once a valid online DDNet endpoint and loaded map SHA256 are available. The ONLINE
state guard excludes offline/main-menu, loading and demo playback. No valid context
means no connection or retry, even when the preference remains enabled. Repeated
frame updates with identical context do not create another connection.

`lucent_ping_online 0` disables immediately, clears remote markers and saves the
preference through standard DDNet settings serialization. `lucent_ping_online 1`
re-enables it, including while waiting in the menu for a future game connection.
`lucent_ping_status` still reports state/count; local markers are independent of
relay availability. Experimental host/join/leave also disable the saved relay flag,
so the selected direct-UDP mode cannot accidentally run alongside automatic relay.
No renderer, LPR2 field, Sender Identity logic, endpoint, heartbeat, reconnect or
production Worker code changed for this preference feature.

Tests use real settings files in isolated temporary test storage, fresh config
manager sessions, and two concurrently running native client-transport processes
with independent profiles. They verify default enablement, bidirectional pings,
server/map switches, missing context, disable/cleanup and config save/reload. A
Miniflare test replaces the DO message handler with a trap; `lp2` replies still work,
proving that WebSocket Hibernation auto-response bypasses that JavaScript callback.
The existing 15-second keepalive and auto-response are reused; no heartbeat was added.

These are CLI/transport tests, not two full graphical Lucent game instances. The
saved build has `HEADLESS_CLIENT=OFF`; full game/GUI verification was not performed.
No alternate build tree, Cloudflare deployment or user profile modification is used.
