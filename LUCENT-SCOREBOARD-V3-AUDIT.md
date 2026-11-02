# Lucent Scoreboard v3 audit

Date: 2026-11-02. Branch: feat/game-ui.

## Scope and investigation

Through rg and source reads, inspected scoreboard v2, its adaptive planner/text fitting, DDNet snapshot/player/finish-time/team data, Client()->Points(), native HTTP, engine jobs, the Lucent map-preview cache, CCachedText/CTextCursor wrapping, Style/Gradient/UI/Motion helpers and HUD coverage AA.

Added Pts, a bounded asynchronous profile service, common typography, measured field allocation and independent red/blue column counts. The centered independent header, floating surfaces, animations, spectators visibility and existing actions remain.

No new configuration, bindings, gameplay, protocol, prediction, physics, ESC or HUD changes. No branch switch, commit, PR, GUI automation, screenshot or game launch. Existing uncommitted v1/v2 work was retained.

## Points source and identity

Source: [official DDNet player JSON](https://ddnet.org/players/?json2=deen), implemented by [official players.py](https://github.com/ddnet/ddnet-scripts/blob/master/servers/scripts/players.py).

The selected value is **points.points**, from the global points ranking. The nested points.total, map score/time, yearly/monthly/rank points and Client()->Points() are not substitutes. Client()->Points() only supplies the local profile value.

Validation requires an object, an exact length-aware UTF-8 match of player to the requested nickname, and a nonnegative integer points.points within int range. Missing/null, malformed, mismatched, fractional, negative, embedded-NUL and overflowing results remain unknown. Unknown displays **—**; validated zero displays **0**.

Direct CLI smoke checks succeeded:

| Profile | HTTP | Bytes | Result |
| --- | --- | --- | --- |
| Existing public profile deen | 200 | 250720 | Exact name; integer points |
| Absent Lucent_scoreboard_missing_profile_7b1e6 | 200 | 2 | Empty object; unknown |

Keys are exact nickname bytes, including case. No fuzzy/case/confusable merging or clan/IP association is performed. Duplicate exact names share a request and public profile value. DDNet ranks are name-based; this cannot authenticate the human behind a name. The Pts tooltip explains that limitation.

Rows resolve their current name, never a retained client-ID result. Rename/slot reuse cannot inherit another name's points. Reset/state changes/shutdown abort transport and mark work for discard. Slots remain pinned until transport/parser termination, including across server switches. Late worker results cannot commit into a new session. Workers own request/name data without accessing the scoreboard object.

Valid global profile cache values can be reused on another server. Rate limits survive reset. Acquisition starts only while the scoreboard is active and online. Closing pauses new acquisition; bounded in-flight work may finish. Demo playback starts no profile requests.

## HTTP, worker and cache policy

There was no reusable arbitrary-player points cache in the inspected client. The new bounded domain cache reuses HttpGet/IHttp::Run, EscapeUrl, Engine::AddJob, IJob and the bundled JSON parser. No independent networking thread/backend or persistent nickname database is added.

| Policy | Value |
| --- | --- |
| Combined active transport/parser slots | 2 maximum |
| Dispatch interval | At most 1 new request/second |
| Main-thread polling and display refresh | At most 4 Hz; name changes refresh lookup |
| Positive / negative TTL | 30 minutes / 2 minutes |
| Ordinary HTTP/transport error global cooldown | At least 10 seconds |
| HTTP 429 / 503 global cooldown | 2 minutes |
| Connect / total timeout | 4 / 12 seconds |
| Low-speed cutoff | Below 256 bytes/sec for 5 seconds |
| HTTP body / parser allocation caps | 2 MiB / 8 MiB |
| Cache | 256 entries; exact names up to 64 UTF-8 bytes |

Eviction uses last-seen age among unused, non-pending entries. Current names are protected before eviction. Deduplication includes pending work, positive values and negative results.

Scheduling selects the oldest attempt, with never-requested names ahead of retries. Negative-cache expiry cannot starve the remaining names in a cold 128-client list.

Transport is asynchronous; bounded JSON decoding runs on engine workers. Rendering only polls, commits completed results and schedules eligible work. It never waits. A cold list intentionally fills gradually instead of dispatching 128 requests at once.

Limitations: cached values can be 30 minutes old, the official generated data has its own cadence, and oversized/unreachable/invalid responses remain unknown. Cache data is process-local.

## Typography and column allocation

CFields reserves measured Time/Score, Pts, Ping, tee/flag and optional DDRace-label widths before allocating name/clan space. Numeric fields are right aligned. Pts reserves six digits and grows for larger validated values. Time reserves the native seconds range; legacy fractional-time formatting remains.

One player-body font is selected for the whole grid from row height and the narrowest animated live panel. A long nickname never individually shrinks its font. Name/clan baseline budgets are allocated first, then extra width follows measured demand. Sparse panels grow to fit content before ellipsizing, bounded by the viewport.

DISALLOW_NEWLINE, STOP_AT_END and ELLIPSIS_AT_END enforce single-line fields. Display control characters become spaces; request/cache keys retain original bytes. Full names/clans remain available through the unlocked cursor. Spectator tooltips use persistent per-client buffers.

Spectators use one grid font and separate name/clan cells, retaining IDs, auth/same-clan colors and context actions. At zero spectators no panel, title or residual extent is reserved.

Rows follow panel geometry and shared reveal; independent player position tweens were removed because reordered rows can cross. DDRace captions have a conditional measured gutter before Time, rather than multiline/below-row text. Team ID, complete membership count/maximum and Super remain available, including groups split across columns.

The planner searches 1–4 columns independently per game team, up to eight total. Larger teams receive more columns; empty teams keep one title/score panel. Quality prioritizes achievable common typography, spectator density and avoiding unnecessary columns. Dense layouts tighten gaps. Pts/gutter minimum width is included in the planner cache key.

All player ranges are partitioned exactly once. No scrollbar, pagination or hidden-player summary is added. Header width remains independently content-based.

**Readability limits:** 128 clients on a narrow/low-resolution viewport cannot always show every full Unicode nickname/clan plus all numeric/status fields at a large font. Every row remains, numeric/status fields are reserved, and long text uses ellipsis with full tooltips. This does not guarantee every glyph is simultaneously visible or that extreme density is comfortably readable. Small viewports can still require a small common font; human pixel-readability checks remain necessary.

## Style and performance

Local/followed and hover fills blend existing ServerSelection and Accent tokens into plum/rose. Shared gradients, HUD coverage-AA geometry and existing Motion helpers remain; no second palette/design system is introduced.

Score/time, country flags, ping colors, tees/dead tees, carried CTF flags, ready marks, auth/clan colors, team colors and cursor actions remain.

The planner is memoized; profile state and work queues are bounded. Player text, DDRace measurements and floating-surface meshes are cached. Frame geometry uses fixed arrays/arithmetic, without a new player sort or per-frame collection allocation. Live CPU/GPU profiling was not performed.

## Verification

Final workflow: scripts/build-lucent-clion.ps1, **Release**, existing cmake-build-release, CLion bundled CMake/Ninja, configured MSVC x64 14.51.36231 and SDK 10.0.28000.0.

Automatic regeneration temporarily left File API without testrunner; the wrapper correctly refused it. File API subsequently updated and -Inspect validated the original workflow again. No alternate tree/compiler, manual cache reconfiguration, IDE preference change or VS fallback was used.

- Complete default **Release build passed** through -Test, including all, game-client, testrunner and all discovered standalone targets.
- Unfiltered C++: **516 run, 511 passed, 4 skipped, 1 failed**. Sole failure: known **Net.Ipv4AndIpv6Work** IPv6 loopback case. It was neither fixed nor excluded; the command correctly returned failure. Three existing tests remain disabled.
- Separate focused HUD/scoreboard/points run: **45/45 passed**.
- All six Lucent standalone executables passed: browser, menu effects, menu interaction, ping, radial transition and settings.
- Rust target passed, including 10 base and 8 engine doctests.
- clang-format --dry-run --Werror on changed C++ files and git diff --check passed.
- Official API CLI smoke checks passed as recorded above.

Eleven new tests cover schema/exact-name validation, zero versus unknown, deduplication, rate/concurrency, negative-cache fairness, actual worker separation, nickname/slot changes, server-reset cancellation, late parser results, HTTP 429 cooldown, field non-overlap, Unicode/control characters, unbalanced teams, DDRace gutters and sparse full-content allocation.

Existing geometry tests were adapted for independent team column counts and dynamic gaps. Combined coverage includes 0/1/2/16/32/64/128 players, empty/balanced/unbalanced teams, spectators 0/1/N, narrow through ultrawide aspect ratios, physical scaling, header independence, exact-once ranges and animation/cache state. Service tests inject recording HTTP transport and use the real engine job pool, without a public-network dependency.

Logs: cmake-build-release/scoreboard-v3-verification.log and cmake-build-release/scoreboard-v3-profile.log.

## Manual verification

GUI/visual verification was deliberately not performed. Human checks remain for font rasterization/readability, dense lists, multilingual strings, DDRace/Super groups across columns, team imbalance, loading/unknown points, rapid server/name changes, spectators, selection colors and cursor tooltip/popup interactions.

## Crash follow-up: HTTP transport failure

The user-reported assert log from 2026-11-02 identifies engine/http.cpp:175, inside IHttpRequest::StatusCode(). The points error branch called this accessor after Done(), but Done() includes ERROR and ABORTED as well as DONE. StatusCode() requires exactly DONE. A failed transport request therefore asserted instead of displaying unknown points. The original tests covered HTTP 429 responses in DONE state but missed transport-error states.

Fixed the points service to capture terminal state first and read StatusCode() only for DONE. ERROR/ABORTED use the ordinary transport-failure cooldown and unknown-value path, without reading response accessors. The engine assertion remains intact.

Added two regression tests covering ERROR, unexpected ABORTED, queue recovery after cooldown, retained successful cache entries, and failure of the tenth request after nine successful profiles. Tests use the existing injected transport and real parser worker pool.

Post-fix Release build passed through scripts/build-lucent-clion.ps1 -Test. Full C++ result: 518 run, 513 passed, 4 skipped, sole failure Net.Ipv4AndIpv6Work. All Lucent standalone executables and Rust passed. Focused points tests: 8/8 passed. Formatting and git diff --check passed.

Logs: cmake-build-release/scoreboard-points-crash-verification.log and scoreboard-points-crash-profile.log. Diagnosis used the supplied assert log, source contract and automated regression tests; no GUI debugger or game launch was used. Gradual loading remains intentional at one new request per second.
