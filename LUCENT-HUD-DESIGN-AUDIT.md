# Lucent Game HUD — Design System Integration & Visual Polish

Date: 2026-11-02. Branch: `feat/game-ui` (not switched).

## Scope and evidence

Source audit and CLI verification only. Computer Use, game launching, screenshots and visual GUI verification were **not performed**, as required by AGENTS.md. This report does not certify the rendered appearance on maps.

The workspace already contained substantial tracked and untracked work, including the HUD, alignment overlay and previous reports. Changes here build on that state. No commit or PR was created. No new configuration variables or editor features were added. Physics, networking, prediction and scoreboard source were not edited for this task.

## Existing design system audit

Research began with `rg --files` and `rg` searches across `lucent/style`, `lucent/ui`, menu, settings, browser and HUD code.

| Source | Existing language and reuse |
| --- | --- |
| `src/game/client/lucent/style/tokens.h` | Canonical rose Accent, dark violet Surface, Text, DetailText, ErrorText; radius 3, hairline 0.5; idle/active borders and selected/hover/pressed opacities. |
| `src/game/client/lucent/ui/surfaces.h`, `surfaces.cpp` | `RADIUS` aliases the shared radius. InputFrame draws an accent perimeter and inset dark surface. This construction is adapted to HUD rectangles without bringing menu input behavior into gameplay. |
| `src/game/client/lucent/ui/controls.h`, `controls.cpp` | Highlight channels distinguish hover, focus and selection. Selection uses accent fill and a stronger perimeter; text uses the shared palette. HUD widgets are passive, so no artificial hover interaction is introduced. |
| `src/game/client/lucent/style/gradient.h` | Existing rose/pink/white animated gradient for interactive menu controls. HUD uses the stable shared accent to keep gameplay information readable; it does not introduce a separate gradient system. |
| `src/game/client/lucent/motion/transition.h`, `clock.h` | Retargetable cubic-out transitions, shared selection duration 0.22 s and text appearance duration 0.12 s. HUD uses the existing transition implementation with gameplay render timestamps. |
| `src/game/client/lucent/browser/browser_players.cpp`, browser list and settings code | Existing default font renderer, approximately 10–13 menu-unit supporting text, denser metadata and larger primary labels. No separate HUD font or bitmap text renderer is needed. |
| `src/game/client/lucent/hud/layout.h` | Existing 300-unit-high HUD coordinate space, scale 50–150%, wrapping status groups and collision-aware utility placement. Menu units are adapted by 0.5 for shared radius and hairline. |

There is no universal spacing/typography token scale in the current menu: many page and row metrics are local. Existing HUD layout metrics remain centralized in `layout.h`; they were not promoted into a second global design system. The separate `ui/panel.h` ingame panel also has legacy literals; it is outside this HUD change.

Before this change, HUD used its own lavender accent, near-white text, purple surfaces, large rounded corners and a fully different alignment capsule surface. Timer had another independent surface opacity and capsule radius. Utility text retained an extra shadow/outline pass; only timer and alignment consistently selected target raster sizes.

## Changes

- `hud/visuals.h` now adapts the canonical Style palette. All scoped widgets use one dark surface and accent hairline treatment. Surface opacity is centralized at 0.82 before the existing user opacity multiplier. Text/secondary/selection/error colors come from existing tokens; positive and frozen states reuse the existing muted green and blue severity tokens rather than duplicate RGB values.
- Radius and perimeter thickness derive from `Style::Radius` and `Style::Hairline` at HUD coordinate density. Timer and alignment use the same Panel helper as hotbar, jump/modifier containers, coordinates and recording. Alignment retains the circular avatar well and actual idle tee rendering.
- Weapon selection uses the existing `CTransition`, with a 0.22 s cubic-out fill/perimeter crossfade. All six weapon states advance together so a hidden inventory slot does not retain a stale selection value. Reset clears these presentation states. Ownership, protocol ordering, active ammo and ninja duration behavior remain intact.
- Alignment retains target hysteresis, outgoing identity/skin capture, two-layer crossfade, freeze interpolation, collision avoidance and vertical row reflow. Reveal/reflow now use the shared selection duration and cubic-out helper; fade uses the shared text appearance duration. No target detection or snapshot/prediction logic was changed.
- Urgent timer emphasis now uses a continuous cosine opacity pulse instead of the abrupt half-second opacity switch. Checkpoint/finish semantics and existing fade remain.
- Every cached Lucent HUD text slot now uses target physical glyph sizes and invalidates on screen-height changes. Recording and coordinate measurements use those same target sizes. Utility text no longer draws a second offset shadow or outline; the unified panel supplies contrast.
- Coordinate panels reserve representative signed numeric cells, and both speed axes use equal cell widths. Typical sign/decimal/digit changes no longer resize the panel or move the second axis. Values beyond those cells still grow to fit; values are not truncated or reformatted.

## Composition and actual sizes

The approved anchors and baseline dimensions are preserved: timer at top center, clock below timer, hotbar at bottom center, independent jump/modifier containers, and alignment as a vertical list with real tees. Coordinates still clear the score area/status bounds; recording still avoids the timer; alignment still avoids lower HUD obstacles and the source-defined chat envelope.

At 100% HUD scale on a 1080-pixel-high screen, one HUD unit is 3.6 pixels. These are geometry calculations, not screenshot measurements:

| Element | HUD units | Approximate physical size |
| --- | --- | --- |
| Main timer font | 9 | 32 px target raster |
| Secondary/body font | 6 | 22 px target raster |
| Clock/utility caption | 5 | 18 px target raster |
| Alignment label | 6.5 | 23 px target raster |
| Hotbar slot | 22 × 20, factor 0.85 | 67.3 × 61.2 px |
| Six-weapon hotbar | 148.5 × 24, factor 0.85 | 454.4 × 73.4 px |
| One status row / icon | 12 / 8 | 43.2 / 28.8 px |
| Alignment row / tee | 18 / 13 | 64.8 / 46.8 px |
| Panel radius | 1.5 before local scale | 5.4 px; hotbar 4.6 px |

The large timer remains the primary text. Supporting captions remain smaller than alignment names; status icons are smaller than weapon/tee artwork. Hotbar keeps its previously approved 15% reduction. Existing tests cover aspect ratios, scaling, wrapping, rotated weapon bounds, timer line clearance, chat/score clearance and alignment reflow. The radius test was updated to the canonical menu-derived radius. A new test verifies selection completion, monotonicity and reversal continuity at 30/60/144/240 FPS; the alignment reflow test now measures half/full shared selection duration.

## Compatibility

The existing `lucent_hud 0` dispatch branches were preserved. Added rendering helpers and transitions are reached from the Lucent HUD presentation path; reset only clears additional presentation state. Existing configuration persistence, jump accounting, inventory/protocol, timer formats, UTF-8 label fitting and alignment tests remain enabled. This is source/test evidence, not a runtime visual comparison of legacy mode.

During editing a Windows encoding error temporarily emptied the untracked HUD implementation. It was restored from the local task history before reapplying this change. The first 220 lines matched the initial source read exactly; status, hotbar (excluding its later bounds assignment), recording and coordinates matched the earlier preserved function hashes. All final builds/tests below ran after restoration. Subsequent writes explicitly used UTF-8.

## Build and tests

Built using `./scripts/build-lucent-vs.ps1` and the saved Visual Studio selection: `x64-Release`, MSVC x64, existing `out/build/x64-Release`. Its CMake build type is `RelWithDebInfo`, as defined by the existing VS configuration. No alternate build tree was created.

Targets: `game-client`, `testrunner`, `lucent-browser-test`, `lucent-menu-test`, `lucent-settings-test`, `lucent-menu-effects-test`, `lucent-radial-transition-test`. Final build: **passed**, including linking `Lucent Client.exe`.

| Verification | Result |
| --- | --- |
| LucentHud, LucentTeeAlignment, LucentGameView | 26/26 passed |
| Browser, menu, settings, menu effects, radial standalone executables | All five passed |
| Full testrunner | 488 tests: 483 passed, 4 skipped, 1 failed; 3 disabled tests not run |
| Isolated repeat of `Net.Ipv4AndIpv6Work` | Same failure: IPv6 loopback receive timeout, `net_socket_read_wait` 0 and `net_udp_recv` -1 instead of 3 |
| Regression excluding that network test | 487 tests: 483 passed, 4 skipped; 3 disabled tests not run |
| `git diff --check` | Passed |

The network failure occurs in the IPv6 phase of `src/test/net_test.cpp`; IPv4 assertions completed first. No network implementation was changed. The failure is reproducible in this environment, but its cause was not diagnosed in this HUD task. The full suite is **not** reported green. Skipped relay tests require separately enabled integration/public-relay fixtures.

Logs under `out/build/x64-Release`: `lucent-hud-design-build.log`, `lucent-hud-design-focused.log`, `lucent-hud-design-regression.log`, `lucent-hud-design-network-retry.log`, `lucent-hud-design-regression-without-network.log`.

## Remaining visual limitations

- Human verification is still required for perceived balance, contrast on bright/dark maps, text crispness during animation and custom HUD/game skins. No rendered appearance has been verified here.
- Transparent panels cannot guarantee contrast at very low user opacity. The existing opacity setting is preserved rather than silently overridden.
- Chat exposes no exact drawn-line bounds to this overlay, so existing conservative source-defined avoidance remains. Expanded/full-width chat can suppress alignment presentation when no safe space remains.
- Physical glyph sizes follow the renderer's existing 6–128 px raster range. Very low resolution combined with minimum HUD scale still needs human inspection; font quantization alone is not proof of visual quality.
- Coordinate cells stabilize ordinary values; unusually large values can expand panels. Long alignment names still use the existing UTF-8-safe measured ellipsis.
- Status inventory changes and timer format/visibility changes can change container geometry immediately; alignment reflow and selection/reveal animations are smooth, but no new global layout-animation system was added.
