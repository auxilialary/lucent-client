# Lucent Scoreboard v2 — centered independent header

Date: 2026-11-02. Branch: `feat/game-ui`.

## Result

The shared scoreboard-wide background and repeated non-team map/time title bars have been removed. The map and timer now appear once in a compact independent floating capsule. Each populated player column has its own Lucent coverage-AA surface, rounded perimeter and border. Team names/scores remain inside the corresponding red/blue columns.

This work builds on the existing uncommitted v1 implementation. No branch switch, commit, PR, gameplay/network/prediction/physics change, GUI automation or game launch was performed.

## Header and layout

- `RenderHeader` measures cached map and timer text independently from the grid. Capsule width is content width plus padding and a small internal gap, capped at **460 UI units or 75% of viewport width**, whichever is smaller.
- Capsule center is the scoreboard center. Neither player count, number of columns nor grid width is an input to the capsule-width calculation.
- Long map titles ellipsize within the cap; the unlocked cursor exposes the full map name in a tooltip. Map-description popup behavior remains available on the map title, including in team modes.
- Header contains only the map name and time. In race modes it shows an available server map record, retaining fractional precision under one hour. Without a record it uses the existing HUD round-clock rules: elapsed round time, time-limit countdown or race-time warmup offset. No HUD code is changed.
- Player panels begin **10 UI units below the 34-unit capsule**, with a fixed gap even while the shared vertical anchor moves. Target panels have **10-unit horizontal gaps**.
- Non-team columns start directly with the score/name/clan/ping headings. Team columns additionally retain compact team titles, scores and red/blue accents. Each panel height follows its own player range, rather than filling every column to a common bottom.
- The bounded adaptive planner still selects up to four columns per team and adjusts row density. Header chrome, team chrome, spectator space and footer headroom are included in the available-height budget. Empty non-team games do not reserve an empty player panel.
- The animated vertical anchor is constrained to keep newly arriving target content within the viewport. Retiring column backgrounds do not push spectators or the footer below the target content.

No player or spectator scrollbar, pagination or hidden-entry summary is introduced. Exact column ranges still partition the complete sorted player list. Nickname/clan fitting, country flags, ping, time/score, local/followed highlights, tees, CTF flags, ready/dead/auth/clan indicators and DDNet team grouping remain in the existing row renderer.

## Spectators

Spectator content and cursor actions continue through the existing rich entry renderer. Its grid adapts width/density to the full spectator count, including 128 spectator-only clients.

At zero spectators the render branch is skipped, the animated extent is reset, and `SpectatorSpace` returns exactly zero even for stale previous layout data. No container, title, entries or residual collapse gap is reserved. On appearance the block has a restrained opacity reveal. All entries remain available without scrolling or an “N others” replacement.

## Motion and Lucent integration

Existing Lucent Motion helpers provide:

- scoreboard opening opacity and 0.98-to-1 scale over 170 ms;
- 110 ms closing opacity with immediate input/activation semantics;
- existing shared 80 ms row reveal;
- 160 ms header-width, panel-bounds, anchor and row-position retargeting;
- 120 ms column-background appearance/disappearance;
- 160 ms spectator opacity reveal.

Column bounds have independent fixed-capacity motion state. Outgoing trajectories continue during fade-out, so rapid reactivation samples the current geometry instead of a frozen stale position. Header width has its own transition, unrelated to these panel transitions. New player data is not clipped to a surface that is still growing.

Surfaces reuse `LucentHud::PanelSurface`, shared plum/slate gradient tokens, coverage-AA panel geometry and rose borders. Existing rose selection highlighting and UI style/opacity/interaction scopes are retained. Capsule typography is 16/13 UI units for map/time; team titles use 14 units. No second palette, rendering backend or configuration switch was added.

Scale remains disabled while the scoreboard cursor is unlocked, preserving native UI hit coordinates. Graphics mapping is restored before popup/cursor rendering. Existing bindings and player/spectator actions remain unchanged.

## Performance and files

Changes are localized to `components/scoreboard.cpp/.h`, `lucent/scoreboard/layout.h` and the scoreboard tests in `src/test/lucent_hud_test.cpp`.

The existing memoized planner and per-client text caches remain. Header map/timer measurements use cached text. Coverage geometry is cached separately for the header, spectators and up to eight columns. Animation and frame geometry use bounded fixed arrays; no new per-frame vector or unbounded search is introduced. Live CPU/GPU profiling was not performed.

## Verification

- `./scripts/build-lucent-clion.ps1 -Inspect`: passed. Selected **Release** profile, existing `cmake-build-release`, CLion bundled CMake/Ninja, configured MSVC x64 14.51.36231 and SDK 10.0.28000.0. No alternate build tree/compiler or Visual Studio fallback script was used.
- `./scripts/build-lucent-clion.ps1 -Test`: complete default **Release build passed**, including `all`, client, testrunner and all discovered Lucent standalone targets.
- Unfiltered C++ result: **505 tests run; 500 passed, 4 skipped, 1 failed**. The only failure is the known **Net.Ipv4AndIpv6Work** IPv6 loopback case at `net_test.cpp:48–49`. It was not fixed or excluded; the full test command correctly returned failure. Three existing tests remain disabled.
- All six standalone Lucent executables passed: browser, menu effects, menu interaction, ping, radial transition and settings.
- Existing Rust test target passed, including 10 base and 8 engine doctests.
- Separate focused command: `testrunner.exe --gtest_filter=LucentScoreboard.*:LucentHud.* --gtest_brief=1` — **34/34 passed**, including **10 scoreboard tests**. This focused result does not replace the unfiltered-suite failure.
- `git diff --check`: passed.

Coverage includes empty games and 1/2/16/32/64/128 players; balanced/empty/unbalanced teams; spectators 0/1/N and spectator-only capacity; multiple aspect ratios and physical scaling; long text; exact-once player ranges; cached layout/coverage invalidation; content-sized capped header centering for different grids; fixed capsule/column gaps; absence of zero-spectator space; panel reversal/fade/reactivation; and round/countdown/race clock semantics.

Logs: `cmake-build-release/scoreboard-v2-verification.log` and `cmake-build-release/scoreboard-v2-profile.log`.

## Visual QA

Visual verification was **not performed**, as required by AGENTS.md. Automated tests validate geometry/state, not screenshots, live font readability or gameplay-session interactions. Human checks should cover 1/2/3+ columns, long multilingual map/player names, race records with fractional times, teams/CTF, rapid reflow and spectator joins/leaves, cursor tooltips and popup actions.
