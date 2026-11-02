# Lucent Scoreboard v1 — Adaptive Floating UI audit

Date: 2026-11-02. Branch: `feat/game-ui`.

## Scope and source investigation

Investigated with `rg` and source reads before implementation:

- `components/scoreboard.cpp/.h`: activation, bindings, unlocked cursor, popup actions, team titles, goals, recording, spectators and sorted player rendering.
- `gameclient.cpp/.h`: snapshot team counts and name/score/DD-team ordering; scoreboard/statboard render registration.
- `lucent/style/tokens.h`, `style/gradient.h`, `ui/scopes.h`, `motion/transition.h`, `motion/clock.h`.
- `lucent/hud/layout.h`, `panel_geometry.h`, `visuals.h` and existing coverage AA tests.
- CLion build script and the selected existing Release cache/File API/toolchain.

No Computer Use, screenshots, GUI automation or game launch were performed. No branch switch, commit or PR was made. No network, prediction, physics, ESC or HUD code was changed.

## Architecture

The existing scoreboard component remains the single integration point. There was no separate configurable legacy scoreboard renderer in this component; the existing DDNet mode-dependent row and action paths remain in use, rather than introducing a second renderer or configuration switch.

New header-only helpers:

- `lucent/scoreboard/layout.h`: bounded adaptive planner, exact column ranges, text fitting and retargetable scalar extents using Lucent Motion.
- `lucent/scoreboard/panel.h`: cached physical-pixel panel geometry using existing HUD coverage AA.

Both headers are registered in the client CMake source list. Tests extend the existing `testrunner` target; no new target, build directory or configuration is needed.

Retained row semantics: race/time score, sixup race and millisecond precision/tooltips; score, nickname, optional ID, clan, country flag, ping coloring; idle/dead tee, CTF carried flag, ready mark, auth and same-clan colors; DDNet team colors/labels/counts, red/blue team names and scores, win/draw title, game goals and demo recording status. Existing friend/mute/emoticon/spectate and map-description actions remain connected.

## Adaptive layout

Layout uses the existing 600-unit UI coordinate space, with width derived from the current aspect ratio. Four candidate column counts per team are evaluated only when layout inputs change. The quality function considers row density, column width and spectator density, with a small penalty for unnecessary columns. Team play reserves distinct red/blue groups and balances columns within each group.

Width grows with measured nickname/clan content, up to the viewport budget. Sparse games use one compact player column. Dense games trade column count and row density against one another. Counts are bounded to the 128-client protocol capacity. Column ranges partition all players exactly once, including empty and unbalanced teams. No player pagination, scrollbar or row limit is introduced.

Names and clans fit by reducing their font size rather than adding ellipses. The allocation between name and clan adapts to measured content, so an empty clan column does not consume a large fixed share. Extreme density and long Unicode strings can require small typography; actual glyph appearance and readability remain part of human visual QA.

Spectators use a separate adaptive grid. All entries retain the original optional ID, clan and auth coloring and spectator context actions. The old “N others…” truncation is removed. At zero spectators no spectator title, background or entries are submitted. Its animated extent settles to exactly zero, including the footer gap. With many spectators the planner widens the layout and adjusts grid density. No spectator scrollbar or page is used.

## Visual integration

Plum translucent surfaces come from existing Style and HUD PanelSurface/Gradient helpers. The two floating surfaces use the existing physical-pixel rounded mesh, transparent coverage fringe and soft border. Local/followed-player highlighting uses the existing rose SelectedSlot treatment. Red/blue underlines retain team distinction; DDNet group backgrounds retain their existing color semantics. Existing title, column, player and spectator typography roles are maintained at compact sizes.

## Motion

- Opening: cubic-out opacity over 170 ms, with scale 0.98 to 1.
- Closing: 110 ms; input and IsActive semantics change immediately.
- Rows: shared 2-unit reveal completes in 80 ms, with no serial queue proportional to player count.
- Surface width/height, vertical anchor, spectator extent and individual player/spectator positions retarget from the currently evaluated value over 160 ms.
- Spectators also have a 160 ms opacity reveal.

New data uses the complete target grid immediately; backgrounds can finish resizing underneath it. Rows are not withheld until their animation finishes and are not clipped to an animating surface. Logical hit rectangles move with the row. Scale is disabled while the scoreboard cursor is unlocked, so cursor hit testing remains in the native UI space. Graphics mapping is restored before popups/cursor rendering, and UI style/opacity/interaction changes use existing RAII scopes.

## Performance

Layout memoization includes viewport, both team counts, team mode, spectators and measured content width. Existing per-client text caches are retained. Name/clan and spectator text widths are remeasured when the corresponding text changes; resize/text reset invalidates measurements. The coverage mesh cache invalidates on bounds or physical framebuffer mapping changes, avoiding repeated trigonometric mesh construction at rest.

New state uses fixed per-client storage and two fixed panel caches. There is no new per-frame vector, sorting allocation or unbounded layout search. Existing DDNet sorted snapshot traversal, team grouping and rich spectator text drawing remain. This is source-level analysis; no live GPU/CPU profiler session was performed.

## Verification

1. `./scripts/build-lucent-clion.ps1 -Inspect` — passed. Selected profile **Release**, existing `cmake-build-release`, CLion bundled CMake/Ninja, configured MSVC x64 14.51.36231 and SDK 10.0.28000.0. No alternate tree or compiler was selected.
2. `./scripts/build-lucent-clion.ps1 -Test` — **Release build passed**, including `all`, `game-client`, `testrunner` and all six discovered Lucent standalone targets. The unfiltered test command correctly returned failure: **501 C++ tests run, 496 passed, 4 skipped, 1 failed**. The sole failure was the existing **Net.Ipv4AndIpv6Work** IPv6 loopback test (`net_test.cpp:48–49`). It was neither fixed nor excluded. Three existing tests remain disabled.
3. All six Lucent standalone executables passed: browser, menu effects, menu interaction, ping, radial transition and settings. Existing Rust target passed, including 10 base doctests and 8 engine doctests.
4. `testrunner.exe --gtest_filter=LucentScoreboard.*` — **6/6 passed**.
5. Final `./scripts/build-lucent-clion.ps1 -Test -TestFilter 'LucentScoreboard.*:LucentHud.*'` — **passed**: complete default Release build, **30/30 focused C++ tests**, all standalone executables and Rust. This is a separate focused run and does not replace the failing unfiltered-suite result above.
6. `git diff --check` — passed.

New tests cover 1, 2, 16, 32, 64 and 128 players; empty, balanced and unbalanced teams; spectators 0/1/N up to the remaining client capacity; aspect ratios 3:4, 1:1, 4:3, 16:9, 21:9 and 32:9; compact/expanded column selection; exact-once partitions for every count 0–128; long-text fitting; virtual-to-physical scaling at 480/720/1080/1440/2160 height; extent retarget continuity, fade durations, scale endpoints and shared reveal completion; layout and coverage-cache invalidation.

Logs are retained in the existing build directory as `scoreboard-verification.log` and `scoreboard-profile-verification.log`.

## Human verification still required

Visual verification was deliberately not performed, as required by AGENTS.md and the work package. Please inspect real race, team/CTF and sixup sessions, dense 128-client lists, long multilingual names, spectator joins/leaves, rapid open/close and cursor popup interactions. Automated checks cover model geometry and state, not screenshots or live font rasterization/readability.
