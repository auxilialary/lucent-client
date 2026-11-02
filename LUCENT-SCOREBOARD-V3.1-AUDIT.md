# Lucent Scoreboard v3.1 — Points Loading & Layout Optimization

Date: 2026-11-02. Branch: feat/game-ui.

## Scope and investigation

Investigated through rg and source reads: scoreboard rendering/order, points service, native HTTP completion/accessor contracts, request/cache scheduler, DDRace label measurements, CFields, adaptive column ranges and existing tests.

Changes are restricted to the scoreboard, its points/layout helpers and tests. Existing floating composition, centered independent header, panel Motion, coverage AA, rose selection, spectators presence rule and actions remain. No new configuration, networking backend, protocol, gameplay, physics, prediction or ESC changes. No branch switch, commit, PR, GUI automation or game launch.

## Source research and actual upstream limits

Inspected the [official DDNet player endpoint implementation](https://github.com/ddnet/ddnet-scripts/blob/master/servers/scripts/players.py) and a single normal response from https://ddnet.org/players/?json2=deen.

**A contractual numeric upstream rate/concurrency limit was not found in the inspected public sources.** The observed HTTP 200 response contained no RateLimit/X-RateLimit or Retry-After fields. This does not establish unlimited capacity or guarantee that 3 requests/sec is an upstream-approved quota. No live 40–128-request load test was performed.

The source remains the exact-name JSON profile and points.points. Unknown remains an em dash; a validated zero remains zero. Response/name/type validation and payload/parser limits are unchanged.

Inspected BestClient [show_points.cpp](https://github.com/BestProjectTeam/BestClient/blob/HEAD/src/game/client/components/bestclient/show_points.cpp) and [show_points.h](https://github.com/BestProjectTeam/BestClient/blob/HEAD/src/game/client/components/bestclient/show_points.h), fetched from HEAD on the audit date. Its DDNet provider uses the official ru.ddnet.org JSON profile mirror, a maximum of 3 concurrent requests, a queue bound of 64, positive TTL of 10 minutes and negative TTL of 30 seconds. Its queue fills free concurrency slots rather than establishing a documented DDNet request-per-second quota. Those choices are reference behavior, not proof of source capacity. No BestClient code was copied.

Research copies are retained under cmake-build-release/v31-audit-*.

## Batch/compact alternatives

No verified multi-name JSON batch route was found in the inspected official player handler. Its json2 argument is a single name.

The compact query endpoint is prefix/substring search with a small truncated result list. It can omit the exact requested name and scans ranking entries; smaller response size is not proof of lower server cost. Using it would require ambiguous-miss fallbacks and potentially two requests per name. It was not substituted for the reliable exact-name lookup.

Broad ranking/CSV/SQL exports are not a verified compact per-session lookup for 1–128 names. No whole database download or third-party replacement source was introduced.

## Selected loading strategy

The following are **per-client application budgets**, not claimed official API limits:

| Control | v3.1 |
| --- | --- |
| Initial pacing | 2 new requests/sec |
| Healthy ceiling | 3 new requests/sec |
| Strict rolling window | At most 3 starts in any half-open one-second window |
| Combined transport/parser slots | At most 4 |
| Completion/dispatch poll | At most 20 Hz |
| Roster reconciliation/display refresh | At most 4 Hz |
| Cache | 256 exact-name entries; at most 128 current names plus pinned old work |
| Positive/negative TTL | Unchanged: 30 minutes / 2 minutes |
| Connect/total timeout | Unchanged: 4 / 12 seconds |
| Response/parser limits | Unchanged: 2 MiB / 8 MiB |

Healthy HTTP 200 responses allow gradual increase: at least eight responses and five seconds of healthy observation per step. Initial growth is 0.5 requests/sec up to 3. Recovery after overload uses smaller 0.25 steps.

Pacing uses start deadlines and a three-start rolling history. Idle time does not accumulate a catch-up burst. The budget/history are retained across TAB cycles, reset and server changes. Fresh positive/negative entries suppress repeated acquisition. Exact duplicate names share one cache/request. A cache-keyed bounded backlog avoids allocating a request for every player at once.

### HTTP 429 / 503 and transport failure

Each points request now uses the existing FailOnErrorStatus(false) request setting. With the default native curl policy, HTTP errors otherwise become ERROR and StatusCode() is not legally readable. Keeping completed HTTP responses in DONE allows the scheduler to recognize 429/503 without changing the networking backend.

On 429/503:

- Global cooldown starts at 120 seconds.
- A distinct repeated overload episode doubles it: 120, 240, 480, capped at 900 seconds.
- Pacing halves, with a floor of 0.25 requests/sec.
- New-dispatch concurrency halves, down to one.
- Related responses arriving during the same cooldown are coalesced rather than applying four independent penalties.
- Existing work can finish; no new dispatch exceeds the reduced limit while it drains.
- HTTP 200 responses during cooldown do not cancel the cooldown or immediately restore intensity.
- Recovery is gradual and capped at 3 requests/sec / 4 slots.

Ordinary transport failures retain a global cooldown of at least 10 seconds and negative caching. ERROR/ABORTED never call StatusCode(), Result() or other DONE-only response accessors. The previous assertion fix remains intact.

The existing public IHttp interface does not expose arbitrary Retry-After response headers. This work uses the fixed adaptive policy above rather than changing that interface/backend.

### Lifecycle, data accuracy and rendering

Reset/session/name changes still cancel obsolete work and discard late parser results. Cancelling transport/parser work remains counted until terminal; opening another server cannot temporarily create a fifth points work slot. Valid global name-profile cache values are reusable across servers.

Parsing remains on the engine worker pool. The render thread does not wait for transport/parser work. The faster poll only checks bounded work slots; more expensive roster matching remains at 4 Hz.

No requests are started in demo playback. Closing TAB pauses acquisition; a bounded request already running may finish. Exact-name public statistics still do not authenticate a person behind a nickname; the existing tooltip limitation remains.

## Per-column DDRace gutter

Previously every column reserved the widest DDRace label found anywhere in the scoreboard.

The render frame now builds per-game-team ordered label-width arrays using the exact sorted alive-then-dead traversal used by RenderScoreboard. Each ColumnRange receives the maximum width of only its own rows:

- Flock-only columns reserve zero gutter.
- A column with multiple DDRace teams reserves its own widest full label.
- A team crossing a column boundary contributes its label width to both affected columns.
- Red and blue game-team ranges are handled separately.
- Empty team panels reserve no label width.

The per-column gutter is passed to both field calculation and label rendering. Membership counts, full captions, Super, colors and tooltip content are retained; no team metadata is dropped.

Freed width enters the existing name/clan width allocation. Numeric field widths and right-side country/Pts/Ping anchors remain stable. One shared body font is selected against the actual width/required gutter of each live animated column, avoiding a narrow label-free panel being penalized by another panel's gutter. The planner retains a conservative worst-case width budget for candidate selection.

Only metadata/field allocation changes: player traversal, ordering, counts and exact-once ranges are unchanged. Existing one-line text flags, complete row list, independent surfaces and no-scrollbar/no-pagination policy remain.

## Verification

Release built successfully through scripts/build-lucent-clion.ps1 using the existing selected Release profile/cache, CLion CMake/Ninja and configured MSVC x64 toolchain. No alternate build tree/compiler or IDE preference changes.

- Focused HUD/scoreboard/points: **51/51 passed**.
- Full C++: **522 run, 517 passed, 4 skipped, 1 failed**. The sole failure remains **Net.Ipv4AndIpv6Work**. It was not fixed or excluded; the full test command returned failure. Three existing tests remain disabled.
- All six Lucent standalone executables passed.
- Existing Rust tests/doctests passed.
- Formatting check and git diff --check passed.

Coverage includes 1/16/40/64/128 unique names, repeated names, cached/session reuse, strict pacing/window/concurrency, idle/no-burst behavior, 429 and 503, adaptive reduction/coalescing/recovery, transport ERROR and unexpected ABORTED, server/nickname changes, late results and the prior tenth-request crash regression. Native timeout errors use the same ERROR path; tests inject that terminal state without requiring a live 12-second network timeout.

Gutter tests cover mixed team labels in a column, a team split across columns, zero-label columns, reclaimed name/clan width and unchanged numeric/flag anchors. Existing geometry, Unicode, full-list and Motion tests remain.

### Deterministic loading model

The service test uses a recording HTTP transport, a real parser worker pool, a 50 ms simulated polling step and responses ready after 80 ms. These are **virtual test times, not production DDNet benchmarks or promises**:

| Unique names | Cold completion |
| --- | --- |
| 1 | 0.15 s |
| 16 | 7.30 s |
| 40 | 16.55 s |
| 64 | 25.45 s |
| 128 | 48.25 s |

Each case embeds duplicates in the roster and confirms exactly one request per unique name. Fresh cache/session reuse adds zero requests. Production time depends on network latency, response sizes, worker availability and upstream feedback.

Logs: cmake-build-release/scoreboard-v31-build.log, scoreboard-v31-profile.log/.xml and scoreboard-v31-verification.log.

## Remaining limits and visual QA

Upstream quotas remain undocumented in the inspected sources; adaptive ceilings are bounded client policy, not a guarantee of server acceptance. Cached values and official generated statistics retain their update cadence. Oversized/unavailable results remain unknown.

The v3 readability limits still apply to 128 players on narrow/low-resolution viewports. Gutter removal gives back actual field space but cannot make all long Unicode strings fit at a large font. The common font, one-line fields and full-text tooltips remain.

GUI/visual verification was deliberately not performed. Human verification remains for real-session loading, mixed/split DDRace groups, panel reflow and dense typography.
